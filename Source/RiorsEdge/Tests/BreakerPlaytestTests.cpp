#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Playtest/BreakerPlaytestComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerPlaytestRatiosTest,
    "RiorsEdge.Playtest.SessionRatios",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerPlaytestRatiosTest::RunTest(const FString& Parameters)
{
    FBreakerPlaytestStats Stats;
    TestEqual(TEXT("Empty accuracy is safe"), Stats.Accuracy(), 0.0f);
    TestEqual(TEXT("Empty weak-point rate is safe"), Stats.WeakPointRate(), 0.0f);
    Stats.ShotsFired = 20;
    Stats.Hits = 10;
    Stats.WeakPointHits = 4;
    TestEqual(TEXT("Accuracy uses shots fired"), Stats.Accuracy(), 50.0f);
    TestEqual(TEXT("Weak-point rate uses hits"), Stats.WeakPointRate(), 40.0f);
    return true;
}

// TTD INSTRUMENTATION (O18's unmeasured half). The pure engagement-gap
// arithmetic, tested world-free in the BreakerRangedBehavior precedent: the
// SAME rule Combat/BreakerEnemy.cpp uses for its TTK sample
// (`EngagedSeconds += FMath::Min(Now - Last, 1.5)`, READ-ONLY to this lane),
// reproduced here as UBreakerPlaytestComponent::AccumulateEngagedSeconds
// rather than edited there.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEngagedSecondsAccumulationTest,
    "RiorsEdge.Playtest.EngagedSecondsAccumulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEngagedSecondsAccumulationTest::RunTest(const FString& Parameters)
{
    using FLib = UBreakerPlaytestComponent;

    // The very first damage event of a life has no previous timestamp: the
    // accumulator must not move (there is no gap to measure yet).
    TestEqual(TEXT("The first hit of a life adds nothing by itself"),
        FLib::AccumulateEngagedSeconds(10.0, -1.0, 0.0f, 1.5f), 0.0f);

    // A gap inside the cap is counted in full.
    TestEqual(TEXT("A sub-cap gap is added in full"),
        FLib::AccumulateEngagedSeconds(10.5, 10.0, 0.0f, 1.5f), 0.5f);

    // A gap past the cap is capped, exactly like the enemy sampler: idle
    // stretches between hits are disengagement, not fighting.
    TestEqual(TEXT("A gap past the cap is capped at the gap"),
        FLib::AccumulateEngagedSeconds(20.0, 10.0, 0.0f, 1.5f), 1.5f);

    // Accumulation is additive across a whole life.
    float Engaged = 0.0f;
    Engaged = FLib::AccumulateEngagedSeconds(1.0, -1.0, Engaged, 1.5f);
    Engaged = FLib::AccumulateEngagedSeconds(1.4, 1.0, Engaged, 1.5f);
    Engaged = FLib::AccumulateEngagedSeconds(1.9, 1.4, Engaged, 1.5f);
    TestEqual(TEXT("Consecutive close hits accumulate their real gaps"), Engaged, 0.9f);

    // A disengagement in the middle of a fight caps only its own gap; the
    // rest of the accumulated total survives untouched.
    Engaged = FLib::AccumulateEngagedSeconds(5.0, 1.9, Engaged, 1.5f);
    TestEqual(TEXT("A mid-fight disengagement caps only its own gap"), Engaged, 2.4f);

    // A zero or negative cap contributes nothing (floored at zero), never
    // subtracts from what was already accumulated.
    TestEqual(TEXT("A zero cap contributes nothing"),
        FLib::AccumulateEngagedSeconds(5.0, 1.0, 2.0f, 0.0f), 2.0f);
    TestEqual(TEXT("A negative cap is floored at zero, never subtracted"),
        FLib::AccumulateEngagedSeconds(5.0, 1.0, 2.0f, -3.0f), 2.0f);

    return true;
}

#endif
