#include "Data/BreakerCensusCommandlet.h"

#include "Data/BreakerCensus.h"
#include "Interaction/BreakerNPC.h"
#include "Items/BreakerAffixLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBreakerCensus, Log, All);

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
    UE_LOG(LogBreakerCensus, Display, TEXT("wrote %s: %d slice, %d aberrant, %d anomalous, %d downside, %d elemental, %d leans across %d archetypes, %d caps"),
        *AffixPath, Affixes.Slice.Num(), Affixes.Aberrant.Num(), Affixes.Anomalous.Num(), Affixes.Downsides.Num(),
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
    return 0;
}
