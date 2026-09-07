#include "Misc/AutomationTest.h"
#include "UI/BreakerSandboxModel.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerSandboxModelTest, "RiorsEdge.UI.Sandbox.Controls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerSandboxModelTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Gym defaults to area level"), BreakerSandbox::ItemLevel(TEXT("Lvl_Gym"), 0, 25, 7, 120), 25);
    TestEqual(TEXT("Anchor defaults to character level"), BreakerSandbox::ItemLevel(TEXT("Lvl_Anchor"), 0, 25, 7, 120), 7);
    TestEqual(TEXT("Fernhall defaults to character level"), BreakerSandbox::ItemLevel(TEXT("Lvl_Fernhall"), 0, 25, 7, 120), 7);
    TestEqual(TEXT("PIE field remains outside gym"), BreakerSandbox::ItemLevel(TEXT("UEDPIE_0_Lvl_Fernhall"), 0, 25, 7, 120), 7);
    TestEqual(TEXT("Explicit chase item level remains available"), BreakerSandbox::ItemLevel(TEXT("Lvl_Anchor"), 120, 25, 7, 120), 120);
    int32 Seed = 0;
    TestTrue(TEXT("Fresh mode ignores previous seed"), BreakerSandbox::Seed(true, TEXT("17"), 23, Seed));
    TestEqual(TEXT("Fresh draw used"), Seed, 23);
    TestTrue(TEXT("Repeat accepts recorded seed"), BreakerSandbox::Seed(false, TEXT("17"), 23, Seed));
    TestEqual(TEXT("Repeat remains deterministic"), Seed, 17);
    TestFalse(TEXT("Invalid repeat cannot silently grant seed zero"), BreakerSandbox::Seed(false, TEXT("nonsense"), 23, Seed));
    TestFalse(TEXT("Empty repeat cannot silently make a fresh item"), BreakerSandbox::Seed(false, TEXT(""), 23, Seed));
    TestFalse(TEXT("Numeric prefix is not a whole seed"), BreakerSandbox::Seed(false, TEXT("17junk"), 23, Seed));
    TestFalse(TEXT("Fraction is not a whole seed"), BreakerSandbox::Seed(false, TEXT("1.5"), 23, Seed));
    TestFalse(TEXT("Positive overflow refused"), BreakerSandbox::Seed(false, TEXT("2147483648"), 23, Seed));
    TestFalse(TEXT("Negative overflow refused"), BreakerSandbox::Seed(false, TEXT("-2147483649"), 23, Seed));
    TestTrue(TEXT("Minimum signed seed accepted"), BreakerSandbox::Seed(false, TEXT("-2147483648"), 23, Seed));
    TestEqual(TEXT("Minimum seed preserved"), Seed, MIN_int32);
    TestTrue(TEXT("Maximum signed seed accepted"), BreakerSandbox::Seed(false, TEXT("+2147483647"), 23, Seed));
    TestEqual(TEXT("Maximum seed preserved"), Seed, MAX_int32);
    return true;
}
#endif
