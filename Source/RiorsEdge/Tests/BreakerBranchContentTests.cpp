#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// Coverage for the two content gaps O27 named: Swift's FRENZY branch, which the
// design document has always specified and the library never authored, and the
// ELEMENTS constellation, whose empty roster is why the Core board drew it as a
// sealed placeholder.
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
    TestEqual(TEXT("Frenzy is a Swift class-point branch"), Frenzy->Currency, EBreakerPointCurrency::ClassPoints);
    TestEqual(TEXT("Frenzy belongs to Swift"), Frenzy->RequiredClass, EBreakerClassId::Swift);

    // Class-Kits §1.3 shape: three entry nodes, four loop nodes, two ability-tier
    // nodes and one keystone, matching Kinetic and Marksman rather than the
    // document's five-tier full-branch shape (the slice is tiers 1-3, §7).
    int32 TierCounts[4] = {};
    for (const UBreakerProgressionNode* Node : Frenzy->Nodes)
    {
        if (!TestTrue(TEXT("Frenzy node is tier 1-3"), Node->Tier >= 1 && Node->Tier <= 3)) continue;
        ++TierCounts[Node->Tier];
    }
    TestEqual(TEXT("Frenzy has three tier-1 entry nodes"), TierCounts[1], 3);
    TestEqual(TEXT("Frenzy has four tier-2 loop nodes"), TierCounts[2], 4);
    TestEqual(TEXT("Frenzy has three tier-3 nodes"), TierCounts[3], 3);

    // Cost and rank curve must match the two branches already shipped, or the
    // board teaches the player two different grammars.
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
            TestEqual(*(Context + TEXT(" tier-3 node is single rank")), Node->MaxRank, 1);
            TestTrue(*(Context + TEXT(" tier-3 node costs 2 or 3")), Node->CostPerRank == 2 || Node->CostPerRank == 3);
        }
        // O27: no node may be purely decorative.
        TestTrue(*(Context + TEXT(" grants an effect or a tag")), Node->Effects.Num() > 0 || Node->GrantedTags.Num() > 0);
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
    // A full Frenzy branch is 21 class points; the slice grant is 10.
    Progression->GrantPlaytestPoints(40, 0);

    const float BaseHealth = Attributes->GetMaxHealth();
    const float BaseCritChance = Attributes->GetCriticalChance();
    const float BaseCritMultiplier = Attributes->GetCriticalMultiplier();
    const float BaseDamage = Attributes->GetDamageMultiplier();

    for (const UBreakerProgressionNode* Node : Frenzy->Nodes)
    {
        BranchContentBuyToMax(*this, Progression, Frenzy, Node->NodeId);
    }
    for (const UBreakerProgressionNode* Node : Frenzy->Nodes)
    {
        TestEqual(*(Node->NodeId.ToString() + TEXT(" reached its maximum rank")),
            Progression->GetNodeRank(Node->NodeId, EBreakerPointCurrency::ClassPoints), Node->MaxRank);
    }

    // Unconditional lines reach the attribute set. Feed is +45 health over two
    // ranks; Trigger Discipline and Rhythm are +3 crit chance each over two
    // ranks; Slipcut Mastery is +20 crit damage.
    TestEqual(TEXT("Feed's health reaches the attribute set"), Attributes->GetMaxHealth() - BaseHealth, 90.0f, 0.001f);
    TestEqual(TEXT("Frenzy's crit chance reaches the attribute set"), Attributes->GetCriticalChance() - BaseCritChance, 0.12f, 0.0001f);
    TestEqual(TEXT("Slipcut Mastery's crit damage reaches the attribute set"), Attributes->GetCriticalMultiplier() - BaseCritMultiplier, 0.20f, 0.0001f);
    // Ammunition Economy is Frenzy's only unconditional +5% damage. Everything
    // else in the branch is Redline-gated and must pay NOTHING standing still —
    // that is the whole point of the branch under O27.
    TestEqual(TEXT("Standing still, only the unconditional damage line pays"), Attributes->GetDamageMultiplier() - BaseDamage, 0.05f, 0.0001f);
    TestEqual(TEXT("Short Leash's move speed composes"), Progression->GetNodeStats().MoveSpeedMultiplier, 1.10f, 0.0001f);

    // Conditional damage is real, and it is worth substantially more than the
    // unconditional line — Loaded 12 + Dry Fire 10 + Overrev 24 = 46% at Redline.
    const FBreakerNodeStats& Stats = Progression->GetNodeStats();
    TestEqual(TEXT("Nothing conditional is live while standing still"), Stats.ActiveConditionalDamagePercent, 0.0f, 0.0001f);
    TestEqual(TEXT("Redline is worth 46% increased damage to a full Frenzy"), Stats.PotentialConditionalDamagePercent, 46.0f, 0.0001f);

    // The keystone publishes the tag Overdrive's variant table already keys on,
    // so owning Bloodrhythm really does rewrite the ultimate.
    TestTrue(TEXT("Bloodrhythm publishes its keystone tag"),
        Stats.GrantedTags.HasTag(BreakerAbilityTags::Keystone_Swift_Bloodrhythm.GetTag()));

    // --- Respec restores exactly -------------------------------------------
    FText Failure;
    TestTrue(TEXT("Class respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::ClassPoints, true, Failure));
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
    {
        // Constellation membership rides the node-id prefix; SBreakerMenu's
        // cluster layout reads the same string, so a node named any other way
        // would render outside the Elements cluster.
        if (Node->NodeId.ToString().StartsWith(TEXT("Core.Elements."))) ElementNodes.Add(Node);
    }
    TestEqual(TEXT("Elements ships six nodes, so its cluster is no longer sealed"), ElementNodes.Num(), 6);

    for (const UBreakerProgressionNode* Node : ElementNodes)
    {
        const FString Context = Node->NodeId.ToString();
        TestEqual(*(Context + TEXT(" spends core points")), Node->Currency, EBreakerPointCurrency::CorePoints);
        TestEqual(*(Context + TEXT(" is class-agnostic")), Node->RequiredClass, EBreakerClassId::None);
        // The rule this constellation exists to obey: pre-resistance, every
        // Elements node must still MOVE something. A node carrying only an
        // unconsumed elemental tag would be the damage-less damage node again.
        TestTrue(*(Context + TEXT(" authors at least one live stat effect")), Node->Effects.Num() > 0);
        TestTrue(*(Context + TEXT(" carries its elemental rule as a tag")), Node->GrantedTags.Num() > 0);
        // §2.4 reserves Elements' More slot and this pass deliberately leaves it
        // empty: no movement condition can express "a reaction fired", so the
        // only authorable form would be an unconditional generalist.
        for (const FBreakerNodeEffect& Effect : Node->Effects)
        {
            TestTrue(*(Context + TEXT(" authors no More multiplier")), Effect.StatBucket != EBreakerNodeStatBucket::MorePercent);
        }
    }

    // --- Purchasable end to end, and the effects land ----------------------
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = BranchContentMakeOwner();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Progression->BindAttributes(Attributes);
    Progression->ApplySliceDefaultsIfFresh();
    Progression->GrantPlaytestPoints(0, 40);

    const float BaseDoT = Attributes->GetDamageOverTimeMultiplier();
    const float BaseCritChance = Attributes->GetCriticalChance();
    const float BaseDamage = Attributes->GetDamageMultiplier();

    // Investment gates are per TREE, not per constellation, and Elements has one
    // gateway — so reaching its tier-2 lanes costs one point somewhere else in
    // Core first. That is the intended cross-constellation pull, not a bug, and
    // the test pays it the way a player would.
    FText PrimerFailure;
    TestTrue(TEXT("A point elsewhere in Core opens the tier-2 gate"),
        Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), PrimerFailure));
    const float PrimedDoT = Attributes->GetDamageOverTimeMultiplier();
    const float PrimedCritChance = Attributes->GetCriticalChance();
    const float PrimedDamage = Attributes->GetDamageMultiplier();

    // The Convergence sits behind the tier-3 investment gate, which a single
    // gateway cannot open — buy the lanes first, exactly as a player would.
    TestTrue(TEXT("Conductive opens the constellation"), BranchContentBuyToMax(*this, Progression, Core, TEXT("Core.Elements.Conductive")));
    TestTrue(TEXT("Charge Up ladders to three ranks"), BranchContentBuyToMax(*this, Progression, Core, TEXT("Core.Elements.ChargeUp")));
    TestTrue(TEXT("Threshold purchases"), BranchContentBuyToMax(*this, Progression, Core, TEXT("Core.Elements.Threshold")));
    TestTrue(TEXT("Catalyst purchases"), BranchContentBuyToMax(*this, Progression, Core, TEXT("Core.Elements.Catalyst")));
    TestTrue(TEXT("Penetrance purchases"), BranchContentBuyToMax(*this, Progression, Core, TEXT("Core.Elements.Penetrance")));
    TestTrue(TEXT("Reaction Chain purchases"), BranchContentBuyToMax(*this, Progression, Core, TEXT("Core.Elements.ReactionChain")));

    // 8 + (7 x 3) + 14 + 25 = 68% into the one additive DoT bucket.
    TestEqual(TEXT("Elements' damage over time reaches the attribute set"),
        Attributes->GetDamageOverTimeMultiplier() - PrimedDoT, 0.68f, 0.0001f);
    // Catalyst 4 x 2 ranks.
    TestEqual(TEXT("Catalyst's crit chance reaches the attribute set"),
        Attributes->GetCriticalChance() - PrimedCritChance, 0.08f, 0.0001f);
    // Penetrance 4 x 2 ranks plus Reaction Chain's 6.
    TestEqual(TEXT("Penetrance and Reaction Chain reach the damage bucket"),
        Attributes->GetDamageMultiplier() - PrimedDamage, 0.14f, 0.0001f);

    // --- Respec restores exactly -------------------------------------------
    FText Failure;
    TestTrue(TEXT("Core respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Respec restores damage over time exactly"), Attributes->GetDamageOverTimeMultiplier(), BaseDoT, 0.0001f);
    TestEqual(TEXT("Respec restores crit chance exactly"), Attributes->GetCriticalChance(), BaseCritChance, 0.0001f);
    TestEqual(TEXT("Respec restores the damage multiplier exactly"), Attributes->GetDamageMultiplier(), BaseDamage, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMoreCeilingWithNewContentTest,
    "RiorsEdge.Progression.MoreCeilingWithNewContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMoreCeilingWithNewContentTest::RunTest(const FString& Parameters)
{
    // O3 is a hard cap of three composed More multipliers per BUILD. Frenzy adds
    // a fifth More option to the content, which is the shape O27 asks for — more
    // options than the cap, so which three you hold is a decision — but the cap
    // itself must not move.
    TArray<const UBreakerProgressionNode*> Nodes;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        for (const UBreakerProgressionNode* Node : Tree->Nodes) Nodes.Add(Node);
    }

    // Every More node in the content, so a build that could hold them all does
    // not get to. Five of them; the aggregator must keep the strongest three.
    TArray<FBreakerNodeRank> Ranks;
    Ranks.Add({TEXT("Core.Precision.Fixate"), 1});            // x1.22 unconditional
    Ranks.Add({TEXT("Core.Volley.Barrage"), 1});              // x1.22 unconditional
    Ranks.Add({TEXT("Core.Velocity.TerminalVelocity"), 1});   // x1.30 airborne
    Ranks.Add({TEXT("Core.Velocity.RedlineDoctrine"), 1});    // x1.20 at Redline
    Ranks.Add({TEXT("Swift.Kinetic.Overpressure"), 1});       // x1.20 sliding
    Ranks.Add({TEXT("Swift.Marksman.Culling"), 1});           // x1.18 unconditional
    Ranks.Add({TEXT("Swift.Frenzy.Bloodrhythm"), 1});         // x1.20 at Redline

    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(
        Nodes, Ranks, nullptr, FBreakerBuildConditionState::All());

    TestEqual(TEXT("Every authored More source is counted honestly"), Stats.DamageMoreSourceCount, 7);
    TestTrue(TEXT("More options outnumber the O3 cap, so holding three is a choice"),
        Stats.DamageMoreSourceCount > UBreakerProgressionComponent::MaxDamageMoreSources);
    // Strongest three only: 1.30 x 1.22 x 1.22.
    TestEqual(TEXT("Only the strongest three More multipliers compose"), Stats.DamageMoreMultiplier, 1.30f * 1.22f * 1.22f, 0.0001f);
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
    TestEqual(TEXT("A Redline More pays nothing off Redline"), Idle.DamageMoreMultiplier, 1.0f, 0.0001f);
    const FBreakerNodeStats Live = UBreakerProgressionComponent::AggregateStats(
        Nodes, RedlineOnly, nullptr, FBreakerBuildConditionState::All());
    TestEqual(TEXT("A Redline More pays x1.20 at Redline"), Live.DamageMoreMultiplier, 1.20f, 0.0001f);
    return true;
}

#endif
