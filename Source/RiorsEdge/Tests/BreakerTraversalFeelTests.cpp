#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Weapons/BreakerWeaponFeel.h"

// ---------------------------------------------------------------------------
// THE TRAVERSAL'S FEEL (D3, owner-ruled): the viewmodel's third mode and the
// exit dip. The stride's maths — nonzero mid-glide, zero at both ends — is
// pinned with the movement mode in RiorsEdge.Movement.LedgeTraversalMode;
// this pins the dip's authored shape. The dip pays through the SAME kick
// spring as the landing and the recoil (one recovery character per
// archetype), off the completed-only broadcast, because the exit's 3 cm
// step-down arrives at zero vertical speed and the landing path can never
// see it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTraversalFeelTest,
    "RiorsEdge.Weapons.TraversalFeel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTraversalFeelTest::RunTest(const FString& Parameters)
{
    const FBreakerViewmodelMotionParams Params;

    // The dip is real: a traversal exit is no longer weightless.
    TestTrue(TEXT("A vault exit dips"), Params.VaultExitKickUnits > 0.0f);
    TestTrue(TEXT("A mantle exit dips"), Params.MantleExitKickUnits > 0.0f);
    // The verbs read differently in the hands exactly as they do in the
    // clock: vault is the not-breaking-stride verb, so it dips lighter.
    TestTrue(TEXT("A vault dips lighter than a mantle"),
        Params.VaultExitKickUnits < Params.MantleExitKickUnits);
    // SUBTLE is the ruled constraint on every ambient channel: no traversal
    // may ever dip harder than the heaviest landing.
    TestTrue(TEXT("The vault dip stays under the heaviest landing"),
        Params.VaultExitKickUnits <= Params.MaxLandingKickUnits);
    TestTrue(TEXT("The mantle dip stays under the heaviest landing"),
        Params.MantleExitKickUnits <= Params.MaxLandingKickUnits);
    return true;
}

#endif
