#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// Coverage for the content gap the owner named directly: "we should begin
// designing and expanding fully on all of swift and all of caster so the
// full trees exist for both." Caster shipped through abilities and the
// class-agnostic Core tree only, with none of Class-Kits §2.3-2.5's three
// branches authored. This file is that content's test, matching
// Tests/BreakerBranchContentTests.cpp's style and macros (helpers are
// prefixed because the module builds in unity mode, so a bare helper name
// from another test file would collide).
namespace BreakerCasterTreeTestHelpers
{
    AActor* CasterTreeMakeOwner()
    {
        return NewObject<AActor>();
    }

    // Buys a node to its maximum rank, failing the test on the first refusal.
    bool CasterTreeBuyToMax(FAutomationTestBase& Test, UBreakerProgressionComponent* Progression,
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

    TArray<UBreakerProgressionTree*> CasterTrees()
    {
        return {
            UBreakerProgressionLibrary::GetCasterSpellbladeTree(),
            UBreakerProgressionLibrary::GetCasterVoidWhispererTree(),
            UBreakerProgressionLibrary::GetCasterMultispellTree()
        };
    }
}

// ---------------------------------------------------------------------------
// Registration: three trees, all required by Caster, all found through the
// same catalogue Swift's branches are found through.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterTreesRegisteredTest,
    "RiorsEdge.Progression.CasterTrees.Registered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterTreesRegisteredTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterTreeTestHelpers;

    const TArray<UBreakerProgressionTree*>& AllTrees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    for (UBreakerProgressionTree* Tree : CasterTrees())
    {
        if (!TestNotNull(TEXT("Caster branch tree exists"), Tree)) continue;
        TestTrue(*(Tree->TreeId.ToString() + TEXT(" is in GetAllFallbackTrees()")), AllTrees.Contains(Tree));
        TestEqual(*(Tree->TreeId.ToString() + TEXT(" is required by Caster")), Tree->RequiredClass, EBreakerClassId::Caster);
        TestEqual(*(Tree->TreeId.ToString() + TEXT(" spends class points")), Tree->Currency, EBreakerPointCurrency::CorePoints);
        TestTrue(*(Tree->TreeId.ToString() + TEXT(" has nodes")), Tree->Nodes.Num() > 0);
    }

    const TArray<UBreakerProgressionTree*> ForCaster = UBreakerProgressionLibrary::GetTreesForClass(EBreakerClassId::Caster);
    for (UBreakerProgressionTree* Tree : CasterTrees())
    {
        TestTrue(*(Tree->TreeId.ToString() + TEXT(" is offered to a Caster")), ForCaster.Contains(Tree));
    }

    const UBreakerClassDefinition* CasterDefinition = UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Caster);
    if (TestNotNull(TEXT("Caster has a fallback class definition"), CasterDefinition))
    {
        for (UBreakerProgressionTree* Tree : CasterTrees())
        {
            TestTrue(*(Tree->TreeId.ToString() + TEXT(" is in Caster's BranchTrees")), CasterDefinition->BranchTrees.Contains(Tree));
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Shape: exactly one cornerstone per branch, and every Caster keystone tag
// (Edgework / Long Dark / Cascade) is granted by exactly one node, which is
// that branch's cornerstone. This is the O37 gate the KeystoneReachability
// test (Tests/BreakerKeystoneReachabilityTests.cpp) now exercises against
// Caster for the first time -- that file is self-closing and untouched here,
// this is the branch-local half of the same guarantee.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterTreesCornerstonesTest,
    "RiorsEdge.Progression.CasterTrees.Cornerstones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterTreesCornerstonesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterTreeTestHelpers;

    const FGameplayTag KeystoneTags[3] = {
        BreakerAbilityTags::Keystone_Caster_Edgework.GetTag(),
        BreakerAbilityTags::Keystone_Caster_LongDark.GetTag(),
        BreakerAbilityTags::Keystone_Caster_Cascade.GetTag()
    };

    for (UBreakerProgressionTree* Tree : CasterTrees())
    {
        if (!TestNotNull(TEXT("Tree exists"), Tree)) continue;
        int32 CornerstoneCount = 0;
        const UBreakerProgressionNode* Cornerstone = nullptr;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node->bCornerstone)
            {
                ++CornerstoneCount;
                Cornerstone = Node;
            }
        }
        TestEqual(*(Tree->TreeId.ToString() + TEXT(" has exactly one cornerstone")), CornerstoneCount, 1);

        if (Cornerstone)
        {
            bool bCarriesAKeystoneTag = false;
            for (const FGameplayTag& Tag : KeystoneTags)
            {
                if (Cornerstone->GrantedTags.HasTag(Tag)) bCarriesAKeystoneTag = true;
            }
            TestTrue(*(Tree->TreeId.ToString() + TEXT(" cornerstone grants one of the three Caster keystone tags")), bCarriesAKeystoneTag);
        }
    }

    // Each keystone tag is granted by exactly one node across all three
    // branches -- no double-authoring, no orphan.
    for (const FGameplayTag& Tag : KeystoneTags)
    {
        int32 GrantCount = 0;
        for (UBreakerProgressionTree* Tree : CasterTrees())
        {
            if (!Tree) continue;
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (Node->GrantedTags.HasTag(Tag)) ++GrantCount;
            }
        }
        TestEqual(*(Tag.ToString() + TEXT(" is granted by exactly one node")), GrantCount, 1);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Integrity: prerequisites resolve inside the SAME tree (cross-branch
// prerequisites would silently never be satisfiable, since PurchaseNode reads
// the prerequisite from the tree it was given).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterTreesPrerequisitesResolveTest,
    "RiorsEdge.Progression.CasterTrees.PrerequisitesResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterTreesPrerequisitesResolveTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterTreeTestHelpers;

    for (UBreakerProgressionTree* Tree : CasterTrees())
    {
        if (!TestNotNull(TEXT("Tree exists"), Tree)) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            const FString Context = Tree->TreeId.ToString() + TEXT(".") + Node->NodeId.ToString();
            for (const FBreakerNodePrerequisite& Prerequisite : Node->Prerequisites)
            {
                const UBreakerProgressionNode* Required = Tree->FindNode(Prerequisite.NodeId);
                TestNotNull(*(Context + TEXT(" prerequisite resolves in the same tree")), Required);
                if (Required)
                {
                    TestTrue(*(Context + TEXT(" prerequisite sits at or below its tier")), Required->Tier <= Node->Tier);
                }
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Enum legality: every authored effect uses a target/bucket the enum
// actually has, and every node grants something (no silently decorative
// purchase). Nodes whose Class-Kits behaviour has no enum representation
// (the majority of this content -- see the block comment on
// GetCasterSpellbladeTree()) ship as GrantedTags only, which still satisfies
// "grants an effect, a tag, or an ability."
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterTreesLegalEffectsTest,
    "RiorsEdge.Progression.CasterTrees.LegalEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterTreesLegalEffectsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterTreeTestHelpers;

    for (UBreakerProgressionTree* Tree : CasterTrees())
    {
        if (!TestNotNull(TEXT("Tree exists"), Tree)) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            const FString Context = Tree->TreeId.ToString() + TEXT(".") + Node->NodeId.ToString();
            const bool bGrantsSomething = Node->Effects.Num() > 0 || Node->GrantedTags.Num() > 0 || Node->GrantedAbilityIds.Num() > 0;
            TestTrue(*(Context + TEXT(" grants an effect, a tag, or an ability")), bGrantsSomething);

            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                TestTrue(*(Context + TEXT(" StatTarget is within the enum's range")),
                    Effect.StatTarget >= EBreakerNodeStatTarget::CriticalChance && Effect.StatTarget < EBreakerNodeStatTarget::Count);
                TestTrue(*(Context + TEXT(" StatBucket is Flat, IncreasedPercent, or MorePercent")),
                    Effect.StatBucket == EBreakerNodeStatBucket::Flat || Effect.StatBucket == EBreakerNodeStatBucket::IncreasedPercent
                    || Effect.StatBucket == EBreakerNodeStatBucket::MorePercent);

                if (Effect.StatBucket == EBreakerNodeStatBucket::MorePercent)
                {
                    // O3: More multipliers may be authored only on branch
                    // keystones and constellation Convergence/Keystone nodes,
                    // i.e. single rank, cost 3+ -- exactly Long Dark's shape.
                    TestTrue(*(Context + TEXT(" authors More only at Convergence/Keystone cost")), Node->CostPerRank >= 3);
                    TestEqual(*(Context + TEXT(" More node is single rank")), Node->MaxRank, 1);
                    TestTrue(*(Context + TEXT(" More stays at or under the 1.30x ceiling")),
                        Effect.ValuePerRank <= (UBreakerProgressionComponent::SingleMoreCeiling - 1.0f) * 100.0f + UE_KINDA_SMALL_NUMBER);
                }
            }

            // Cost grammar this file promises to match: 1 (entry/loop), 2
            // (tier-3 ability-shaped), 3 (keystone) -- the same grammar
            // Frenzy/Kinetic/Marksman use, no second grammar invented.
            const bool bCostInGrammar = Node->CostPerRank == 1 || Node->CostPerRank == 2 || Node->CostPerRank == 3;
            TestTrue(*(Context + TEXT(" cost is in the 1/2/3 grammar")), bCostInGrammar);
            TestTrue(*(Context + TEXT(" tier is 1-3, matching the slice cut every other branch uses")),
                Node->Tier >= 1 && Node->Tier <= 3);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// O3/O34 budget: the greediest legal single-branch Caster build (a full
// Void Whisperer walk, the only Caster branch that authors a MorePercent
// effect at all) must not push the composed More product past the ceiling.
// A4 (owner ruling 2026-08-16): the DoT More lane EXISTS now — Long Dark's
// DamageOverTime-targeted 1.30x composes onto the DamageOverTimeMultiplier
// attribute's More product, counts as one source in the shared O34 budget,
// and never touches the direct-hit Damage More product. This test is the
// pin the old comment promised would change when the lane landed.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCasterTreesMoreCeilingTest,
    "RiorsEdge.Progression.CasterTrees.MoreCeiling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCasterTreesMoreCeilingTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCasterTreeTestHelpers;

    UBreakerProgressionTree* VoidWhisperer = UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    if (!TestNotNull(TEXT("Void Whisperer tree exists"), VoidWhisperer)) return false;

    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    AActor* Owner = CasterTreeMakeOwner();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Progression->BindAttributes(Attributes);
    Progression->ChoosePermanentClassById(EBreakerClassId::Caster);
    // A full Void Whisperer branch is well under 30; grant generously so every
    // rank of every node purchases with room to spare.
    Progression->GrantPlaytestPoints(40, 0);

    FText CommitFailure;
    TestTrue(*FString::Printf(TEXT("Committing to Void Whisperer succeeds (%s)"), *CommitFailure.ToString()),
        Progression->CommitToBranch(VoidWhisperer->TreeId, CommitFailure));
    for (const UBreakerProgressionNode* Node : VoidWhisperer->Nodes)
    {
        CasterTreeBuyToMax(*this, Progression, VoidWhisperer, Node->NodeId);
    }

    const FBreakerNodeStats& Stats = Progression->GetNodeStats();
    TestTrue(TEXT("Long Dark's keystone tag is live once bought"),
        Stats.GrantedTags.HasTag(BreakerAbilityTags::Keystone_Caster_LongDark.GetTag()));

    // A4: Long Dark PAYS. It counts as one held More source in the shared
    // budget, lands on the DoT lane's More product, and leaves the direct-hit
    // Damage More product neutral — DoT only is the tax the canon prices.
    TestEqual(TEXT("Long Dark counts as one More source in the shared O34 budget (A4)"), Stats.DamageMoreSourceCount, 1);
    TestEqual(TEXT("Long Dark leaves the direct-hit Damage More product neutral (DoT only)"), Stats.DamageMoreMultiplier, 1.0f, 0.0001f);
    TestEqual(TEXT("Long Dark's 1.30x rides the DoT lane's More product (A4)"),
        Attributes->GetAttributeAggregator().ComposedMoreProduct(EBreakerAggregatedAttribute::DamageOverTimeMultiplier), 1.30f, 0.0001f);
    // And it reaches an actual tick: the whole-tick multiplier composed for a
    // DoT application carries the 1.30x (times whatever Increased the walk
    // bought, additively — the A4 bucket rule).
    // Ability-delivered: a Void Whisperer's ticks come from Caster abilities, so
    // the lane the tick folds beside the DoT bucket is the ability pool (O55).
    TestTrue(TEXT("A full Void Whisperer's DoT tick pays the Long Dark More"),
        UBreakerCombatComponent::ComposeDotSourcePower(Attributes, nullptr, EBreakerDamageDelivery::Ability) >= 1.30f - UE_KINDA_SMALL_NUMBER);

    // A hard, content-independent ceiling check regardless: whatever the
    // product is, it must never exceed the O3-cap composed against the
    // per-More ceiling.
    const float AbsoluteCeiling = FMath::Pow(UBreakerProgressionComponent::SingleMoreCeiling,
        static_cast<float>(UBreakerProgressionComponent::MaxDamageMoreSources));
    TestTrue(TEXT("The composed product stays under the O3 ceiling"), Stats.DamageMoreMultiplier <= AbsoluteCeiling + UE_KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// MS12 Cascade re-authored (owner ruling 2026-08-16): the designed "1.25x
// More vs 3+ status targets" ships as a TARGET-RIDER INCREASED line — +25%
// Increased Damage conditioned on TargetMultiStatus, published through
// BuildTargetConditionRiders and resolved target-side in ReceiveDamage.
// Caster's third More slot stays unspent; the keystone's payoff is real.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCascadeTargetRiderTest,
    "RiorsEdge.Progression.CasterTrees.CascadeTargetRider",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCascadeTargetRiderTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionTree* Multispell = UBreakerProgressionLibrary::GetCasterMultispellTree();
    if (!TestNotNull(TEXT("Multispell tree exists"), Multispell)) return false;

    TArray<const UBreakerProgressionNode*> Nodes;
    for (const UBreakerProgressionNode* Node : Multispell->Nodes) Nodes.Add(Node);
    TArray<FBreakerNodeRank> Ranks;
    Ranks.Add({ TEXT("Caster.Multispell.Cascade"), 1 });

    // The node authors NO More: the slot stays unspent (the re-authoring's
    // whole point), and the payoff is not a source in the O34 budget.
    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    TestEqual(TEXT("Cascade spends no More source (re-authored as Increased)"), Stats.DamageMoreSourceCount, 0);
    TestEqual(TEXT("Cascade's rider is not composed source-side (deferred to the target site)"),
        Stats.DamageMultiplier, 1.0f, 0.0001f);

    // The rider row itself: one line, Damage / Increased / +25, keyed on the
    // stacking predicate Class-Kits MS12 always named.
    const TArray<FBreakerTargetConditionRider> Riders =
        UBreakerProgressionComponent::BuildTargetConditionRiders(Nodes, Ranks);
    if (!TestEqual(TEXT("Cascade publishes exactly one target rider"), Riders.Num(), 1)) return false;
    TestTrue(TEXT("The rider keys on TargetMultiStatus (3+ distinct statuses)"),
        Riders[0].Condition == EBreakerBuildCondition::TargetMultiStatus);
    TestTrue(TEXT("The rider targets Damage"), Riders[0].StatTarget == EBreakerNodeStatTarget::Damage);
    TestEqual(TEXT("The rider is +25% Increased (O2 PLACEHOLDER, owner ruling 2026-08-16)"), Riders[0].Percent, 25.0f, 0.0001f);
    return true;
}

#endif
