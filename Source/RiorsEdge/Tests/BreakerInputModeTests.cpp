#include "Misc/AutomationTest.h"
#include "Characters/BreakerInputModeMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// The two input-mode rules the settings screen's hold/toggle switches and
// the ADS sensitivity slider rest on. Pure — no pawn, no input component —
// so the rule is proved, not the wiring. The character is the thin caller
// (ApplySprintEdge / ApplyAimEdge / Look).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerInputHoldToggleTest,
    "RiorsEdge.Input.HoldToggle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerInputHoldToggleTest::RunTest(const FString& Parameters)
{
    using namespace BreakerInputMode;

    // HOLD: engaged follows the edge both ways, whatever it was before.
    TestTrue(TEXT("Hold: a press engages"), NextEngaged(false, true, false));
    TestTrue(TEXT("Hold: a press while engaged stays engaged"), NextEngaged(true, true, false));
    TestFalse(TEXT("Hold: a release disengages"), NextEngaged(true, false, false));
    TestFalse(TEXT("Hold: a release while disengaged stays disengaged"), NextEngaged(false, false, false));

    // TOGGLE: a press flips, a release is ignored.
    TestTrue(TEXT("Toggle: a press engages"), NextEngaged(false, true, true));
    TestFalse(TEXT("Toggle: a press while engaged disengages"), NextEngaged(true, true, true));
    TestTrue(TEXT("Toggle: a release while engaged is ignored"), NextEngaged(true, false, true));
    TestFalse(TEXT("Toggle: a release while disengaged is ignored"), NextEngaged(false, false, true));

    // Two full press-release cycles return to where they started, in both
    // modes — the property that makes a mode switch mid-session safe.
    for (const bool bToggle : { false, true })
    {
        bool bEngaged = false;
        bEngaged = NextEngaged(bEngaged, true, bToggle);
        bEngaged = NextEngaged(bEngaged, false, bToggle);
        bEngaged = NextEngaged(bEngaged, true, bToggle);
        bEngaged = NextEngaged(bEngaged, false, bToggle);
        TestFalse(bToggle ? TEXT("Toggle: two presses return to the start") : TEXT("Hold: two cycles return to the start"), bEngaged);
    }
    // And in toggle mode the state after one press-release is ON: that is
    // the whole point of a toggle.
    TestTrue(TEXT("Toggle: one press-release leaves it engaged"),
        NextEngaged(NextEngaged(false, true, true), false, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerScopedSensitivityAimOnlyTest,
    "RiorsEdge.Settings.ScopedSensitivity.AimOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerScopedSensitivityAimOnlyTest::RunTest(const FString& Parameters)
{
    using namespace BreakerInputMode;

    // Not aiming: the multiplier is invisible whatever it holds.
    TestEqual(TEXT("Hip fire is the base sensitivity"), LookGain(1.3f, 0.4f, false), 1.3f);
    TestEqual(TEXT("Hip fire ignores a multiplier above one"), LookGain(0.8f, 3.0f, false), 0.8f);

    // Aiming: base times scoped.
    TestEqual(TEXT("Aiming multiplies by the scoped value"), LookGain(1.3f, 0.5f, true), 0.65f, UE_KINDA_SMALL_NUMBER);
    TestEqual(TEXT("Aiming with a multiplier above one raises the gain"), LookGain(0.8f, 2.0f, true), 1.6f, UE_KINDA_SMALL_NUMBER);

    // The shipped 1.0 is the identity in both states — the default changes
    // nothing the owner already felt.
    TestEqual(TEXT("A 1.0 multiplier is the identity while aiming"), LookGain(1.3f, 1.0f, true), 1.3f);
    TestEqual(TEXT("A 1.0 multiplier is the identity at the hip"), LookGain(1.3f, 1.0f, false), 1.3f);
    return true;
}

#endif
