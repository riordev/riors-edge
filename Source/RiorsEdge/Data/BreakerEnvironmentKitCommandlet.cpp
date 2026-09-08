#include "Data/BreakerEnvironmentKitCommandlet.h"

#if WITH_EDITOR
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshCompiler.h"
#endif

UBreakerEnvironmentKitCommandlet::UBreakerEnvironmentKitCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UBreakerEnvironmentKitCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    struct FSource { const TCHAR* Name; const TCHAR* Relative; };
    const FSource Sources[] = {
        { TEXT("CommonTree_1"), TEXT("stylized-nature-megakit/glTF/CommonTree_1.gltf") },
        { TEXT("DeadTree_1"), TEXT("stylized-nature-megakit/glTF/DeadTree_1.gltf") },
        { TEXT("Bush_Common"), TEXT("stylized-nature-megakit/glTF/Bush_Common.gltf") },
        { TEXT("Fern_1"), TEXT("stylized-nature-megakit/glTF/Fern_1.gltf") },
        { TEXT("Column_Pipes"), TEXT("modular-sci-fi-megakit/glTF/Columns/Column_Pipes.gltf") },
        { TEXT("Column_MetalSupport"), TEXT("modular-sci-fi-megakit/glTF/Columns/Column_MetalSupport.gltf") },
        { TEXT("Door_Metal"), TEXT("modular-sci-fi-megakit/glTF/Platforms/Door_Metal.gltf") }
    };
    const bool bAuditOnly = FParse::Param(*Params, TEXT("AuditOnly"));
    const bool bReplace = FParse::Param(*Params, TEXT("ReplaceExisting"));
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    auto& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    int32 Failures = 0;
    for (const FSource& Source : Sources)
    {
        const FString Destination = FString(TEXT("/Game/Breaker/EnvironmentKit/")) + Source.Name;
        Registry.ScanPathsSynchronous({ Destination }, true);
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPath(FName(*Destination), Assets, true);
        if (!bAuditOnly && (Assets.IsEmpty() || bReplace))
        {
            const FString Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Assets/zones/kit") / Source.Relative);
            if (!FPaths::FileExists(Filename))
            {
                UE_LOG(LogTemp, Error, TEXT("[EnvironmentKit] Missing source: %s"), *Filename);
                ++Failures;
                continue;
            }
            // The vendored sci-fi pack keeps PNGs in Textures although its
            // glTF URIs are basenames. Stage only declared companions, retaining
            // original glTF material definitions and leaving the source untouched.
            FString Json;
            TSharedPtr<FJsonObject> Document;
            if (!FFileHelper::LoadFileToString(Json, *Filename)
                || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Document) || !Document)
            { ++Failures; continue; }
            const FString Stage = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("EnvironmentKitImport") / Source.Name);
            IFileManager::Get().MakeDirectory(*Stage, true);
            bool bCompanionsValid = true;
            for (const TCHAR* Kind : { TEXT("buffers"), TEXT("images") })
            {
                const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
                if (!Document->TryGetArrayField(Kind, Entries)) continue;
                for (const auto& Entry : *Entries)
                {
                    const TSharedPtr<FJsonObject> Object = Entry->AsObject();
                    FString Uri;
                    if (!Object || !Object->TryGetStringField(TEXT("uri"), Uri) || Uri.StartsWith(TEXT("data:"))) continue;
                    // Selected inputs use simple local basenames, never paths.
                    if (Uri.IsEmpty() || Uri != FPaths::GetCleanFilename(Uri) || Uri.Contains(TEXT(":")))
                    { bCompanionsValid = false; continue; }
                    FString Companion = FPaths::GetPath(Filename) / Uri;
                    if (!FPaths::FileExists(Companion))
                        Companion = FPaths::ProjectDir() / TEXT("Assets/zones/kit/modular-sci-fi-megakit/Textures") / Uri;
                    if (!FPaths::FileExists(Companion)
                        || IFileManager::Get().Copy(*(Stage / Uri), *Companion, true) != COPY_OK)
                    {
                        UE_LOG(LogTemp, Error, TEXT("[EnvironmentKit] Missing or uncopyable companion: %s"), *Companion);
                        bCompanionsValid = false;
                    }
                }
            }
            const FString StagedFilename = Stage / FPaths::GetCleanFilename(Filename);
            if (!bCompanionsValid || !FFileHelper::SaveStringToFile(Json, *StagedFilename))
            { ++Failures; continue; }
            UAssetImportTask* Task = NewObject<UAssetImportTask>(this);
            Task->Filename = StagedFilename;
            Task->DestinationPath = Destination;
            Task->bAutomated = true;
            Task->bAsync = false;
            Task->bSave = true;
            Task->bReplaceExisting = bReplace;
            Task->bReplaceExistingSettings = bReplace;
            // Import original glTF with its own materials/textures. No merged
            // geometry bake, override material, or name-dependent slot rewrite.
            AssetTools.ImportAssetTasks({ Task });
            const TArray<UObject*>& Imported = Task->GetObjects();
            UE_LOG(LogTemp, Display, TEXT("[EnvironmentKit] %s: import returned %d objects"), Source.Name, Imported.Num());
            if (Imported.IsEmpty()) { ++Failures; continue; }
            FStaticMeshCompilingManager::Get().FinishAllCompilation();
            Assets.Reset();
            Registry.ScanPathsSynchronous({ Destination }, true);
            Registry.GetAssetsByPath(FName(*Destination), Assets, true);
        }
        else
            UE_LOG(LogTemp, Display, TEXT("[EnvironmentKit] %s: auditing existing destination (no overwrite)"), Source.Name);

        int32 Meshes = 0;
        bool bValid = true;
        for (const FAssetData& Asset : Assets)
        {
            UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
            if (!Mesh) continue;
            ++Meshes;
            const FVector Extent = Mesh->GetBounds().BoxExtent;
            bool bMeshValid = !Extent.ContainsNaN() && Extent.GetMax() > UE_SMALL_NUMBER
                && Mesh->GetNumLODs() > 0 && !Mesh->GetStaticMaterials().IsEmpty();
            int32 TextureCount = 0;
            bool bHasImportedTexture = false;
            for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
            {
                UMaterialInterface* Material = Slot.MaterialInterface;
                if (!Material || Material->GetPathName().StartsWith(TEXT("/Engine/")))
                {
                    bMeshValid = false;
                    continue;
                }
                TSet<const UTexture*> Textures;
                Material->GetReferencedTexturesAndOverrides(Textures);
                TextureCount += Textures.Num();
                for (const UTexture* Texture : Textures)
                    bHasImportedTexture |= Texture && Texture->GetPathName().StartsWith(Destination + TEXT("/"));
                UE_LOG(LogTemp, Display, TEXT("[EnvironmentKit] material %s slot=%s textures=%d"),
                    *Material->GetPathName(), *Slot.MaterialSlotName.ToString(), Textures.Num());
                for (const UTexture* Texture : Textures)
                    UE_LOG(LogTemp, Display, TEXT("[EnvironmentKit] texture %s"), *GetPathNameSafe(Texture));
            }
            bMeshValid &= TextureCount > 0 && bHasImportedTexture;
            UE_LOG(LogTemp, Display, TEXT("[EnvironmentKit] mesh %s extent=%s slots=%d textures=%d audit=%s"),
                *Mesh->GetPathName(), *Extent.ToString(), Mesh->GetStaticMaterials().Num(), TextureCount,
                bMeshValid ? TEXT("PASS") : TEXT("FAIL"));
            bValid &= bMeshValid;
        }
        if (Meshes == 0 || !bValid)
        {
            UE_LOG(LogTemp, Error, TEXT("[EnvironmentKit] %s failed loaded mesh/material audit; inspect saved import before retrying with -ReplaceExisting"), Source.Name);
            ++Failures;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[EnvironmentKit] seven fixed sources audited; failures=%d"), Failures);
    return Failures == 0 ? 0 : 1;
#else
    UE_LOG(LogTemp, Error, TEXT("Environment kit import requires an editor build."));
    return 1;
#endif
}
