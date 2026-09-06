#include "Misc/AutomationTest.h"

#include "Game/BreakerZoneBuilder.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Save/BreakerMissionContent.h"
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

#endif
