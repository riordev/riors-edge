#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// O54/O55/O56: THE THREE DAMAGE POOLS
// ---------------------------------------------------------------------------
// Three additive pools, not one: Increased Weapon Damage, Increased Ability
// Damage, and a smaller shared Increased Damage that feeds both.
//
//   weapon-delivered  = (1 + (Weapon  + Shared) / 100) x MoreProduct
//   ability-delivered = (1 + (Ability + Shared) / 100) x MoreProduct
//
// The law is world-free, so these tests are too: no actor, no world, no ability
// system. That is the only reason the aggregation rule is provable at all, and
// it is why UBreakerDamageLibrary::ComposeSourcePools takes the three sums as
// arguments rather than reaching for an attribute set.
//
// The rule these tests exist to hold is the one a future edit is most likely to
// break by accident: a hit reads ONE lane. Folding the other lane in "so
// abilities feel better" is not a buff, it is the partition deleted, and it is
// indistinguishable from the pre-split code that made every ability build a
// weapon build with extra steps.
// ---------------------------------------------------------------------------

namespace BreakerDamagePoolTest
{
    // Distinctively named for the unity build, per the project's twice-bitten
    // rule about anonymous-namespace collisions.
    UBreakerAttributeSet* PoolTestMakeAttributes()
    {
        return NewObject<UBreakerAttributeSet>(GetTransientPackage());
    }
}

// (a) The composition law itself, both directions, with the shared pool in both
// and each specific pool in exactly one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDamagePoolCompositionTest,
    "RiorsEdge.Combat.Pools.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDamagePoolCompositionTest::RunTest(const FString& Parameters)
{
    // 40% weapon, 25% ability, 10% shared, no Mores.
    const float Weapon = UBreakerDamageLibrary::ComposeSourcePools(40.0f, 25.0f, 10.0f, 1.0f, EBreakerDamageDelivery::Weapon);
    const float Ability = UBreakerDamageLibrary::ComposeSourcePools(40.0f, 25.0f, 10.0f, 1.0f, EBreakerDamageDelivery::Ability);

    TestEqual(TEXT("A weapon hit composes weapon plus shared"), Weapon, 1.50f, 0.0001f);
    TestEqual(TEXT("An ability composes ability plus shared"), Ability, 1.35f, 0.0001f);

    // Neither lane reads the other's SPECIFIC pool. Moving one specific pool
    // must move exactly one lane, which is what "partition" means and what a
    // single number could not tell you.
    const float WeaponAfter = UBreakerDamageLibrary::ComposeSourcePools(90.0f, 25.0f, 10.0f, 1.0f, EBreakerDamageDelivery::Weapon);
    const float AbilityAfter = UBreakerDamageLibrary::ComposeSourcePools(90.0f, 25.0f, 10.0f, 1.0f, EBreakerDamageDelivery::Ability);
    TestEqual(TEXT("Raising the weapon pool moves the weapon lane"), WeaponAfter, 2.00f, 0.0001f);
    TestEqual(TEXT("Raising the weapon pool leaves the ability lane untouched"), AbilityAfter, Ability, 0.0001f);

    // The shared pool moves BOTH, by the same number of points, which is the
    // whole of what O56 means by "feeds both".
    const float WeaponShared = UBreakerDamageLibrary::ComposeSourcePools(40.0f, 25.0f, 30.0f, 1.0f, EBreakerDamageDelivery::Weapon);
    const float AbilityShared = UBreakerDamageLibrary::ComposeSourcePools(40.0f, 25.0f, 30.0f, 1.0f, EBreakerDamageDelivery::Ability);
    TestEqual(TEXT("A shared point is worth a weapon point in the weapon lane"), WeaponShared - Weapon, 0.20f, 0.0001f);
    TestEqual(TEXT("A shared point is worth an ability point in the ability lane"), AbilityShared - Ability, 0.20f, 0.0001f);

    // ADDITIVE, not multiplicative. 40 + 10 is 1.50, never 1.40 x 1.10 == 1.54.
    // This is the locked rule, and the shared pool is the contributor most
    // likely to be implemented as a second multiplier by someone reasoning
    // about it as "a bonus on top".
    TestNotEqual(TEXT("Shared joins the additive bucket rather than multiplying"), Weapon, 1.40f * 1.10f);

    // The More product multiplies the composed bucket and nothing else, and the
    // floors hold: an authored negative can shrink a hit to nothing and can
    // never invert it into healing.
    TestEqual(TEXT("More multiplies the composed bucket"),
        UBreakerDamageLibrary::ComposeSourcePools(40.0f, 25.0f, 10.0f, 1.30f, EBreakerDamageDelivery::Weapon), 1.50f * 1.30f, 0.0001f);
    TestEqual(TEXT("A bucket driven below zero floors at zero rather than inverting"),
        UBreakerDamageLibrary::ComposeSourcePools(-500.0f, 0.0f, 0.0f, 1.0f, EBreakerDamageDelivery::Weapon), 0.0f, 0.0001f);
    TestEqual(TEXT("A negative More floors at zero too"),
        UBreakerDamageLibrary::ComposeSourcePools(40.0f, 0.0f, 0.0f, -2.0f, EBreakerDamageDelivery::Weapon), 0.0f, 0.0001f);

    return true;
}

// (b) A source authors at most ONE specific pool, plus optionally the shared
// one. Two specific pools from one source is one bucket double-dipped, and the
// stat-target mapping is where that is checkable rather than left to review.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDamagePoolOneSpecificPerSourceTest,
    "RiorsEdge.Combat.Pools.OneSpecificPoolPerSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDamagePoolOneSpecificPerSourceTest::RunTest(const FString& Parameters)
{
    // Every stat target names at most one pool, so a single node effect cannot
    // reach two specific buckets however it is authored. The enum IS the
    // enforcement here: an effect carries exactly one StatTarget.
    int32 WeaponTargets = 0;
    int32 AbilityTargets = 0;
    int32 SharedTargets = 0;
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerNodeStatTarget::Count); ++Index)
    {
        switch (BreakerDamagePoolFor(static_cast<EBreakerNodeStatTarget>(Index)))
        {
        case EBreakerDamagePool::Weapon:  ++WeaponTargets;  break;
        case EBreakerDamagePool::Ability: ++AbilityTargets; break;
        case EBreakerDamagePool::Shared:  ++SharedTargets;  break;
        default: break;
        }
    }
    TestEqual(TEXT("Exactly one stat target names the weapon pool"), WeaponTargets, 1);
    TestEqual(TEXT("Exactly one stat target names the ability pool"), AbilityTargets, 1);
    TestEqual(TEXT("Exactly one stat target names the shared pool"), SharedTargets, 1);

    // And the contribution layer: a shared bid lands in both lanes with the
    // SAME percentage, in each lane's single additive bucket. If this ever
    // became "half to each" the shared pool would silently stop being a legal
    // contributor to two buckets and start being a split one.
    FBreakerAttributeContribution Offer;
    Offer.AddSharedIncreasedDamage(12.0f);
    TestEqual(TEXT("A shared bid reaches the weapon lane in full"),
        Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), 12.0f, 0.0001f);
    TestEqual(TEXT("A shared bid reaches the ability lane in full"),
        Offer.GetIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier), 12.0f, 0.0001f);

    // A specific bid reaches one lane and leaves the other alone.
    FBreakerAttributeContribution Specific;
    Specific.AddIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier, 30.0f);
    TestEqual(TEXT("An ability bid does not reach the weapon lane"),
        Specific.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), 0.0f, 0.0001f);

    return true;
}

// (c) O74: ONE More ceiling, spanning the pools. A per-lane ceiling would be the
// second budget the single ceiling exists to delete, so the clamp has to apply
// to the ability lane by exactly the same number as the weapon lane.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDamagePoolOneMoreCeilingTest,
    "RiorsEdge.Combat.Pools.OneMoreCeiling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDamagePoolOneMoreCeilingTest::RunTest(const FString& Parameters)
{
    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();

    TestTrue(TEXT("The weapon lane is More-capped"),
        FBreakerAttributeAggregator::IsMoreCappedAttribute(EBreakerAggregatedAttribute::DamageMultiplier));
    TestTrue(TEXT("The ability lane is More-capped by the same rule"),
        FBreakerAttributeAggregator::IsMoreCappedAttribute(EBreakerAggregatedAttribute::AbilityDamageMultiplier));

    // Four ceiling-height Mores in each lane, submitted from both contributors
    // so the clamp is exercised ACROSS layers rather than inside one. Neither
    // lane may compose past the single ceiling.
    FBreakerAttributeAggregator Aggregator;
    float Bases[FBreakerAttributeAggregator::AttributeCount] = {};
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::DamageMultiplier)] = 1.0f;
    Bases[static_cast<int32>(EBreakerAggregatedAttribute::AbilityDamageMultiplier)] = 1.0f;
    Aggregator.CaptureBases(Bases);

    FBreakerAttributeContribution Trees;
    FBreakerAttributeContribution Items;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Trees.ComposeSharedMoreDamage(FBreakerAttributeAggregator::SingleMoreCeiling);
        Items.ComposeSharedMoreDamage(FBreakerAttributeAggregator::SingleMoreCeiling);
    }
    Aggregator.SetContribution(EBreakerAttributeContributor::Progression, Trees);
    Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, Items);

    TestEqual(TEXT("The weapon lane clamps at the one ceiling"),
        Aggregator.ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier), Ceiling, 0.0001f);
    TestEqual(TEXT("The ability lane clamps at the SAME ceiling, not one of its own"),
        Aggregator.ComposedMoreProduct(EBreakerAggregatedAttribute::AbilityDamageMultiplier), Ceiling, 0.0001f);

    return true;
}

// (d) The seam every submission site uses. A request filled from a live
// attribute set carries a split that recomposes EXACTLY to the composed value,
// in whichever lane it named — the property the target-rider recomposition
// depends on, and the one that silently degrades if a lane is added and the
// filler is not taught about it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDamagePoolFillSourcePoolsTest,
    "RiorsEdge.Combat.Pools.FillSourcePools",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDamagePoolFillSourcePoolsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerDamagePoolTest;

    UBreakerAttributeSet* Attributes = PoolTestMakeAttributes();
    FBreakerAttributeContribution Offer;
    Offer.AddIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier, 60.0f);
    Offer.AddIncreasedPercent(EBreakerAggregatedAttribute::AbilityDamageMultiplier, 20.0f);
    Offer.AddSharedIncreasedDamage(10.0f);
    Offer.ComposeMore(EBreakerAggregatedAttribute::DamageMultiplier, 1.30f);
    Attributes->ApplyAttributeContribution(EBreakerAttributeContributor::Progression, Offer);

    FBreakerDamageRequest WeaponHit;
    UBreakerDamageLibrary::FillSourcePools(Attributes, EBreakerDamageDelivery::Weapon, WeaponHit);
    FBreakerDamageRequest AbilityHit;
    UBreakerDamageLibrary::FillSourcePools(Attributes, EBreakerDamageDelivery::Ability, AbilityHit);

    TestEqual(TEXT("The weapon request reads the weapon lane"),
        WeaponHit.SourceDamageMultiplier, Attributes->GetDamageMultiplier(), 0.0001f);
    TestEqual(TEXT("The ability request reads the ability lane"),
        AbilityHit.SourceDamageMultiplier, Attributes->GetAbilityDamageMultiplier(), 0.0001f);
    TestTrue(TEXT("The two lanes are genuinely different numbers"),
        !FMath::IsNearlyEqual(WeaponHit.SourceDamageMultiplier, AbilityHit.SourceDamageMultiplier, 0.001f));

    // Both requests carry the split, and both recompose exactly. Ability
    // submissions never carried one before this pass, which is why they could
    // not take target-side riders at all.
    for (const FBreakerDamageRequest& Request : {WeaponHit, AbilityHit})
    {
        TestTrue(TEXT("The request carries the source split"), Request.bHasSourceSplit);
        TestEqual(TEXT("The split recomposes to the composed multiplier"),
            (1.0f + Request.SourceIncreasedPercent / 100.0f) * Request.SourceMoreProduct,
            Request.SourceDamageMultiplier, 0.001f);
    }
    TestEqual(TEXT("The weapon request records its delivery"),
        static_cast<int32>(WeaponHit.Delivery), static_cast<int32>(EBreakerDamageDelivery::Weapon));
    TestEqual(TEXT("The ability request records its delivery"),
        static_cast<int32>(AbilityHit.Delivery), static_cast<int32>(EBreakerDamageDelivery::Ability));

    // A source with no attribute set at all — a hazard, an enemy, a bare rig —
    // gets the identity and no split, rather than a stale one from the struct.
    FBreakerDamageRequest Hazard;
    UBreakerDamageLibrary::FillSourcePools(nullptr, EBreakerDamageDelivery::Weapon, Hazard);
    TestEqual(TEXT("No attribute set composes to the identity"), Hazard.SourceDamageMultiplier, 1.0f, 0.0001f);
    TestFalse(TEXT("No attribute set carries no split"), Hazard.bHasSourceSplit);

    return true;
}

#endif
