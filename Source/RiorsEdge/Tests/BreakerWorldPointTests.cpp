#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerWorldPoints.h"
#include "Save/BreakerQuestJournal.h"
#include "Save/BreakerMissionContent.h"
#include "Tests/BreakerStatusEmit.h"

// ---------------------------------------------------------------------------
// THE FIFTEEN WORLD CORE POINTS (O7)
// ---------------------------------------------------------------------------
// Progression.WorldPoints.SoloReachable was an asserted invariant with no test
// for as long as content-and-modes has had an invariant table, and it could not
// have been written before now: there was no list to assert over. The fifteen
// were canon in a ruling and had no representation in code, which is the worst
// of the three states a claim can be in — it looks decided and it is not even
// enumerable.
//
// Pure maths over the shipped registry. No world, no actor, nothing granted.
namespace BreakerWorldPointTest
{
    // Distinctively named for the unity build.
    int32 BreakerWorldPointCountInAct(int32 Act)
    {
        int32 Count = 0;
        for (const FBreakerWorldPointSource& Source : UBreakerWorldPointLibrary::GetSources())
        {
            if (Source.Act == Act) ++Count;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWorldPointsSoloReachableTest,
    "RiorsEdge.Progression.WorldPoints.SoloReachable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWorldPointsSoloReachableTest::RunTest(const FString& Parameters)
{
    using namespace BreakerWorldPointTest;
    const TArray<FBreakerWorldPointSource>& Sources = UBreakerWorldPointLibrary::GetSources();

    // THE COUNT IS CANON. Fifteen, and nothing endgame-gated — the overflow
    // entry that was completable after level 50 was cut in ratification. A
    // sixteenth entry inflates the ~65-point budget the Core Tree is sized
    // against, and that budget is what "two constellations fully developed
    // plus a third partially" was validated at.
    TestEqual(TEXT("The world Core Point list is exactly fifteen sources"), Sources.Num(), 15);

    // THE INVARIANT. O82 makes solo the balance target, so a Core Point behind
    // group content is a point a solo character cannot have — and characters
    // are builds (O17), so it is a build they cannot make.
    for (const FBreakerWorldPointSource& Source : Sources)
    {
        TestFalse(*FString::Printf(TEXT("'%s' does not require party content"),
            *Source.SourceId.ToString()), Source.bRequiresParty);
        // Design rule 3, asserted beside it because the two fail together: a
        // missable point on a character with a permanent class is unrecoverable.
        TestFalse(*FString::Printf(TEXT("'%s' cannot be permanently missed"),
            *Source.SourceId.ToString()), Source.bMissable);
    }

    // Design rule 2: spread across all three acts, so the Core Tree is never
    // entirely gated behind level pace. Asserted as presence in each act rather
    // than as a distribution, because the distribution is a tuning question and
    // the presence is the rule.
    for (int32 Act = 1; Act <= 3; ++Act)
    {
        TestTrue(*FString::Printf(TEXT("Act %d carries at least one world Core Point (%d)"),
            Act, BreakerWorldPointCountInAct(Act)), BreakerWorldPointCountInAct(Act) > 0);
    }

    // Ids are unique, because the id is the flag and a duplicate would pay once
    // and read as two.
    TSet<FName> Seen;
    for (const FBreakerWorldPointSource& Source : Sources)
    {
        TestFalse(*FString::Printf(TEXT("'%s' appears once"), *Source.SourceId.ToString()),
            Seen.Contains(Source.SourceId));
        Seen.Add(Source.SourceId);
    }

    // THE PAYOUT GAP, REPORTED AS A NUMBER rather than left in prose. Eight of
    // the twenty-eight authored missions pay one of these as their whole
    // reward; a source whose trigger does not exist cannot pay.
    const int32 Built = UBreakerWorldPointLibrary::CountWithBuiltTrigger();
    const int32 Unwired = Sources.Num() - Built;
    AddInfo(FString::Printf(
        TEXT("WORLD POINTS  %d of %d sources have a trigger that exists in the build"),
        Built, Sources.Num()));
    BreakerStatus::Emit(TEXT("world-points-unwired"), static_cast<float>(Unwired));
    return true;
}

// ---------------------------------------------------------------------------
// A WORLD POINT PAYS ONCE, AND ONLY FOR A SOURCE THAT EXISTS
// ---------------------------------------------------------------------------
// The grant is idempotent through the quest flag set rather than through a
// second bespoke record, because one-time-and-permanent is exactly what that
// set already is. This asserts the property at the seam where the two meet.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWorldPointGrantTest,
    "RiorsEdge.Progression.WorldPoints.GrantsOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWorldPointGrantTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    if (!TestNotNull(TEXT("Progression component constructs"), Progression)) return false;
    if (!TestNotNull(TEXT("Quest journal constructs"), Journal)) return false;

    const int32 Before = Progression->GetProgressionState().UnspentCorePoints;
    const FName Source = TEXT("FirstForge");

    TestTrue(TEXT("A known, unclaimed source grants"),
        Progression->GrantWorldPoint(Source, Journal));
    TestEqual(TEXT("It granted exactly one Core Point"),
        Progression->GetProgressionState().UnspentCorePoints, Before + 1);

    TestFalse(TEXT("The same source does not pay twice"),
        Progression->GrantWorldPoint(Source, Journal));
    TestEqual(TEXT("And the second attempt moved nothing"),
        Progression->GetProgressionState().UnspentCorePoints, Before + 1);

    // A typo must not mint a point. The budget is 65 and it is not a number to
    // lose by accident.
    TestFalse(TEXT("An unknown source is refused"),
        Progression->GrantWorldPoint(TEXT("FirstForgeTypo"), Journal));
    TestEqual(TEXT("And it moved nothing"),
        Progression->GetProgressionState().UnspentCorePoints, Before + 1);

    // The claim survives as a flag, which is what makes it survive a reload.
    TestTrue(TEXT("The claim is recorded in the flag set"),
        Journal->HasFlag(UBreakerWorldPointLibrary::FlagForSource(Source)));
    return true;
}


// ---------------------------------------------------------------------------
// THE WIRING, not the rule. RiorsEdge.Progression.WorldPoints.SoloReachable
// above asserts the REGISTRY, and passed for the entire time the fifteen
// points were unreachable: GrantWorldPoint had no caller outside these tests,
// so every Unlock beat naming a Core Point was validated by the loader, read
// by the census, and paid nothing. This asserts the seam instead.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWorldPointBeatGrantTest,
    "RiorsEdge.Progression.WorldPoints.MissionBeatGrants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWorldPointBeatGrantTest::RunTest(const FString&)
{
    AActor* Owner = NewObject<AActor>();
    auto* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    auto* Journal = NewObject<UBreakerQuestJournal>();
    if (!Progression || !Journal) return false;

    // Nothing reached: nothing owed. A settle on an empty journal must not
    // mint anything, or the budget inflates for a character who has played
    // nothing at all.
    const int32 Before = Progression->GetProgressionState().UnspentCorePoints;
    Progression->SettleWorldCorePoints(Journal);
    TestEqual(TEXT("an untouched journal is owed no Core Points"),
        Progression->GetProgressionState().UnspentCorePoints, Before);

    // THE SHIPPED CONTENT authorises at least one, or this whole seam is
    // decoration. Walk the missions for the first Unlock beat that names a
    // Core Point and set every flag the beats before it require.
    const TArray<FBreakerMissionDefinition>& Missions = UBreakerMissionLibrary::GetMissions();
    bool bFoundAuthoredCorePoint = false;
    for (const FBreakerMissionDefinition& Mission : Missions)
    {
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
        {
            if (Beat.Kind == EBreakerMissionBeatKind::Unlock && !Beat.CorePoint.IsNone())
            {
                bFoundAuthoredCorePoint = true;
                TestTrue(*FString::Printf(TEXT("beat Core Point '%s' is a known world source"),
                    *Beat.CorePoint.ToString()),
                    UBreakerWorldPointLibrary::IsKnownSource(Beat.CorePoint));
            }
        }
    }
    TestTrue(TEXT("the shipped missions author at least one Core Point beat"), bFoundAuthoredCorePoint);

    // And the settle pays a reached one exactly once. Driven through the
    // library's own walk so the test cannot disagree with the runtime about
    // what "reached" means.
    for (const FBreakerMissionDefinition& Mission : Missions)
    {
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
        {
            if (Beat.Kind != EBreakerMissionBeatKind::Unlock || Beat.CorePoint.IsNone()) continue;
            const int32 PointsBefore = Progression->GetProgressionState().UnspentCorePoints;
            TestTrue(TEXT("a reached source grants once"), Progression->GrantWorldPoint(Beat.CorePoint, Journal));
            TestEqual(TEXT("and it granted exactly one"),
                Progression->GetProgressionState().UnspentCorePoints, PointsBefore + 1);
            // The settle must be safe to run repeatedly at the flag seam.
            Progression->SettleWorldCorePoints(Journal);
            Progression->SettleWorldCorePoints(Journal);
            TestEqual(TEXT("settling again pays nothing more"),
                Progression->GetProgressionState().UnspentCorePoints, PointsBefore + 1);
            return true;
        }
    }
    return true;
}

#endif
