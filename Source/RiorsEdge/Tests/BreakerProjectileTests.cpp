#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "Combat/BreakerProjectileBase.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "GameFramework/Actor.h"

// The shared projectile base (Ability-Implementation-Spec §5.5's missing hook).
// Two projectiles existed before it and both hardcoded their own travel,
// collision, lifetime and impact; Fracture would have been a third copy.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerProjectileTravelTest,
    "RiorsEdge.Combat.Projectile.Travel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerProjectileTravelTest::RunTest(const FString& Parameters)
{
    const FVector Origin(0.0f, 0.0f, 100.0f);
    const FVector Forward(1.0f, 0.0f, 0.0f);

    TestEqual(TEXT("A projectile travels speed times time"),
        ABreakerProjectileBase::PositionAfter(Origin, Forward, 1000.0f, 2.0f), FVector(2000.0f, 0.0f, 100.0f));
    // Direction need not arrive normalized; a caller passing a view vector
    // scaled by range must not get a projectile that moves at range times
    // speed.
    TestEqual(TEXT("Direction is normalized before use"),
        ABreakerProjectileBase::PositionAfter(Origin, Forward * 500.0f, 1000.0f, 1.0f), FVector(1000.0f, 0.0f, 100.0f));
    TestEqual(TEXT("A zero direction goes nowhere rather than to NaN"),
        ABreakerProjectileBase::PositionAfter(Origin, FVector::ZeroVector, 1000.0f, 1.0f), Origin);

    TestEqual(TEXT("Flight time is distance over speed"),
        ABreakerProjectileBase::FlightTimeOver(2000.0f, 1000.0f), 2.0f);
    // A speed of zero is a stationary projectile, and dividing by it would be
    // an infinity somebody eventually compares against.
    TestTrue(TEXT("A zero-speed projectile never arrives"),
        ABreakerProjectileBase::FlightTimeOver(2000.0f, 0.0f) > 1.0e6f);

    TestEqual(TEXT("Range is speed times lifetime"),
        ABreakerProjectileBase::MaximumTravelDistance(1100.0f, 6.0f), 6600.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerProjectileDefaultsTest,
    "RiorsEdge.Combat.Projectile.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerProjectileDefaultsTest::RunTest(const FString& Parameters)
{
    const ABreakerProjectileBase* Base = GetDefault<ABreakerProjectileBase>();
    if (!TestNotNull(TEXT("The projectile base has a default object"), Base)) return false;
    // A projectile with no lifetime leaks one actor per shot.
    TestTrue(TEXT("Every projectile has a finite lifetime"), Base->MaximumLifetime > 0.0f);
    TestTrue(TEXT("Every projectile has a collision radius"), Base->CollisionRadiusCm > 0.0f);
    TestFalse(TEXT("Nothing has impacted before it is fired"), Base->HasImpacted());

    // The re-expression is deliberately not a retune: the enemy orb keeps every
    // number it shipped with, and the ranged-enemy test that reads them still
    // reads them off the base's properties.
    const ABreakerEnemyProjectile* Orb = GetDefault<ABreakerEnemyProjectile>();
    if (!TestNotNull(TEXT("The enemy orb has a default object"), Orb)) return false;
    TestEqual(TEXT("The orb kept its 6s lifetime"), Orb->MaximumLifetime, 6.0f);
    TestEqual(TEXT("The orb kept its 30 cm collision"), Orb->CollisionRadiusCm, 30.0f);
    TestEqual(TEXT("The orb kept its glow"), Orb->GlowIntensity, 2400.0f);
    TestEqual(TEXT("The orb kept its glow radius"), Orb->GlowRadius, 700.0f);
    TestTrue(TEXT("The orb is still an oversized visual"), Orb->VisualScale > 0.5f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerProjectileCarriedStatusTest,
    "RiorsEdge.Combat.Projectile.CarriedStatus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerProjectileCarriedStatusTest::RunTest(const FString& Parameters)
{
    ABreakerProjectileBase* Projectile = NewObject<ABreakerProjectileBase>();

    FBreakerCarriedStatus Good;
    Good.Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    Good.Spec.Duration = 4.0f;
    Good.Spec.TickInterval = 1.0f;
    Good.Spec.BaseDamagePerTick = 6.0f;
    Projectile->AddImpactStatus(Good);
    TestEqual(TEXT("A valid status is carried"), Projectile->GetImpactStatuses().Num(), 1);

    // A mis-authored payload is rejected at ARMING time, where it is
    // debuggable, rather than silently doing nothing on impact.
    FBreakerCarriedStatus NoTag;
    NoTag.Spec.Duration = 4.0f;
    Projectile->AddImpactStatus(NoTag);
    FBreakerCarriedStatus NoDuration;
    NoDuration.Spec.StatusTag = Good.Spec.StatusTag;
    NoDuration.Spec.Duration = 0.0f;
    Projectile->AddImpactStatus(NoDuration);
    TestEqual(TEXT("A tagless or durationless status is refused"), Projectile->GetImpactStatuses().Num(), 1);

    // MS7 makes Fracture apply two cycle positions per cast, which is why the
    // projectile carries a LIST and not one status.
    FBreakerCarriedStatus Second = Good;
    Second.Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false);
    Projectile->AddImpactStatus(Second);
    TestEqual(TEXT("A projectile can carry two statuses"), Projectile->GetImpactStatuses().Num(), 2);

    Projectile->SetImpactStatuses(TArray<FBreakerCarriedStatus>());
    TestEqual(TEXT("Setting an empty list clears the payload"), Projectile->GetImpactStatuses().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStatusCycleTest,
    "RiorsEdge.Combat.Cycle.Deterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusCycleTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerStatusCycleComponent* Cycle = UBreakerStatusCycleComponent::FindOrAdd(Owner);
    if (!TestNotNull(TEXT("The cycle component is created on demand"), Cycle)) return false;
    TestTrue(TEXT("FindOrAdd returns the same component twice"),
        UBreakerStatusCycleComponent::FindOrAdd(Owner) == Cycle);

    // Zero-setup: Bleed and Poison are what a Caster can apply today.
    const int32 Length = Cycle->GetCycleLength();
    TestTrue(TEXT("The cycle ships seeded"), Length >= 2);

    // Deterministic, because the HUD previews the next position and a preview
    // that can lie is worse than no preview.
    const FGameplayTag First = Cycle->PeekNext(0);
    const FGameplayTag Second = Cycle->PeekNext(1);
    TestTrue(TEXT("Peeking does not advance"), Cycle->PeekNext(0) == First);
    TestTrue(TEXT("The lookahead is the next position"), Second != First);

    TestTrue(TEXT("Advancing returns the position it consumed"), Cycle->AdvanceCycle() == First);
    TestTrue(TEXT("The cursor moved onto the peeked position"), Cycle->PeekNext(0) == Second);

    // It WRAPS. A cycle that runs off the end is a crash or a stuck position.
    for (int32 Index = 0; Index < Length; ++Index) Cycle->AdvanceCycle();
    TestTrue(TEXT("The cycle wraps back around"), Cycle->PeekNext(0) == Second);

    // Growth is idempotent by tag: Siphon granting Void twice must not make
    // Void come round twice as often.
    FBreakerCycleEntry Void;
    Void.Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Void"), false);
    Void.Spec.Duration = 4.0f;
    Void.Spec.TickInterval = 1.0f;
    Void.DamageFamily = EBreakerDamageFamily::Elemental;
    Cycle->AddStatusType(Void);
    const int32 Grown = Cycle->GetCycleLength();
    Cycle->AddStatusType(Void);
    TestEqual(TEXT("Adding the same status twice does not lengthen the cycle"), Cycle->GetCycleLength(), Grown);
    TestEqual(TEXT("Adding a new status lengthens it once"), Grown, Length + 1);

    // Removal keeps the cursor legal; a cursor past the end reads out of bounds.
    Cycle->RemoveStatusType(Void.Spec.StatusTag);
    TestEqual(TEXT("Removal shortens the cycle"), Cycle->GetCycleLength(), Length);
    TestTrue(TEXT("The cursor is still inside the cycle"), Cycle->GetCursor() < Cycle->GetCycleLength());

    // Fracture refuses to fire on an empty cycle rather than spending 30 Mana
    // on a bullet; the emptiness itself must at least be representable.
    for (const FGameplayTag& Tag : Cycle->GetAvailableStatusTypes()) Cycle->RemoveStatusType(Tag);
    TestEqual(TEXT("A cycle can be emptied"), Cycle->GetCycleLength(), 0);
    TestFalse(TEXT("An empty cycle peeks to nothing"), Cycle->PeekNext(0).IsValid());
    TestFalse(TEXT("An empty cycle advances to nothing"), Cycle->AdvanceCycle().IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFractureDefaultsTest,
    "RiorsEdge.Abilities.Fracture.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFractureDefaultsTest::RunTest(const FString& Parameters)
{
    const UBreakerAbility_Fracture* Fracture = GetDefault<UBreakerAbility_Fracture>();
    if (!TestNotNull(TEXT("Fracture has a default object"), Fracture)) return false;
    TestEqual(TEXT("Fracture applies one cycle position at base"), Fracture->CyclePositionsPerCast, 1);
    TestNotNull(TEXT("Fracture has a projectile class to spawn"), Fracture->ProjectileClass.Get());
    // The shot must outrange nothing silently: a projectile whose lifetime
    // kills it before it crosses a room is a dead zone at the edge of the
    // reticle.
    const ABreakerProjectileBase* Base = GetDefault<ABreakerProjectileBase>();
    TestTrue(TEXT("Fracture's round crosses a useful distance before expiring"),
        ABreakerProjectileBase::MaximumTravelDistance(Fracture->ProjectileSpeed, Base->MaximumLifetime) > 5000.0f);
    return true;
}

#endif
