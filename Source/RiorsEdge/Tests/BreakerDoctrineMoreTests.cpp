#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Progression/BreakerBuildConditions.h"
#include "Attributes/BreakerAttributeAggregation.h"

// ---------------------------------------------------------------------------
// O95: A DOCTRINE AUTHORS NO MORE MULTIPLIER, AND ITS KEYSTONES STILL PAY.
//
// Two halves, and the second is the one that matters. Deleting a multiplier is
// trivial and leaves a keystone that costs three points and does nothing --
// exactly the silent content this project has shipped four times. So the
// prohibition is asserted over EVERY doctrine node, and each of the four
// keystones that lost a More is asserted to move a named lane instead.
//
// Four, not three. Three went through the AddDamageMore helper;
// Caster.VoidWhisperer.LongDark targeted the DamageOverTime pool directly and
// was invisible to a search for the helper. A test that enumerates trees rather
// than call sites cannot make that mistake again, which is why half one walks
// the content.
//
// World-free on purpose: AggregateStats is a static over nodes, ranks and a
// condition state, so the rule is proved with no actor. That the WIRING carries
// a doctrine rank to this aggregator at all is a different assertion and lives
// in RiorsEdge.Progression.PointPools.EveryPoolIsRouted, which is where it
// belongs -- that seam has been wrong twice.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDoctrineKeystonesPayTest,
    "RiorsEdge.Progression.Doctrine.KeystonesPayWithoutMores",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDoctrineKeystonesPayTest::RunTest(const FString& Parameters)
{
    TArray<const UBreakerProgressionNode*> Nodes;
    int32 DoctrineNodes = 0;
    TArray<FString> Offenders;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree) continue;
        const bool bDoctrine = Tree->Currency == EBreakerPointCurrency::DoctrinePoints;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            Nodes.Add(Node);
            if (!bDoctrine) continue;
            ++DoctrineNodes;

            // ---- HALF ONE: not one More anywhere in a doctrine ------------
            // Every doctrine node, not the four that changed, because the rule
            // is about the LAYER. A sixteenth doctrine authored next year with
            // a keystone multiplier fails here on the commit that adds it.
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                if (Effect.StatBucket == EBreakerNodeStatBucket::MorePercent)
                {
                    Offenders.Add(FString::Printf(TEXT("%s (%s)"),
                        *Node->NodeId.ToString(), *Tree->TreeId.ToString()));
                }
            }
        }
    }
    // Guard the guard: an empty walk would pass the offender check vacuously,
    // and a renamed currency or an empty fallback set is exactly how that
    // happens. Fifteen doctrines carry well over a hundred nodes.
    TestTrue(TEXT("The doctrine layer has nodes to check at all"), DoctrineNodes > 100);
    TestEqual(*FString::Printf(TEXT("No doctrine node authors a More (O95): %s"),
        Offenders.Num() ? *FString::Join(Offenders, TEXT(", ")) : TEXT("none")),
        Offenders.Num(), 0);

    // ---- HALF TWO: each replacement moves a named lane ---------------------
    // One rank at a time, so a lane that moved because some OTHER node happens
    // to author it cannot be mistaken for this keystone paying.
    auto Compose = [&Nodes](const TCHAR* NodeId, const FBreakerBuildConditionState& Conditions,
                            FBreakerAttributeContribution& OutContribution)
    {
        TArray<FBreakerNodeRank> Ranks;
        Ranks.Add({FName(NodeId), 1});
        return UBreakerProgressionComponent::AggregateStats(Nodes, Ranks, &OutContribution, Conditions);
    };

    FBreakerBuildConditionState Sliding;
    Sliding.Set(EBreakerBuildCondition::Sliding, true);
    FBreakerBuildConditionState Redline;
    Redline.Set(EBreakerBuildCondition::Redline, true);
    FBreakerBuildConditionState Aiming;
    Aiming.Set(EBreakerBuildCondition::Aiming, true);
    const FBreakerBuildConditionState Idle;

    // Overpressure -- x1.20 while Sliding became "momentum stops decaying while
    // sliding". Conditional, so it must read UNCHANGED at rest and move under
    // the condition: a replacement that paid unconditionally would be a
    // different node wearing this one name.
    {
        FBreakerAttributeContribution Rest, Live;
        const FBreakerNodeStats AtRest = Compose(TEXT("Swift.Kinetic.Overpressure"), Idle, Rest);
        const FBreakerNodeStats WhileSliding = Compose(TEXT("Swift.Kinetic.Overpressure"), Sliding, Live);
        TestEqual(TEXT("Overpressure: decay is untouched when not sliding"),
            AtRest.ClassResourceDecayMultiplier, 1.0f, 0.0001f);
        TestEqual(TEXT("Overpressure: decay stops entirely while sliding"),
            WhileSliding.ClassResourceDecayMultiplier, 0.0f, 0.0001f);
        TestEqual(TEXT("Overpressure: and composes no More in either state"),
            WhileSliding.DamageMoreMultiplier, 1.0f, 0.0001f);
    }

    // Culling -- x1.18 unconditional became +18% WeaponDamage, and then took a
    // second ruling. The unconditional replacement was measured moving two
    // Core-level metrics on its own (at-cap 6.53x -> 6.79x, parity 0.647 ->
    // 0.622), which is a doctrine doing Core's job. A doctrine magnitude is
    // conditional on the doctrine's own axis, and Marksman's axis is aimed
    // fire, so the line pays down sights and nowhere else.
    {
        FBreakerAttributeContribution Rest, Live;
        const FBreakerNodeStats Hip = Compose(TEXT("Swift.Marksman.Culling"), Idle, Rest);
        const FBreakerNodeStats Ads = Compose(TEXT("Swift.Marksman.Culling"), Aiming, Live);
        // The half that makes this a ruling rather than a rename: firing from
        // the hip, the keystone's weapon line pays NOTHING.
        TestEqual(TEXT("Culling: the weapon lane is untouched from the hip"),
            Hip.DamageMultiplier, 1.0f, 0.0001f);
        TestEqual(TEXT("Culling: and moves down sights"),
            Ads.DamageMultiplier, 1.18f, 0.0001f);
        // The narrow lane, not the shared one. The ability lane must NOT move:
        // that is the difference between WeaponDamage and Damage, and it is
        // invisible in the weapon figure above.
        TestEqual(TEXT("Culling: the ability lane does not move with it"),
            Ads.AbilityDamageMultiplier, 1.0f, 0.0001f);
        TestEqual(TEXT("Culling: it reaches the weapon attribute as Increased"),
            Live.GetIncreasedPercent(EBreakerAggregatedAttribute::DamageMultiplier), 18.0f, 0.0001f);
        TestEqual(TEXT("Culling: and not as a More"),
            Live.GetMore(EBreakerAggregatedAttribute::DamageMultiplier), 1.0f, 0.0001f);
    }

    // Bloodrhythm -- x1.20 at Redline became +20% fire rate at Redline, which
    // changes what the WEAPON DOES rather than a number behind it. FireRate has
    // no node-stats field: it composes onto the attribute contribution only, so
    // this is the one of the four that a fixture reading FBreakerNodeStats
    // cannot see at all.
    {
        FBreakerAttributeContribution Rest, Live;
        Compose(TEXT("Swift.Frenzy.Bloodrhythm"), Idle, Rest);
        const FBreakerNodeStats AtRedline = Compose(TEXT("Swift.Frenzy.Bloodrhythm"), Redline, Live);
        TestEqual(TEXT("Bloodrhythm: fire rate is untouched off Redline"),
            Rest.GetIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier), 0.0f, 0.0001f);
        TestEqual(TEXT("Bloodrhythm: fire rate climbs at Redline"),
            Live.GetIncreasedPercent(EBreakerAggregatedAttribute::FireRateMultiplier), 20.0f, 0.0001f);
        TestEqual(TEXT("Bloodrhythm: and composes no More at Redline"),
            AtRedline.DamageMoreMultiplier, 1.0f, 0.0001f);
    }

    // Long Dark -- x1.30 on the DamageOverTime pool became +30% ability
    // duration. The largest of the four, and the doctrine own axis rather than
    // a substitute for a multiplier: Void Whisperer is zones that keep working
    // after you have looked away.
    {
        FBreakerAttributeContribution Contribution;
        const FBreakerNodeStats Stats = Compose(TEXT("Caster.VoidWhisperer.LongDark"), Idle, Contribution);
        TestEqual(TEXT("Long Dark: ability duration lengthens"),
            Stats.AbilityDurationMultiplier, 1.30f, 0.0001f);
        // The pool it used to multiply now reads clean, which is the half of
        // this that a deletion could fake -- so it is asserted beside the half
        // that a deletion could not.
        TestEqual(TEXT("Long Dark: and the damage-over-time More is gone"),
            Stats.DamageOverTimeMultiplier, 1.0f, 0.0001f);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
