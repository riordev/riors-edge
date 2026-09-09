#include "Data/BreakerCensusCommandlet.h"

#include "Abilities/BreakerAbilityData.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Data/BreakerCensus.h"
#include "Interaction/BreakerNPC.h"
#include "Items/BreakerAffixLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Misc/Parse.h"
#include "AssetCompilingManager.h"
#if WITH_EDITOR
#include "Game/BreakerGameMode.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogBreakerCensus, Log, All);

namespace
{
    int32 BreakerCreateRescueMap(const TCHAR* MapName = TEXT("Lvl_ErasedEarth"))
    {
#if WITH_EDITOR
        const FString PackageName = FString::Printf(TEXT("/Game/Breaker/Maps/%s"), MapName);
        const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
        // This is a creation command, never an editor or migration for an existing map.
        if (FPackageName::DoesPackageExist(PackageName) || IFileManager::Get().FileExists(*Filename)
            || FindPackage(nullptr, *PackageName))
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("[CreateRescueMap] Refusing existing package %s"), *PackageName);
            return 1;
        }
        UPackage* Package = CreatePackage(*PackageName);
        UWorld::InitializationValues Init;
        Init.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
        UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(MapName), Package,
            true, ERHIFeatureLevel::Num, &Init);
        if (!World)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("[CreateRescueMap] World creation failed: %s"), *PackageName);
            return 1;
        }
        ON_SCOPE_EXIT { World->DestroyWorld(false); };
        World->SetFlags(RF_Public | RF_Standalone);
        Package->SetPackageFlags(PKG_ContainsMap);
        AWorldSettings* Settings = World->GetWorldSettings();
        APlayerStart* Start = World->SpawnActor<APlayerStart>(FVector(0, 0, 200), FRotator::ZeroRotator);
        ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1200), FRotator(-38, -35, 0));
        ASkyLight* Sky = World->SpawnActor<ASkyLight>();
        AActor* Atmosphere = World->SpawnActor<AActor>();
        if (!Settings || !Start || !Sun || !Sky || !Atmosphere)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("[CreateRescueMap] Required map actor creation failed: %s"), *PackageName);
            return 1;
        }
        Settings->DefaultGameMode = ABreakerGameMode::StaticClass();
        Start->SetActorLabel(TEXT("Rescue Arrival"));
        Sun->SetActorLabel(TEXT("Garden Daylight"));
        Sun->GetComponent()->SetMobility(EComponentMobility::Movable);
        // O2 presentation placeholders: a warm readable afternoon, no baked-light dependency.
        Sun->GetComponent()->SetIntensity(5.0f);
        Sun->GetComponent()->SetLightColor(FLinearColor(1.0f, 0.94f, 0.82f));
        Sun->GetComponent()->SetAtmosphereSunLight(true);
        Sky->SetActorLabel(TEXT("Garden Sky Fill"));
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(1.0f);
        Sky->GetLightComponent()->SetRealTimeCapture(true);
        Atmosphere->SetActorLabel(TEXT("Erased Earth Atmosphere"));
        USkyAtmosphereComponent* Air = NewObject<USkyAtmosphereComponent>(Atmosphere, TEXT("Atmosphere"));
        Atmosphere->AddInstanceComponent(Air);
        Atmosphere->SetRootComponent(Air);
        Air->RegisterComponent();
        Package->MarkPackageDirty();
        if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("[CreateRescueMap] Cannot create output directory: %s"), *Filename);
            return 1;
        }
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, World, *Filename, SaveArgs))
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("[CreateRescueMap] Save failed: %s"), *Filename);
            return 1;
        }
        UE_LOG(LogBreakerCensus, Display, TEXT("[CreateRescueMap] Created %s; runtime campaign builder supplies terrain and encounters."), *Filename);
        return 0;
#else
        UE_LOG(LogBreakerCensus, Error, TEXT("[CreateRescueMap] Requires an editor build."));
        return 1;
#endif
    }

    int32 BreakerAuditArmMeshes()
    {
#if WITH_EDITOR
        int32 Failures = 0;
        for (const TCHAR* Path : { TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"),
            TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple") })
        {
            USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Path);
            if (!Mesh) { UE_LOG(LogBreakerCensus, Error, TEXT("[MeshAudit] Missing %s"), Path); ++Failures; continue; }
            FAssetCompilingManager::Get().FinishAllCompilation();
            const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
            if (!Model || Model->LODModels.IsEmpty())
            { UE_LOG(LogBreakerCensus, Error, TEXT("[MeshAudit] No imported LOD0 for %s"), Path); ++Failures; continue; }
            const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
            UE_LOG(LogBreakerCensus, Display, TEXT("[MeshAudit] %s bounds origin=%s extent=%s sections=%d"), Path,
                *Mesh->GetBounds().Origin.ToString(), *Mesh->GetBounds().BoxExtent.ToString(), Model->LODModels[0].Sections.Num());
            int32 ArmOnlySections = 0;
            for (int32 SectionIndex = 0; SectionIndex < Model->LODModels[0].Sections.Num(); ++SectionIndex)
            {
                const FSkelMeshSection& Section = Model->LODModels[0].Sections[SectionIndex];
                TArray<FString> BoneNames, WeightedArms, WeightedBody;
                for (FBoneIndexType Bone : Section.BoneMap)
                    if (Bone < Skeleton.GetNum()) BoneNames.Add(Skeleton.GetBoneName(Bone).ToString());
                FBox Bounds(ForceInit);
                bool bUnprovenWeights = Section.SoftVertices.IsEmpty();
                for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
                {
                    Bounds += FVector(Vertex.Position);
                    bool bHasValidInfluence = false;
                    for (int32 Influence = 0; Influence < UE_ARRAY_COUNT(Vertex.InfluenceWeights); ++Influence)
                    {
                        if (Vertex.InfluenceWeights[Influence] == 0) continue;
                        if (!Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[Influence])) { bUnprovenWeights = true; continue; }
                        const int32 Bone = Section.BoneMap[Vertex.InfluenceBones[Influence]];
                        if (Bone >= Skeleton.GetNum()) { bUnprovenWeights = true; continue; }
                        bHasValidInfluence = true;
                        bool bArm = false;
                        for (int32 Ancestor = Bone; Ancestor != INDEX_NONE; Ancestor = Skeleton.GetParentIndex(Ancestor))
                        {
                            const FString Name = Skeleton.GetBoneName(Ancestor).ToString();
                            if (Name.StartsWith(TEXT("clavicle_")) || Name.StartsWith(TEXT("upperarm_"))) { bArm = true; break; }
                        }
                        (bArm ? WeightedArms : WeightedBody).AddUnique(Skeleton.GetBoneName(Bone).ToString());
                    }
                    bUnprovenWeights |= !bHasValidInfluence;
                }
                const bool bArmOnly = !bUnprovenWeights && !WeightedArms.IsEmpty() && WeightedBody.IsEmpty();
                ArmOnlySections += bArmOnly ? 1 : 0;
                const FSkeletalMaterial* Material = Mesh->GetMaterials().IsValidIndex(Section.MaterialIndex) ? &Mesh->GetMaterials()[Section.MaterialIndex] : nullptr;
                UE_LOG(LogBreakerCensus, Display, TEXT("[MeshAudit] section=%d material=%d slot=%s asset=%s triangles=%d softVertices=%d bounds=%s armOnly=%s"),
                    SectionIndex, Section.MaterialIndex, Material ? *Material->MaterialSlotName.ToString() : TEXT("missing"),
                    Material && Material->MaterialInterface ? *Material->MaterialInterface->GetPathName() : TEXT("none"),
                    Section.NumTriangles, Section.SoftVertices.Num(), *Bounds.ToString(), bArmOnly ? TEXT("YES") : TEXT("NO/UNPROVEN"));
                UE_LOG(LogBreakerCensus, Display, TEXT("[MeshAudit] boneMap=[%s] weightedArm=[%s] weightedTorsoOrOther=[%s]"),
                    *FString::Join(BoneNames, TEXT(",")), *FString::Join(WeightedArms, TEXT(",")), *FString::Join(WeightedBody, TEXT(",")));
            }
            UE_LOG(LogBreakerCensus, Display, TEXT("[MeshAudit] isolatedArmSections=%d; mixed weighted sections cannot be isolated by section visibility alone. Empty soft vertices are unproven, not safe."), ArmOnlySections);
        }
        return Failures ? 1 : 0;
#else
        UE_LOG(LogBreakerCensus, Error, TEXT("[MeshAudit] Imported section audit requires an editor build."));
        return 1;
#endif
    }
}

UBreakerCensusCommandlet::UBreakerCensusCommandlet()
{
    IsClient = false;
    IsServer = false;
    // An EDITOR commandlet, deliberately. With IsEditor false the process
    // boots the game engine without GEditor, and the Kismet module asserts
    // on load before Main() is reached.
    IsEditor = true;
    LogToConsole = true;
}

int32 UBreakerCensusCommandlet::Main(const FString& Params)
{
    if (FParse::Param(*Params, TEXT("CreateRescueMap"))) return BreakerCreateRescueMap();
    if (FParse::Param(*Params, TEXT("CreateFinaleMaps")))
    {
        int32 Failures = 0;
        // Fixed authored destinations only; no caller-supplied package path.
        // An existing map is refused individually, never overwritten. The
        // other missing map can still be created after a partial prior run.
        for (const TCHAR* MapName : { TEXT("Lvl_StrippedEarth"), TEXT("Lvl_WinningEarth") })
        {
            const int32 Result = BreakerCreateRescueMap(MapName);
            UE_LOG(LogBreakerCensus, Display, TEXT("[CreateFinaleMaps] %s: %s"), MapName,
                Result == 0 ? TEXT("CREATED") : TEXT("FAILED OR EXISTING (not overwritten)"));
            Failures += Result != 0 ? 1 : 0;
        }
        return Failures == 0 ? 0 : 1;
    }
    // Optional read-only geometry probe exits BEFORE every canonical data export.
    if (FParse::Param(*Params, TEXT("MeshAudit"))) return BreakerAuditArmMeshes();
    const TArray<UBreakerProgressionTree*>& Trees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    const FString Json = BreakerCensus::Serialize(BreakerCensus::Export(Trees));

    int32 NodeCount = 0;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        NodeCount += Tree ? Tree->Nodes.Num() : 0;
    }

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::RelativePath());
    if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *Path);
        return 1;
    }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d trees, %d nodes"), *Path, Trees.Num(), NodeCount);

    // The affix library, re-serialised from what the game loaded. A file that
    // failed validation loaded as EMPTY pools, and writing those back would
    // erase the library the author was mid-way through editing — so a dirty
    // load refuses to write and reports every complaint instead.
    const TArray<FString>& AffixErrors = UBreakerAffixLibrary::GetDataErrors();
    if (!AffixErrors.IsEmpty())
    {
        for (const FString& Error : AffixErrors)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("%s"), *Error);
        }
        UE_LOG(LogBreakerCensus, Error, TEXT("%s did not load clean; not rewriting it"), *BreakerCensus::AffixesRelativePath());
        return 1;
    }
    const FBreakerAffixLibraryData& Affixes = UBreakerAffixLibrary::GetData();
    const FString AffixJson = BreakerCensus::ExportAffixes(Affixes);
    const FString AffixPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::AffixesRelativePath());
    if (!FFileHelper::SaveStringToFile(AffixJson, *AffixPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *AffixPath);
        return 1;
    }
    int32 LeanRows = 0;
    for (const FBreakerArchetypeLeans& Table : Affixes.Leans) { LeanRows += Table.Rows.Num(); }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d slice, %d aberrant, %d unwritten, %d downside, %d elemental, %d leans across %d archetypes, %d caps"),
        *AffixPath, Affixes.Slice.Num(), Affixes.Aberrant.Num(), Affixes.Unwritten.Num(), Affixes.Downsides.Num(),
        Affixes.Elemental.AffixId.IsNone() ? 0 : 1, LeanRows, Affixes.Leans.Num(), Affixes.Caps.Num());

    // The quest registry, by the same rule: a dirty load is an EMPTY registry
    // and is never written back over the file.
    const TArray<FString>& QuestErrors = UBreakerQuestLibrary::GetDataErrors();
    if (!QuestErrors.IsEmpty())
    {
        for (const FString& Error : QuestErrors)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("%s"), *Error);
        }
        UE_LOG(LogBreakerCensus, Error, TEXT("%s did not load clean; not rewriting it"), *BreakerCensus::QuestsRelativePath());
        return 1;
    }
    const TArray<FBreakerQuestDefinition>& Quests = UBreakerQuestLibrary::GetFallbackQuests();
    const TArray<FName>& Flags = UBreakerQuestLibrary::GetRegisteredFlags();
    const FString QuestJson = BreakerCensus::ExportQuests(Quests, Flags);
    const FString QuestPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::QuestsRelativePath());
    if (!FFileHelper::SaveStringToFile(QuestJson, *QuestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *QuestPath);
        return 1;
    }
    int32 ObjectiveCount = 0;
    for (const FBreakerQuestDefinition& Quest : Quests) { ObjectiveCount += Quest.Objectives.Num(); }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d quests, %d objectives, %d flags"),
        *QuestPath, Quests.Num(), ObjectiveCount, Flags.Num());

    // The dialogue. The file carries em dashes, so it is written UTF-8
    // without BOM like the others and the writer leaves them unescaped.
    const TArray<FString>& DialogueErrors = ABreakerNPC::GetDialogueErrors();
    if (!DialogueErrors.IsEmpty())
    {
        for (const FString& Error : DialogueErrors)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("%s"), *Error);
        }
        UE_LOG(LogBreakerCensus, Error, TEXT("%s did not load clean; not rewriting it"), *BreakerCensus::DialogueRelativePath());
        return 1;
    }
    const FBreakerDialogueData& Dialogue = ABreakerNPC::GetDialogueData();
    const FString DialogueJson = BreakerCensus::ExportDialogue(Dialogue);
    const FString DialoguePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::DialogueRelativePath());
    if (!FFileHelper::SaveStringToFile(DialogueJson, *DialoguePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *DialoguePath);
        return 1;
    }
    int32 DialogueNodes = 0;
    int32 DialogueChoices = 0;
    int32 DialogueEntries = 0;
    for (const FBreakerDialogueRow& Row : Dialogue.Npcs)
    {
        DialogueNodes += Row.Nodes.Num();
        DialogueEntries += Row.Entries.Num();
        for (const FBreakerDialogueNode& Node : Row.Nodes) { DialogueChoices += Node.Choices.Num(); }
    }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d npcs, %d nodes, %d choices, %d entries"),
        *DialoguePath, Dialogue.Npcs.Num(), DialogueNodes, DialogueChoices, DialogueEntries);

    // The missions, last, after the registries they resolve against. Same
    // rule: a dirty load is an EMPTY list and is never written back.
    const TArray<FString>& MissionErrors = UBreakerMissionLibrary::GetDataErrors();
    if (!MissionErrors.IsEmpty())
    {
        for (const FString& Error : MissionErrors)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("%s"), *Error);
        }
        UE_LOG(LogBreakerCensus, Error, TEXT("%s did not load clean; not rewriting it"), *BreakerCensus::MissionsRelativePath());
        return 1;
    }
    for (const FString& Warning : UBreakerMissionLibrary::GetDataWarnings())
    {
        UE_LOG(LogBreakerCensus, Warning, TEXT("%s"), *Warning);
    }
    const TArray<FBreakerMissionRift>& Rifts = UBreakerMissionLibrary::GetRifts();
    const TArray<FBreakerMissionDefinition>& Missions = UBreakerMissionLibrary::GetMissions();
    const FString MissionJson = BreakerCensus::ExportMissions(Rifts, Missions);
    const FString MissionPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::MissionsRelativePath());
    if (!FFileHelper::SaveStringToFile(MissionJson, *MissionPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *MissionPath);
        return 1;
    }
    int32 BeatCount = 0;
    for (const FBreakerMissionDefinition& Mission : Missions) { BeatCount += Mission.Beats.Num(); }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d missions, %d beats, %d rifts"),
        *MissionPath, Missions.Num(), BeatCount, Rifts.Num());

    // The ability numerics. A dirty load leaves every row at zero cost, zero
    // cooldown and zero window, and writing those back would erase the
    // table — so, as with the others, a dirty load refuses to write.
    const TArray<FString>& AbilityErrors = BreakerAbilityData::GetDataErrors();
    if (!AbilityErrors.IsEmpty())
    {
        for (const FString& Error : AbilityErrors)
        {
            UE_LOG(LogBreakerCensus, Error, TEXT("%s"), *Error);
        }
        UE_LOG(LogBreakerCensus, Error, TEXT("%s did not load clean; not rewriting it"), *BreakerCensus::AbilitiesRelativePath());
        return 1;
    }
    const TArray<UBreakerAbilityDefinition*>& Abilities = UBreakerAbilityDefinition::GetFallbackRegistry();
    const FString AbilityJson = BreakerCensus::ExportAbilities(Abilities);
    const FString AbilityPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::AbilitiesRelativePath());
    if (!FFileHelper::SaveStringToFile(AbilityJson, *AbilityPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBreakerCensus, Error, TEXT("could not write %s"), *AbilityPath);
        return 1;
    }
    int32 UltimateCount = 0;
    int32 VariantCount = 0;
    for (const UBreakerAbilityDefinition* Definition : Abilities)
    {
        if (!Definition) { continue; }
        if (Definition->IsUltimate()) { ++UltimateCount; }
        VariantCount += Definition->Variants.Num();
    }
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d abilities, %d ultimates, %d variants"),
        *AbilityPath, Abilities.Num(), UltimateCount, VariantCount);
    return 0;
}
