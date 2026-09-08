#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Abilities/BreakerAbilityData.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Data/BreakerCensus.h"
#include "Interaction/BreakerNPC.h"
#include "Items/BreakerAffixLibrary.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerWorldPoints.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// THE COMMITTED CENSUS IS THE LIVE LIBRARY, OR THIS IS RED.
//
// Docs/STATE.md regenerates from Data/progression.json (status.py
// --from-data). A node edit that lands without a re-export would make the
// report measure last week's trees and say nothing about it: the silent
// staleness the census moved to data to get rid of. So the file is pinned to
// a fresh export of the same trees, byte for byte after line endings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCensusFreshTest,
    "RiorsEdge.Data.Census.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCensusFreshTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerProgressionTree*>& Trees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    int32 NodeCount = 0;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        NodeCount += Tree ? Tree->Nodes.Num() : 0;
    }
    // The shipped configuration: an export of nothing cannot be fresh.
    TestEqual(TEXT("Every fallback tree is exported"), Trees.Num(), 16);
    TestTrue(TEXT("The census carries the authored library, not a stub"), NodeCount >= 100);

    const FString Fresh = BreakerCensus::Serialize(BreakerCensus::Export(Trees));

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::RelativePath());
    FString Committed;
    if (!FFileHelper::LoadFileToString(Committed, *Path))
    {
        AddError(FString::Printf(TEXT("%s is missing. Run `bash Scripts/ue-census.sh` and commit the file."), *Path));
        return false;
    }
    Committed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

    if (Committed != Fresh)
    {
        AddError(FString::Printf(
            TEXT("%s is STALE against the built trees (%d trees, %d nodes). ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/progression.json in the same change as the library edit."),
            *Path, Trees.Num(), NodeCount));
        return false;
    }
    return true;
}

// THE COMMITTED AFFIX FILE IS THE LOADED LIBRARY, OR THIS IS RED.
//
// Data/affixes.json is what UBreakerAffixLibrary loads, so the file cannot go
// stale against the pools the way the census could against the trees; what
// it CAN do is drift from the canonical spelling — a row out of pool order,
// a hand-typed "helmet", a number written with a trailing zero — and load
// fine while the commandlet would rewrite it differently on the next run.
// Pinning the file to a re-export of what was loaded keeps one spelling, so
// a diff on this file is always a change of content and never of form. The
// validator's complaints are reported first: a broken file should name its
// breaks, not fail a byte comparison.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixesFreshTest,
    "RiorsEdge.Data.Affixes.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixesFreshTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = UBreakerAffixLibrary::GetDataErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    const FBreakerAffixLibraryData& Data = UBreakerAffixLibrary::GetData();
    int32 LeanRows = 0;
    for (const FBreakerArchetypeLeans& Table : Data.Leans)
    {
        LeanRows += Table.Rows.Num();
    }
    // The shipped configuration: an export of nothing cannot be fresh.
    TestTrue(TEXT("The slice pool carries the authored library, not a stub"), Data.Slice.Num() >= 20);
    TestTrue(TEXT("The Aberrant pool is populated"), Data.Aberrant.Num() > 0);
    TestTrue(TEXT("The Anomalous pool is populated"), Data.Anomalous.Num() > 0);
    TestTrue(TEXT("The downside pool is populated"), Data.Downsides.Num() > 0);
    TestTrue(TEXT("The elemental row is Core.ElementalResist"),
        Data.Elemental.AffixId == FName(TEXT("Core.ElementalResist")));
    TestEqual(TEXT("Every archetype has a lean table"),
        Data.Leans.Num(), static_cast<int32>(EBreakerWeaponArchetype::Count));
    TestTrue(TEXT("The lean tables carry rows"), LeanRows > 0);
    AddInfo(FString::Printf(TEXT("Affix library: %d slice, %d aberrant, %d anomalous, %d downside, 1 elemental, %d leans across %d archetypes, %d caps"),
        Data.Slice.Num(), Data.Aberrant.Num(), Data.Anomalous.Num(), Data.Downsides.Num(), LeanRows, Data.Leans.Num(), Data.Caps.Num()));

    const FString Fresh = BreakerCensus::ExportAffixes(Data);

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::AffixesRelativePath());
    FString Committed;
    if (!FFileHelper::LoadFileToString(Committed, *Path))
    {
        AddError(FString::Printf(TEXT("%s is missing. Run `bash Scripts/ue-census.sh` and commit the file."), *Path));
        return false;
    }
    Committed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

    if (Committed != Fresh)
    {
        AddError(FString::Printf(
            TEXT("%s is not in canonical form against what the library loaded. ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/affixes.json in the same change."),
            *Path));
        return false;
    }
    return true;
}

// THE COMMITTED QUEST FILE IS THE LOADED REGISTRY, OR THIS IS RED.
//
// Data/quests.json is what UBreakerQuestLibrary loads: the flag registry and
// the Act I chain. Same pin as the affix file — one canonical spelling, so a
// diff on it is always a change of content — plus the shipped configuration:
// the chain in play order, the registry at its full size, and the progress
// counters kept OUT of it (they are counter identifiers, not gates).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerQuestsFreshTest,
    "RiorsEdge.Data.Quests.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerQuestsFreshTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = UBreakerQuestLibrary::GetDataErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    const TArray<FBreakerQuestDefinition>& Quests = UBreakerQuestLibrary::GetFallbackQuests();
    const TArray<FName>& Flags = UBreakerQuestLibrary::GetRegisteredFlags();

    TestEqual(TEXT("Three acts author eight quests"), Quests.Num(), 8);
    const TArray<FName> ChainOrder = { TEXT("Quest.FirstContract"), TEXT("Quest.KessSalvage"), TEXT("Quest.Pattern"), TEXT("Quest.Deeper") };
    for (int32 Index = 0; Index < ChainOrder.Num() && Index < Quests.Num(); ++Index)
    {
        TestEqual(TEXT("File order is chain order"), Quests[Index].QuestId, ChainOrder[Index]);
    }
    int32 ObjectiveCount = 0;
    for (const FBreakerQuestDefinition& Quest : Quests) { ObjectiveCount += Quest.Objectives.Num(); }
    TestEqual(TEXT("Fourteen objectives including rescue and finale"), ObjectiveCount, 14);
    TestEqual(TEXT("Forty-five registered flags"), Flags.Num(), 45);
    TestTrue(TEXT("A quest flag is registered"), UBreakerQuestLibrary::IsRegisteredFlag(BreakerQuestFlags::FirstContractTurnedIn));
    TestFalse(TEXT("A progress counter is not a registered flag"), UBreakerQuestLibrary::IsRegisteredFlag(BreakerQuestFlags::FirstContractKillCounter));
    AddInfo(FString::Printf(TEXT("Quest registry: %d quests, %d objectives, %d flags"), Quests.Num(), ObjectiveCount, Flags.Num()));

    const FString Fresh = BreakerCensus::ExportQuests(Quests, Flags);

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::QuestsRelativePath());
    FString Committed;
    if (!FFileHelper::LoadFileToString(Committed, *Path))
    {
        AddError(FString::Printf(TEXT("%s is missing. Run `bash Scripts/ue-census.sh` and commit the file."), *Path));
        return false;
    }
    Committed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

    if (Committed != Fresh)
    {
        AddError(FString::Printf(
            TEXT("%s is not in canonical form against what the library loaded. ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/quests.json in the same change."),
            *Path));
        return false;
    }
    return true;
}

// THE COMMITTED DIALOGUE FILE IS THE LOADED CONVERSATION, OR THIS IS RED.
//
// Data/dialogue.json is what ABreakerNPC loads for both Anchor NPCs. Same pin
// as the other two data files, plus the shipped configuration: the two rows
// by id and the node, choice and entry counts the walkers in
// BreakerQuestLoopTests exercise.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDialogueFreshTest,
    "RiorsEdge.Data.Dialogue.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDialogueFreshTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = ABreakerNPC::GetDialogueErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    const FBreakerDialogueData& Data = ABreakerNPC::GetDialogueData();
    TestEqual(TEXT("Seven authored dialogue rows including physical mission actors"), Data.Npcs.Num(), 7);
    const FBreakerDialogueRow* Kess = Data.Npcs.FindByPredicate([](const FBreakerDialogueRow& Row) { return Row.Id == FName(TEXT("ForgeKeeper")); });
    const FBreakerDialogueRow* Quartermaster = Data.Npcs.FindByPredicate([](const FBreakerDialogueRow& Row) { return Row.Id == FName(TEXT("Quartermaster")); });
    const FBreakerDialogueRow* Survivor = Data.Npcs.FindByPredicate([](const FBreakerDialogueRow& Row) { return Row.Id == FName(TEXT("Survivor")); });
    if (!Kess || !Quartermaster || !Survivor)
    {
        AddError(TEXT("ForgeKeeper, Quartermaster and Survivor are the three rows"));
        return false;
    }
    TestEqual(TEXT("Kess has ten nodes"), Kess->Nodes.Num(), 10);
    TestEqual(TEXT("Kess has three entries"), Kess->Entries.Num(), 3);
    TestEqual(TEXT("The Quartermaster has twenty-seven nodes"), Quartermaster->Nodes.Num(), 27);
    TestEqual(TEXT("The Quartermaster has nineteen entries"), Quartermaster->Entries.Num(), 19);
    TestEqual(TEXT("The Survivor has five conversation stages"), Survivor->Nodes.Num(), 5);
    TestEqual(TEXT("The Survivor has four state overrides"), Survivor->Entries.Num(), 4);
    for (const FName Id : { FName(TEXT("Researcher")), FName(TEXT("FinaleFragment")), FName(TEXT("AlternateSelf")), FName(TEXT("FinaleDevice")) })
        TestTrue(TEXT("Each finale role has its own authored dialogue identity"), Data.Npcs.ContainsByPredicate([Id](const FBreakerDialogueRow& Row) { return Row.Id == Id; }));

    int32 NodeCount = 0;
    int32 ChoiceCount = 0;
    int32 EntryCount = 0;
    for (const FBreakerDialogueRow& Row : Data.Npcs)
    {
        NodeCount += Row.Nodes.Num();
        EntryCount += Row.Entries.Num();
        for (const FBreakerDialogueNode& Node : Row.Nodes) { ChoiceCount += Node.Choices.Num(); }
    }
    TestEqual(TEXT("Fifty-nine nodes"), NodeCount, 59);
    TestEqual(TEXT("One hundred fourteen choices"), ChoiceCount, 114);
    TestEqual(TEXT("Thirty-seven entries"), EntryCount, 37);
    AddInfo(FString::Printf(TEXT("Dialogue: %d npcs, %d nodes, %d choices, %d entries"), Data.Npcs.Num(), NodeCount, ChoiceCount, EntryCount));

    const FString Fresh = BreakerCensus::ExportDialogue(Data);

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::DialogueRelativePath());
    FString Committed;
    if (!FFileHelper::LoadFileToString(Committed, *Path))
    {
        AddError(FString::Printf(TEXT("%s is missing. Run `bash Scripts/ue-census.sh` and commit the file."), *Path));
        return false;
    }
    Committed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

    if (Committed != Fresh)
    {
        AddError(FString::Printf(
            TEXT("%s is not in canonical form against what the NPCs loaded. ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/dialogue.json in the same change."),
            *Path));
        return false;
    }
    return true;
}

// THE COMMITTED MISSION FILE IS THE LOADED STORY, OR THIS IS RED.
//
// Data/missions.json is what UBreakerMissionLibrary loads: the rift list and
// the act-one mission over the four quests. Same pin as the other data files,
// plus the shipped configuration: one mission, its beat count, the arrival
// flag the loader registers, both Core point sources known, and the doctrine
// grant at one benchmark's worth for the one act authored. The unnamed boss
// is the file's one recorded gap and is counted as such.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMissionsFreshTest,
    "RiorsEdge.Data.Missions.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMissionsFreshTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = UBreakerMissionLibrary::GetDataErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    const TArray<FBreakerMissionRift>& Rifts = UBreakerMissionLibrary::GetRifts();
    const TArray<FBreakerMissionDefinition>& Missions = UBreakerMissionLibrary::GetMissions();

    TestEqual(TEXT("Three authored rifts"), Rifts.Num(), 3);
    TestEqual(TEXT("Four authored missions across three acts"), Missions.Num(), 4);
    const FBreakerMissionDefinition* First = Missions.FindByPredicate([](const FBreakerMissionDefinition& Mission) { return Mission.MissionId == TEXT("Act1.Fernhall"); });
    const FBreakerMissionDefinition* Second = Missions.FindByPredicate([](const FBreakerMissionDefinition& Mission) { return Mission.MissionId == TEXT("Act2.Breach"); });
    if (!TestNotNull(TEXT("Original Act I retained"), First) || !TestNotNull(TEXT("Authored Act II retained"), Second)) return false;
    TestEqual(TEXT("Breach remains act two"), Second->Act, 2);
    TestEqual(TEXT("Breach has two earned quests"), Second->Quests.Num(), 2);
    TestEqual(TEXT("Breach has eleven authored beats"), Second->Beats.Num(), 11);
    const FBreakerMissionDefinition* Rescue = Missions.FindByPredicate([](const FBreakerMissionDefinition& Mission) { return Mission.MissionId == TEXT("Act3.Survivor"); });
    if (!TestNotNull(TEXT("Authored Survivor rescue exists"), Rescue)) return false;
    TestEqual(TEXT("Rescue begins Act III"), Rescue->Act, 3);
    TestEqual(TEXT("Rescue has one earned quest"), Rescue->Quests.Num(), 1);
    TestEqual(TEXT("Rescue has eight ordered beats"), Rescue->Beats.Num(), 8);
    const FBreakerMissionDefinition* Finale = Missions.FindByPredicate([](const FBreakerMissionDefinition& Mission) { return Mission.MissionId == TEXT("Act3.Finale"); });
    if (!TestNotNull(TEXT("Finale is authored separately from rescue"), Finale)) return false;
    TestEqual(TEXT("Finale remains in the third act"), Finale->Act, 3);
    TestEqual(TEXT("Finale has eleven ordered beats"), Finale->Beats.Num(), 11);
    const FBreakerMissionDefinition& ActOne = *First;
    TestEqual(TEXT("The mission is Act1.Fernhall"), ActOne.MissionId, FName(TEXT("Act1.Fernhall")));
    TestEqual(TEXT("It is act one"), ActOne.Act, 1);
    TestEqual(TEXT("It runs the four-quest chain"), ActOne.Quests.Num(), 4);
    TestEqual(TEXT("Twenty beats"), ActOne.Beats.Num(), 20);
    TestTrue(TEXT("The loader registers the arrival flag"),
        UBreakerMissionLibrary::IsRegisteredMissionFlag(FName(TEXT("Mission.Act1.Fernhall.Arrived"))));
    TestFalse(TEXT("A quest flag is not a mission flag"),
        UBreakerMissionLibrary::IsRegisteredMissionFlag(BreakerQuestFlags::FirstContractTurnedIn));

    int32 DoctrineSum = 0;
    TArray<FName> CorePoints;
    int32 UnnamedBosses = 0;
    for (const FBreakerMissionBeat& Beat : ActOne.Beats)
    {
        DoctrineSum += Beat.DoctrinePoints;
        if (!Beat.CorePoint.IsNone()) { CorePoints.Add(Beat.CorePoint); }
        if (Beat.Kind == EBreakerMissionBeatKind::Boss && Beat.Boss.IsNone()) { ++UnnamedBosses; }
    }
    // Act I still pays one benchmark. Later chapter grants are checked in the
    // dedicated Act II flow test rather than added to this Act I subtotal.
    TestEqual(TEXT("Act one pays one benchmark of doctrine points"), DoctrineSum, UBreakerProgressionLibrary::DoctrinePointsPerBenchmark);
    TestTrue(TEXT("The file stays within the whole doctrine grant"), DoctrineSum <= UBreakerProgressionLibrary::DoctrinePointGrant);
    TestEqual(TEXT("Two Core points"), CorePoints.Num(), 2);
    TestTrue(TEXT("FirstForge is a known source"), CorePoints.Contains(FName(TEXT("FirstForge"))) && UBreakerWorldPointLibrary::IsKnownSource(FName(TEXT("FirstForge"))));
    TestTrue(TEXT("ActOneBoss is a known source"), CorePoints.Contains(FName(TEXT("ActOneBoss"))) && UBreakerWorldPointLibrary::IsKnownSource(FName(TEXT("ActOneBoss"))));
    TestEqual(TEXT("Every Boss beat names its boss (O214: the Holdfast on the Undercroft)"), UnnamedBosses, 0);
    TestEqual(TEXT("The loader has no unnamed-boss warning left"), UBreakerMissionLibrary::GetDataWarnings().Num(), 0);
    AddInfo(FString::Printf(TEXT("Missions: %d missions, %d beats, %d rifts; %d of %d doctrine points granted, %d of %d Core points; %d warnings"),
        Missions.Num(), ActOne.Beats.Num(), Rifts.Num(),
        DoctrineSum, UBreakerProgressionLibrary::DoctrinePointGrant,
        CorePoints.Num(), UBreakerProgressionLibrary::CoreWorldPointGrant,
        UBreakerMissionLibrary::GetDataWarnings().Num()));

    const FString Fresh = BreakerCensus::ExportMissions(Rifts, Missions);

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::MissionsRelativePath());
    FString Committed;
    if (!FFileHelper::LoadFileToString(Committed, *Path))
    {
        AddError(FString::Printf(TEXT("%s is missing. Run `bash Scripts/ue-census.sh` and commit the file."), *Path));
        return false;
    }
    Committed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

    if (Committed != Fresh)
    {
        AddError(FString::Printf(
            TEXT("%s is not in canonical form against what the library loaded. ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/missions.json in the same change."),
            *Path));
        return false;
    }
    return true;
}

// THE COMMITTED ABILITY FILE IS THE LOADED REGISTRY, OR THIS IS RED.
//
// Data/abilities.json is what the fallback registry overlays its numerics
// from. Same pin as the other data files, plus the shipped configuration:
// the thirty-five rows, five ultimates each carrying a base row and three
// keystones, and no Caster cooldown anywhere (Mana is the cooldown,
// Class-Kits §2.1). The literal value pins live in BreakerAbilityTests and
// BreakerUnbuiltClassTests; this test proves the file is the table those
// read.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitiesFreshTest,
    "RiorsEdge.Data.Abilities.Fresh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitiesFreshTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = BreakerAbilityData::GetDataErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    const TArray<UBreakerAbilityDefinition*>& Abilities = UBreakerAbilityDefinition::GetFallbackRegistry();
    TestEqual(TEXT("Thirty-five rows, seven per class"), Abilities.Num(), 35);

    int32 UltimateCount = 0;
    int32 VariantCount = 0;
    int32 CasterCooldowns = 0;
    for (const UBreakerAbilityDefinition* Definition : Abilities)
    {
        if (!Definition)
        {
            AddError(TEXT("A registry row is null"));
            continue;
        }
        if (Definition->IsUltimate())
        {
            ++UltimateCount;
            TestEqual(FString::Printf(TEXT("%s carries a base row and three keystones"), *Definition->AbilityId.ToString()),
                Definition->Variants.Num(), 4);
        }
        else
        {
            TestEqual(FString::Printf(TEXT("%s carries no variants"), *Definition->AbilityId.ToString()),
                Definition->Variants.Num(), 0);
        }
        VariantCount += Definition->Variants.Num();
        if (Definition->ClassId == EBreakerClassId::Caster && Definition->CooldownSeconds != 0.0f)
        {
            ++CasterCooldowns;
        }
    }
    TestEqual(TEXT("Five ultimates"), UltimateCount, 5);
    TestEqual(TEXT("Twenty variant rows"), VariantCount, 20);
    TestEqual(TEXT("No Caster row has a cooldown: Mana is the cooldown"), CasterCooldowns, 0);
    AddInfo(FString::Printf(TEXT("Ability registry: %d abilities, %d ultimates, %d variants"), Abilities.Num(), UltimateCount, VariantCount));

    const FString Fresh = BreakerCensus::ExportAbilities(Abilities);

    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / BreakerCensus::AbilitiesRelativePath());
    FString Committed;
    if (!FFileHelper::LoadFileToString(Committed, *Path))
    {
        AddError(FString::Printf(TEXT("%s is missing. Run `bash Scripts/ue-census.sh` and commit the file."), *Path));
        return false;
    }
    Committed.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

    if (Committed != Fresh)
    {
        AddError(FString::Printf(
            TEXT("%s is not in canonical form against what the registry loaded. ")
            TEXT("Run `bash Scripts/ue-census.sh` and commit Data/abilities.json in the same change."),
            *Path));
        return false;
    }
    return true;
}

// THE ABILITY CLASSES' NUMBERS ARE THE FILE'S, OR THIS IS RED.
//
// Every EditDefaultsOnly float and int32 an ability class declares below
// UBreakerGameplayAbility is a key on its row's "numbers" object, applied
// onto the class default object at load. The shipped configuration: one
// key per property on every row, one hundred and twenty-two keys across the
// registry (one hundred and nineteen declarations; the Gunsmith deploy
// base's PlacementRangeCm is carried once by each of its four subclasses),
// and a missing key answers with the caller's default.
//
// The file is the tuning authority; compiled initializers are failed-load fallbacks.
// Verify the loaded class defaults, so editing a magnitude needs no C++ edit.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilitiesNumbersTest,
    "RiorsEdge.Data.Abilities.Numbers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilitiesNumbersTest::RunTest(const FString& Parameters)
{
    const TArray<FString>& LoadErrors = BreakerAbilityData::GetDataErrors();
    for (const FString& Error : LoadErrors)
    {
        AddError(Error);
    }
    if (!LoadErrors.IsEmpty())
    {
        return false;
    }

    const TArray<UBreakerAbilityDefinition*>& Abilities = UBreakerAbilityDefinition::GetFallbackRegistry();
    int32 KeyCount = 0;
    int32 RowsWithNumbers = 0;
    for (const UBreakerAbilityDefinition* Definition : Abilities)
    {
        if (!Definition)
        {
            AddError(TEXT("A registry row is null"));
            continue;
        }
        const FString Id = Definition->AbilityId.ToString();
        const TArray<FNumericProperty*> Properties = BreakerAbilityData::NumberProperties(Definition->AbilityClass.Get());
        TestEqual(FString::Printf(TEXT("%s carries one key per numeric property"), *Id), Definition->Numbers.Num(), Properties.Num());
        TestEqual(FString::Printf(TEXT("%s recorded one compiled value per numeric property"), *Id), Definition->CompiledNumbers.Num(), Properties.Num());
        KeyCount += Definition->Numbers.Num();
        if (Properties.Num() > 0)
        {
            ++RowsWithNumbers;
        }
        for (const FNumericProperty* Property : Properties)
        {
            const FName Key = Property->GetFName();
            const float* Authored = Definition->Numbers.Find(Key);
            const float* Compiled = Definition->CompiledNumbers.Find(Key);
            if (!Authored || !Compiled)
            {
                AddError(FString::Printf(TEXT("%s: %s is declared on %s but not carried"), *Id, *Key.ToString(), *Property->GetOwnerClass()->GetName()));
                continue;
            }
            const void* RuntimeValue = Property->ContainerPtrToValuePtr<void>(Definition->AbilityClass->GetDefaultObject());
            const float Applied = Property->IsFloatingPoint()
                ? static_cast<float>(Property->GetFloatingPointPropertyValue(RuntimeValue))
                : static_cast<float>(Property->GetSignedIntPropertyValue(RuntimeValue));
            TestEqual(FString::Printf(TEXT("%s.%s: runtime applies the authored value"), *Id, *Key.ToString()), Applied, *Authored);
            TestEqual(FString::Printf(TEXT("%s.%s: Number reads the authored value"), *Id, *Key.ToString()), Definition->Number(Key, -1.0f), *Authored);
        }
        TestEqual(FString::Printf(TEXT("%s: a key the file does not name answers with the default"), *Id),
            Definition->Number(FName(TEXT("Breaker.NoSuchNumber")), 7.0f), 7.0f);
    }
    TestEqual(TEXT("One hundred and forty-two numbers including Fracture cast duration"), KeyCount, 142);
    TestEqual(TEXT("Twenty-eight rows carry numbers; seven classes keep theirs as constexpr or in the body"), RowsWithNumbers, 28);

    // Order is the class's declaration order, super first: the Gunsmith
    // deploy base's range precedes anything a deployable declares itself,
    // and Cleave's first number is its range.
    const UBreakerAbilityDefinition* Turret = UBreakerAbilityDefinition::FindFallback(FName(TEXT("Gunsmith.Turret")));
    const UBreakerAbilityDefinition* Cleave = UBreakerAbilityDefinition::FindFallback(FName(TEXT("Caster.Cleave")));
    const UBreakerAbilityDefinition* Siphon = UBreakerAbilityDefinition::FindFallback(FName(TEXT("Caster.Siphon")));
    const UBreakerAbilityDefinition* Fracture = UBreakerAbilityDefinition::FindFallback(FName(TEXT("Caster.Fracture")));
    if (TestNotNull(TEXT("Fracture cast timing row is reachable"), Fracture))
        TestEqual(TEXT("Fracture cast duration is authored"), Fracture->Number(TEXT("BaseCastSeconds"), -1), 0.35f);
    if (TestNotNull(TEXT("Siphon Drain tuning row is reachable"), Siphon))
    {
        TestEqual(TEXT("Drain rank-one threshold is authored"), Siphon->Number(TEXT("DrainRankOneThreshold"), -1), 0.10f);
        TestEqual(TEXT("Drain rank-two threshold is authored"), Siphon->Number(TEXT("DrainRankTwoThreshold"), -1), 0.15f);
    }
    const UBreakerAbilityDefinition* Rot = UBreakerAbilityDefinition::FindFallback(FName(TEXT("Caster.Rot")));
    if (TestNotNull(TEXT("Rot node tuning row is reachable"), Rot))
    {
        TestEqual(TEXT("Standing Water first rank is authored"), Rot->Number(TEXT("StandingWaterRankOneManaPerSecond"), -1), 2.0f);
        TestEqual(TEXT("Standing Water second rank is authored"), Rot->Number(TEXT("StandingWaterRankTwoManaPerSecond"), -1), 4.0f);
        TestEqual(TEXT("Zonework flat strip is authored"), Rot->Number(TEXT("ZoneworkAdditionalArmorReduction"), -1), 20.0f);
        TestEqual(TEXT("Lingering one-time growth is authored"), Rot->Number(TEXT("LingeringRefreshGrowthCm"), -1), 100.0f);
        TestEqual(TEXT("Wellspring self-placement reach is authored"), Rot->Number(TEXT("WellspringSelfPlacementRadiusCm"), -1), 150.0f);
        TestEqual(TEXT("Wellspring ground normal is authored"), Rot->Number(TEXT("WellspringMinimumGroundNormalZ"), -1), 0.7f);
    }
    const UBreakerAbilityDefinition* Closequarter = UBreakerAbilityDefinition::FindFallback(FName(TEXT("Caster.Closequarter")));
    if (TestNotNull(TEXT("Closequarter node tuning row is reachable"), Closequarter))
    {
        TestEqual(TEXT("Momentum Transfer first rank is authored"), Closequarter->Number(TEXT("MomentumTransferRankOneSeconds"), -1), 2.0f);
        TestEqual(TEXT("Momentum Transfer second rank is authored"), Closequarter->Number(TEXT("MomentumTransferRankTwoSeconds"), -1), 3.0f);
    }
    const UBreakerAbilityDefinition* Resonance = UBreakerAbilityDefinition::FindFallback(FName(TEXT("Caster.Resonance")));
    if (TestNotNull(TEXT("Resonance resource tuning row is reachable"), Resonance))
    {
        TestEqual(TEXT("Payment rank-one knob is authored"), Resonance->Number(TEXT("PaymentRankOneManaPerStatus"), -1), 2.0f);
        TestEqual(TEXT("Payment rank-two knob is authored"), Resonance->Number(TEXT("PaymentRankTwoManaPerStatus"), -1), 4.0f);
    }
    if (Turret && Cleave)
    {
        TestEqual(TEXT("Follow Through rank-one knob is authored"), Cleave->Number(TEXT("FollowThroughRankOneKillRefund"), -1), 3.0f);
        TestEqual(TEXT("Follow Through rank-two knob is authored"), Cleave->Number(TEXT("FollowThroughRankTwoKillRefund"), -1), 6.0f);
        const TArray<FNumericProperty*> TurretNumbers = BreakerAbilityData::NumberProperties(Turret->AbilityClass.Get());
        const TArray<FNumericProperty*> CleaveNumbers = BreakerAbilityData::NumberProperties(Cleave->AbilityClass.Get());
        TestTrue(TEXT("Turret's first number is the deploy base's placement range"),
            TurretNumbers.Num() > 0 && TurretNumbers[0]->GetFName() == FName(TEXT("PlacementRangeCm")));
        TestTrue(TEXT("Cleave's first number is its range"),
            CleaveNumbers.Num() > 0 && CleaveNumbers[0]->GetFName() == FName(TEXT("RangeCm")));
        TestEqual(TEXT("Turret's placement range is the file's"), Turret->Number(FName(TEXT("PlacementRangeCm")), 0.0f), 800.0f);
    }
    else
    {
        AddError(TEXT("Gunsmith.Turret and Caster.Cleave are registry rows"));
    }
    TestEqual(TEXT("A class outside the ability hierarchy carries no numbers"),
        BreakerAbilityData::NumberProperties(UObject::StaticClass()).Num(), 0);
    TestEqual(TEXT("A null class carries no numbers"),
        BreakerAbilityData::NumberProperties(nullptr).Num(), 0);
    AddInfo(FString::Printf(TEXT("Ability numbers: %d keys across %d rows"), KeyCount, RowsWithNumbers));
    return true;
}

#endif
