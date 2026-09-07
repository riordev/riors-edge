// ---------------------------------------------------------------------------
// ROLE FONT BUILDER — editor-only asset automation.
//
// Imports seven supplied static OFL faces with the editor font factory, then
// builds the display/body/mono runtime composites. Source bytes and all faces
// are preflighted before role mutation. Individual package saves report failure;
// this is not a multi-file disk transaction. Run headless:
//
//   UnrealEditor-Cmd.exe <project> -ExecCmds="BreakerBuildRoleFonts quit"
//       -unattended -nullrhi
//
// Idempotent: an existing role font is rebuilt in place and re-saved. The
// consumers load the fonts by path (BreakerMenu.cpp's BreakerRoleFont) and
// fall back to the engine faces when they are absent, so this command failing
// degrades the menus' look, never their function.
// ---------------------------------------------------------------------------

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Factories/FontFileImportFactory.h"
#include "Fonts/CompositeFont.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    struct FBreakerFontSource { const TCHAR* Face; const TCHAR* File; };
    const FBreakerFontSource BreakerFontSources[] = {
        {TEXT("FF_BarlowCondensedSemiBold"), TEXT("BarlowCondensed-SemiBold.ttf")},
        {TEXT("FF_BarlowCondensedBold"), TEXT("BarlowCondensed-Bold.ttf")},
        {TEXT("FF_SourceSans3Regular"), TEXT("SourceSans3-Regular.ttf")},
        {TEXT("FF_SourceSans3Medium"), TEXT("SourceSans3-Medium.ttf")},
        {TEXT("FF_SourceSans3SemiBold"), TEXT("SourceSans3-SemiBold.ttf")},
        {TEXT("FF_SometypeMonoMedium"), TEXT("SometypeMono-Medium.ttf")},
        {TEXT("FF_SometypeMonoBold"), TEXT("SometypeMono-Bold.ttf")}
    };

    bool BreakerImportFontFaces()
    {
        // Preflight the entire fixed source set before modifying any asset.
        TArray<TArray<uint8>> Payloads;
        for (const auto& Source : BreakerFontSources)
        {
            auto& Bytes = Payloads.AddDefaulted_GetRef();
            const FString Filename = FPaths::ProjectDir() / TEXT("Assets/fonts") / Source.File;
            if (!FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() < 12)
            { UE_LOG(LogTemp, Error, TEXT("[BreakerFonts] Missing/invalid source %s"), *Filename); return false; }
        }
        int32 Index = 0;
        for (const auto& Source : BreakerFontSources)
        {
            const FString PackageName = FString(TEXT("/Game/Breaker/UI/Fonts/Faces/")) + Source.Face;
            UPackage* Package = CreatePackage(*PackageName);
            if (!Package) return false;
            Package->FullyLoad();
            const bool bExisted = FindObject<UFontFace>(Package, Source.Face) != nullptr;
            auto* Factory = NewObject<UFontFileImportFactory>();
            Factory->BatchCreateFontAsset = EBatchCreateFontAsset::No;
            const auto& Bytes = Payloads[Index++];
            const uint8* Buffer = Bytes.GetData();
            auto* Face = Cast<UFontFace>(Factory->FactoryCreateBinary(UFontFace::StaticClass(), Package, FName(Source.Face),
                RF_Public | RF_Standalone, nullptr, TEXT("ttf"), Buffer, Buffer + Bytes.Num(), GWarn));
            if (!Face) { UE_LOG(LogTemp, Error, TEXT("[BreakerFonts] Import failed %s"), Source.Face); return false; }
            Face->SourceFilename = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Assets/fonts") / Source.File);
            Face->MarkPackageDirty();
            if (!bExisted) FAssetRegistryModule::AssetCreated(Face);
            FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
            const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
            if (!UPackage::SavePackage(Package, Face, *Filename, Args)) return false;
            UE_LOG(LogTemp, Display, TEXT("[BreakerFonts] imported %s"), Source.Face);
        }
        return true;
    }

    UFontFace* BreakerLoadFace(const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Breaker/UI/Fonts/Faces/%s.%s"), Name, Name);
        UFontFace* Face = LoadObject<UFontFace>(nullptr, *Path);
        if (!Face)
        {
            UE_LOG(LogTemp, Error, TEXT("[BreakerFonts] missing face %s"), *Path);
        }
        return Face;
    }

    bool BreakerBuildRoleFont(const TCHAR* AssetName, const TArray<TPair<FName, const TCHAR*>>& Entries)
    {
        TArray<UFontFace*> Faces;
        for (const auto& Entry : Entries)
        {
            auto* Face = BreakerLoadFace(Entry.Value);
            if (!Face) return false;
            Faces.Add(Face);
        }
        const FString PackageName = FString::Printf(TEXT("/Game/Breaker/UI/Fonts/%s"), AssetName);
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package) return false;
        Package->FullyLoad();

        UFont* Font = FindObject<UFont>(Package, AssetName);
        const bool bExisted = Font != nullptr;
        if (!Font)
        {
            Font = NewObject<UFont>(Package, FName(AssetName), RF_Public | RF_Standalone);
        }
        if (!Font) return false;

        // RUNTIME, not OFFLINE: Slate composites glyphs live, and an
        // offline-cached font renders nothing through FSlateFontInfo.
        Font->FontCacheType = EFontCacheType::Runtime;
        // Through the accessors, not the member. UFont::CompositeFont has been
        // UE_DEPRECATED since 5.7 — public access still COMPILES on 5.8 and only
        // warns, which is exactly how a deprecation reaches the release that
        // removes it. Mutating writes take GetMutableInternalCompositeFont();
        // the read below takes the const one.
        Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Empty();
        int32 FaceIndex = 0;
        for (const TPair<FName, const TCHAR*>& Entry : Entries)
        {
            FTypefaceEntry& Typeface = Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.AddDefaulted_GetRef();
            Typeface.Name = Entry.Key;
            Typeface.Font = FFontData(Faces[FaceIndex++]);
        }

        Font->MarkPackageDirty();
        if (!bExisted)
        {
            FAssetRegistryModule::AssetCreated(Font);
        }

        const FString FileName = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const bool bSaved = UPackage::SavePackage(Package, Font, *FileName, SaveArgs);
        UE_LOG(LogTemp, Display, TEXT("[BreakerFonts] %s %s (%d typefaces)"),
            AssetName, bSaved ? TEXT("saved") : TEXT("FAILED TO SAVE"),
            Font->GetInternalCompositeFont().DefaultTypeface.Fonts.Num());
        return bSaved;
    }

    void BreakerBuildRoleFonts(const TArray<FString>& Args)
    {
        int32 Built = 0;
        bool bReady = BreakerImportFontFaces();
        // No role can be rewritten until every imported face is loadable.
        if (bReady) for (const auto& Source : BreakerFontSources) bReady = BreakerLoadFace(Source.Face) != nullptr && bReady;
        // Token-sheet families: display 600/700, body 400/500/600,
        // numeric 500/700 behind stable Regular/Medium role keys.
        if (bReady)
        {
            Built += BreakerBuildRoleFont(TEXT("F_BreakerDisplay"),
                { { FName(TEXT("SemiBold")), TEXT("FF_BarlowCondensedSemiBold") },
                  { FName(TEXT("Bold")), TEXT("FF_BarlowCondensedBold") } }) ? 1 : 0;
            Built += BreakerBuildRoleFont(TEXT("F_BreakerBody"),
                { { FName(TEXT("Regular")), TEXT("FF_SourceSans3Regular") },
                  { FName(TEXT("Medium")), TEXT("FF_SourceSans3Medium") },
                  { FName(TEXT("SemiBold")), TEXT("FF_SourceSans3SemiBold") } }) ? 1 : 0;
            Built += BreakerBuildRoleFont(TEXT("F_BreakerMono"),
                { { FName(TEXT("Regular")), TEXT("FF_SometypeMonoMedium") },
                  { FName(TEXT("Medium")), TEXT("FF_SometypeMonoBold") } }) ? 1 : 0;
        }
        UE_LOG(LogTemp, Display, TEXT("[BreakerFonts] built %d of 3 role fonts"), Built);

        // A headless -ExecCmds run has no automation framework to split a
        // trailing "; SoftQuit" for it — the first attempt at this sat in an
        // idle editor forever on exactly that. So the command carries its own
        // exit: pass "quit" and it requests a graceful engine exit once the
        // saves are flushed. Interactive use omits the argument and keeps the
        // editor.
        const bool bQuit = Args.ContainsByPredicate(
            [](const FString& Arg) { return Arg.Equals(TEXT("quit"), ESearchCase::IgnoreCase); });
        if (bQuit)
        {
            RequestEngineExit(TEXT("BreakerBuildRoleFonts complete"));
        }
    }

    FAutoConsoleCommand BreakerBuildRoleFontsCmd(
        TEXT("BreakerBuildRoleFonts"),
        TEXT("Builds the three Fieldplate role fonts (display/body/mono) from the imported font faces and saves them. Pass 'quit' to exit afterward (headless runs)."),
        FConsoleCommandWithArgsDelegate::CreateStatic(&BreakerBuildRoleFonts));
}

#endif // WITH_EDITOR
