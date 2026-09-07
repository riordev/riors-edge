#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Game/BreakerWaveBudget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerBreachCompositionTest,
    "RiorsEdge.Encounters.Breach.AuthoredEscalation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBreachCompositionTest::RunTest(const FString& Parameters)
{
    const FBreakerWaveBudgetParams Profile = UBreakerWaveBudgetLibrary::MakeBreachWaveBudget();
    const auto First = UBreakerWaveBudgetLibrary::SolveWave(1, 1, Profile);
    const auto Second = UBreakerWaveBudgetLibrary::SolveWave(2, 1, Profile);
    const auto Third = UBreakerWaveBudgetLibrary::SolveWave(3, 1, Profile);
    const auto Boss = UBreakerWaveBudgetLibrary::SolveWave(4, 1, Profile);
    TestEqual(TEXT("Armoured opening has one Warden"), First.Wardens, 1);
    TestEqual(TEXT("Opening allows reading armour without ranged crossfire"), First.RangedSources(), 0);
    TestTrue(TEXT("Second formation introduces the flanker"), Second.Skirmishers > 0);
    TestEqual(TEXT("Lattice held until combined formation"), Second.Lattices, 0);
    TestTrue(TEXT("Third formation combines armour, flanker, ranged and elite"),
        Third.Wardens == 1 && Third.Skirmishers > 0 && Third.Lattices > 0 && Third.Elites == 1);
    TestTrue(TEXT("Commander finishes the fourth encounter"), Boss.bBoss);
    TestEqual(TEXT("No budget adds overlap the commander's own summons"), Boss.TotalEnemies(), 1);
    for (int32 Party = 1; Party <= 4; ++Party)
        for (int32 Wave = 1; Wave <= 4; ++Wave)
        {
            FString Reason;
            TestTrue(*FString::Printf(TEXT("Wave %d party %d remains legal"), Wave, Party),
                UBreakerWaveBudgetLibrary::IsCompositionLegal(
                    UBreakerWaveBudgetLibrary::SolveWave(Wave, Party, Profile), Party, Profile, Reason));
        }
    float Delay = 0;
    TestFalse(TEXT("No wave follows the Marshal"), UBreakerWaveBudgetLibrary::GetAutoAdvanceDelay(4, Profile, Delay));
    return true;
}
#endif
