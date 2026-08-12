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

#endif
