#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Movement/BreakerCharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeystoneOverrideSuspensionTest,
    "RiorsEdge.Movement.KeystoneOverrides.Suspension",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeystoneOverrideSuspensionTest::RunTest(const FString& Parameters)
{
    // Terminal Velocity (Class-Kits.md:192): both chains are booleans, keyed
    // and duration-expiring, modeled on PushSpeedMultiplier's own trio. This
    // test covers push/pop/independence for both the dash-cooldown chain and
    // the (since-retired) wall-ride-timer chain.
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();

    // --- Dash cooldown suspension ------------------------------------------
    TestFalse(TEXT("Dash cooldown starts unsuspended"), Movement->IsDashCooldownSuspended());

    Movement->PushDashCooldownSuspension(TEXT("Keystone.Swift.TerminalVelocity"), 8.0f);
    TestTrue(TEXT("A pushed suspension reads active"), Movement->IsDashCooldownSuspended());

    Movement->PopDashCooldownSuspension(TEXT("Keystone.Swift.TerminalVelocity"));
    TestFalse(TEXT("Popping the only key clears the suspension"), Movement->IsDashCooldownSuspended());

    // Two independent keys: popping one leaves the other holding the gate,
    // and popping the second is what actually clears it.
    Movement->PushDashCooldownSuspension(TEXT("Keystone.Swift.TerminalVelocity"), 8.0f);
    Movement->PushDashCooldownSuspension(TEXT("Debuff.SomeOtherSource"), 8.0f);
    TestTrue(TEXT("Two keys pushed still reads active"), Movement->IsDashCooldownSuspended());
    Movement->PopDashCooldownSuspension(TEXT("Keystone.Swift.TerminalVelocity"));
    TestTrue(TEXT("Popping one of two independent keys leaves the other active"), Movement->IsDashCooldownSuspended());
    Movement->PopDashCooldownSuspension(TEXT("Debuff.SomeOtherSource"));
    TestFalse(TEXT("Popping the second key clears the suspension"), Movement->IsDashCooldownSuspended());

    // A NAME_None key is refused, matching PushSpeedMultiplier's own guard.
    Movement->PushDashCooldownSuspension(NAME_None, 8.0f);
    TestFalse(TEXT("An invalid key is not pushed"), Movement->IsDashCooldownSuspended());

    // Popping a key that was never pushed is a harmless no-op.
    Movement->PopDashCooldownSuspension(TEXT("Never.Pushed"));
    TestFalse(TEXT("Popping an absent key is a no-op"), Movement->IsDashCooldownSuspended());

    // The wall-ride suspension chain retired with its verb (Part One-R);
    // the dash chain above is the suspension shape's surviving regression.
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeystoneOverrideExpiryTest,
    "RiorsEdge.Movement.KeystoneOverrides.Expiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeystoneOverrideExpiryTest::RunTest(const FString& Parameters)
{
    // NOT driven the way PushSpeedMultiplier's own test drives expiry,
    // because it doesn't: RiorsEdge.Movement.SpeedMultiplier never advances
    // time or asserts an expired multiplier stops applying. A NewObject()
    // component has no World, so PushSpeedMultiplier's own Duration > 0
    // branch is unreachable there too — copying that (non-existent) pattern
    // would leave expiry equally untested here. Instead this exercises the
    // one piece of the prune loop that needs no UWorld: the shared
    // IsSuspensionActive(ExpiryTime, Now) predicate, which both new prune
    // loops call verbatim in place of re-deriving the comparison inline.
    using FMovement = UBreakerCharacterMovementComponent;

    TestTrue(TEXT("A never-expiring entry (-1) stays active at any time"),
        FMovement::IsSuspensionActive(-1.0, 0.0));
    TestTrue(TEXT("A never-expiring entry stays active arbitrarily far in the future"),
        FMovement::IsSuspensionActive(-1.0, 100000.0));
    TestTrue(TEXT("An entry strictly before its expiry is active"),
        FMovement::IsSuspensionActive(10.0, 9.0));
    TestFalse(TEXT("An entry exactly at its expiry has expired"),
        FMovement::IsSuspensionActive(10.0, 10.0));
    TestFalse(TEXT("An entry past its expiry has expired"),
        FMovement::IsSuspensionActive(10.0, 10.5));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKeystoneOverrideDashGateTest,
    "RiorsEdge.Movement.KeystoneOverrides.DashGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKeystoneOverrideDashGateTest::RunTest(const FString& Parameters)
{
    // TryDash's own cooldown/bSliding gate needs a live World (it early-outs
    // on !World before either bSliding or the cooldown term is even read),
    // and RiorsEdge.Movement.StateSafety already proves that: it asserts
    // TryDash returns false on a bare NewObject() component purely because
    // GetWorld() is null there, not because of bSliding or the cooldown. That
    // half of the claim — "bSliding still blocks a dash while the cooldown is
    // suspended" — is NOT exercised here for the same reason: this suite
    // builds no world, so it can never reach the branch where !World is
    // false. What IS exercised: GetComposedDashCooldownMultiplier() is
    // unaffected by the suspension being pushed, which was the other half of
    // the brief and needs no world at all.
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>();

    const float BaselineMultiplier = Movement->GetComposedDashCooldownMultiplier();
    Movement->PushDashCooldownSuspension(TEXT("Keystone.Swift.TerminalVelocity"), 8.0f);
    TestEqual(TEXT("The dash cooldown SCALE is unaffected by the suspension being active"),
        Movement->GetComposedDashCooldownMultiplier(), BaselineMultiplier);
    Movement->PopDashCooldownSuspension(TEXT("Keystone.Swift.TerminalVelocity"));
    TestEqual(TEXT("...and unaffected by it being popped, too"),
        Movement->GetComposedDashCooldownMultiplier(), BaselineMultiplier);

    // TryDash still safely refuses with no world, exactly as StateSafety
    // documents, regardless of a pushed suspension — the suspension must not
    // manufacture a world the gate does not have.
    Movement->PushDashCooldownSuspension(TEXT("Keystone.Swift.TerminalVelocity"), 8.0f);
    TestFalse(TEXT("Dash still safely rejects a component without a world, suspension notwithstanding"),
        Movement->TryDash(FVector::ForwardVector));

    return true;
}

#endif
