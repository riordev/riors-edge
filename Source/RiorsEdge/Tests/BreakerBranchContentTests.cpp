#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// Coverage for the two content gaps O27 named: Swift's FRENZY branch, which the
// design document has always specified and the library never authored, and the
// ELEMENTS constellation, whose empty roster is why the Core board drew it as a
// sealed placeholder.
//
// EXTENDED for Swift's TIER-4 rewrite tier (Class-Kits §1.3-1.5, F9-F11 /
// K9-K11 / M9-M11), which the slice dropped entirely. The Frenzy test's shape
// pins were re-set to include it rather than relaxed, and the More-ceiling test
// gained a block proving the nine new nodes add no More multiplier.
//
// The bar these tests hold is the one the project learned the hard way: a node
// must be PURCHASABLE, its effect must actually LAND somewhere gameplay reads,
// and a respec must give back exactly what it took. A node that only publishes
// a tag nothing consumes is the failure mode, not the baseline.
//
// Helper names are prefixed because the module builds in unity mode and a bare
// MakeOwner already exists in BreakerAttributeAggregationTests.cpp.
namespace BreakerBranchContentTestHelpers
{
    // A freshly constructed actor is ROLE_Authority, which is all the component
    // needs to take its server paths. It is deliberately NOT an ability-system
    // owner: the loose-tag publication path is exercised separately.
    AActor* BranchContentMakeOwner()
    {
        return NewObject<AActor>();
    }

    // Buys a node to its maximum rank, failing the test on the first refusal.
    bool BranchContentBuyToMax(FAutomationTestBase& Test, UBreakerProgressionComponent* Progression,
        UBreakerProgressionTree* Tree, FName NodeId)
    {
        const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
        if (!Test.TestNotNull(*(NodeId.ToString() + TEXT(" exists in its tree")), Node)) return false;

        bool bAllBought = true;
        for (int32 Rank = 0; Rank < Node->MaxRank; ++Rank)
        {
            FText Failure;
            const bool bBought = Progression->PurchaseNode(Tree, NodeId, Failure);
            Test.TestTrue(*FString::Printf(TEXT("%s rank %d purchases (%s)"), *NodeId.ToString(), Rank + 1, *Failure.ToString()), bBought);
            bAllBought &= bBought;
        }
        return bAllBought;
    }
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFrenzyBranchTest,
    "RiorsEdge.Progression.FrenzyBranch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFrenzyBranchTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchContentTestHelpers;

    UBreakerProgressionTree* Frenzy = UBreakerProgressionLibrary::GetSwiftFrenzyTree();
    if (!TestNotNull(TEXT("Frenzy tree exists"), Frenzy)) return false;
    TestEqual(TEXT("Frenzy is a Swift class-point branch"), Frenzy->Currency, EBreakerPointCurrency::DoctrinePoints);
    TestEqual(TEXT("Frenzy belongs to Swift"), Frenzy->RequiredClass, EBreakerClassId::Swift);

    // FRENZY'S SHAPE, RE-PINNED DELIBERATELY (was {3, 4, 3} across tiers 1-3,
    // with tier 4 asserted not to exist).
    //
    // The old counts pinned a branch that stopped at tier 3 because the slice
    // dropped every Tier-4 rewrite node in Class-Kits §1.3 — F9 Second Wind,
    // F10 Redline Trigger, F11 No Safety. All three are now authored.
    //
    // RE-SET AGAIN when the keystone left tier 3: the shape is {3, 4, 2, 4} —
    // three entry nodes, four loop nodes, two ability-tier nodes, and four
    // tier-4 nodes of which three are rewrites and one is Bloodrhythm. The
    // keystone moved because its 8-point cornerstone gate plus its 3-point cost
    // needed 11 against a doctrine wallet of 8, so no character could buy it.
    // The SUM across tiers 3 and 4 is unchanged at six: one node was repriced,
    // none was added or lost, and that is the part worth reading.
    int32 TierCounts[5] = {};
    for (const UBreakerProgressionNode* Node : Frenzy->Nodes)
    {
        if (!TestTrue(TEXT("Frenzy node is tier 1-4"), Node->Tier >= 1 && Node->Tier <= 4)) continue;
        ++TierCounts[Node->Tier];
    }
    TestEqual(TEXT("Frenzy has three tier-1 entry nodes"), TierCounts[1], 3);
    TestEqual(TEXT("Frenzy has four tier-2 loop nodes"), TierCounts[2], 4);
    TestEqual(TEXT("Frenzy has two tier-3 nodes (the keystone moved to tier 4)"), TierCounts[3], 2);
    TestEqual(TEXT("Frenzy has four tier-4 nodes (F9-F11 plus Bloodrhythm)"), TierCounts[4], 4);

    // Cost and rank curve must match the two branches already shipped, or the
    // board teaches the player two different grammars. Tier 3 and tier 4 share
    // one rule here — single rank, two points — because §0.2 prices tier-4
    // rewrites at 2 and the keystone now costs the same, which is what makes
    // the 8-point wallet divide into four picks with nothing stranded.
    for (const UBreakerProgressionNode* Node : Frenzy->Nodes)
    {
        const FString Context = Node->NodeId.ToString();
        if (Node->Tier <= 2)
        {
            TestEqual(*(Context + TEXT(" entry/loop node costs 1")), Node->CostPerRank, 1);
            TestEqual(*(Context + TEXT(" entry/loop node has two ranks")), Node->MaxRank, 2);
        }
        else
        {
            TestEqual(*(Context + TEXT(" tier-3/4 node is single rank")), Node->MaxRank, 1);
            TestEqual(*(Context + TEXT(" tier-3/4 node costs 2")), Node->CostPerRank, 2);
        }
        // O27: no node may be purely decorative.
        TestTrue(*(Context + TEXT(" grants an effect or a tag")), Node->Effects.Num() > 0 || Node->GrantedTags.Num() > 0);
    }

    // The three rewrites are named individually, because a count alone would
    // pass if some future pass swapped one identity for another.
    for (const FName RewriteId : {FName(TEXT("Swift.Frenzy.SecondWind")),
        FName(TEXT("Swift.Frenzy.RedlineTrigger")), FName(TEXT("Swift.Frenzy.NoSafety"))})
    {
        const UBreakerProgressionNode* Rewrite = Frenzy->FindNode(RewriteId);
        if (!TestNotNull(*(RewriteId.ToString() + TEXT(" is authored")), Rewrite)) continue;
        TestEqual(*(RewriteId.ToString() + TEXT(" sits at tier 4")), Rewrite->Tier, 4);
        // Re-pinned 2026-08-16 (the loop valve): No Safety's two halves are
        // now real authored lines — ClassResourceDecay +100 and AbilityCost
        // +40, both Class-Kits §1.3 F11 transcriptions — while Second Wind and
        // Redline Trigger stay pure rule tags (their consumers, Cadence Break
        // and the Damage Ramp affix, still do not read them). The boundary
        // that still holds for all three: loop-economy lines only, never a
        // damage or combat stat.
        for (const FBreakerNodeEffect& Effect : Rewrite->Effects)
        {
            TestTrue(*(RewriteId.ToString() + TEXT(" authors only loop-economy lines")),
                Effect.StatTarget == EBreakerNodeStatTarget::ClassResourceDecay
                || Effect.StatTarget == EBreakerNodeStatTarget::AbilityCost);
        }
        if (RewriteId == FName(TEXT("Swift.Frenzy.NoSafety")))
        {
            TestEqual(*(RewriteId.ToString() + TEXT(" authors both F11 halves")), Rewrite->Effects.Num(), 2);
        }
        else
        {
            TestEqual(*(RewriteId.ToString() + TEXT(" is a rule tag, not a stat line")), Rewrite->Effects.Num(), 0);
        }
        TestTrue(*(RewriteId.ToString() + TEXT(" publishes its rule tag")), Rewrite->GrantedTags.Num() > 0);
        // Every prerequisite resolves inside this tree, so a rewrite can never
        // be stranded behind a node in a branch the player did not buy (O15
        // keeps ordinary nodes freely mixable, but a DANGLING prerequisite
        // would be unpurchasable outright).
        TestTrue(*(RewriteId.ToString() + TEXT(" builds on an earlier node")), Rewrite->Prerequisites.Num() > 0);
        for (const FBreakerNodePrerequisite& Prerequisite : Rewrite->Prerequisites)
        {
            const UBreakerProgressionNode* Required = Frenzy->FindNode(Prerequisite.NodeId);
            if (!TestNotNull(*(RewriteId.ToString() + TEXT(" prerequisite resolves inside Frenzy")), Required)) continue;
            TestTrue(*(RewriteId.ToString() + TEXT(" prerequisite sits at or below tier 4")), Required->Tier <= 4);
        }
    }

    // --- Every node is purchasable, and the effects land -------------------
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = BranchContentMakeOwner();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    // Isolate node content from the per-spent-point baseline; that floor has its
    // own coverage in BreakerAttributeAggregationTests.
    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Progression->BindAttributes(Attributes);
    Progression->ApplySliceDefaultsIfFresh();
    // A full Frenzy branch is 27 class points (was 21, before F9-F11 added
    // three single-rank nodes at 2 points each); the slice grant is 10.
    Progression->GrantPlaytestPoints(40, 0);

    const float BaseHealth = Attributes->GetMaxHealth();
    const float BaseCritChance = Attributes->GetCriticalChance();
    const float BaseCritMultiplier = Attributes->GetCriticalMultiplier();
    const float BaseDamage = Attributes->GetDamageMultiplier();

    // O37: Bloodrhythm is the branch cornerstone and refuses purchase without
    // commitment, so a full-branch walk commits first — which is exactly what
    // a real Frenzy character does.
    FText CommitFailure;
    TestTrue(*FString::Printf(TEXT("Committing to Frenzy succeeds (%s)"), *CommitFailure.ToString()),
        Progression->CommitToBranch(Frenzy->TreeId, CommitFailure));
    for (const UBreakerProgressionNode* Node : Frenzy->Nodes)
    {
        BranchContentBuyToMax(*this, Progression, Frenzy, Node->NodeId);
    }
    for (const UBreakerProgressionNode* Node : Frenzy->Nodes)
    {
        TestEqual(*(Node->NodeId.ToString() + TEXT(" reached its maximum rank")),
            Progression->GetNodeRank(Node->NodeId, EBreakerPointCurrency::DoctrinePoints), Node->MaxRank);
    }

    // Unconditional lines reach the attribute set. Feed is +45 health over two
    // ranks; Trigger Discipline and Rhythm are +3 crit chance each over two
    // ranks; Slipcut Mastery is +20 crit damage.
    TestEqual(TEXT("Feed's health reaches the attribute set"), Attributes->GetMaxHealth() - BaseHealth, 90.0f, 0.001f);
    TestEqual(TEXT("Frenzy's crit chance reaches the attribute set"), Attributes->GetCriticalChance() - BaseCritChance, 0.12f, 0.0001f);
    TestEqual(TEXT("Slipcut Mastery's crit damage reaches the attribute set"), Attributes->GetCriticalMultiplier() - BaseCritMultiplier, 0.20f, 0.0001f);
    // NOTHING. Ammunition Economy used to be Frenzy's one unconditional +5%
    // damage and is now Redline-gated like the rest, so a full Frenzy standing
    // still adds no damage at all — which is a stronger statement of the same
    // O27 point the old 0.05 was making around the edge of. A doctrine may not
    // author a magnitude on a generic damage pool without a condition on its
    // own axis (Progression.AxisOverlap), and Frenzy's axis is Redline.
    TestEqual(TEXT("Standing still, a full Frenzy adds no damage whatsoever"), Attributes->GetDamageMultiplier() - BaseDamage, 0.0f, 0.0001f);
    TestEqual(TEXT("Short Leash's move speed composes"), Progression->GetNodeStats().MoveSpeedMultiplier, 1.10f, 0.0001f);

    // Conditional damage is now the branch's WHOLE damage contribution —
    // Loaded 12 + Dry Fire 10 + Overrev 24 + Ammunition Economy 5 = 51% at
    // Redline, where it used to be 46 conditional plus 5 that paid anywhere.
    const FBreakerNodeStats& Stats = Progression->GetNodeStats();
    TestEqual(TEXT("Nothing conditional is live while standing still"), Stats.ActiveConditionalDamagePercent, 0.0f, 0.0001f);
    TestEqual(TEXT("Redline is worth 51% increased damage to a full Frenzy"), Stats.PotentialConditionalDamagePercent, 51.0f, 0.0001f);

    // THE TIER-4 PINS ABOVE ARE ABOUT SHAPE; THESE TWO ARE ABOUT POWER.
    //
    // Both damage equalities immediately above are UNCHANGED by F9-F11, and
    // that is the assertion, not an accident of the diff. The tier-4 nodes
    // moved the branch's DAMAGE output by exactly zero — No Safety's 2026-08-16
    // lines are loop-economy (decay and ability cost), which touch neither
    // DamageMultiplier nor the conditional-damage display. If a later pass
    // gives a tier-4 node a damage line, those two equalities fail first and
    // loudest — which is the correct place for that conversation to happen.
    //
    // The three rewrites publish their rules, so a consumer that learns to read
    // them finds them there.
    TestTrue(TEXT("Second Wind publishes its rule tag"), Stats.GrantedTags.HasTag(BreakerNodeTags::Node_SecondWind.GetTag()));
    TestTrue(TEXT("Redline Trigger publishes its rule tag"), Stats.GrantedTags.HasTag(BreakerNodeTags::Node_RedlineTrigger.GetTag()));
    TestTrue(TEXT("No Safety publishes its rule tag"), Stats.GrantedTags.HasTag(BreakerNodeTags::Node_NoSafety.GetTag()));
    // WHAT THIS DOES NOT COVER, STATED PLAINLY. Nothing above proves those
    // three rules DO anything: no Momentum component, no Damage Ramp affix and
    // no Cadence Break ability exists in this fixture (the suite constructs no
    // UWorld at all), so the tags are asserted to be PUBLISHED and nothing
    // more. The same limitation the Caster branch tests carry, for the same
    // reason. A tag with no consumer is inert by design here and is only a bug
    // once its consumer ships and does not read it.
    //
    // The keystone publishes the tag Overdrive's variant table already keys on,
    // so owning Bloodrhythm really does rewrite the ultimate.
    TestTrue(TEXT("Bloodrhythm publishes its keystone tag"),
        Stats.GrantedTags.HasTag(BreakerAbilityTags::Keystone_Swift_Bloodrhythm.GetTag()));

    // --- Respec restores exactly -------------------------------------------
    FText Failure;
    TestTrue(TEXT("Class respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
    TestEqual(TEXT("Respec restores health exactly"), Attributes->GetMaxHealth(), BaseHealth, 0.0001f);
    TestEqual(TEXT("Respec restores crit chance exactly"), Attributes->GetCriticalChance(), BaseCritChance, 0.0001f);
    TestEqual(TEXT("Respec restores crit damage exactly"), Attributes->GetCriticalMultiplier(), BaseCritMultiplier, 0.0001f);
    TestEqual(TEXT("Respec restores the damage multiplier exactly"), Attributes->GetDamageMultiplier(), BaseDamage, 0.0001f);
    TestEqual(TEXT("Respec restores move speed exactly"), Progression->GetNodeStats().MoveSpeedMultiplier, 1.0f, 0.0001f);
    TestFalse(TEXT("Respec drops the keystone tag"),
        Progression->GetNodeStats().GrantedTags.HasTag(BreakerAbilityTags::Keystone_Swift_Bloodrhythm.GetTag()));
    return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerElementsConstellationTest,
    "RiorsEdge.Progression.ElementsConstellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerElementsConstellationTest::RunTest(const FString& Parameters)
{
    using namespace BreakerBranchContentTestHelpers;

    UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();

    TArray<const UBreakerProgressionNode*> ElementNodes;
    for (const UBreakerProgressionNode* Node : Core->Nodes)
        if (Node->Constellation == FName(TEXT("Entropy"))) ElementNodes.Add(Node);
    TestEqual(TEXT("Entropy ships its entire major wedge"), ElementNodes.Num(), 11);
    for (const auto* Node : ElementNodes)
    {
        TestEqual(TEXT("Entropy spends Core points"), Node->Currency, EBreakerPointCurrency::CorePoints);
        TestEqual(TEXT("Entropy remains generic"), Node->RequiredClass, EBreakerClassId::None);
        TestTrue(TEXT("Authored effect or rule identity"), !Node->Effects.IsEmpty() || !Node->GrantedTags.IsEmpty());
        for (const auto& Effect : Node->Effects)
            if (Effect.StatBucket == EBreakerNodeStatBucket::MorePercent)
                TestEqual(TEXT("More belongs to convergence"), Node->CoreRole, EBreakerCoreNodeRole::Convergence);
    }
    auto* Attributes = NewObject<UBreakerAttributeSet>();
    auto* Progression = NewObject<UBreakerProgressionComponent>(BranchContentMakeOwner());
    Progression->IncreasedDamagePerSpentPoint = 0;
    Progression->BindAttributes(Attributes);
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(29, Progression->ExperienceCurve));
    const float BaseDoT = Attributes->GetDamageOverTimeMultiplier();
    const float BaseCritChance = Attributes->GetCriticalChance();
    const float BaseDamage = Attributes->GetDamageMultiplier();
    for (const TCHAR* Id : {TEXT("Core.Entropy.Attunement"), TEXT("Core.Entropy.Catalyst"), TEXT("Core.Entropy.Threshold"),
        TEXT("Core.Entropy.Decay"), TEXT("Core.Entropy.Density"), TEXT("Core.Entropy.Conductive"), TEXT("Core.Entropy.Sympathy"),
        TEXT("Core.Entropy.Penetrance"), TEXT("Core.Entropy.Sequence"), TEXT("Core.Entropy.Cascade"), TEXT("Core.Entropy.LongDark")})
        if (!BranchContentBuyToMax(*this, Progression, Core, Id)) return false;
    TestEqual(TEXT("Complete major costs twenty-six"), Progression->GetConstellationInvestment(Core, TEXT("Entropy")), 26);
    const auto& Stats = Progression->GetNodeStats();
    TestEqual(TEXT("Entropy does not change generic physical DoT"), Attributes->GetDamageOverTimeMultiplier(), BaseDoT, .0001f);
    TestEqual(TEXT("Entropy does not author crit chance"), Attributes->GetCriticalChance(), BaseCritChance, .0001f);
    TestEqual(TEXT("Elemental scope does not inflate generic weapon damage"), Attributes->GetDamageMultiplier(), BaseDamage, .0001f);
    TestEqual(TEXT("Ranked Rot damage authored"), Stats.RotDamageIncreasedPercent, 21.f, .0001f);
    TestEqual(TEXT("Ranked thresholds authored"), Stats.ElementalThresholdMultiplier, .85f, .0001f);
    TestEqual(TEXT("Sequence duration reaches lane"), Stats.StatusDurationMultiplier, 1.12f, .0001f);
    TestTrue(TEXT("Purchased Density reaches primitive"), Stats.bRotDensity);
    TestTrue(TEXT("Purchased Long Dark reaches primitive"), Stats.bLongDark);
    TestEqual(TEXT("Elemental convergence occupies one shared More slot"), Stats.DamageMoreSourceCount, 1);
    FText Failure;
    if (!TestTrue(TEXT("Core respec"), Progression->RespecCore(Failure))) return false;
    TestFalse(TEXT("Respec clears Long Dark"), Progression->GetNodeStats().bLongDark);
    TestEqual(TEXT("Respec clears Rot bonus"), Progression->GetNodeStats().RotDamageIncreasedPercent, 0.f);
    TestEqual(TEXT("Respec restores physical DoT"), Attributes->GetDamageOverTimeMultiplier(), BaseDoT, .0001f);
    TestEqual(TEXT("Respec restores critical chance"), Attributes->GetCriticalChance(), BaseCritChance, .0001f);
    TestEqual(TEXT("Respec restores generic damage"), Attributes->GetDamageMultiplier(), BaseDamage, .0001f);
    return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMoreCeilingWithNewContentTest,
    "RiorsEdge.Progression.MoreCeilingWithNewContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMoreCeilingWithNewContentTest::RunTest(const FString& Parameters)
{
    // Pure aggregation stress fixture: all authored Mores, not a legal allocation.
    // Actual budget/reachability is verified by CoreRoster.RingReachability.
    TArray<const UBreakerProgressionNode*> Nodes;
    for (const auto* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
        for (const UBreakerProgressionNode* Node : Tree->Nodes) Nodes.Add(Node);
    TArray<FBreakerNodeRank> Ranks;
    for (const auto* Node : Nodes)
        if (Node->Currency == EBreakerPointCurrency::CorePoints && Node->Effects.ContainsByPredicate(
            [](const FBreakerNodeEffect& Effect) { return Effect.StatBucket == EBreakerNodeStatBucket::MorePercent; }))
            Ranks.Add({Node->NodeId, 1});
    TestEqual(TEXT("Literal roster offers ten More convergences"), Ranks.Num(), 10);
    // O95: these three author no More any more. They stay in the fixture
    // deliberately -- owning every doctrine keystone must not change the
    // multiplier layer at all, and that is asserted below.
    Ranks.Add({TEXT("Swift.Kinetic.Overpressure"), 1});       // decay rule, no More
    Ranks.Add({TEXT("Swift.Marksman.Culling"), 1});           // weapon pool, no More
    Ranks.Add({TEXT("Swift.Frenzy.Bloodrhythm"), 1});         // fire rate, no More

    FBreakerAttributeContribution MoreOffer;
    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(
        Nodes, Ranks, &MoreOffer, FBreakerBuildConditionState::All());

    TestEqual(TEXT("All ten scoped More sources counted"), Stats.DamageMoreSourceCount, 10);
    TestTrue(TEXT("More choices exceed shared three slots"), Stats.DamageMoreSourceCount > UBreakerProgressionComponent::MaxDamageMoreSources);
    // Raw magnitude selects Overflow1.26, Collapse1.24 and Compound1.24.
    // Other scope-specific sources cannot become generic weapon multipliers.
    TestEqual(TEXT("Weapon composes selected Collapse only"), Stats.DamageMoreMultiplier, 1.24f, .0001f);
    TestEqual(TEXT("Ability composes selected Overflow only"), Stats.AbilityDamageMoreMultiplier, 1.26f, .0001f);
    TestEqual(TEXT("Physical DoT composes selected Compound only"), MoreOffer.GetMore(EBreakerAggregatedAttribute::DamageOverTimeMultiplier), 1.24f, .0001f);
    // A hard upper bound stated independently of the content, so a future node
    // authored above the ceiling fails here rather than in a playtest.
    const float AbsoluteCeiling = FMath::Pow(UBreakerProgressionComponent::SingleMoreCeiling,
        static_cast<float>(UBreakerProgressionComponent::MaxDamageMoreSources));
    TestTrue(TEXT("The composed product stays under the O3 ceiling"), Stats.DamageMoreMultiplier <= AbsoluteCeiling + UE_KINDA_SMALL_NUMBER);

    // Bloodrhythm is Redline-gated: standing still it is not merely weaker, it
    // is absent from the product entirely.
    TArray<FBreakerNodeRank> RedlineOnly;
    RedlineOnly.Add({TEXT("Swift.Frenzy.Bloodrhythm"), 1});
    const FBreakerNodeStats Idle = UBreakerProgressionComponent::AggregateStats(Nodes, RedlineOnly);
    TestEqual(TEXT("A doctrine keystone composes no More off Redline"), Idle.DamageMoreMultiplier, 1.0f, 0.0001f);
    const FBreakerNodeStats Live = UBreakerProgressionComponent::AggregateStats(
        Nodes, RedlineOnly, nullptr, FBreakerBuildConditionState::All());
    // O95: NOR AT REDLINE. The condition was never what made this node's
    // multiplier legal; a doctrine authors none in any state. What the
    // condition still gates is the replacement -- a fire-rate line that pays at
    // Redline and nowhere else -- so the shape of the node is unchanged and
    // only the lane it lands in has moved.
    TestEqual(TEXT("...and none at Redline either"), Live.DamageMoreMultiplier, 1.0f, 0.0001f);
    // That the REPLACEMENT pays is asserted next door, in
    // RiorsEdge.Progression.Doctrine.KeystonesPayWithoutMores, because this
    // fixture composes every More node at once and cannot attribute a lane to
    // one of them. FireRate in particular has no FBreakerNodeStats field at
    // all — it reaches only the attribute contribution — so a fixture reading
    // node stats is structurally blind to Bloodrhythm's replacement.

    // --- The ceiling against the TIER-4 REWRITES (F9-F11, K9-K11, M9-M11) ---
    //
    // A tier-4 rewrite node is exactly where a fourth More is tempting: it is
    // the branch's most dramatic node and it is not the keystone, so O3's
    // "one per branch keystone" rule is easy to forget. None of the nine
    // authors one, and this is the guard that keeps it that way — a build that
    // owns EVERY More in the content plus EVERY Swift tier-4 node must compose
    // to precisely the same product as the same build without them.
    TArray<FBreakerNodeRank> WithRewrites = Ranks;
    for (const TCHAR* RewriteId : {TEXT("Swift.Frenzy.SecondWind"), TEXT("Swift.Frenzy.RedlineTrigger"), TEXT("Swift.Frenzy.NoSafety"),
        TEXT("Swift.Kinetic.MomentumShield"), TEXT("Swift.Kinetic.SpendToLive"), TEXT("Swift.Kinetic.NoGround"),
        TEXT("Swift.Marksman.Reserve"), TEXT("Swift.Marksman.Overpenetration"), TEXT("Swift.Marksman.CalledShot")})
    {
        WithRewrites.Add({FName(RewriteId), 1});
    }
    const FBreakerNodeStats Rewritten = UBreakerProgressionComponent::AggregateStats(
        Nodes, WithRewrites, nullptr, FBreakerBuildConditionState::All());
    TestEqual(TEXT("The nine tier-4 rewrites add no More source"), Rewritten.DamageMoreSourceCount, Stats.DamageMoreSourceCount);
    TestEqual(TEXT("The nine tier-4 rewrites do not move the composed More product"),
        Rewritten.DamageMoreMultiplier, Stats.DamageMoreMultiplier, 0.0001f);
    // This is the same all-source aggregation stress case, not a legal
    // allocation or a generic product of differently scoped More sources.
    // Adding Swift rewrites must leave its bounded weapon lane unchanged.
    TestTrue(TEXT("Worst case with the tier-4 content stays under the O3 ceiling"),
        Rewritten.DamageMoreMultiplier <= AbsoluteCeiling + UE_KINDA_SMALL_NUMBER);
    // WHAT THIS DOES NOT COVER: the aggregator is exercised with every
    // condition forced true at once, which no real character can hold — it is
    // an upper bound, not a reachable state. It also says nothing about
    // Unwritten items, the other More source outside the class layer, which
    // this pass did not touch.
    return true;
}

#endif
