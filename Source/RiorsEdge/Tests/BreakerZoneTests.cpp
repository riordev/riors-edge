#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Combat/BreakerZoneActor.h"
#include "Combat/BreakerZoneMath.h"

// The zone primitive (Ability-Implementation-Spec §5.3). Membership, cadence
// and expiry are pure arithmetic and live in UBreakerZoneMath precisely so they
// can be proven with no world, no actor and no overlap query — the same
// precedent as BreakerRangedBehavior and BreakerMonsterChassis.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerZoneMembershipTest,
    "RiorsEdge.Combat.Zone.Membership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneMembershipTest::RunTest(const FString& Parameters)
{
    const FVector Center(0.0f, 0.0f, 0.0f);
    const float Radius = 400.0f;   // Class-Kits C3: a 4 m Rot
    const float HalfHeight = 250.0f;

    TestTrue(TEXT("The centre is inside"), UBreakerZoneMath::IsInsideZone(Center, Radius, HalfHeight, Center));
    TestTrue(TEXT("Just inside the rim is inside"),
        UBreakerZoneMath::IsInsideZone(Center, Radius, HalfHeight, FVector(399.0f, 0.0f, 0.0f)));
    TestFalse(TEXT("Just outside the rim is outside"),
        UBreakerZoneMath::IsInsideZone(Center, Radius, HalfHeight, FVector(401.0f, 0.0f, 0.0f)));

    // The cylinder is the point of the half-height: a target standing on a
    // crate inside the footprint is in the puddle...
    TestTrue(TEXT("A target on a crate inside the footprint is inside"),
        UBreakerZoneMath::IsInsideZone(Center, Radius, HalfHeight, FVector(100.0f, 0.0f, 200.0f)));
    // ...and one on the roof above it is not.
    TestFalse(TEXT("A target on the roof above is outside"),
        UBreakerZoneMath::IsInsideZone(Center, Radius, HalfHeight, FVector(100.0f, 0.0f, 600.0f)));

    // Zero half-height is a sphere, which is what a boss telegraph in the air
    // wants.
    TestFalse(TEXT("A sphere excludes a point outside its radius in Z"),
        UBreakerZoneMath::IsInsideZone(Center, Radius, 0.0f, FVector(0.0f, 0.0f, 401.0f)));
    TestTrue(TEXT("A sphere includes a point inside its radius in Z"),
        UBreakerZoneMath::IsInsideZone(Center, Radius, 0.0f, FVector(0.0f, 0.0f, 399.0f)));

    // A degenerate zone contains nothing rather than everything.
    TestFalse(TEXT("A zero-radius zone contains nothing"),
        UBreakerZoneMath::IsInsideZone(Center, 0.0f, HalfHeight, Center));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerZoneCadenceTest,
    "RiorsEdge.Combat.Zone.TickCadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneCadenceTest::RunTest(const FString& Parameters)
{
    // The bug this test exists to prevent: a cadence that RESETS its countdown
    // on each tick loses the remainder every frame, so the same zone ticks
    // slower at 60fps than at 30fps. Sixty frames of 1/60s across a 1.0s
    // interval must deliver exactly one tick, and the leftover must carry.
    // Frame times here are exact binary fractions (1/64, 1/128) rather than
    // 1/60 and 1/120: this test is about the ACCUMULATOR losing time, and a
    // decimal frame time would fail it for float summation error instead,
    // which would be a test bug wearing the costume of the bug being hunted.
    float Countdown = 1.0f;
    int32 Total = 0;
    for (int32 Frame = 0; Frame < 64; ++Frame)
    {
        Total += UBreakerZoneMath::ConsumeTicks(Countdown, 1.0f / 64.0f, 1.0f);
    }
    TestEqual(TEXT("One second of frames is exactly one tick"), Total, 1);

    // Frame rate must not change the tick count over the same wall time.
    float SlowCountdown = 1.0f;
    int32 SlowTotal = 0;
    for (int32 Frame = 0; Frame < 640; ++Frame)
    {
        SlowTotal += UBreakerZoneMath::ConsumeTicks(SlowCountdown, 1.0f / 64.0f, 1.0f);
    }
    float FastCountdown = 1.0f;
    int32 FastTotal = 0;
    for (int32 Frame = 0; Frame < 1280; ++Frame)
    {
        FastTotal += UBreakerZoneMath::ConsumeTicks(FastCountdown, 1.0f / 128.0f, 1.0f);
    }
    TestEqual(TEXT("Ten seconds is ten ticks at one frame rate"), SlowTotal, 10);
    TestEqual(TEXT("Ten seconds is ten ticks at double the frame rate"), FastTotal, 10);

    // A long frame owes several ticks and delivers them all.
    float LongFrame = 1.0f;
    TestEqual(TEXT("A 3.5s frame on a 1s zone owes three ticks"),
        UBreakerZoneMath::ConsumeTicks(LongFrame, 3.5f, 1.0f), 3);

    // ...up to the hitch guard, past which the excess is DISCARDED rather than
    // banked: a zone must not fire a burst the instant a stall ends.
    float Hitch = 0.1f;
    const int32 Delivered = UBreakerZoneMath::ConsumeTicks(Hitch, 30.0f, 0.1f, 8);
    TestEqual(TEXT("A hitch is capped"), Delivered, 8);
    TestTrue(TEXT("Nothing is banked past the cap"), Hitch > 0.0f);

    // A paused zone does not age; that is the Long Dark keystone.
    TestEqual(TEXT("An unpaused zone ages"), UBreakerZoneMath::RemainingAfter(6.0f, 1.0f, false), 5.0f);
    TestEqual(TEXT("A paused zone does not age"), UBreakerZoneMath::RemainingAfter(6.0f, 1.0f, true), 6.0f);

    // A zero or negative interval cannot spin: it delivers nothing rather than
    // looping forever.
    float Degenerate = 1.0f;
    TestEqual(TEXT("A zero interval delivers nothing"), UBreakerZoneMath::ConsumeTicks(Degenerate, 1.0f, 0.0f), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerZoneAntiStackTest,
    "RiorsEdge.Combat.Zone.AntiStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneAntiStackTest::RunTest(const FString& Parameters)
{
    const float Radius = 400.0f;

    // VW4: a recast on top of a live zone refreshes it rather than stacking.
    TestTrue(TEXT("A recast in the same place refreshes"),
        UBreakerZoneMath::ShouldRefreshExisting(FVector::ZeroVector, FVector(50.0f, 0.0f, 0.0f), Radius, 0.5f));
    // But two puddles a real distance apart are two puddles. Overlap is
    // deliberately NOT the test: two 4 m zones 7 m apart overlap slightly and
    // are obviously separate.
    TestFalse(TEXT("A cast 3 m away is a second puddle"),
        UBreakerZoneMath::ShouldRefreshExisting(FVector::ZeroVector, FVector(300.0f, 0.0f, 0.0f), Radius, 0.5f));
    TestFalse(TEXT("A degenerate zone never absorbs a recast"),
        UBreakerZoneMath::ShouldRefreshExisting(FVector::ZeroVector, FVector::ZeroVector, 0.0f, 0.5f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRotDefinitionTest,
    "RiorsEdge.Abilities.Rot.Payload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRotDefinitionTest::RunTest(const FString& Parameters)
{
    const UBreakerAbility_Rot* Rot = GetDefault<UBreakerAbility_Rot>();
    if (!TestNotNull(TEXT("Rot has a default object"), Rot)) return false;

    // Class-Kits §2.2 C3 quotes these three exactly. They are the only Rot
    // numbers the design supplies, so they are the only ones pinned here —
    // everything else is O2 PLACEHOLDER and must stay free to move.
    TestEqual(TEXT("Rot is a 4 m zone"), Rot->RadiusCm, 400.0f);
    TestEqual(TEXT("Rot lasts 6 s"), Rot->DurationSeconds, 6.0f);
    TestEqual(TEXT("Rot strips a flat 40 armour"), Rot->FlatArmorReduction, 40.0f);

    // Aim: a trace that hit places the puddle at the hit; one that missed
    // places it at the end of the aim line rather than swallowing the cast or
    // dropping it underfoot.
    const FVector View(0.0f, 0.0f, 100.0f);
    const FVector Forward(1.0f, 0.0f, 0.0f);
    TestEqual(TEXT("A hit places the puddle at the hit"),
        UBreakerAbility_Rot::AimPoint(View, Forward, 2500.0f, true, FVector(500.0f, 0.0f, 0.0f)), FVector(500.0f, 0.0f, 0.0f));
    TestEqual(TEXT("A miss places it at the end of the aim line"),
        UBreakerAbility_Rot::AimPoint(View, Forward, 2500.0f, false, FVector::ZeroVector), FVector(2500.0f, 0.0f, 100.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerZoneActorCadenceTest,
    "RiorsEdge.Combat.Zone.ActorCadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneActorCadenceTest::RunTest(const FString& Parameters)
{
    // The actor's own clock, driven directly rather than from Tick — the same
    // precedent as UBreakerAbilityStateComponent::AdvanceTime. There is no
    // world here, so membership (an overlap query) is not exercised; the
    // cadence, the lifetime and the pause are.
    ABreakerZoneActor* Zone = NewObject<ABreakerZoneActor>();

    FBreakerZoneSpec Spec;
    Spec.RadiusCm = 400.0f;
    Spec.Duration = 6.0f;
    Spec.TickInterval = 1.0f;
    Zone->ConfigureZone(Spec, nullptr);

    TestEqual(TEXT("A freshly placed zone has ticked nothing"), Zone->GetTicksDelivered(), 0);
    TestEqual(TEXT("A freshly placed zone carries its full duration"), Zone->GetRemainingDuration(), 6.0f);

    // The first tick lands one interval in, not on the placement frame: a zone
    // that damages the instant it lands makes its cadence unreadable and hands
    // a free tick to anyone who recasts on top of it.
    Zone->AdvanceZone(0.5f);
    TestEqual(TEXT("Half an interval delivers nothing"), Zone->GetTicksDelivered(), 0);
    Zone->AdvanceZone(0.5f);
    TestEqual(TEXT("The first tick lands one interval in"), Zone->GetTicksDelivered(), 1);

    for (int32 Index = 0; Index < 4; ++Index) Zone->AdvanceZone(1.0f);
    TestEqual(TEXT("Five seconds is five ticks"), Zone->GetTicksDelivered(), 5);
    TestEqual(TEXT("The lifetime burned down with them"), Zone->GetRemainingDuration(), 1.0f);

    // Long Dark (VW12): a paused zone keeps working and stops ageing.
    Zone->SetExpiryPaused(true);
    for (int32 Index = 0; Index < 5; ++Index) Zone->AdvanceZone(1.0f);
    TestEqual(TEXT("A paused zone keeps ticking"), Zone->GetTicksDelivered(), 10);
    TestEqual(TEXT("A paused zone does not age"), Zone->GetRemainingDuration(), 1.0f);

    // VW4: a refresh resets the clock rather than banking duration.
    Zone->RefreshDuration(6.0f);
    TestEqual(TEXT("A refresh restores the full duration"), Zone->GetRemainingDuration(), 6.0f);
    Zone->RefreshDuration(2.0f);
    TestEqual(TEXT("A refresh never shortens a longer remaining life"), Zone->GetRemainingDuration(), 6.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerZoneSpecDefaultsTest,
    "RiorsEdge.Combat.Zone.SpecDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerZoneSpecDefaultsTest::RunTest(const FString& Parameters)
{
    const FBreakerZoneSpec Spec;
    // A default zone must be inert rather than dangerous: nothing spawned by
    // accident should damage or strip anything.
    TestEqual(TEXT("A default zone deals no damage"), Spec.TickDamage.BaseDamage, 0.0f);
    TestFalse(TEXT("A default zone applies no status"), Spec.bAppliesStatus);
    TestEqual(TEXT("A default zone strips no armour"), Spec.FlatArmorReduction, 0.0f);
    TestTrue(TEXT("A default zone still has a legal cadence"), Spec.TickInterval > 0.0f);

    const ABreakerZoneActor* Defaults = GetDefault<ABreakerZoneActor>();
    if (!TestNotNull(TEXT("The zone actor has a default object"), Defaults)) return false;
    TestTrue(TEXT("The hitch guard is a real cap"), Defaults->MaximumTicksPerAdvance >= 1);
    TestTrue(TEXT("The refresh radius fraction is inside the zone"),
        Defaults->RefreshFractionOfRadius > 0.0f && Defaults->RefreshFractionOfRadius <= 1.0f);
    return true;
}

#endif
