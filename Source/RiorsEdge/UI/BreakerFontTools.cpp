// ---------------------------------------------------------------------------
// ROLE FONT BUILDER — editor-only asset automation.
//
// Scripts/import_fonts.py imports the seven Fieldplate TTFs as UFontFace
// assets, but the composite half of the pack's instruction — "build one Font
// Family per role" — cannot be scripted from Python: FFontData and
// FTypefaceEntry carry no Python wrappers. This command is that missing half,
// in the C++ the structs actually live in. Run headless:
//
//   UnrealEditor-Cmd.exe <project> -ExecCmds="BreakerBuildRoleFonts; SoftQuit"
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
#include "Fonts/CompositeFont.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    UFontFace* BreakerLoadFace(const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Breaker/UI/Fonts/Faces/%s.%s"), Name, Name);
        UFontFace* Face = LoadObject<UFontFace>(nullptr, *Path);
        if (!Face)
        {
            UE_LOG(LogTemp, Error, TEXT("[BreakerFonts] missing face %s — run Scripts/import_fonts.py first"), *Path);
        }
        return Face;
    }

    bool BreakerBuildRoleFont(const TCHAR* AssetName, const TArray<TPair<FName, const TCHAR*>>& Entries)
    {
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
        for (const TPair<FName, const TCHAR*>& Entry : Entries)
        {
            UFontFace* Face = BreakerLoadFace(Entry.Value);
            if (!Face) return false;
            FTypefaceEntry& Typeface = Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.AddDefaulted_GetRef();
            Typeface.Name = Entry.Key;
            Typeface.Font = FFontData(Face);
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
        // Weights exactly as the pack states them: display 600/700, body
        // 400/500/600, mono 400/500.
        Built += BreakerBuildRoleFont(TEXT("F_BreakerDisplay"),
            { { FName(TEXT("SemiBold")), TEXT("FF_ArchivoSemiBold") },
              { FName(TEXT("Bold")), TEXT("FF_ArchivoBold") } }) ? 1 : 0;
        Built += BreakerBuildRoleFont(TEXT("F_BreakerBody"),
            { { FName(TEXT("Regular")), TEXT("FF_PlexSansRegular") },
              { FName(TEXT("Medium")), TEXT("FF_PlexSansMedium") },
              { FName(TEXT("SemiBold")), TEXT("FF_PlexSansSemiBold") } }) ? 1 : 0;
        Built += BreakerBuildRoleFont(TEXT("F_BreakerMono"),
            { { FName(TEXT("Regular")), TEXT("FF_PlexMonoRegular") },
              { FName(TEXT("Medium")), TEXT("FF_PlexMonoMedium") } }) ? 1 : 0;
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
