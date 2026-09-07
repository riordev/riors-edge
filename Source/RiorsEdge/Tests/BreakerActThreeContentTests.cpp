#include "Misc/AutomationTest.h"
#include "Interaction/BreakerNPC.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSurvivorDialogueTest, "RiorsEdge.Missions.ActThree.SurvivorDialogue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSurvivorDialogueTest::RunTest(const FString& Parameters)
{
    const FBreakerDialogueData& Data = ABreakerNPC::GetDialogueData();
    const FBreakerDialogueRow* Quartermaster = Data.Npcs.FindByPredicate([](const FBreakerDialogueRow& R) { return R.Id == TEXT("Quartermaster"); });
    const FBreakerDialogueRow* Survivor = Data.Npcs.FindByPredicate([](const FBreakerDialogueRow& R) { return R.Id == TEXT("Survivor"); });
    if (!TestNotNull(TEXT("Quartermaster row"), Quartermaster) || !TestNotNull(TEXT("friendly Survivor row"), Survivor)) return false;
    ABreakerNPC* NPC = NewObject<ABreakerNPC>();
    auto Load = [&](const FBreakerDialogueRow& Row) { NPC->StartNodeId = Row.StartNodeId; NPC->DialogueNodes = Row.Nodes; NPC->EntryOverrides = Row.Entries; };
    Load(*Quartermaster);
    FBreakerQuestFlagSet Flags;
    FBreakerDialogueNode Offer;
    if (!TestTrue(TEXT("authored rescue offer"), NPC->FindDialogueNode(TEXT("SurvivorOffer"), Offer))) return false;
    TArray<FBreakerDialogueChoice> Visible;
    Flags.Add(BreakerQuestFlags::SurvivorOffered);
    NPC->GetVisibleChoices(Offer, Flags, Visible);
    TestFalse(TEXT("Offered alone cannot bypass Breach turn-in"), Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == BreakerQuestFlags::SurvivorAccepted; }));
    Flags.Add(TEXT("Quest.Breach.TurnedIn"));
    NPC->GetVisibleChoices(Offer, Flags, Visible);
    const FBreakerDialogueChoice* Accept = Visible.FindByPredicate([](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == BreakerQuestFlags::SurvivorAccepted; });
    if (!TestNotNull(TEXT("earned acceptance is actually visible"), Accept)) return false;
    Flags.Add(Accept->SetsQuestFlag);
    Load(*Survivor);
    TestEqual(TEXT("first encounter enters Meet"), NPC->ResolveStartNodeId(Flags), FName(TEXT("Meet")));
    FBreakerDialogueNode Meet;
    if (!TestTrue(TEXT("Meet resolves"), NPC->FindDialogueNode(TEXT("Meet"), Meet))) return false;
    NPC->GetVisibleChoices(Meet, Flags, Visible);
    const FBreakerDialogueChoice* Start = Visible.FindByPredicate([](const FBreakerDialogueChoice& C) { return C.Action == EBreakerDialogueAction::StartSurvivorEscort; });
    if (!TestNotNull(TEXT("first meeting invokes physical escort action"), Start)) return false;
    TestEqual(TEXT("meeting only records Met"), Start->SetsQuestFlag, BreakerQuestFlags::SurvivorMet);
    Flags.Add(Start->SetsQuestFlag);
    TestEqual(TEXT("persistent Met selects retry"), NPC->ResolveStartNodeId(Flags), FName(TEXT("Retry")));
    FBreakerDialogueNode Retry;
    if (!TestTrue(TEXT("Retry resolves"), NPC->FindDialogueNode(TEXT("Retry"), Retry))) return false;
    NPC->GetVisibleChoices(Retry, Flags, Visible);
    TestTrue(TEXT("already-set Met does not hide physical restart action"), Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.Action == EBreakerDialogueAction::StartSurvivorEscort; }));
    FBreakerDialogueNode TurnIn;
    if (!TestTrue(TEXT("TurnIn resolves"), NPC->FindDialogueNode(TEXT("TurnIn"), TurnIn))) return false;
    Flags.Add(BreakerQuestFlags::SurvivorExtracted);
    NPC->GetVisibleChoices(TurnIn, Flags, Visible);
    TestFalse(TEXT("extraction outside Anchor cannot expose turn-in"), Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == BreakerQuestFlags::SurvivorTurnedIn; }));
    Flags.Add(BreakerQuestFlags::SurvivorReachedAnchor);
    TestEqual(TEXT("safe arrival selects Survivor turn-in"), NPC->ResolveStartNodeId(Flags), FName(TEXT("TurnIn")));
    NPC->GetVisibleChoices(TurnIn, Flags, Visible);
    TestTrue(TEXT("arrival exposes actual completion choice"), Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == BreakerQuestFlags::SurvivorTurnedIn; }));
    Flags.Add(BreakerQuestFlags::SurvivorTurnedIn);
    NPC->GetVisibleChoices(TurnIn, Flags, Visible);
    TestFalse(TEXT("completion cannot be repeated"), Visible.ContainsByPredicate([](const FBreakerDialogueChoice& C) { return C.SetsQuestFlag == BreakerQuestFlags::SurvivorTurnedIn; }));
    for (const FBreakerDialogueRow& Row : Data.Npcs)
        for (const FBreakerDialogueNode& Node : Row.Nodes)
            for (const FBreakerDialogueChoice& Choice : Node.Choices)
            {
                TestFalse(TEXT("no dialogue fabricates extraction proof"), Choice.SetsQuestFlag == BreakerQuestFlags::SurvivorExtracted);
                TestFalse(TEXT("no dialogue fabricates actual Anchor arrival"), Choice.SetsQuestFlag == BreakerQuestFlags::SurvivorReachedAnchor);
                if (Choice.Action == EBreakerDialogueAction::StartSurvivorEscort)
                    TestEqual(TEXT("escort action belongs to the dedicated friendly NPC"), Row.Id, FName(TEXT("Survivor")));
            }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSurvivorMissionLawTest, "RiorsEdge.Missions.ActThree.SurvivorOrderAndReward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSurvivorMissionLawTest::RunTest(const FString& Parameters)
{
    const auto& Missions = UBreakerMissionLibrary::GetMissions();
    const FBreakerMissionDefinition* Mission = Missions.FindByPredicate([](const FBreakerMissionDefinition& M) { return M.MissionId == TEXT("Act3.Survivor"); });
    if (!TestNotNull(TEXT("authored Survivor mission"), Mission)) return false;
    TestEqual(TEXT("early Act III, no fake finale"), Mission->Act, 3);
    if (!TestEqual(TEXT("eight explicit ordered beats"), Mission->Beats.Num(), 8)) return false;
    TestFalse(TEXT("rescue is not a fabricated boss completion"), Mission->Beats.ContainsByPredicate([](const FBreakerMissionBeat& B) { return B.Kind == EBreakerMissionBeatKind::Boss; }));
    FBreakerQuestDefinition Quest;
    if (!TestTrue(TEXT("actual rescue quest"), UBreakerQuestLibrary::FindQuest(TEXT("Quest.Survivor"), Quest))) return false;
    TestEqual(TEXT("three manual rescue objectives"), Quest.Objectives.Num(), 3);
    TestEqual(TEXT("authored rescue gear count"), Quest.Reward.ItemCount, 2);
    TestEqual(TEXT("rescue reward follows Earth area level"), Quest.Reward.ItemLevel, 30);
    TestEqual(TEXT("rescue reward rarity"), Quest.Reward.MinimumRarity, EBreakerItemRarity::Exceptional);
    for (const FBreakerQuestObjective& Objective : Quest.Objectives)
    {
        TestEqual(TEXT("rescue objectives never count ordinary kills"), Objective.RequiredCount, 0);
        TestTrue(TEXT("rescue objective has no kill counter"), Objective.ProgressCounter.IsNone());
    }
    // Pure gate/entitlement fixture; root runtime test/probe owns actual escort and travel proof.
    FBreakerQuestFlagSet Flags;
    for (const FBreakerMissionDefinition& Prior : Missions)
        if (Prior.Act < 3)
            for (const FBreakerMissionBeat& Beat : Prior.Beats)
                for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    TestEqual(TEXT("prior two chapters owe four"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 4);
    TestTrue(TEXT("unaccepted rescue cannot complete"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.survivor_extraction"), Flags).IsEmpty());
    TestEqual(TEXT("orders tracker names Quartermaster, not quest turn-in NPC"), UBreakerMissionLibrary::TrackerLine(Mission->Beats[0], Flags), FString::Printf(UBreakerMissionLibrary::SpeakToVerb, TEXT("QUARTERMASTER")));
    Flags.Add(BreakerQuestFlags::SurvivorAccepted);
    TestTrue(TEXT("Hub visit before the rescue grants no return proof"), UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"), Flags).IsEmpty());
    for (FName Flag : UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Earth.Unindustrialized"), Flags)) Flags.Add(Flag);
    TestTrue(TEXT("actual destination helper selects Earth arrival flag"), Flags.Has(UBreakerMissionLibrary::ArrivedFlagFor(TEXT("Act3.Survivor"))));
    TestTrue(TEXT("arrival without meeting cannot finish extraction"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.survivor_extraction"), Flags).IsEmpty());
    Flags.Add(BreakerQuestFlags::SurvivorMet);
    TestTrue(TEXT("wrong encounter identity cannot rescue"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.other"), Flags).IsEmpty());
    const TArray<FName> Extracted = UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.survivor_extraction"), Flags);
    if (!TestEqual(TEXT("dedicated extraction selects one manual proof flag"), Extracted.Num(), 1)) return false;
    TestEqual(TEXT("only extraction flag supplied"), Extracted[0], BreakerQuestFlags::SurvivorExtracted);
    Flags.Add(Extracted[0]);
    TestEqual(TEXT("outside Anchor quest still active"), UBreakerQuestLibrary::ComputeQuestState(Quest, Flags), EBreakerQuestState::Active);
    TestTrue(TEXT("repeat extraction cannot grant again"), UBreakerMissionLibrary::WorldEncounterCompletionFlagsFor(TEXT("earth.survivor_extraction"), Flags).IsEmpty());
    TestTrue(TEXT("wrong destination cannot complete return"), UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Fernhall"), Flags).IsEmpty());
    const TArray<FName> Arrived = UBreakerMissionLibrary::ArrivalFlagsFor(TEXT("Hub"), Flags);
    if (!TestEqual(TEXT("Hub arrival selects one proof flag"), Arrived.Num(), 1)) return false;
    TestEqual(TEXT("Hub flag is distinct from Earth entry"), Arrived[0], BreakerQuestFlags::SurvivorReachedAnchor);
    Flags.Add(Arrived[0]);
    TestEqual(TEXT("arrival alone does not pay doctrine"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 4);
    TestEqual(TEXT("all physical objectives ready for Survivor turn-in"), UBreakerQuestLibrary::ComputeQuestState(Quest, Flags), EBreakerQuestState::ReadyToTurnIn);
    TestEqual(TEXT("return tracker names actual Survivor"), UBreakerMissionLibrary::TrackerLine(Mission->Beats[5], Flags), FString::Printf(UBreakerMissionLibrary::ReturnToVerb, TEXT("THE SURVIVOR")));
    Flags.Add(BreakerQuestFlags::SurvivorTurnedIn);
    TestEqual(TEXT("completed rescue raises cumulative entitlement to six"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 6);
    TestNull(TEXT("all rescue beats completed"), UBreakerMissionLibrary::CurrentBeat(*Mission, Flags));
    const FBreakerQuestFlagSet Restored = Flags;
    TestEqual(TEXT("restore retains six, never grants a second rescue"), UBreakerMissionLibrary::DoctrinePointEntitlement(Restored), 6);
    return true;
}
#endif
