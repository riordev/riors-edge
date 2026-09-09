#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/BreakerGameplayAbility.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// O266. Gear can now bid into the ability lanes, and the ONE thing that has to
// be true about that is the thing this file pins: gear and the tree land in the
// SAME additive bucket. Before the bridge, every stat that reached gameplay
// outside the aggregator composed gear against tree MULTIPLICATIVELY — +20% of
// each read x1.44 against a rule that says x1.40 — and the equipment
// component's own comments record that as a repeated bug class. A lane that
// silently reintroduced it would look correct in isolation and be wrong in play.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityLaneAdditiveTest,
    "RiorsEdge.Attributes.AbilityLanes.Additive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityLaneAdditiveTest::RunTest(const FString& Parameters)
{
    // Two contributors, twenty percent each, exactly as a player would have
    // one rolled line and one purchased node.
    for (const EBreakerAggregatedAttribute Lane : {
        EBreakerAggregatedAttribute::AbilityCastRateMultiplier,
        EBreakerAggregatedAttribute::AbilityAreaMultiplier,
        EBreakerAggregatedAttribute::AbilityCooldownReduction })
    {
        FBreakerAttributeAggregator Aggregator;
        // Every ability lane is a MULTIPLIER whose neutral base is 1.0, so the
        // fold has something to scale. A zero base composes to zero however
        // much either layer bids, which is what this fixture caught first.
        Aggregator.SetBase(Lane, 1.0f);
        FBreakerAttributeContribution Gear;
        Gear.AddIncreasedPercent(Lane, 20.0f);
        FBreakerAttributeContribution Tree;
        Tree.AddIncreasedPercent(Lane, 20.0f);
        Aggregator.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
        Aggregator.SetContribution(EBreakerAttributeContributor::Progression, Tree);

        const float Composed = Aggregator.Compose(Lane);
        TestEqual(TEXT("gear and tree add rather than multiply"), Composed, 1.40f, 0.0001f);
        TestTrue(TEXT("and never reach the multiplicative answer"), Composed < 1.44f - 0.0001f);
    }
    return true;
}

// The lanes carry an author, which is what separates a bridge from plumbing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityLaneAuthoredTest,
    "RiorsEdge.Items.AbilityLanes.Authored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityLaneAuthoredTest::RunTest(const FString& Parameters)
{
    const TArray<FBreakerAffixDefinition>& Affixes = UBreakerAffixLibrary::GetSliceAffixPool();
    for (const EBreakerStatTarget Target : {
        EBreakerStatTarget::AbilityCastRate,
        EBreakerStatTarget::AbilityArea,
        EBreakerStatTarget::AbilityCooldownReduction })
    {
        const FBreakerAffixDefinition* Line = Affixes.FindByPredicate(
            [Target](const FBreakerAffixDefinition& Affix) { return Affix.StatTarget == Target; });
        if (!TestNotNull(TEXT("an authored gear line bids into the lane"), Line)) continue;
        // Increased percentages, because that is the bucket the aggregator adds
        // in; a Flat or More line here would not share the tree's bucket at all.
        TestEqual(TEXT("and bids as an Increased percentage"), Line->StatBucket, EBreakerStatBucket::IncreasedPercent);
        TestTrue(TEXT("with a tier ladder that climbs"), Line->ValueAtT1 > Line->ValueAtT12);
        TestTrue(TEXT("and at least one slot it can roll on"), Line->AllowedSlots.Num() > 0);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
