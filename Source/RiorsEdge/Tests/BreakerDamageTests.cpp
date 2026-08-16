#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerShieldRoutingTest,
    "RiorsEdge.Combat.Damage.ShieldRouting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerShieldRoutingTest::RunTest(const FString& Parameters)
{
    FBreakerDamageRequest Request;
    Request.BaseDamage = 75.0f;
    Request.bCanCritical = false;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    FBreakerDefenseState Defense;
    Defense.Health = 100.0f;
    Defense.Shield = 50.0f;

    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestEqual(TEXT("Shield absorbs its available amount"), Result.ShieldDamage, 50.0f);
    TestEqual(TEXT("Overflow reaches health"), Result.HealthDamage, 25.0f);
    TestTrue(TEXT("Shield break is reported"), Result.bShieldBroken);
    TestFalse(TEXT("Target survives"), Result.bKilled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArmorTest,
    "RiorsEdge.Combat.Damage.ArmorAndPenetration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArmorTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("100 armour mitigates 50 percent"), UBreakerDamageLibrary::CalculateArmorMitigation(100.0f, 0.0f), 0.5f);
    TestEqual(TEXT("Penetration reduces effective armour"), UBreakerDamageLibrary::CalculateArmorMitigation(100.0f, 50.0f), 1.0f / 3.0f);
    TestEqual(TEXT("Mitigation caps at 80 percent"), UBreakerDamageLibrary::CalculateArmorMitigation(10000.0f, 0.0f), 0.8f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSnapshotDotTest,
    "RiorsEdge.Combat.Damage.SnapshotCriticalDot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSnapshotDotTest::RunTest(const FString& Parameters)
{
    FBreakerStatusApplicationSpec Status;
    Status.BaseDamagePerTick = 10.0f;
    Status.InitialStacks = 2;
    Status.ProcCoefficient = 0.25f;
    Status.Snapshot.SourcePower = 1.2f;
    Status.Snapshot.DamageOverTimeMultiplier = 1.5f;
    Status.Snapshot.CriticalMultiplier = 2.0f;
    Status.Snapshot.bRolledCritical = true;

    FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(Status, EBreakerDamageFamily::Physical, 3, nullptr, FVector::ZeroVector, false);
    Tick.bBypassShield = true;
    FBreakerDefenseState Defense;
    Defense.Health = 100.0f;
    Defense.Shield = 100.0f;
    Defense.Armor = 100.0f;

    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Tick, Defense);
    // Raw: 10 * 2 stacks * 1.2 power * 1.5 DoT * 2 crit = 72.
    // Physical bypass DoT receives half of normal 50% armour mitigation: 25%.
    TestEqual(TEXT("Snapshot critical fixes raw tick damage"), Result.RawDamage, 72.0f);
    TestEqual(TEXT("Physical bypass DoT uses half armour mitigation"), Result.HealthDamage, 54.0f);
    TestEqual(TEXT("Bypass leaves shield untouched"), Result.RemainingShield, 100.0f);
    TestTrue(TEXT("Snapshot critical result persists"), Result.bCritical);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLethalDamageTest,
    "RiorsEdge.Combat.Damage.LethalResult",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLethalDamageTest::RunTest(const FString& Parameters)
{
    FBreakerDamageRequest Request;
    Request.BaseDamage = 150.0f;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Request.bCanCritical = false;
    Request.bBypassShield = true;
    FBreakerDefenseState Defense;
    Defense.Health = 80.0f;
    Defense.Shield = 50.0f;

    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestEqual(TEXT("Health damage reports actual health removed"), Result.HealthDamage, 80.0f);
    TestEqual(TEXT("Health clamps at zero"), Result.RemainingHealth, 0.0f);
    TestTrue(TEXT("Lethal result is reported"), Result.bKilled);
    return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDotInstigatorTest,
    "RiorsEdge.Combat.Damage.DotTickCarriesInstigator",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDotInstigatorTest::RunTest(const FString& Parameters)
{
    FBreakerStatusApplicationSpec Status;
    Status.BaseDamagePerTick = 10.0f;

    AActor* Applier = NewObject<AActor>(GetTransientPackage());
    const FBreakerDamageRequest Credited = UBreakerDamageLibrary::MakeSnapshotDotTick(Status, EBreakerDamageFamily::Physical, 1, Applier, FVector::ZeroVector, false);
    TestTrue(TEXT("Tick carries its applier"), Credited.Instigator.Get() == Applier);

    const FBreakerDamageRequest Uncredited = UBreakerDamageLibrary::MakeSnapshotDotTick(Status, EBreakerDamageFamily::Physical, 1, nullptr, FVector::ZeroVector, false);
    TestFalse(TEXT("An applierless tick credits nobody"), Uncredited.Instigator.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDotSourceLocationTest,
    "RiorsEdge.Combat.Damage.DotTickCarriesSourceLocation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDotSourceLocationTest::RunTest(const FString& Parameters)
{
    // The facing-armour half of the DoT snapshot rule: a tick built with the
    // application-time source position must carry it, or ReceiveDamage skips
    // the facing multiplier and a Bleed applied to a Warden's exposed BACK
    // quietly re-acquires the frontal mitigation on every tick.
    FBreakerStatusApplicationSpec Status;
    Status.BaseDamagePerTick = 10.0f;

    const FVector ApplicationPoint(-500.0f, 120.0f, 30.0f);
    const FBreakerDamageRequest Located = UBreakerDamageLibrary::MakeSnapshotDotTick(
        Status, EBreakerDamageFamily::Physical, 1, nullptr, ApplicationPoint, true);
    TestTrue(TEXT("The tick says it has a source location"), Located.bHasSourceLocation);
    TestTrue(TEXT("The tick carries the application-time position"),
        Located.SourceLocation.Equals(ApplicationPoint, 0.001f));

    // A status applied by nothing positioned (a hazard, a test) stays honest:
    // no location, so the facing step stays skipped rather than judging every
    // tick from the world origin.
    const FBreakerDamageRequest Unlocated = UBreakerDamageLibrary::MakeSnapshotDotTick(
        Status, EBreakerDamageFamily::Physical, 1, nullptr, FVector::ZeroVector, false);
    TestFalse(TEXT("A positionless application sets no source location"), Unlocated.bHasSourceLocation);

    // End to end through the resolver: the facing multiplier a tick's carried
    // location produces must be the same one a direct hit from that angle
    // gets — rear multiplier 0 against a target facing +X, source behind it.
    const float RearMultiplier = UBreakerDamageLibrary::GetFacingArmorMultiplier(
        FVector::ForwardVector, FVector::ZeroVector, ApplicationPoint, 0.0f, 0.15f);
    TestEqual(TEXT("The carried position lands in the rear arc"), RearMultiplier, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOverkillReportTest,
    "RiorsEdge.Combat.Damage.OverkillReported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOverkillReportTest::RunTest(const FString& Parameters)
{
    // The owner reads damage numbers for TTK/balance, so a killing blow must
    // REPORT its full size while still APPLYING only what the target had. The
    // 900-damage-rocket-on-30-HP case, verbatim.
    FBreakerDamageRequest Request;
    Request.BaseDamage = 900.0f;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Request.bCanCritical = false;
    FBreakerDefenseState Defense;
    Defense.Health = 30.0f;

    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestEqual(TEXT("Applied health damage stays clamped to remaining health"), Result.HealthDamage, 30.0f);
    TestEqual(TEXT("Health still clamps at zero"), Result.RemainingHealth, 0.0f);
    TestTrue(TEXT("The blow kills"), Result.bKilled);
    TestEqual(TEXT("The clamp's discard is reported as overkill"), Result.OverkillDamage, 870.0f);
    TestEqual(TEXT("Applied plus overkill reconstructs the true blow"),
        Result.HealthDamage + Result.OverkillDamage, 900.0f);

    // A survivable hit reports zero overkill — the field only ever carries
    // what the clamp discarded, never a duplicate of applied damage.
    FBreakerDefenseState Healthy;
    Healthy.Health = 5000.0f;
    Healthy.Shield = 100.0f;
    const FBreakerDamageResult Survived = UBreakerDamageLibrary::ResolveDamage(Request, Healthy);
    TestEqual(TEXT("A survivable hit has no overkill"), Survived.OverkillDamage, 0.0f);

    // Shield overflow composes: 900 into 100 shield (not bypassed) + 30 health
    // still reports the remainder past death as overkill.
    FBreakerDamageRequest Shielded = Request;
    Shielded.bBypassShield = false;
    FBreakerDefenseState Fragile;
    Fragile.Health = 30.0f;
    Fragile.Shield = 100.0f;
    const FBreakerDamageResult Broken = UBreakerDamageLibrary::ResolveDamage(Shielded, Fragile);
    TestEqual(TEXT("Shield takes its share first"), Broken.ShieldDamage, 100.0f);
    TestEqual(TEXT("Health takes what it had"), Broken.HealthDamage, 30.0f);
    TestEqual(TEXT("The rest is overkill"), Broken.OverkillDamage, 770.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHitContextTest,
    "RiorsEdge.Combat.Damage.HitContextPopulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHitContextTest::RunTest(const FString& Parameters)
{
    // World-free half of the SI-8 plumbing: the request carries the dealer and
    // the resolved result supplies everything else the context reports.
    AActor* Dealer = NewObject<AActor>(GetTransientPackage());
    AActor* Target = NewObject<AActor>(GetTransientPackage());

    FBreakerDamageRequest Request;
    Request.BaseDamage = 200.0f;
    Request.bCanCritical = false;
    Request.bWeakPointHit = true;
    Request.WeakPointMultiplier = 2.0f;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Request.SetInstigator(Dealer);

    FBreakerDefenseState Defense;
    Defense.Health = 100.0f;
    const FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);

    FBreakerHitContext Context;
    Context.Instigator = Request.Instigator.Get();
    Context.Target = Target;
    Context.Result = Result;
    Context.bFromDoT = Request.bIsDamageOverTime;
    Context.bWeakPoint = Result.bWeakPoint;
    Context.DamageFamily = Request.DamageFamily;

    TestTrue(TEXT("Context names the dealer"), Context.Instigator == Dealer);
    TestTrue(TEXT("Context names the target"), Context.Target == Target);
    TestTrue(TEXT("Weak point survives into the context"), Context.bWeakPoint);
    TestFalse(TEXT("A direct hit is not a DoT"), Context.bFromDoT);
    TestTrue(TEXT("Damage family survives into the context"), Context.DamageFamily == EBreakerDamageFamily::TrueDamage);
    TestTrue(TEXT("A lethal hit reports the kill"), Context.Result.bKilled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOutgoingModifierTest,
    "RiorsEdge.Combat.Damage.OutgoingModifierChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOutgoingModifierTest::RunTest(const FString& Parameters)
{
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(GetTransientPackage());

    FBreakerDamageRequest Request;
    Request.BaseDamage = 100.0f;
    Combat->ApplyOutgoingModifiers(Request);
    TestEqual(TEXT("An empty chain changes nothing"), Request.BaseDamage, 100.0f);
    TestEqual(TEXT("An empty chain leaves the multiplier alone"), Request.SourceDamageMultiplier, 1.0f);

    Combat->PushOutgoingModifier(TEXT("FlatA"), 15.0f, 1.0f, 0.0f);
    Combat->PushOutgoingModifier(TEXT("FlatB"), 5.0f, 1.0f, 0.0f);
    Combat->PushOutgoingModifier(TEXT("MoreA"), 0.0f, 1.30f, 0.0f);
    Combat->PushOutgoingModifier(TEXT("MoreB"), 0.0f, 1.20f, 0.0f);

    FBreakerDamageRequest Modified;
    Modified.BaseDamage = 100.0f;
    Combat->ApplyOutgoingModifiers(Modified);
    TestEqual(TEXT("Flat bonuses sum into base damage"), Modified.BaseDamage, 120.0f);
    TestEqual(TEXT("More multipliers compose as a product"), Modified.SourceDamageMultiplier, 1.56f, 0.0001f);

    // Re-pushing a key replaces it rather than stacking with itself.
    Combat->PushOutgoingModifier(TEXT("FlatA"), 15.0f, 1.0f, 0.0f);
    TestEqual(TEXT("Re-push does not double the More product"), Combat->GetComposedMoreMultiplier(), 1.56f, 0.0001f);

    Combat->RemoveOutgoingModifier(TEXT("MoreB"));
    TestEqual(TEXT("Removal drops that factor"), Combat->GetComposedMoreMultiplier(), 1.30f, 0.0001f);

    // O34: the composed product is clamped at THE one aggregator-derived
    // ceiling, loudly. With no attribute set bound the attribute side is 1.0,
    // so the chain may spend the whole budget — 1.30^3, not the deleted 2.20.
    AddExpectedError(TEXT("exceeds the"), EAutomationExpectedErrorFlags::Contains, 0);
    Combat->PushOutgoingModifier(TEXT("MoreC"), 0.0f, 1.30f, 0.0f);
    Combat->PushOutgoingModifier(TEXT("MoreD"), 0.0f, 1.30f, 0.0f);
    Combat->PushOutgoingModifier(TEXT("MoreE"), 0.0f, 1.30f, 0.0f);
    TestEqual(TEXT("Composed More product clamps at the ceiling"),
        Combat->GetComposedMoreMultiplier(), FBreakerAttributeAggregator::ComposedMoreCeiling(), 0.0001f);
    return true;
}
