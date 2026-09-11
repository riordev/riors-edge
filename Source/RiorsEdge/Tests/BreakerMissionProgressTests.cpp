#include "Misc/AutomationTest.h"

#include "Game/BreakerZoneBuilder.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Interaction/BreakerNPC.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerQuestJournal.h"

#if WITH_DEV_AUTOMATION_TESTS

// THE MISSION SEAMS, WALKED ON THE SHIPPED FILE (O43, O186).
//
// A mission's position is a pure read of the journal's flags, and the two
// seams that move it -- arrival completing a Travel beat, a rift run
// completing a Boss beat -- gate on the beat being CURRENT, the rule
// NotifyEnemyKilled already applies to an objective. This walks
// Act1.Fernhall beat by beat on a bare flag set, setting each beat's own
// completion flags in order, and asserts at every step what the position is,
// what each seam would set, and what the doctrine pool is owed. Nothing here
// touches a world, a save or a level; the component half runs on a bare
// component the way the ability-unlock tests do.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMissionProgressTest,
    "RiorsEdge.Missions.Progress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    const TCHAR* BreakerMissionProgressKindName(EBreakerMissionBeatKind Kind)
    {
        switch (Kind)
        {
        case EBreakerMissionBeatKind::Dialogue: return TEXT("Dialogue");
        case EBreakerMissionBeatKind::Travel: return TEXT("Travel");
        case EBreakerMissionBeatKind::Encounter: return TEXT("Encounter");
        case EBreakerMissionBeatKind::Boss: return TEXT("Boss");
        case EBreakerMissionBeatKind::Return: return TEXT("Return");
        case EBreakerMissionBeatKind::Reward: return TEXT("Reward");
        case EBreakerMissionBeatKind::Unlock: return TEXT("Unlock");
        }
        return TEXT("?");
    }

    int32 BreakerMissionProgressIndexOf(const FBreakerMissionDefinition& Mission, const TCHAR* BeatId)
    {
        return Mission.Beats.IndexOfByPredicate([BeatId](const FBreakerMissionBeat& Beat) { return Beat.BeatId == FName(BeatId); });
    }
}

bool FBreakerMissionProgressTest::RunTest(const FString& Parameters)
{
    const int32 PerBenchmark = UBreakerProgressionLibrary::DoctrinePointsPerBenchmark;

    const FBreakerMissionDefinition* Found = UBreakerMissionLibrary::GetMissions().FindByPredicate(
        [](const FBreakerMissionDefinition& Candidate) { return Candidate.MissionId == FName(TEXT("Act1.Fernhall")); });
    if (!TestNotNull(TEXT("Act1.Fernhall is in the shipped file"), Found)) return false;
    const FBreakerMissionDefinition& Mission = *Found;
    const int32 BeatCount = Mission.Beats.Num();
    TestEqual(TEXT("Act one is twenty beats"), BeatCount, 20);

    const int32 TravelIndex = BreakerMissionProgressIndexOf(Mission, TEXT("Travel"));
    const int32 DeeperIndex = BreakerMissionProgressIndexOf(Mission, TEXT("Deeper"));
    const int32 UndercroftIndex = BreakerMissionProgressIndexOf(Mission, TEXT("Undercroft"));
    const int32 DoctrineIndex = Mission.Beats.IndexOfByPredicate(
        [](const FBreakerMissionBeat& Beat) { return Beat.Kind == EBreakerMissionBeatKind::Unlock && Beat.DoctrinePoints > 0; });
    TestEqual(TEXT("The Travel beat is second"), TravelIndex, 1);
    TestTrue(TEXT("Deeper, the Undercroft and the doctrine Unlock are in the file"),
        DeeperIndex != INDEX_NONE && UndercroftIndex != INDEX_NONE && DoctrineIndex != INDEX_NONE);
    TestEqual(TEXT("The Undercroft follows Deeper"), UndercroftIndex, DeeperIndex + 1);
    TestEqual(TEXT("The Undercroft completes on the sweep"),
        Mission.Beats[UndercroftIndex].CompletesOn, FName(TEXT("Quest.Deeper.SweepDone")));

    const FName Arrived = UBreakerMissionLibrary::ArrivedFlagFor(Mission.MissionId);
    const FBreakerRiftDefinition Substation = UBreakerZoneBuilder::FernhallRiftFor(FName(TEXT("substation")));
    const FBreakerRiftDefinition Entry = UBreakerZoneBuilder::FernhallRiftFor(NAME_None);
    TestFalse(TEXT("The substation is not the entry yard"), Substation.AreaName.EqualTo(Entry.AreaName));
    TestEqual(TEXT("Undercroft has stable authored identity"), Substation.EncounterId, FName(TEXT("fernhall.substation")));
    FBreakerRiftDefinition Renamed = Substation;
    Renamed.AreaName = FText::FromString(TEXT("A revised display title"));
    TestEqual(TEXT("Boss lookup survives copy changes"), UBreakerMissionLibrary::BossForRift(Renamed), FName(TEXT("Holdfast")));
    FBreakerRiftDefinition Impostor = Entry;
    Impostor.AreaName = Substation.AreaName;
    TestTrue(TEXT("Matching display name cannot select another encounter boss"), UBreakerMissionLibrary::BossForRift(Impostor).IsNone());
    FBreakerRiftDefinition Unidentified = Substation;
    Unidentified.EncounterId = NAME_None;
    TestTrue(TEXT("Unidentified dev rifts cannot select a story boss"), UBreakerMissionLibrary::BossForRift(Unidentified).IsNone());

    // ---- the walk -----------------------------------------------------------
    FBreakerQuestFlagSet Flags;
    TestEqual(TEXT("No flags: nothing is complete"), UBreakerMissionLibrary::BeatsCompleted(Mission, Flags), 0);
    TestEqual(TEXT("No flags: no doctrine point is owed"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), 0);
    TestEqual(TEXT("No flags: arriving in Fernhall completes nothing (the job is not yet taken)"),
        UBreakerMissionLibrary::ArrivalFlagsFor(ABreakerTravelPoint::FernhallDestinationId, Flags).Num(), 0);

    int32 Previous = 0;
    for (int32 Index = 0; Index < BeatCount; ++Index)
    {
        const FBreakerMissionBeat& Beat = Mission.Beats[Index];
        const FString Where = FString::Printf(TEXT("[%d %s %s]"), Index, BreakerMissionProgressKindName(Beat.Kind), *Beat.BeatId.ToString());
        const TArray<FName> Own = UBreakerMissionLibrary::BeatCompletionFlags(Beat);

        // A beat whose flags are still to be set is the current beat: order
        // is the gate, and nothing before it is left undone. A beat with
        // nothing of its own to set (a Reward after its Return, an Unlock)
        // closed with the beat before it.
        const int32 Completed = UBreakerMissionLibrary::BeatsCompleted(Mission, Flags);
        const FBreakerMissionBeat* Current = UBreakerMissionLibrary::CurrentBeat(Mission, Flags);
        TestTrue(Where + TEXT(" is not behind the walk"), Completed >= Previous);
        TestTrue(Where + TEXT(" is reached in order"), Completed >= Index);
        if (!Flags.HasAll(Own))
        {
            TestEqual(Where + TEXT(" is the current beat until its flags are set"), Completed, Index);
            if (TestNotNull(Where + TEXT(" is current"), Current))
            {
                TestEqual(Where + TEXT(" current beat kind"), static_cast<int32>(Current->Kind), static_cast<int32>(Beat.Kind));
                TestEqual(Where + TEXT(" current beat id"), Current->BeatId, Beat.BeatId);
            }
        }

        // The arrival seam: exactly the current Travel beat's flag, nowhere else.
        const TArray<FName> Arrival = UBreakerMissionLibrary::ArrivalFlagsFor(ABreakerTravelPoint::FernhallDestinationId, Flags);
        if (Completed == TravelIndex)
        {
            TestEqual(Where + TEXT(" arriving in Fernhall completes the Travel beat"), Arrival.Num(), 1);
            if (Arrival.Num() == 1) TestEqual(Where + TEXT(" ...on the mission's arrival flag"), Arrival[0], Arrived);
        }
        else
        {
            TestEqual(Where + TEXT(" arriving in Fernhall completes nothing"), Arrival.Num(), 0);
        }
        TestEqual(Where + TEXT(" arriving at the hub completes nothing"),
            UBreakerMissionLibrary::ArrivalFlagsFor(ABreakerTravelPoint::HubDestinationId, Flags).Num(), 0);

        // The boss seam: the substation completes the Undercroft only once
        // Deeper has been accepted, and the entry yard never does.
        const TArray<FName> Sweep = UBreakerMissionLibrary::RiftCompletionFlagsFor(Substation, Flags);
        TestEqual(Where + TEXT(" display rename preserves completion"),
            UBreakerMissionLibrary::RiftCompletionFlagsFor(Renamed, Flags).Num(), Sweep.Num());
        TestEqual(Where + TEXT(" matching copy cannot counterfeit completion"),
            UBreakerMissionLibrary::RiftCompletionFlagsFor(Impostor, Flags).Num(), 0);
        TestEqual(Where + TEXT(" unidentified rift cannot complete a story beat"),
            UBreakerMissionLibrary::RiftCompletionFlagsFor(Unidentified, Flags).Num(), 0);
        if (Completed == UndercroftIndex)
        {
            TestEqual(Where + TEXT(" the substation run completes the Undercroft"), Sweep.Num(), 1);
            if (Sweep.Num() == 1) TestEqual(Where + TEXT(" ...on the sweep flag"), Sweep[0], FName(TEXT("Quest.Deeper.SweepDone")));
        }
        else
        {
            TestEqual(Where + TEXT(" the substation run completes nothing"), Sweep.Num(), 0);
        }
        TestEqual(Where + TEXT(" the entry yard never completes the Undercroft"),
            UBreakerMissionLibrary::RiftCompletionFlagsFor(Entry, Flags).Num(), 0);

        // The doctrine pool: nothing until the Unlock is reached, one
        // benchmark from then on.
        TestEqual(Where + TEXT(" doctrine entitlement"),
            UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), Completed > DoctrineIndex ? PerBenchmark : 0);

        for (const FName& Flag : Own) Flags.Add(Flag);
        const int32 After = UBreakerMissionLibrary::BeatsCompleted(Mission, Flags);
        TestTrue(Where + TEXT(" is complete once its flags are set"), After > Index);
        Previous = After;
    }

    TestEqual(TEXT("Every beat is complete"), UBreakerMissionLibrary::BeatsCompleted(Mission, Flags), BeatCount);
    TestNull(TEXT("No beat is current"), UBreakerMissionLibrary::CurrentBeat(Mission, Flags));
    TestEqual(TEXT("Act one pays one benchmark"), UBreakerMissionLibrary::DoctrinePointEntitlement(Flags), PerBenchmark);
    TestEqual(TEXT("Done: arriving completes nothing"),
        UBreakerMissionLibrary::ArrivalFlagsFor(ABreakerTravelPoint::FernhallDestinationId, Flags).Num(), 0);
    TestEqual(TEXT("Done: the substation completes nothing"),
        UBreakerMissionLibrary::RiftCompletionFlagsFor(Substation, Flags).Num(), 0);

    // ---- the shipped configuration ----------------------------------------
    // One benchmark per authored act; eight when four acts exist. The gap
    // between the file's sum and the whole grant is the campaign's, and the
    // entitlement never exceeds the grant.
    FBreakerQuestFlagSet Everything;
    for (const FBreakerMissionDefinition& Each : UBreakerMissionLibrary::GetMissions())
    {
        for (const FBreakerMissionBeat& Beat : Each.Beats)
        {
            for (const FName& Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Everything.Add(Flag);
        }
    }
    const int32 Whole = UBreakerMissionLibrary::DoctrinePointEntitlement(Everything);
    TestEqual(TEXT("Every authored act pays one benchmark"), Whole, PerBenchmark * UBreakerMissionLibrary::GetMissions().Num());
    TestTrue(TEXT("...and never more than the whole grant"), Whole <= UBreakerProgressionLibrary::DoctrinePointGrant);

    // ---- the component half -------------------------------------------------
    UBreakerProgressionComponent* Fresh = NewObject<UBreakerProgressionComponent>();
    TestEqual(TEXT("A fresh component holds no doctrine points"),
        Fresh->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    Fresh->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("Settling act one pays the benchmark"),
        Fresh->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), PerBenchmark);
    TestEqual(TEXT("...and the counter says so"),
        Fresh->GetProgressionState().LevelDoctrinePointsGranted, PerBenchmark);
    Fresh->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("A second settle pays nothing"),
        Fresh->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), PerBenchmark);
    TestEqual(TEXT("...and the counter is unmoved"),
        Fresh->GetProgressionState().LevelDoctrinePointsGranted, PerBenchmark);

    // A character already paid the whole pool keeps it: the settle is
    // monotonic and a counter above the entitlement claws nothing back.
    UBreakerProgressionComponent* Paid = NewObject<UBreakerProgressionComponent>();
    FBreakerProgressionState PaidState = Paid->GetProgressionState();
    PaidState.UnspentDoctrinePoints = UBreakerProgressionLibrary::DoctrinePointGrant;
    PaidState.LevelDoctrinePointsGranted = UBreakerProgressionLibrary::DoctrinePointGrant;
    Paid->LoadProgressionState(PaidState);
    Paid->SettleDoctrineEntitlement(Flags);
    TestEqual(TEXT("A character already paid keeps its points"),
        Paid->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), UBreakerProgressionLibrary::DoctrinePointGrant);
    TestEqual(TEXT("...and its counter"),
        Paid->GetProgressionState().LevelDoctrinePointsGranted, UBreakerProgressionLibrary::DoctrinePointGrant);
    return true;
}

// THE TRACKER LINE, WALKED ON THE SHIPPED FILE (O195, O186).
//
// Every string the corner can draw is built here from the rows the line is
// derived from -- the quest's giver, the objective's text and count, the
// destination's name -- never from a literal, so the assertion is that the
// line has one home and the HUD's corner reads it. The walk sets each beat's
// flags in order as the Progress walk does and asks the line of the current
// beat at every step.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMissionTrackerLineTest,
    "RiorsEdge.Missions.TrackerLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    const FBreakerQuestDefinition* BreakerMissionTrackerQuestWhere(TFunctionRef<bool(const FBreakerQuestDefinition&)> Match)
    {
        return UBreakerQuestLibrary::GetFallbackQuests().FindByPredicate(Match);
    }

    const FBreakerQuestObjective* BreakerMissionTrackerObjective(const FBreakerQuestDefinition& Quest, FName ObjectiveId)
    {
        return Quest.Objectives.FindByPredicate([ObjectiveId](const FBreakerQuestObjective& Candidate) { return Candidate.ObjectiveId == ObjectiveId; });
    }

    // The expected objective line, from the rows: text, and "  n/N" when
    // counted. The same shape the tracker prints, restated here so a drift
    // in either the format or the clamp shows as a mismatch.
    FString BreakerMissionTrackerObjectiveLine(const FBreakerQuestObjective& Objective, int32 Count)
    {
        FString Line = Objective.Text.ToUpper();
        if (Objective.RequiredCount > 0)
        {
            Line += FString::Printf(TEXT("  %d/%d"), FMath::Clamp(Count, 0, Objective.RequiredCount), Objective.RequiredCount);
        }
        return Line;
    }
}

bool FBreakerMissionTrackerLineTest::RunTest(const FString& Parameters)
{
    const FBreakerMissionDefinition* Found = UBreakerMissionLibrary::GetMissions().FindByPredicate(
        [](const FBreakerMissionDefinition& Candidate) { return Candidate.MissionId == FName(TEXT("Act1.Fernhall")); });
    if (!TestNotNull(TEXT("Act1.Fernhall is in the shipped file"), Found)) return false;
    const FBreakerMissionDefinition& Mission = *Found;
    TestEqual(TEXT("Act one is twenty beats"), Mission.Beats.Num(), 20);

    const int32 SpillIndex = BreakerMissionProgressIndexOf(Mission, TEXT("Spill"));
    const int32 UndercroftIndex = BreakerMissionProgressIndexOf(Mission, TEXT("Undercroft"));
    TestTrue(TEXT("Spill and the Undercroft are in the file"), SpillIndex != INDEX_NONE && UndercroftIndex != INDEX_NONE);

    FBreakerQuestFlagSet Flags;
    for (int32 Index = 0; Index < Mission.Beats.Num(); ++Index)
    {
        const FBreakerMissionBeat& Beat = Mission.Beats[Index];
        const FString Where = FString::Printf(TEXT("[%d %s %s]"), Index, BreakerMissionProgressKindName(Beat.Kind), *Beat.BeatId.ToString());

        // The beat under test is the current one whenever it has flags of
        // its own to set; a Reward or Unlock closed with the beat before it
        // and is asked directly, since the corner it would draw is the same.
        const FBreakerMissionBeat* Current = UBreakerMissionLibrary::CurrentBeat(Mission, Flags);
        const bool bExpectEmpty = Beat.Kind == EBreakerMissionBeatKind::Reward || Beat.Kind == EBreakerMissionBeatKind::Unlock;
        if (!bExpectEmpty && TestNotNull(Where + TEXT(" is current"), Current))
        {
            TestEqual(Where + TEXT(" current beat id"), Current->BeatId, Beat.BeatId);
        }

        const FString Line = UBreakerMissionLibrary::TrackerLine(Beat, Flags);
        TestEqual(Where + TEXT(" the line is empty iff the beat pays rather than asks"), Line.IsEmpty(), bExpectEmpty);

        switch (Beat.Kind)
        {
        case EBreakerMissionBeatKind::Dialogue:
        {
            const FBreakerQuestDefinition* Quest = BreakerMissionTrackerQuestWhere(
                [&Beat](const FBreakerQuestDefinition& Candidate) { return Candidate.AcceptedFlag == Beat.CompletesOn; });
            if (TestNotNull(Where + TEXT(" accepts a quest by flag (the unverbed branch is never taken)"), Quest))
            {
                const FBreakerDialogueRow* Speaker = ABreakerNPC::GetDialogueData().Npcs.FindByPredicate([&Beat](const FBreakerDialogueRow& Row) { return Row.Id == Beat.Npc; });
                if (TestNotNull(Where + TEXT(" has an authored speaker"), Speaker))
                    TestEqual(Where + TEXT(" speaks to the beat's NPC"), Line,
                        FString::Printf(UBreakerMissionLibrary::SpeakToVerb, *Speaker->DisplayName.ToUpper()));
            }
            break;
        }

        case EBreakerMissionBeatKind::Return:
        {
            const FBreakerQuestDefinition* Quest = BreakerMissionTrackerQuestWhere(
                [&Beat](const FBreakerQuestDefinition& Candidate) { return Candidate.TurnedInFlag == Beat.CompletesOn; });
            if (TestNotNull(Where + TEXT(" turns in a quest by flag (the unverbed branch is never taken)"), Quest))
            {
                const FBreakerDialogueRow* Speaker = ABreakerNPC::GetDialogueData().Npcs.FindByPredicate([&Beat](const FBreakerDialogueRow& Row) { return Row.Id == Beat.Npc; });
                if (TestNotNull(Where + TEXT(" has an authored turn-in NPC"), Speaker))
                    TestEqual(Where + TEXT(" returns to the beat's NPC"), Line,
                        FString::Printf(UBreakerMissionLibrary::ReturnToVerb, *Speaker->DisplayName.ToUpper()));
            }
            break;
        }

        case EBreakerMissionBeatKind::Travel:
        {
            FBreakerTravelDestination Destination;
            if (TestTrue(Where + TEXT(" the destination resolves"), ABreakerTravelPoint::FindDestination(Beat.Destination, Destination)))
            {
                TestEqual(Where + TEXT(" names the destination"), Line, Destination.DisplayName.ToString().ToUpper());
            }
            break;
        }

        case EBreakerMissionBeatKind::Encounter:
        {
            const FBreakerQuestDefinition* Quest = BreakerMissionTrackerQuestWhere(
                [&Beat](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == Beat.Quest; });
            if (!TestNotNull(Where + TEXT(" the quest resolves"), Quest)) break;
            if (!TestTrue(Where + TEXT(" counts at least one objective"), Beat.Objectives.Num() > 0)) break;
            const FBreakerQuestObjective* First = BreakerMissionTrackerObjective(*Quest, Beat.Objectives[0]);
            if (!TestNotNull(Where + TEXT(" the first objective resolves"), First)) break;

            TestEqual(Where + TEXT(" the first objective, uncounted"), Line, BreakerMissionTrackerObjectiveLine(*First, 0));

            // The counter as NotifyEnemyKilled raises it, one short of done.
            if (First->RequiredCount > 1)
            {
                FBreakerQuestFlagSet Nearly = Flags;
                Nearly.RaiseCounter(First->ProgressCounter, First->RequiredCount - 1);
                TestEqual(Where + TEXT(" the counter reads one short"),
                    UBreakerMissionLibrary::TrackerLine(Beat, Nearly), BreakerMissionTrackerObjectiveLine(*First, First->RequiredCount - 1));
            }

            // The first objective done: the line moves to the second.
            if (Beat.Objectives.Num() > 1)
            {
                const FBreakerQuestObjective* Second = BreakerMissionTrackerObjective(*Quest, Beat.Objectives[1]);
                if (TestNotNull(Where + TEXT(" the second objective resolves"), Second))
                {
                    FBreakerQuestFlagSet FirstDone = Flags;
                    FirstDone.Add(First->CompletionFlag);
                    TestEqual(Where + TEXT(" the line moves to the second objective"),
                        UBreakerMissionLibrary::TrackerLine(Beat, FirstDone), BreakerMissionTrackerObjectiveLine(*Second, 0));
                }
            }
            break;
        }

        case EBreakerMissionBeatKind::Boss:
        {
            const FBreakerQuestObjective* Closes = nullptr;
            for (const FName& QuestId : Mission.Quests)
            {
                const FBreakerQuestDefinition* Quest = BreakerMissionTrackerQuestWhere(
                    [QuestId](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == QuestId; });
                if (!Quest) continue;
                Closes = Quest->Objectives.FindByPredicate(
                    [&Beat](const FBreakerQuestObjective& Candidate) { return Candidate.CompletionFlag == Beat.CompletesOn; });
                if (Closes) break;
            }
            if (TestNotNull(Where + TEXT(" closes one of the mission's objectives"), Closes))
            {
                TestEqual(Where + TEXT(" the sweep, uncounted"), Line, BreakerMissionTrackerObjectiveLine(*Closes, 0));
            }
            const FBreakerMissionRift* Rift = UBreakerMissionLibrary::GetRifts().FindByPredicate(
                [&Beat](const FBreakerMissionRift& Candidate) { return Candidate.RiftId == Beat.Rift; });
            TestNotNull(Where + TEXT(" the rift resolves"), Rift);
            break;
        }

        case EBreakerMissionBeatKind::Reward:
        case EBreakerMissionBeatKind::Unlock:
            break;
        }

        for (const FName& Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) Flags.Add(Flag);
    }
    TestNull(TEXT("Every beat is complete"), UBreakerMissionLibrary::CurrentBeat(Mission, Flags));

    // ---- the shipped configuration ----------------------------------------
    // Spill counts Thin then Elite, so the move to a second objective is
    // exercised on the shipped file; the Undercroft closes Deeper's sweep,
    // a counted objective of five.
    if (SpillIndex != INDEX_NONE)
    {
        TestEqual(TEXT("Spill counts two objectives"), Mission.Beats[SpillIndex].Objectives.Num(), 2);
    }
    if (UndercroftIndex != INDEX_NONE)
    {
        const FBreakerMissionBeat& Undercroft = Mission.Beats[UndercroftIndex];
        const FBreakerQuestDefinition* Deeper = BreakerMissionTrackerQuestWhere(
            [](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == FName(TEXT("Quest.Deeper")); });
        const FBreakerQuestObjective* Sweep = Deeper ? Deeper->Objectives.FindByPredicate(
            [&Undercroft](const FBreakerQuestObjective& Candidate) { return Candidate.CompletionFlag == Undercroft.CompletesOn; }) : nullptr;
        if (TestNotNull(TEXT("The Undercroft closes Deeper's sweep"), Sweep))
        {
            TestEqual(TEXT("The sweep counts five"), Sweep->RequiredCount, 5);
            TestEqual(TEXT("The sweep line on a fresh set"), UBreakerMissionLibrary::TrackerLine(Undercroft, FBreakerQuestFlagSet()),
                BreakerMissionTrackerObjectiveLine(*Sweep, 0));
        }
    }
    return true;
}

// THE TRACKER LINES: the story's ask first, then every live quest no mission
// sequences. Walked on a bare flag set through the Watchkeeper's contract,
// the one shipped quest outside every mission: absent until offered,
// its objective with the count while Active, the return verb once the count
// is met, gone once turned in. Every expected string is built from the rows
// the same way the TrackerLine walk builds its own -- the quest's giver, the
// objective's text and count -- so the assertion is that the quest's line
// has the same one home the beat's line does.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMissionTrackerLinesTest,
    "RiorsEdge.Missions.TrackerLines",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMissionTrackerLinesTest::RunTest(const FString& Parameters)
{
    // ---- the shipped configuration ----------------------------------------
    // Exactly one quest in the registry belongs to no mission, and it is the
    // Watchkeeper's. A second unlisted quest is either a mission that forgot
    // to list it or a side quest this test does not yet walk; either way it
    // is named here.
    TArray<FName> Unlisted;
    for (const FBreakerQuestDefinition& Quest : UBreakerQuestLibrary::GetFallbackQuests())
    {
        bool bListed = false;
        for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
        {
            if (Mission.Quests.Contains(Quest.QuestId)) { bListed = true; break; }
        }
        if (!bListed) Unlisted.Add(Quest.QuestId);
    }
    const FString UnlistedNames = FString::JoinBy(Unlisted, TEXT(", "), [](const FName& QuestId) { return QuestId.ToString(); });
    TestEqual(FString::Printf(TEXT("Exactly one quest belongs to no mission (found: %s)"), *UnlistedNames), Unlisted.Num(), 1);
    if (Unlisted.Num() == 1)
    {
        TestEqual(TEXT("The quest no mission lists is the Watchkeeper's"), Unlisted[0], FName(TEXT("Quest.Watch")));
    }

    const FBreakerQuestDefinition* Watch = BreakerMissionTrackerQuestWhere(
        [](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == FName(TEXT("Quest.Watch")); });
    if (!TestNotNull(TEXT("Quest.Watch is in the shipped registry"), Watch)) return false;
    if (!TestEqual(TEXT("Quest.Watch has one objective"), Watch->Objectives.Num(), 1)) return false;
    const FBreakerQuestObjective& Flank = Watch->Objectives[0];
    TestEqual(TEXT("The Watchkeeper's giver row"), Watch->Giver, FString(TEXT("WATCHKEEPER")));
    TestEqual(TEXT("The flank's authored text"), Flank.Text, FString(TEXT("Clear the substation flank")));
    TestEqual(TEXT("The flank counts six"), Flank.RequiredCount, 6);
    TestEqual(TEXT("The flank's counter"), Flank.ProgressCounter, FName(TEXT("Quest.Watch.Kills")));

    // ---- fresh: the story's first ask, and nothing else ------------------
    FBreakerQuestFlagSet Flags;
    {
        const TArray<FString> Lines = UBreakerMissionLibrary::TrackerLines(Flags);
        if (TestEqual(TEXT("Fresh set: one line"), Lines.Num(), 1))
        {
            TestEqual(TEXT("Fresh set: the Quartermaster's ask"), Lines[0], FString(TEXT("SPEAK TO THE QUARTERMASTER")));
        }
    }

    // ---- offered: the giver, by the same verb the Dialogue beat uses -------
    Flags.Add(Watch->OfferedFlag);
    {
        const TArray<FString> Lines = UBreakerMissionLibrary::TrackerLines(Flags);
        if (TestEqual(TEXT("Offered: two lines"), Lines.Num(), 2))
        {
            TestEqual(TEXT("Offered: the story's ask stays first"), Lines[0], FString(TEXT("SPEAK TO THE QUARTERMASTER")));
            TestEqual(TEXT("Offered: speak to the Watchkeeper"), Lines[1], FString::Printf(UBreakerMissionLibrary::SpeakToVerb, TEXT("WATCHKEEPER")));
        }
    }

    // ---- active: the objective with its count -----------------------------
    Flags.Add(Watch->AcceptedFlag);
    {
        const TArray<FString> Lines = UBreakerMissionLibrary::TrackerLines(Flags);
        if (TestEqual(TEXT("Active: two lines"), Lines.Num(), 2))
        {
            TestEqual(TEXT("Active: the story's ask stays first"), Lines[0], FString(TEXT("SPEAK TO THE QUARTERMASTER")));
            TestEqual(TEXT("Active: the flank at zero, as the rows spell it"), Lines[1], FString(TEXT("CLEAR THE SUBSTATION FLANK  0/6")));
            TestEqual(TEXT("Active: the flank at zero, as the objective line builds it"), Lines[1], BreakerMissionTrackerObjectiveLine(Flank, 0));
        }
    }

    // The counter as NotifyEnemyKilled raises it, one short of done.
    {
        FBreakerQuestFlagSet Nearly = Flags;
        Nearly.RaiseCounter(Flank.ProgressCounter, Flank.RequiredCount - 1);
        const TArray<FString> Lines = UBreakerMissionLibrary::TrackerLines(Nearly);
        if (TestEqual(TEXT("Nearly: two lines"), Lines.Num(), 2))
        {
            TestEqual(TEXT("Nearly: the flank reads one short"), Lines[1], FString(TEXT("CLEAR THE SUBSTATION FLANK  5/6")));
        }
    }

    // ---- ready: the count met and the flag set, the return verb -----------
    Flags.RaiseCounter(Flank.ProgressCounter, Flank.RequiredCount);
    Flags.Add(Flank.CompletionFlag);
    {
        const TArray<FString> Lines = UBreakerMissionLibrary::TrackerLines(Flags);
        if (TestEqual(TEXT("Ready: two lines"), Lines.Num(), 2))
        {
            TestEqual(TEXT("Ready: the story's ask stays first"), Lines[0], FString(TEXT("SPEAK TO THE QUARTERMASTER")));
            TestEqual(TEXT("Ready: return to the Watchkeeper"), Lines[1], FString::Printf(UBreakerMissionLibrary::ReturnToVerb, TEXT("WATCHKEEPER")));
        }
    }

    // ---- turned in: back to the story alone --------------------------------
    Flags.Add(Watch->TurnedInFlag);
    {
        const TArray<FString> Lines = UBreakerMissionLibrary::TrackerLines(Flags);
        if (TestEqual(TEXT("Turned in: one line"), Lines.Num(), 1))
        {
            TestEqual(TEXT("Turned in: the story's ask alone"), Lines[0], FString(TEXT("SPEAK TO THE QUARTERMASTER")));
        }
    }

    // A quest a mission lists never repeats below the beat line: Quest.Watch
    // is the only live quest, and the story's own quests stay silent even
    // when offered, because their beat speaks for them.
    {
        const FBreakerQuestDefinition* First = BreakerMissionTrackerQuestWhere(
            [](const FBreakerQuestDefinition& Candidate) { return Candidate.QuestId == FName(TEXT("Quest.FirstContract")); });
        if (TestNotNull(TEXT("Quest.FirstContract is in the shipped registry"), First))
        {
            FBreakerQuestFlagSet Story;
            Story.Add(First->OfferedFlag);
            const TArray<FString> Lines = UBreakerMissionLibrary::TrackerLines(Story);
            TestEqual(TEXT("A mission's own quest is not repeated below its beat"), Lines.Num(), 1);
        }
    }
    return true;
}

#endif
