#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionTree.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFallbackTreeIntegrityTest,
    "RiorsEdge.Progression.FallbackTreeIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFallbackTreeIntegrityTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerProgressionTree*>& Trees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    // Core, plus THREE branches for EVERY class: Swift (Class-Kits §1.3-1.5),
    // Caster (§2.3-2.5), and — authored 2026-08-16 under the owner's "do all
    // 5 classes" ruling — Gunsmith (Class-Kits-Gunsmith §4), Tank
    // (Class-Kits-Tank §3-5) and Support (Class-Kits-Support §4). The count
    // moved 4 -> 7 when Caster's branch layer landed and 7 -> 16 when the
    // last three classes' did; each move was the point of its pass, not
    // drift. Counted rather than sampled deliberately: a tree silently
    // failing to register is exactly the class of gap this file exists to
    // catch.
    TestEqual(TEXT("Core tree plus three branches for each of the five classes exist"), Trees.Num(), 16);

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

    // SWIFT BRANCH SIZE AND CEILING, RE-PINNED DELIBERATELY (was 10 / 11 / 10,
    // and "every Swift node is tier 1-3").
    //
    // The old pins described a TRUNCATED Swift: the slice authored tiers 1-3
    // and dropped every Tier-4 rewrite node Class-Kits §1.3-1.5 specifies —
    // F9-F11, K9-K11, M9-M11. Those nine are now authored, three per branch, so
    // each count rises by exactly three and the tier ceiling rises from 3 to 4.
    // Nothing was relaxed to make new content pass: the equalities are still
    // exact, and they are still the only thing standing between authored
    // content and silent drift. The number moved because somebody added nine
    // nodes on purpose and said so in this diff.
    //
    // Tier 5 is STILL excluded, and that is not an oversight either — §0.2's
    // fifth tier is the keystone tier, and the compressed ladder has no fifth
    // tier at all. The keystone used to sit at tier 3, BELOW the rewrites, and
    // now sits at tier 4 beside them: the doctrine wallet is 8 and the keystone
    // costs 2 behind a gate of 6, so it is the last of four picks rather than a
    // rung on the way up. See the block comment above GetSwiftKineticTree.
    TestEqual(TEXT("Frenzy ships thirteen nodes: ten, plus F9-F11"), UBreakerProgressionLibrary::GetSwiftFrenzyTree()->Nodes.Num(), 13);
    TestEqual(TEXT("Kinetic ships fourteen nodes: eleven, plus K9-K11"), UBreakerProgressionLibrary::GetSwiftKineticTree()->Nodes.Num(), 14);
    TestEqual(TEXT("Marksman ships thirteen nodes: ten, plus M9-M11"), UBreakerProgressionLibrary::GetSwiftMarksmanTree()->Nodes.Num(), 13);
    for (const UBreakerProgressionTree* Tree : {UBreakerProgressionLibrary::GetSwiftFrenzyTree(),
        UBreakerProgressionLibrary::GetSwiftKineticTree(), UBreakerProgressionLibrary::GetSwiftMarksmanTree()})
    {
        int32 TierFourCount = 0;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            const FString Context = Node->NodeId.ToString();
            TestTrue(TEXT("Swift branch node is tier 1-4"), Node->Tier >= 1 && Node->Tier <= 4);
            // TIER 4 NO LONGER MEANS "REWRITE". The keystone moved up into this
            // tier when its cornerstone gate was removed, so the tier holds two
            // KINDS of node with different grammars: three rewrites at one rank
            // for two points authoring only loop-economy lines, and one
            // cornerstone whose whole job is a stat line on the doctrine's own
            // axis. Every assertion below is about the rewrite grammar, so the
            // cornerstone is excluded here rather than exempted individually --
            // an exemption per assertion is how a keystone quietly acquires a
            // rewrite's restrictions or loses its own.
            if (Node->Tier != 4 || Node->bCornerstone) continue;
            ++TierFourCount;

            // The rewrite tier's grammar, stated as an assertion rather than a
            // comment: §0.2 prices tier 4 at one rank for two points.
            TestEqual(*(Context + TEXT(" tier-4 rewrite is single rank")), Node->MaxRank, 1);
            TestEqual(*(Context + TEXT(" tier-4 rewrite costs 2")), Node->CostPerRank, 2);
            // O3: a class-layer More may live only on a branch keystone, and
            // all three of Swift's are already spent. A More appearing at
            // tier 4 would be a fourth against a budget of three.
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                TestTrue(*(Context + TEXT(" tier-4 rewrite authors no More multiplier")),
                    Effect.StatBucket != EBreakerNodeStatBucket::MorePercent);
            }
            // The old pin here was "tier-4 rewrites author NO stat effect",
            // and it failed exactly as intended when the loop valve landed
            // (2026-08-16): the tier-4 trio's decay downsides ARE stat lines
            // now — ClassResourceDecay through the valve, AbilityCost for No
            // Safety's discount half, both Class-Kits-transcribed. The re-set
            // pin is the boundary that still holds: a tier-4 rewrite may
            // author ONLY loop-economy lines (decay / cost), never a damage
            // or combat stat — that would be a different node with a
            // different fantasy, and a content decision, not a refactor.
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                TestTrue(*(Context + TEXT(" tier-4 rewrite authors only loop-economy lines (ClassResourceDecay/AbilityCost)")),
                    Effect.StatTarget == EBreakerNodeStatTarget::ClassResourceDecay
                    || Effect.StatTarget == EBreakerNodeStatTarget::AbilityCost);
            }
            TestTrue(*(Context + TEXT(" tier-4 rewrite carries its rule as a tag")), Node->GrantedTags.Num() > 0);
            // A rewrite with no prerequisite is a rewrite of nothing. The
            // generic loop above already proves prerequisites resolve inside
            // the same tree and sit at or below this node's tier.
            TestTrue(*(Context + TEXT(" tier-4 rewrite builds on an earlier node")), Node->Prerequisites.Num() > 0);
        }
        // Three REWRITES, counted excluding the cornerstone that now shares
        // their tier. Four tier-4 nodes, three of them rewrites.
        TestEqual(TEXT("Each Swift branch ships exactly three tier-4 rewrites"), TierFourCount, 3);
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
    TestEqual(TEXT("The retired class pool is empty"), Progression->GetUnspentPoints(EBreakerPointCurrency::ClassPoints_Retired), 0);
    TestEqual(TEXT("An uncommitted character holds no doctrine points"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
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

    // O111: THE DOCTRINE WALLET IS FILLED BY COMMITMENT, NOT BY LEVELLING.
    // A doctrine node is unaffordable until the Forge pays the eight, and that
    // ordering is the ruling rather than a rig detail -- so the purchase is
    // asserted to FAIL first, then to succeed once committed.
    FText CommitFailure;
    TestFalse(TEXT("A doctrine node is unaffordable before commitment"),
        Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestTrue(TEXT("Committing to Kinetic succeeds"),
        Progression->CommitToBranch(TEXT("Doctrine.Swift.Kinetic"), CommitFailure));
    // COMMITMENT PAYS NOTHING. It used to hand over the whole eight; the pool
    // is now earned at four benchmarks, so committing chooses WHERE points go
    // and the benchmarks decide WHEN they exist. This fixture reaches the
    // points by levelling to the cap rather than by committing.
    TestEqual(TEXT("Committing pays no points by itself"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    // Reach the last benchmark the way the game does -- XP, not a setter.
    const FBreakerExperienceCurve BenchmarkCurve;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(
        UBreakerProgressionLibrary::CorePointCapLevel, BenchmarkCurve) - Progression->GetTotalExperience());
    TestEqual(TEXT("Reaching the last benchmark pays the whole pool"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),
        UBreakerProgressionLibrary::DoctrinePointGrant);

    const int32 CoreBeforeDoctrine = Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints);
    TestTrue(TEXT("A doctrine node purchases from doctrine points"), Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestEqual(TEXT("Doctrine points are spent"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),
        UBreakerProgressionLibrary::DoctrinePointGrant - 1);
    TestEqual(TEXT("...and core points are untouched"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), CoreBeforeDoctrine);
    TestEqual(TEXT("Slide speed reflects the class node"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);

    // Respec clears effects and refunds every point of that currency.
    TestFalse(TEXT("Respec away from a Forge is rejected"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, false, Failure));
    TestTrue(TEXT("Respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    // Fifty, not the slice's twelve: this fixture now levels to the cap to
    // reach the doctrine benchmarks, and Core pays one per level on the way.
    TestEqual(TEXT("Core points are fully refunded"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints),
        UBreakerProgressionLibrary::CorePointCapLevel);
    TestEqual(TEXT("Core ranks are cleared"), Progression->GetNodeRank(TEXT("Core.Precision.Sightline"), EBreakerPointCurrency::CorePoints), 0);
    TestEqual(TEXT("Core effects are cleared"), Progression->GetNodeStats().CriticalChanceBonus, 0.0f, 0.0001f);
    TestEqual(TEXT("Class allocation survives a core respec"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.12f, 0.0001f);

    // A DOCTRINE RESPEC IS NOW A REFUND, AND IT USED TO ZERO THE WALLET. That
    // was right while commitment paid the eight -- they belonged to the
    // commitment being cleared. They are earned at benchmarks now, so zeroing
    // would delete points a player was paid for reaching level 40, and nothing
    // would ever hand them back.
    TestTrue(TEXT("Doctrine respec at a Forge succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, true, Failure));
    TestEqual(TEXT("The doctrine wallet is refunded in full"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),
        UBreakerProgressionLibrary::DoctrinePointGrant);
    TestEqual(TEXT("The commitment is cleared with it"), Progression->GetProgressionState().CommittedBranch, FName(NAME_None));
    TestEqual(TEXT("Doctrine effects are cleared"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.0f, 0.0001f);
    // AND THE FARM STAYS CLOSED, which is the half worth keeping from the old
    // rule. Re-committing pays nothing, so respec-then-recommit cannot mint: the
    // grant is a function of level settled against a counter, not an event.
    TestTrue(TEXT("Re-committing succeeds"),
        Progression->CommitToBranch(TEXT("Doctrine.Swift.Kinetic"), CommitFailure));
    TestEqual(TEXT("...and pays nothing, so eight never becomes sixteen"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),
        UBreakerProgressionLibrary::DoctrinePointGrant);
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
    // SIGHTLINE ALONE, because Long Lens's damage line is now gated on Aiming
    // (Progression.AxisOverlap: a doctrine may not author a magnitude on a
    // generic damage pool unconditionally). Standing there not aiming, Core's
    // +4% is the whole of it.
    TestEqual(TEXT("An unaimed build gets only Core's unconditional damage line"), Stats.DamageMultiplier, 1.04f, 0.0001f);
    // AND THE ADDITIVITY THIS ASSERTION EXISTS FOR, which changing the number
    // alone would have quietly dropped: down sights, Sightline's +4% and Long
    // Lens's +3% across two ranks land in ONE bucket -- 4 + 3 + 3 = 10, not
    // 1.04 x 1.03 x 1.03. Across nodes AND across ranks, which is the whole
    // claim, and it needs both lines live to be worth making.
    FBreakerBuildConditionState Aiming;
    Aiming.Set(EBreakerBuildCondition::Aiming, true);
    const FBreakerNodeStats Aimed = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks, nullptr, Aiming);
    TestEqual(TEXT("Increased damage stacks additively across nodes and ranks"), Aimed.DamageMultiplier, 1.10f, 0.0001f);
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

// ---------------------------------------------------------------------------
// Multi-rank More validator (owner ruling 2026-08-16). Rank never scales a
// More — AggregateStats refuses to multiply one by rank — so a node with
// MaxRank > 1 authoring a MorePercent effect is authored nonsense: it
// promises ranks it cannot pay. UBreakerProgressionComponent::
// IsNodeMoreAuthoringLegal is the static rule; this test runs it over EVERY
// registered tree so an offender fails red at authoring time, and proves the
// validator itself bites on a synthetic offender.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMultiRankMoreValidatorTest,
    "RiorsEdge.Progression.MultiRankMoreValidator",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMultiRankMoreValidatorTest::RunTest(const FString& Parameters)
{
    // Every registered tree passes today, and must keep passing.
    int32 NodesScanned = 0;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!TestNotNull(TEXT("Registered tree is valid"), Tree)) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            ++NodesScanned;
            FString Reason;
            if (!UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Node, &Reason))
            {
                AddError(FString::Printf(TEXT("Multi-rank More: %s"), *Reason));
            }
        }
    }
    TestTrue(TEXT("The scan actually walked the registered content"), NodesScanned > 100);

    // The validator bites: a synthetic node with MaxRank 2 and a MorePercent
    // effect fails, with a reason naming the node.
    UBreakerProgressionNode* Offender = NewObject<UBreakerProgressionNode>();
    Offender->NodeId = TEXT("Test.Synthetic.MultiRankMore");
    Offender->MaxRank = 2;
    FBreakerNodeEffect IllegalMore;
    IllegalMore.StatTarget = EBreakerNodeStatTarget::Damage;
    IllegalMore.StatBucket = EBreakerNodeStatBucket::MorePercent;
    IllegalMore.ValuePerRank = 25.0f;
    Offender->Effects.Add(IllegalMore);

    FString OffenderReason;
    TestFalse(TEXT("A MaxRank-2 node authoring a MorePercent effect is illegal"),
        UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Offender, &OffenderReason));
    TestTrue(TEXT("The refusal names the offending node"), OffenderReason.Contains(TEXT("Test.Synthetic.MultiRankMore")));

    // And the boundary holds in both directions: the same effect at MaxRank 1
    // is legal (that is every shipped keystone), and a multi-rank node with
    // no More is untouched by this rule.
    Offender->MaxRank = 1;
    TestTrue(TEXT("The same More at MaxRank 1 is legal (keystone shape)"),
        UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Offender));
    UBreakerProgressionNode* MultiRankIncreased = NewObject<UBreakerProgressionNode>();
    MultiRankIncreased->NodeId = TEXT("Test.Synthetic.MultiRankIncreased");
    MultiRankIncreased->MaxRank = 3;
    FBreakerNodeEffect LegalIncreased;
    LegalIncreased.StatTarget = EBreakerNodeStatTarget::Damage;
    LegalIncreased.StatBucket = EBreakerNodeStatBucket::IncreasedPercent;
    LegalIncreased.ValuePerRank = 5.0f;
    MultiRankIncreased->Effects.Add(LegalIncreased);
    TestTrue(TEXT("A multi-rank Increased node is untouched by the rule"),
        UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(MultiRankIncreased));
    return true;
}

#endif
