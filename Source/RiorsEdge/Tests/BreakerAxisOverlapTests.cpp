#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Progression/BreakerBuildConditions.h"

// ---------------------------------------------------------------------------
// A DOCTRINE MAY NOT AUTHOR AN UNCONDITIONAL MAGNITUDE ON A GENERIC DAMAGE POOL.
//
// The ruling, in the owner's words: generic Increased Damage is Core's, and a
// doctrine spending any of eight points on it has spent its identity on what
// Core does with 222. Where a doctrine's rule carries a magnitude, that
// magnitude is conditional on the doctrine's own axis, or it sits on a stat
// target no Core wheel authors.
//
// WHY THE PREDICATE IS SHAPED LIKE THIS. The obvious way to write this test is
// to map each doctrine to a Core axis and forbid overlap. That map does not
// exist in code — the closest thing is Core's Constellation field, and Phase 4
// deletes all seven constellations for a twelve-wheel atlas, so a test written
// against them would be measuring a configuration on its way out. The three
// generic pools are not going anywhere: Damage is O54's shared pool, and
// WeaponDamage and AbilityDamage are its two delivery lanes. A rule stated over
// those survives the atlas without an edit.
//
// It is deliberately NARROW. It does not catch a doctrine authoring generic
// CriticalChance, and it cannot see whether a condition really is that
// doctrine's own axis rather than a fig leaf borrowed from another. Both of
// those need reading. What it catches is the form that has actually gone wrong
// and been measured going wrong: Swift.Marksman.Culling's unconditional +18%
// WeaponDamage moved the at-cap band 6.53x -> 6.79x and parity 0.647 -> 0.622
// on one edit, which is a doctrine node moving two Core-level metrics.
//
// EXPECTED RED, ENUMERATED. The violations it finds are content to be
// rewritten, not a bug in the predicate.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAxisOverlapTest,
    "RiorsEdge.Progression.AxisOverlap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAxisOverlapTest::RunTest(const FString& Parameters)
{
    auto IsGenericDamagePool = [](EBreakerNodeStatTarget Target)
    {
        return Target == EBreakerNodeStatTarget::Damage
            || Target == EBreakerNodeStatTarget::WeaponDamage
            || Target == EBreakerNodeStatTarget::AbilityDamage;
    };

    int32 DoctrineNodes = 0;
    int32 ConditionalLines = 0;
    TArray<FString> Violations;

    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree || Tree->Currency != EBreakerPointCurrency::DoctrinePoints) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            ++DoctrineNodes;
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                if (!IsGenericDamagePool(Effect.StatTarget)) continue;

                // Always is the only unconditional condition. A line naming any
                // other condition has claimed an axis, and whether the axis it
                // claimed is really its own is a reading question this test
                // does not pretend to answer.
                if (Effect.Condition != EBreakerBuildCondition::Always)
                {
                    ++ConditionalLines;
                    continue;
                }
                // AlsoRequires cannot rescue an Always primary: SatisfiesAll
                // treats Always as true by definition, so a line with Always
                // plus extra requirements is still gated — but a line whose
                // PRIMARY is Always and which requires nothing else pays in
                // every state there is.
                if (Effect.AlsoRequires.Num() > 0)
                {
                    ++ConditionalLines;
                    continue;
                }

                const UEnum* TargetEnum = StaticEnum<EBreakerNodeStatTarget>();
                Violations.Add(FString::Printf(TEXT("%s authors unconditional %s %+g"),
                    *Node->NodeId.ToString(),
                    TargetEnum ? *TargetEnum->GetNameStringByValue(static_cast<int64>(Effect.StatTarget)) : TEXT("?"),
                    Effect.ValuePerRank));
            }
        }
    }

    // Guard the guard: an empty walk would pass vacuously, and a renamed
    // currency is exactly how that happens.
    TestTrue(TEXT("The doctrine layer has nodes to check at all"), DoctrineNodes > 100);
    // And the predicate must be capable of finding a legal line, or "no
    // violations" would only mean the walk found nothing at all.
    TestTrue(TEXT("Conditional damage lines exist, so the predicate is discriminating"),
        ConditionalLines > 0);

    TestEqual(*FString::Printf(
        TEXT("No doctrine node authors an unconditional magnitude on a generic damage pool (%d found): %s"),
        Violations.Num(),
        Violations.Num() ? *FString::Join(Violations, TEXT("; ")) : TEXT("none")),
        Violations.Num(), 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
