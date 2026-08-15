#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFallbackTreeIntegrityTest,
    "RiorsEdge.Progression.FallbackTreeIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFallbackTreeIntegrityTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerProgressionTree*>& Trees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    // Core, plus THREE Swift branches (Class-Kits §1.3-1.5), plus THREE Caster
    // branches (§2.3-2.5). Caster shipped with no branch trees at all until
    // now — it played through abilities and the Core tree only — so this count
    // moving from 4 to 7 is the whole point of that pass rather than drift.
    // Counted rather than sampled deliberately: a tree silently failing to
    // register is exactly the class of gap this file exists to catch.
    TestEqual(TEXT("Core tree plus three Swift and three Caster branches exist"), Trees.Num(), 7);

    TSet<FName> SeenNodeIds;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        if (!TestNotNull(TEXT("Tree is valid"), Tree)) return false;
        TestTrue(TEXT("Tree has an id"), !Tree->TreeId.IsNone());
        TestTrue(TEXT("Tree has nodes"), Tree->Nodes.Num() > 0);

        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!TestNotNull(TEXT("Node is valid"), Node)) return false;
            const FString Context = Node->NodeId.ToString();

            TestFalse(*(Context + TEXT(" id is unique")), SeenNodeIds.Contains(Node->NodeId));
            SeenNodeIds.Add(Node->NodeId);

            TestEqual(*(Context + TEXT(" uses its tree's currency")), Node->Currency, Tree->Currency);
            TestEqual(*(Context + TEXT(" matches its tree's class")), Node->RequiredClass, Tree->RequiredClass);
            TestTrue(*(Context + TEXT(" has a display name")), !Node->DisplayName.IsEmpty());
            TestTrue(*(Context + TEXT(" has a description")), !Node->Description.IsEmpty());
            TestTrue(*(Context + TEXT(" has at least one rank")), Node->MaxRank >= 1);

            // Cost grammar: 1 minor, 2 notable, 3 convergence, 5 keystone.
            const bool bCostInGrammar = Node->CostPerRank == 1 || Node->CostPerRank == 2 || Node->CostPerRank == 3 || Node->CostPerRank == 5;
            TestTrue(*(Context + TEXT(" cost is in the 1/2/3/5 grammar")), bCostInGrammar);

            // A node that grants nothing at all is a content bug.
            const bool bGrantsSomething = Node->Effects.Num() > 0 || Node->GrantedTags.Num() > 0 || Node->GrantedAbilityIds.Num() > 0;
            TestTrue(*(Context + TEXT(" grants an effect, a tag, or an ability")), bGrantsSomething);

            for (const FBreakerNodePrerequisite& Prerequisite : Node->Prerequisites)
            {
                const UBreakerProgressionNode* Required = Tree->FindNode(Prerequisite.NodeId);
                if (!TestNotNull(*(Context + TEXT(" prerequisite resolves in the same tree")), Required)) continue;
                TestTrue(*(Context + TEXT(" prerequisite rank is reachable")), Prerequisite.RequiredRank <= Required->MaxRank);
                TestTrue(*(Context + TEXT(" prerequisite sits at or below its tier")), Required->Tier <= Node->Tier);
            }
        }
    }

    // CONTENT SIZE, CHANGED DELIBERATELY UNDER O27 (was Core 15 / branches 8).
    //
    // The pin is not being relaxed to make new content pass — it is being
    // re-set, because O27 rules that "every avenue (affixes, nodes, weapons)
    // needs significantly more options than the slice currently has" and that
    // choices must beat accumulation. Fifteen Core nodes could not carry a
    // 2.5x additive band with the per-point accumulation baseline cut from 1.0%
    // to 0.25%; the power that left accumulation had to land somewhere, and it
    // landed in the nine new Core nodes (the Velocity constellation's six, plus
    // Called Shot, Salvo and Barrage) and five new Swift branch nodes.
    //
    // The numbers below are still EXACT equalities on purpose. The point of the
    // pin was never the value 15 — it is that content cannot drift without
    // somebody saying so in a diff.
    //
    // Re-set again for the ELEMENTS constellation: the Core board rendered
    // Elements as a sealed placeholder because its roster was empty, and O5
    // plus Core-Constellations §6 both say it is designed-but-unshipped rather
    // than cut. Six nodes joined the Core tree (24 -> 30).
    const UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    TestEqual(TEXT("Core slice ships exactly the authored 30"), Core->Nodes.Num(), 30);
    TestEqual(TEXT("Core slice spends Core Points"), Core->Currency, EBreakerPointCurrency::CorePoints);

    // Swift branches, tiers 1-3 only: no tier 4/5 content leaked into the slice.
    TestEqual(TEXT("Frenzy ships ten nodes"), UBreakerProgressionLibrary::GetSwiftFrenzyTree()->Nodes.Num(), 10);
    TestEqual(TEXT("Kinetic ships eleven nodes"), UBreakerProgressionLibrary::GetSwiftKineticTree()->Nodes.Num(), 11);
    TestEqual(TEXT("Marksman ships ten nodes"), UBreakerProgressionLibrary::GetSwiftMarksmanTree()->Nodes.Num(), 10);
    for (const UBreakerProgressionTree* Tree : {UBreakerProgressionLibrary::GetSwiftFrenzyTree(),
        UBreakerProgressionLibrary::GetSwiftKineticTree(), UBreakerProgressionLibrary::GetSwiftMarksmanTree()})
    {
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            TestTrue(TEXT("Swift slice node is tier 1-3"), Node->Tier >= 1 && Node->Tier <= 3);
        }
    }

    // O3: More multipliers may be authored only on branch keystones and
    // constellation Convergence/Keystone nodes. In this content that is exactly
    // "single rank, cost 3 or more" — which is also how SBreakerMenu classifies
    // a node as a Convergence, so the board cannot disagree with the rule.
    int32 MoreNodeCount = 0;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                if (Effect.StatBucket != EBreakerNodeStatBucket::MorePercent) continue;
                ++MoreNodeCount;
                const FString Context = Node->NodeId.ToString();
                TestTrue(*(Context + TEXT(" authors More only at Convergence/Keystone cost")), Node->CostPerRank >= 3);
                TestEqual(*(Context + TEXT(" More node is single rank")), Node->MaxRank, 1);
                TestTrue(*(Context + TEXT(" More stays at or under the 1.30x ceiling")),
                    Effect.ValuePerRank <= (UBreakerProgressionComponent::SingleMoreCeiling - 1.0f) * 100.0f + UE_KINDA_SMALL_NUMBER);
            }
        }
    }
    // Six More options against a hard cap of three is the choice O3 describes;
    // one or two would make the cap decorative.
    TestTrue(TEXT("More options outnumber the O3 cap of three"), MoreNodeCount > UBreakerProgressionComponent::MaxDamageMoreSources);

    TestNotNull(TEXT("Swift has a fallback class definition"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Swift));
    TestNotNull(TEXT("Fallback node lookup finds a core node"), UBreakerProgressionLibrary::FindFallbackNode(TEXT("Core.Kinesis.AirJump")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodePurchaseFlowTest,
    "RiorsEdge.Progression.NodePurchaseFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodePurchaseFlowTest::RunTest(const FString& Parameters)
{
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    UBreakerProgressionTree* Kinetic = UBreakerProgressionLibrary::GetSwiftKineticTree();

    FText Failure;
    TestFalse(TEXT("Purchase without points is rejected"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));
    TestFalse(TEXT("Rejection carries a reason the UI can show"), Failure.IsEmpty());

    Progression->ApplySliceDefaultsIfFresh();
    TestEqual(TEXT("Slice defaults lock Swift"), Progression->GetProgressionState().PermanentClass, EBreakerClassId::Swift);
    TestEqual(TEXT("Slice defaults grant 10 class points"), Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), 10);
    TestEqual(TEXT("Slice defaults grant 12 core points"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 12);
    TestTrue(TEXT("Available trees include the core tree and both Swift branches"), Progression->GetAvailableTrees().Num() >= 3);

    TestFalse(TEXT("Unknown node is rejected"), Progression->PurchaseNode(Core, TEXT("Core.Does.Not.Exist"), Failure));
    TestFalse(TEXT("Node from the wrong tree is rejected"), Progression->PurchaseNode(Core, TEXT("Swift.Kinetic.Carry"), Failure));
    TestFalse(TEXT("Prerequisite is enforced"), Progression->PurchaseNode(Core, TEXT("Core.Precision.TunnelVision"), Failure));

    TestTrue(TEXT("Gateway purchase succeeds"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));
    TestEqual(TEXT("Gateway rank is recorded"), Progression->GetNodeRank(TEXT("Core.Precision.Sightline"), EBreakerPointCurrency::CorePoints), 1);
    TestEqual(TEXT("Cost is spent from core points"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 11);
    TestEqual(TEXT("Tree investment tracks spend"), Progression->GetTreeInvestment(Core), 1);
    TestFalse(TEXT("Rank cap is enforced"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));

    // Tier-2 investment gate: prerequisite met, gate not yet.
    TestFalse(TEXT("Investment gate rejects an early tier 2 node"), Progression->CanPurchaseNode(Core, TEXT("Core.Precision.TunnelVision"), Failure));
    TestTrue(TEXT("A second gateway can be bought"), Progression->PurchaseNode(Core, TEXT("Core.Volley.TriggerDiscipline"), Failure));
    TestTrue(TEXT("Investment gate opens at two points"), Progression->CanPurchaseNode(Core, TEXT("Core.Precision.TunnelVision"), Failure));
    TestTrue(TEXT("Tier 2 notable purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.TunnelVision"), Failure));

    // Effects are live: crit chance and crit damage both moved.
    TestEqual(TEXT("Crit chance aggregates from Sightline"), Progression->GetNodeStats().CriticalChanceBonus, 0.07f, 0.0001f);
    TestEqual(TEXT("Crit damage aggregates from Tunnel Vision"), Progression->GetNodeStats().CriticalMultiplierBonus, 0.22f, 0.0001f);

    // Class points are a separate wallet with its own tree.
    TestTrue(TEXT("Class node purchases from class points"), Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestEqual(TEXT("Class points are spent, core points untouched"), Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), 9);
    TestEqual(TEXT("Slide speed reflects the class node"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);

    // Respec clears effects and refunds every point of that currency.
    TestFalse(TEXT("Respec away from a Forge is rejected"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, false, Failure));
    TestTrue(TEXT("Respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Core points are fully refunded"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 12);
    TestEqual(TEXT("Core ranks are cleared"), Progression->GetNodeRank(TEXT("Core.Precision.Sightline"), EBreakerPointCurrency::CorePoints), 0);
    TestEqual(TEXT("Core effects are cleared"), Progression->GetNodeStats().CriticalChanceBonus, 0.0f, 0.0001f);
    TestEqual(TEXT("Class allocation survives a core respec"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);

    TestTrue(TEXT("Class respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::ClassPoints, true, Failure));
    TestEqual(TEXT("Class points are fully refunded"), Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints), 10);
    TestEqual(TEXT("Class effects are cleared"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.0f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerNodeStatAggregationTest,
    "RiorsEdge.Progression.NodeStatAggregation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerNodeStatAggregationTest::RunTest(const FString& Parameters)
{
    TArray<const UBreakerProgressionNode*> Nodes;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        for (const UBreakerProgressionNode* Node : Tree->Nodes) Nodes.Add(Node);
    }

    TArray<FBreakerNodeRank> Ranks;
    Ranks.Add({TEXT("Core.Precision.Sightline"), 1});     // +7 crit chance
    Ranks.Add({TEXT("Core.Bulwark.SetStance"), 1});       // +6 block, +90 health
    Ranks.Add({TEXT("Core.Kinesis.LightFooting"), 1});    // +5 dodge, +12% move
    Ranks.Add({TEXT("Core.Affliction.Deepen"), 2});       // +18% DoT per rank
    Ranks.Add({TEXT("Swift.Kinetic.AirWork"), 1});        // +12% air control
    Ranks.Add({TEXT("Swift.Marksman.LongLens"), 2});      // +18 crit damage per rank

    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    TestEqual(TEXT("Flat crit chance sums into a fraction"), Stats.CriticalChanceBonus, 0.07f, 0.0001f);
    TestEqual(TEXT("Flat crit damage scales with rank"), Stats.CriticalMultiplierBonus, 0.36f, 0.0001f);
    TestEqual(TEXT("Block chance converts to a fraction"), Stats.BlockChanceBonus, 0.06f, 0.0001f);
    TestEqual(TEXT("Dodge chance converts to a fraction"), Stats.DodgeChanceBonus, 0.05f, 0.0001f);
    TestEqual(TEXT("Health is a flat bonus"), Stats.BonusHealth, 90.0f, 0.0001f);
    TestEqual(TEXT("Increased move speed becomes a multiplier"), Stats.MoveSpeedMultiplier, 1.12f, 0.0001f);
    TestEqual(TEXT("Increased air control becomes a multiplier"), Stats.AirControlMultiplier, 1.12f, 0.0001f);
    TestEqual(TEXT("Increased DoT stacks additively across ranks"), Stats.DamageOverTimeMultiplier, 1.36f, 0.0001f);
    // Sightline's +4% and Long Lens's +3% per rank over two ranks, one bucket.
    TestEqual(TEXT("Increased damage stacks additively across nodes and ranks"), Stats.DamageMultiplier, 1.10f, 0.0001f);
    TestEqual(TEXT("Untouched multipliers stay neutral"), Stats.SlideSpeedMultiplier, 1.0f, 0.0001f);

    // Rule-rewrite and verb nodes publish tags instead of stats.
    TArray<FBreakerNodeRank> VerbRanks;
    VerbRanks.Add({TEXT("Core.Bulwark.Read"), 3});
    const FBreakerNodeStats InertStats = UBreakerProgressionComponent::AggregateStats(Nodes, VerbRanks);
    TestTrue(TEXT("Read publishes its tag"), InertStats.GrantedTags.HasTag(BreakerNodeTags::Node_Read.GetTag()));
    TestFalse(TEXT("Read at rank 3 without Parry grants no verb"), InertStats.GrantedTags.HasTag(BreakerNodeTags::Verb_Parry.GetTag()));
    TestEqual(TEXT("Read at rank 3 without Parry moves no stat"), InertStats.MoveSpeedMultiplier, 1.0f, 0.0001f);

    // Ranks beyond the node's cap cannot inflate the aggregate.
    TArray<FBreakerNodeRank> OverRanks;
    OverRanks.Add({TEXT("Core.Affliction.Deepen"), 9});
    const FBreakerNodeStats ClampedStats = UBreakerProgressionComponent::AggregateStats(Nodes, OverRanks);
    TestEqual(TEXT("Rank is clamped to the node's max"), ClampedStats.DamageOverTimeMultiplier, 1.54f, 0.0001f);

    // Unknown ids in a loaded save are ignored, not fatal.
    TArray<FBreakerNodeRank> StaleRanks;
    StaleRanks.Add({TEXT("Core.Removed.Node"), 4});
    const FBreakerNodeStats StaleStats = UBreakerProgressionComponent::AggregateStats(Nodes, StaleRanks);
    TestEqual(TEXT("Unknown node ids contribute nothing"), StaleStats.CriticalChanceBonus, 0.0f, 0.0001f);
    return true;
}

#endif
