#include "Misc/AutomationTest.h"
#include "UI/BreakerHUDMath.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerAbilityCooldownHUDTest,
    "RiorsEdge.UI.AbilityCooldownRadial", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityCooldownHUDTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    TestEqual(TEXT("Fresh cooldown is entirely gray"), AbilityRecoveryFraction(10, 10), 0.0f);
    TestEqual(TEXT("Half elapsed means half color"), AbilityRecoveryFraction(5, 10), 0.5f);
    TestEqual(TEXT("Ready is fully colored"), AbilityRecoveryFraction(0, 10), 1.0f);
    TestEqual(TEXT("No cooldown has no waiting fill"), AbilityRecoveryFraction(0, 0), 1.0f);
    TestEqual(TEXT("Overlong remaining clamps gray"), AbilityRecoveryFraction(11, 10), 0.0f);
    TestTrue(TEXT("Fill starts at twelve"), AbilityRadialPoint(0).Equals(FVector2D(0, -1), 0.0001));
    TestTrue(TEXT("Quarter fill proceeds clockwise to three"), AbilityRadialPoint(0.25f).Equals(FVector2D(1, 0), 0.0001));
    TestTrue(TEXT("Half fill reaches six"), AbilityRadialPoint(0.5f).Equals(FVector2D(0, 1), 0.0001));
    TestTrue(TEXT("Full fill closes at twelve"), AbilityRadialPoint(1).Equals(AbilityRadialPoint(0), 0.0001));
    TestEqual(TEXT("Long cooldown rounds upward"), AbilityCooldownText(10.01f), FString(TEXT("11")));
    TestEqual(TEXT("Short cooldown shows tenths"), AbilityCooldownText(2.45f), FString(TEXT("2.5")));
    TestEqual(TEXT("Active cooldown cannot show zero"), AbilityCooldownText(0.001f), FString(TEXT("0.1")));
    TestTrue(TEXT("Ready has no countdown"), AbilityCooldownText(0).IsEmpty());
    TestTrue(TEXT("DoT remains below direct-hit type size"), BreakerUI::DamageDoTPixels < BreakerUI::DamageBodyPixels);
    return true;
}
#endif
