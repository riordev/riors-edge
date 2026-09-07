#include "Misc/AutomationTest.h"
#include "Data/BreakerCensus.h"
#include "Interaction/BreakerNPC.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFinaleChoiceAtomicTest, "RiorsEdge.Missions.Finale.AtomicChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFinaleChoiceAtomicTest::RunTest(const FString& Parameters)
{
    using namespace BreakerQuestFlags;
    for (const bool bSeal : {true, false})
    {
        UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
        const FName Chosen = bSeal ? FinaleSeal : FinaleHold;
        const FName Other = bSeal ? FinaleHold : FinaleSeal;
        int32 Notices = 0, Completions = 0, Saves = 0;
        Journal->OnFlagSet.AddLambda([&](FName Flag)
        {
            if (Notices == 0) TestEqual(TEXT("Reward completion precedes ancillary choice observers"), Flag, FinaleTurnedIn);
            ++Notices;
            if (Flag == FinaleTurnedIn) ++Completions;
            TestTrue(TEXT("Every listener already sees the complete durable decision"),
                Journal->HasFlag(Chosen) && Journal->HasFlag(FinaleTurnedIn) && !Journal->HasFlag(Other));
            TestFalse(TEXT("A reentrant listener cannot switch the decision"),
                Journal->CommitExclusiveChoice(Other, Chosen, FinaleTurnedIn));
        });
        FBreakerQuestFlagSet Saved;
        Journal->OnPersistRequested.AddLambda([&] { ++Saves; Saved = Journal->GetState(); });
        TestTrue(TEXT("The decision commits"), Journal->CommitExclusiveChoice(Chosen, Other, FinaleTurnedIn));
        TestEqual(TEXT("Two new flags notify existing consumers"), Notices, 2);
        TestEqual(TEXT("Reward completion fires once"), Completions, 1);
        TestEqual(TEXT("One atomic persistence request"), Saves, 1);
        TestTrue(TEXT("Persisted state contains choice and common completion"), Saved.Has(Chosen) && Saved.Has(FinaleTurnedIn));
        TestFalse(TEXT("Repeated selection cannot pay again"), Journal->CommitExclusiveChoice(Chosen, Other, FinaleTurnedIn));
        UBreakerQuestJournal* Restored = NewObject<UBreakerQuestJournal>();
        Restored->RestoreFrom(Saved);
        TestFalse(TEXT("Reload cannot switch choices"), Restored->CommitExclusiveChoice(Other, Chosen, FinaleTurnedIn));
        TestEqual(TEXT("No extra persistence or rewards on repeat"), Saves, 1);
    }
    UBreakerQuestJournal* Empty = NewObject<UBreakerQuestJournal>();
    TestFalse(TEXT("Invalid identities do not partially mutate"), Empty->CommitExclusiveChoice(NAME_None, FinaleHold, FinaleTurnedIn));
    TestTrue(TEXT("Invalid call leaves journal empty"), Empty->GetFlags().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFinaleOrderedContentTest, "RiorsEdge.Missions.Finale.OrderedContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFinaleOrderedContentTest::RunTest(const FString& Parameters)
{
    using namespace BreakerQuestFlags;
    const auto& Missions = UBreakerMissionLibrary::GetMissions();
    const FBreakerMissionDefinition* Finale = Missions.FindByPredicate([](const FBreakerMissionDefinition& M) { return M.MissionId == TEXT("Act3.Finale"); });
    if (!TestNotNull(TEXT("Finale is authored"), Finale)) return false;
    TestEqual(TEXT("Finale is still Act III"), Finale->Act, 3);
    TestEqual(TEXT("Eleven ordered earned beats"), Finale->Beats.Num(), 11);
    for (const auto& Beat : Finale->Beats) TestTrue(TEXT("Alternate self is never a boss fight"), Beat.Kind != EBreakerMissionBeatKind::Boss);
    FBreakerQuestDefinition Quest;
    if (!TestTrue(TEXT("Finale quest resolves"), UBreakerQuestLibrary::FindQuest(TEXT("Quest.Finale"), Quest))) return false;
    TestEqual(TEXT("Four manual objectives"), Quest.Objectives.Num(), 4);
    for (const auto& Objective : Quest.Objectives)
        TestTrue(TEXT("Generic kills cannot complete finale objectives"), Objective.RequiredCount == 0 && Objective.ProgressCounter.IsNone());
    TestEqual(TEXT("Common reward contains two items"), Quest.Reward.ItemCount, 2);
    TestEqual(TEXT("Common reward item level"), Quest.Reward.ItemLevel, 50);

    // Pure story-gate fixture. Prior flags model earned earlier chapters;
    // physical recovery, travel, NPC identity and level gates have runtime tests.
    FBreakerQuestFlagSet Flags;
    for (const auto& Mission : Missions)
    {
        if (Mission.MissionId == Finale->MissionId) break;
        for (FName QuestId : Mission.Quests)
        {
            FBreakerQuestDefinition Earlier;
            if (!UBreakerQuestLibrary::FindQuest(QuestId, Earlier)) continue;
            Flags.Add(Earlier.OfferedFlag); Flags.Add(Earlier.AcceptedFlag); Flags.Add(Earlier.TurnedInFlag);
            for (const auto& Objective : Earlier.Objectives) Flags.Add(Objective.CompletionFlag);
        }
        for (const auto& Beat : Mission.Beats) Flags.Add(Beat.CompletesOn);
    }
    TestEqual(TEXT("Existing three benchmarks still total six"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 6);
    TestTrue(TEXT("No fragment reward before acceptance and arrival"),
        UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.rior_fragment"), Flags).IsEmpty());
    Flags.Add(FinaleOffered); Flags.Add(FinaleAccepted);
    for (FName Flag : UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Earth.Stripped"), Flags)) Flags.Add(Flag);
    TestTrue(TEXT("Stripped arrival is distinct"), Flags.Has(TEXT("Mission.Act3.Finale.Arrived")));
    TestTrue(TEXT("Early Hub cannot skip physical recovery"), UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"), Flags).IsEmpty());
    for (FName Flag : UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.rior_fragment"), Flags)) Flags.Add(Flag);
    TestTrue(TEXT("Dedicated encounter completes physical objective only"), Flags.Has(FinaleFragmentRecovered) && !Flags.Has(FinaleReconstructed));
    for (FName Flag : UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"), Flags)) Flags.Add(Flag);
    TestTrue(TEXT("Reconstruction requires actual return"), Flags.Has(FinaleReturnedWithFragment));
    TestTrue(TEXT("Won cannot precede reconstruction"), UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Earth.Won"), Flags).IsEmpty());
    Flags.Add(FinaleReconstructed);
    for (FName Flag : UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Earth.Won"), Flags)) Flags.Add(Flag);
    TestTrue(TEXT("Won arrival is a separate flag"), Flags.Has(FinaleArrivedWon));
    TestTrue(TEXT("Hub cannot skip meeting the living alternate"), UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"), Flags).IsEmpty());
    Flags.Add(FinaleMetAlternate);
    for (FName Flag : UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"), Flags)) Flags.Add(Flag);
    TestTrue(TEXT("Final actual return precedes device"), Flags.Has(FinaleReturnedFromWon));
    TestEqual(TEXT("The device has not paid before a choice"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 6);
    const FBreakerDialogueData& Dialogue = ABreakerNPC::GetDialogueData();
    const FBreakerDialogueRow* Device = Dialogue.Npcs.FindByPredicate([](const FBreakerDialogueRow& R) { return R.Id == TEXT("FinaleDevice"); });
    const FBreakerDialogueRow* Researcher = Dialogue.Npcs.FindByPredicate([](const FBreakerDialogueRow& R) { return R.Id == TEXT("Researcher"); });
    if (!TestNotNull(TEXT("Physical device dialogue"), Device) || !TestNotNull(TEXT("Professional colleague has a dedicated role"), Researcher)) return false;
    ABreakerNPC* NPC = NewObject<ABreakerNPC>();
    NPC->StartNodeId = Researcher->StartNodeId; NPC->DialogueNodes = Researcher->Nodes; NPC->EntryOverrides = Researcher->Entries;
    FBreakerDialogueNode Offer;
    if (!TestTrue(TEXT("Researcher offer exists"), NPC->FindDialogueNode(TEXT("FinaleOffer"), Offer))) return false;
    TArray<FBreakerDialogueChoice> Visible;
    FBreakerQuestFlagSet Unearned; Unearned.Add(FinaleOffered);
    NPC->GetVisibleChoices(Offer, Unearned, Visible);
    TestFalse(TEXT("A forged offer alone does not bypass rescue"), Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == FinaleAccepted; }));
    Unearned.Add(SurvivorTurnedIn); NPC->GetVisibleChoices(Offer, Unearned, Visible);
    TestTrue(TEXT("Earned acceptance remains visible before setting Accepted"), Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == FinaleAccepted; }));
    NPC->StartNodeId = Device->StartNodeId; NPC->DialogueNodes = Device->Nodes; NPC->EntryOverrides = Device->Entries;
    FBreakerDialogueNode ChoiceNode;
    if (!TestTrue(TEXT("Physical device choice node"), NPC->FindDialogueNode(TEXT("Choice"), ChoiceNode))) return false;
    NPC->GetVisibleChoices(ChoiceNode, Flags, Visible);
    TestTrue(TEXT("Both actual device actions are reachable after ordered work"),
        Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.Action == EBreakerDialogueAction::SealRifts; })
        && Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.Action == EBreakerDialogueAction::HoldRifts; }));
    for (const auto& Row : Dialogue.Npcs) for (const auto& Node : Row.Nodes) for (const auto& Choice : Node.Choices)
    {
        TestTrue(TEXT("No dialogue counterfeits physical recovery or arrival"), Choice.SetsQuestFlag != FinaleFragmentRecovered
            && Choice.SetsQuestFlag != FinaleReturnedWithFragment && Choice.SetsQuestFlag != FinaleArrivedWon && Choice.SetsQuestFlag != FinaleReturnedFromWon);
        if (Choice.SetsQuestFlag == FinaleTurnedIn)
            TestTrue(TEXT("Common completion is bound only to an atomic physical device action"), Row.Id == TEXT("FinaleDevice")
                && (Choice.Action == EBreakerDialogueAction::SealRifts || Choice.Action == EBreakerDialogueAction::HoldRifts));
    }
    for (const bool bSeal : {true, false})
    {
        UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>(); Journal->RestoreFrom(Flags);
        Journal->CommitExclusiveChoice(bSeal ? FinaleSeal : FinaleHold, bSeal ? FinaleHold : FinaleSeal, FinaleTurnedIn);
        TestEqual(TEXT("Either ending reaches exactly the same eight-point pool"), UBreakerMissionLibrary::DoctrinePointEntitlement(Journal->GetState()), 8);
        TestEqual(TEXT("Either ending completes the same reward quest"), UBreakerQuestLibrary::ComputeQuestState(Quest, Journal->GetState()), EBreakerQuestState::Complete);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerFinaleBenchmarksTest, "RiorsEdge.Missions.Finale.BenchmarkIdentities",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerFinaleBenchmarksTest::RunTest(const FString& Parameters)
{
    TArray<FBreakerMissionDefinition> Missions = UBreakerMissionLibrary::GetMissions();
    TSet<FName> Ids; int32 Total = 0, ActThree = 0;
    for (const auto& Mission : Missions) for (const auto& Beat : Mission.Beats) if (Beat.DoctrinePoints > 0)
    {
        TestEqual(TEXT("Every source is one benchmark"), Beat.DoctrinePoints, 2);
        TestFalse(TEXT("Identity is unique across acts"), Ids.Contains(Beat.Benchmark));
        Ids.Add(Beat.Benchmark); Total += Beat.DoctrinePoints;
        if (Mission.Act == 3) ActThree += Beat.DoctrinePoints;
    }
    TestEqual(TEXT("Four identities, not four acts"), Ids.Num(), 4);
    TestEqual(TEXT("Global eight-point cap preserved"), Total, 8);
    TestEqual(TEXT("Act III has rescue and finale benchmarks"), ActThree, 4);
    if (Missions.IsEmpty() || Missions.Last().Beats.IsEmpty()) return false;
    Missions.Last().Beats.Last().Benchmark = TEXT("Act3.Survivor");
    FBreakerMissionData Parsed; TArray<FString> Errors;
    TestFalse(TEXT("Duplicate benchmark cannot pay a second time"), UBreakerMissionLibrary::ParseMissionsJson(
        BreakerCensus::ExportMissions(UBreakerMissionLibrary::GetRifts(), Missions), Parsed, Errors));
    TestTrue(TEXT("Refusal identifies duplicate grant"), Errors.ContainsByPredicate([](const FString& E) { return E.Contains(TEXT("granted twice")); }));
    return true;
}
#endif
