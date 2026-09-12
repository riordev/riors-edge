#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerCoreWheelMath.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionTree.h"
#include "Save/BreakerMissionContent.h"
#include "Save/BreakerQuestJournal.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFallbackTreeIntegrityTest,
    "RiorsEdge.Progression.FallbackTreeIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFallbackTreeIntegrityTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerProgressionTree*>& Trees = UBreakerProgressionLibrary::GetAllFallbackTrees();
    // Core, plus THREE branches for EVERY class: Swift (Class-Kits Â§1.3-1.5),
    // Caster (Â§2.3-2.5), and â€” authored 2026-08-16 under the owner's "do all
    // 5 classes" ruling â€” Gunsmith (Class-Kits-Gunsmith Â§4), Tank
    // (Class-Kits-Tank Â§3-5) and Support (Class-Kits-Support Â§4). The count
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

            // Cost grammar: 1 minor, 2 notable, 3 convergence, 5 keystone â€”
            // plus 0 for exactly the GRANTED node (O139): seeded, never
            // purchasable-for-free, and cost 0 is what makes its
            // respec-no-refund property arithmetic. Keyed to the one id so a
            // second cost-0 node cannot ride in under this exemption.
            const bool bGrantedNode = Node->NodeId == UBreakerProgressionComponent::SwiftGrantedDashNodeId;
            const bool bCostInGrammar = Node->CostPerRank == 1 || Node->CostPerRank == 2 || Node->CostPerRank == 3 || Node->CostPerRank == 5
                || (bGrantedNode && Node->CostPerRank == 0);
            TestTrue(*(Context + TEXT(" cost is in the 1/2/3/5 grammar (0 for the granted node alone)")), bCostInGrammar);

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

    const UBreakerProgressionTree* Core = UBreakerProgressionLibrary::GetCoreSliceTree();
    if (!TestNotNull(TEXT("Live Core"), Core)) return false;
    TestEqual(TEXT("Accepted replacement node count"), Core->Nodes.Num(), 187);
    TestEqual(TEXT("Every wedge is an entry"), Core->EntryNodeIds.Num(), 22);
    TestTrue(TEXT("Later entries require an owned neighboring wedge"), Core->bRestrictEntryToOwnedNeighbor);
    TestEqual(TEXT("Explicit ring order includes every wedge"), Core->CoreWedgeOrder.Num(), 22);
    int32 EdgeCount = 0, Offered = 0;
    TMap<EBreakerCoreNodeRole, int32> Roles;
    TSet<FString> Edges;
    for (const FBreakerNodeEdge& Edge : Core->AdjacencyEdges)
    {
        ++EdgeCount;
        TestNotNull(*(Edge.A.ToString() + TEXT(" (edge A) resolves")), Core->FindNode(Edge.A));
        TestNotNull(*(Edge.B.ToString() + TEXT(" (edge B) resolves")), Core->FindNode(Edge.B));
        TestTrue(TEXT("Graph has no self edge"), Edge.A != Edge.B);
        FString A = Edge.A.ToString(), B = Edge.B.ToString();
        if (A > B) Swap(A, B);
        const FString Key = A + TEXT("|") + B;
        TestFalse(TEXT("Each undirected edge is authored once"), Edges.Contains(Key));
        Edges.Add(Key);
    }
    TestEqual(TEXT("Eleven major graphs, eleven minor graphs and twenty-two ring edges"), EdgeCount, 242);
    for (int32 Index = 0; Index < Core->EntryNodeIds.Num(); ++Index)
    {
        const FName Entry = Core->EntryNodeIds[Index];
        const auto* Gateway = Core->FindNode(Entry);
        if (!TestNotNull(TEXT("Gateway resolves"), Gateway)) return false;
        TestTrue(TEXT("Every entry carries gateway role"), Gateway->CoreRole == EBreakerCoreNodeRole::Gateway);
        TestEqual(TEXT("Gateway order agrees with wedge order"), Gateway->Constellation, Core->CoreWedgeOrder[Index]);
        const FName Next = Core->EntryNodeIds[(Index + 1) % Core->EntryNodeIds.Num()];
        TestTrue(TEXT("Every neighboring gateway pair connects, including wrap"), Core->AdjacencyEdges.ContainsByPredicate(
            [&](const FBreakerNodeEdge& E) { return (E.A == Entry && E.B == Next) || (E.B == Entry && E.A == Next); }));
    }
    TestEqual(TEXT("Core spends Core Points"), Core->Currency, EBreakerPointCurrency::CorePoints);
    for (const UBreakerProgressionNode* Node : Core->Nodes)
    {
        const FString Context = Node->NodeId.ToString();
        TestFalse(*(Context + TEXT(" is not a retired travel bead")), Context.StartsWith(TEXT("Core.Travel.")));
        TestTrue(*(Context + TEXT(" retains an authored effect or rule identity")),
            Node->Effects.Num() > 0 || Node->GrantedTags.Num() > 0 || Node->GrantedAbilityIds.Num() > 0);
        TestTrue(TEXT("Every Core node has an explicit role"), Node->CoreRole != EBreakerCoreNodeRole::Legacy);
        ++Roles.FindOrAdd(Node->CoreRole);
        Offered += Node->MaxRank * Node->CostPerRank;
        const bool bRanked = Node->CoreRole == EBreakerCoreNodeRole::LaneMinor;
        TestEqual(TEXT("Only lane minors have three ranks"), Node->MaxRank, bRanked ? 3 : 1);
        TestEqual(TEXT("Core roles do not add hidden tree-investment gates"), Node->RequiredTreeInvestment, 0);
        TestEqual(TEXT("Only keystones require eighteen local points"), Node->RequiredConstellationInvestment,
            Node->CoreRole == EBreakerCoreNodeRole::Keystone ? 18 : 0);
        for (const auto& Requirement : Node->Prerequisites)
            TestEqual(TEXT("Onward travel opens at rank one"), Requirement.RequiredRank, 1);
        for (const auto& Group : Node->PrerequisiteGroups)
        {
            TestTrue(TEXT("Counted lane group has a reachable threshold"), Group.MinimumSatisfied > 0 && Group.MinimumSatisfied <= Group.Candidates.Num());
            for (const auto& Requirement : Group.Candidates)
            {
                TestNotNull(TEXT("Counted prerequisite belongs to this tree"), Core->FindNode(Requirement.NodeId));
                TestEqual(TEXT("Counted lane choice uses only rank one"), Requirement.RequiredRank, 1);
            }
        }
        if (Node->CoreRole == EBreakerCoreNodeRole::Convergence)
        {
            if (!TestEqual(TEXT("Convergence has one counted lane gate"), Node->PrerequisiteGroups.Num(), 1)) return false;
            TestEqual(TEXT("Convergence requires two distinct completed lanes"), Node->PrerequisiteGroups[0].MinimumSatisfied, 2);
            TestEqual(TEXT("Minor/major convergence price follows its two/three lanes"), Node->CostPerRank, Node->PrerequisiteGroups[0].Candidates.Num());
        }
        else
        {
            const int32 Cost = Node->CoreRole == EBreakerCoreNodeRole::Keystone ? 5 : Node->CoreRole == EBreakerCoreNodeRole::LaneNotable ? 2 : 1;
            TestEqual(TEXT("Each role has its exact accepted price"), Node->CostPerRank, Cost);
        }
    }
    TestEqual(TEXT("Accepted full-rank offered budget"), Offered, 429);
    TestEqual(TEXT("Gateway count"), Roles.FindRef(EBreakerCoreNodeRole::Gateway), 22);
    TestEqual(TEXT("Ranked minor count"), Roles.FindRef(EBreakerCoreNodeRole::LaneMinor), 55);
    TestEqual(TEXT("Notable count"), Roles.FindRef(EBreakerCoreNodeRole::LaneNotable), 55);
    TestEqual(TEXT("Link count"), Roles.FindRef(EBreakerCoreNodeRole::Link), 22);
    TestEqual(TEXT("Convergence count"), Roles.FindRef(EBreakerCoreNodeRole::Convergence), 22);
    TestEqual(TEXT("Keystone count"), Roles.FindRef(EBreakerCoreNodeRole::Keystone), 11);

    // SWIFT BRANCH SIZE: thirteen each. O272 makes a doctrine six pairs
    // (travel -> impactful, twelve nodes) plus one root outside the pairs —
    // Kinetic's granted Longstride, Marksman's Deadeye, Frenzy's Feed. The
    // counts are exact so a node added or lost announces itself here.
    TestEqual(TEXT("Frenzy ships thirteen nodes: six pairs plus Feed"), UBreakerProgressionLibrary::GetSwiftFrenzyTree()->Nodes.Num(), 13);
    TestEqual(TEXT("Kinetic ships thirteen nodes: six pairs plus Longstride"), UBreakerProgressionLibrary::GetSwiftKineticTree()->Nodes.Num(), 13);
    TestEqual(TEXT("Marksman ships thirteen nodes: six pairs plus Deadeye"), UBreakerProgressionLibrary::GetSwiftMarksmanTree()->Nodes.Num(), 13);

    // THE NINE REWRITES (Class-Kits F9-F11, K9-K11, M9-M11), BY ID. Under
    // O272 each is the impactful half of a pair: tier 1, single rank, one
    // point, unlocked by its travel and nothing else. The keystone is the
    // only tier-4 node in a Swift branch and the only one that carries an
    // investment gate; the tier no longer identifies a rewrite, so the
    // rewrites are named rather than counted.
    struct FBreakerSwiftRewriteRow
    {
        const UBreakerProgressionTree* Tree;
        TArray<const TCHAR*> Rewrites;
    };
    const TArray<FBreakerSwiftRewriteRow> SwiftRewrites = {
        { UBreakerProgressionLibrary::GetSwiftFrenzyTree(),
            { TEXT("Swift.Frenzy.SecondWind"), TEXT("Swift.Frenzy.RedlineTrigger"), TEXT("Swift.Frenzy.NoSafety") } },
        { UBreakerProgressionLibrary::GetSwiftKineticTree(),
            { TEXT("Swift.Kinetic.MomentumShield"), TEXT("Swift.Kinetic.SpendToLive"), TEXT("Swift.Kinetic.NoGround") } },
        { UBreakerProgressionLibrary::GetSwiftMarksmanTree(),
            { TEXT("Swift.Marksman.Reserve"), TEXT("Swift.Marksman.Overpenetration"), TEXT("Swift.Marksman.CalledShot") } },
    };
    for (const FBreakerSwiftRewriteRow& Row : SwiftRewrites)
    {
        int32 Keystones = 0;
        for (const UBreakerProgressionNode* Node : Row.Tree->Nodes)
        {
            const FString Context = Node->NodeId.ToString();
            if (Node->bCornerstone)
            {
                ++Keystones;
                TestEqual(*(Context + TEXT(" keystone alone sits at tier 4")), Node->Tier, 4);
                TestEqual(*(Context + TEXT(" keystone costs 1")), Node->CostPerRank, 1);
            }
            else
            {
                TestEqual(*(Context + TEXT(" non-keystone sits at tier 1")), Node->Tier, 1);
            }
        }
        TestEqual(TEXT("Each Swift branch ships exactly one keystone"), Keystones, 1);

        for (const TCHAR* RewriteId : Row.Rewrites)
        {
            const UBreakerProgressionNode* Node = Row.Tree->FindNode(RewriteId);
            const FString Context(RewriteId);
            if (!TestNotNull(*(Context + TEXT(" is authored")), Node)) continue;
            TestFalse(*(Context + TEXT(" rewrite is not the keystone")), Node->bCornerstone);
            TestEqual(*(Context + TEXT(" rewrite is single rank")), Node->MaxRank, 1);
            TestEqual(*(Context + TEXT(" rewrite costs 1")), Node->CostPerRank, 1);
            TestEqual(*(Context + TEXT(" rewrite sits at tier 1")), Node->Tier, 1);
            // O3: a class-layer More may live only on a branch keystone, and
            // all three of Swift's are already spent (O95: none, in fact). A
            // More on a rewrite would be a fourth against a budget of three.
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                TestTrue(*(Context + TEXT(" rewrite authors no More multiplier")),
                    Effect.StatBucket != EBreakerNodeStatBucket::MorePercent);
            }
            // A rewrite may author ONLY loop-economy lines (decay / cost),
            // never a damage or combat stat — that would be a different node
            // with a different fantasy, and a content decision, not a
            // refactor. The trio's decay downsides ARE such lines:
            // ClassResourceDecay through the valve, AbilityCost for No
            // Safety's discount half, both Class-Kits-transcribed.
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                TestTrue(*(Context + TEXT(" rewrite authors only loop-economy lines (ClassResourceDecay/AbilityCost)")),
                    Effect.StatTarget == EBreakerNodeStatTarget::ClassResourceDecay
                    || Effect.StatTarget == EBreakerNodeStatTarget::AbilityCost);
            }
            TestTrue(*(Context + TEXT(" rewrite carries its rule as a tag")), Node->GrantedTags.Num() > 0);
            // A rewrite with no prerequisite is a rewrite of nothing: it is
            // the impactful half of a pair and follows exactly its travel.
            // The generic loop above already proves prerequisites resolve
            // inside the same tree and sit at or below this node's tier.
            TestEqual(*(Context + TEXT(" rewrite follows exactly one travel")), Node->Prerequisites.Num(), 1);
        }
    }

    // Core More placement follows explicit roles. Minor convergences cost
    // two, so price alone cannot identify a legal convergence.
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
                if (Node->Currency == EBreakerPointCurrency::CorePoints)
                    TestTrue(*(Context + TEXT(" authors More only on an explicit convergence or keystone")),
                        Node->CoreRole == EBreakerCoreNodeRole::Convergence || Node->CoreRole == EBreakerCoreNodeRole::Keystone);
                else
                    TestTrue(*(Context + TEXT(" preserves branch More placement")), Node->CostPerRank >= 3);
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
    // Owned, because the Core respec at the foot of this test is paid
    // through a wallet on the owner (O213), and an ownerless component holds
    // no wallet to pay from.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(NewObject<AActor>());
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
    TestFalse(TEXT("Prerequisite is enforced"), Progression->PurchaseNode(Core, TEXT("Core.Precision.CalledShot"), Failure));

    TestTrue(TEXT("Gateway purchase succeeds"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));
    TestEqual(TEXT("Gateway rank is recorded"), Progression->GetNodeRank(TEXT("Core.Precision.Sightline"), EBreakerPointCurrency::CorePoints), 1);
    TestEqual(TEXT("Cost is spent from core points"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), 11);
    TestEqual(TEXT("Tree investment tracks spend"), Progression->GetTreeInvestment(Core), 1);
    TestFalse(TEXT("Rank cap is enforced"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Sightline"), Failure));

    TestTrue(TEXT("Gateway opens its first ranked lane"), Progression->CanPurchaseNode(Core, TEXT("Core.Precision.Angle"), Failure));
    TestFalse(TEXT("Notable still requires its minor"), Progression->CanPurchaseNode(Core, TEXT("Core.Precision.CalledShot"), Failure));
    TestTrue(TEXT("Clockwise neighbor is reachable"), Progression->CanPurchaseNode(Core, TEXT("Core.Vector.Line"), Failure));
    TestTrue(TEXT("Wraparound neighbor is reachable"), Progression->CanPurchaseNode(Core, TEXT("Core.Threat.Presence"), Failure));
    TestFalse(TEXT("Non-neighbor gateway is refused"), Progression->CanPurchaseNode(Core, TEXT("Core.Ballistics.WeightOfIt"), Failure));
    TestTrue(TEXT("Angle rank one purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Angle"), Failure));
    TestTrue(TEXT("Rank one opens Called Shot"), Progression->PurchaseNode(Core, TEXT("Core.Precision.CalledShot"), Failure));
    TestFalse(TEXT("One completed lane cannot open convergence"), Progression->CanPurchaseNode(Core, TEXT("Core.Precision.Fixate"), Failure));
    TestTrue(TEXT("Cadence rank one purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Cadence"), Failure));
    TestTrue(TEXT("Second lane notable purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.TriggerDiscipline"), Failure));
    TestTrue(TEXT("Two distinct notables open convergence"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Fixate"), Failure));
    TestEqual(TEXT("Rank-one convergence route costs ten"), Progression->GetTreeInvestment(Core), 10);
    TestFalse(TEXT("Convergence does not bypass the local keystone gate"), Progression->CanPurchaseNode(Core, TEXT("Core.Precision.Deadeye"), Failure));
    TestTrue(TEXT("Optional Angle rank two purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Angle"), Failure));
    TestTrue(TEXT("Optional Angle rank three purchases"), Progression->PurchaseNode(Core, TEXT("Core.Precision.Angle"), Failure));
    TestFalse(TEXT("Rank four is refused"), Progression->CanPurchaseNode(Core, TEXT("Core.Precision.Angle"), Failure));
    TestEqual(TEXT("Three Angle ranks aggregate crit chance"), Progression->GetNodeStats().CriticalChanceBonus, 0.06f, 0.0001f);
    TestEqual(TEXT("Sightline and Trigger Discipline aggregate critical damage"), Progression->GetNodeStats().CriticalMultiplierBonus, 0.31f, 0.0001f);

    // The empty Doctrine wallet cannot purchase; commitment selects the board
    // while completed mission benchmarks supply its points.
    FText CommitFailure;
    TestFalse(TEXT("A doctrine node is unaffordable before commitment"),
        Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestTrue(TEXT("Committing to Kinetic succeeds"),
        Progression->CommitToBranch(TEXT("Doctrine.Swift.Kinetic"), CommitFailure));
    // COMMITMENT PAYS NOTHING. It used to hand over the whole eight; the pool
    // is now earned at four benchmarks, so committing chooses WHERE points go
    // and the benchmarks decide WHEN they exist. This fixture reaches the
    // level prerequisite through XP, then settles the completed mission journal.
    TestEqual(TEXT("Committing pays no points by itself"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    // Reach the last benchmark the way the game does -- XP, not a setter.
    const FBreakerExperienceCurve BenchmarkCurve;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(
        UBreakerProgressionLibrary::CorePointCapLevel, BenchmarkCurve) - Progression->GetTotalExperience());
    // Completed-campaign journal fixture, not a playthrough claim. The native
    // finale probe earns the physical actions; this tests spending their pool.
    TestEqual(TEXT("Level cap alone pays no Doctrine"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 0);
    TestEqual(TEXT("Fixture reaches actual level fifty"), Progression->GetCharacterLevel(), 50);
    UBreakerQuestJournal* Journal = NewObject<UBreakerQuestJournal>();
    FBreakerQuestFlagSet CompletedCampaign;
    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
            for (FName Flag : UBreakerMissionLibrary::BeatCompletionFlags(Beat)) CompletedCampaign.Add(Flag);
    CompletedCampaign.Add(TEXT("Quest.Finale.Seal")); // One explicit ending in this completed-state fixture.
    Journal->RestoreFrom(CompletedCampaign);
    TestEqual(TEXT("Authored completed benchmarks owe exactly eight"), UBreakerMissionLibrary::DoctrinePointEntitlement(Journal->GetState()), 8);
    Progression->SettleDoctrineEntitlement(Journal->GetState());
    TestEqual(TEXT("Settlement records exactly eight paid"), Progression->GetProgressionState().LevelDoctrinePointsGranted, 8);
    Progression->SettleDoctrineEntitlement(Journal->GetState());
    TestEqual(TEXT("Replaying the same journal cannot pay sixteen"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), 8);
    TestEqual(TEXT("Reaching the last benchmark pays the whole pool"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),
        UBreakerProgressionLibrary::DoctrinePointGrant);

    const int32 CoreBeforeDoctrine = Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints);
    TestTrue(TEXT("A doctrine node purchases from doctrine points"), Progression->PurchaseNode(Kinetic, TEXT("Swift.Kinetic.Carry"), Failure));
    TestEqual(TEXT("Doctrine points are spent"), Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints),
        UBreakerProgressionLibrary::DoctrinePointGrant - 1);
    TestEqual(TEXT("...and core points are untouched"), Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints), CoreBeforeDoctrine);
    // O272: Carry is single rank at the old two-rank total (24).
    TestEqual(TEXT("Slide speed reflects the class node"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.24f, 0.0001f);

    // Respec clears effects and refunds every point of that currency. The
    // Forge gate is Doctrine's (O213); the Core respec is a level rule and a
    // wallet, exercised by RiorsEdge.Progression.CoreRespec.FreeThenRiftglass.
    // This fixture is at the cap, past the free level, with no wallet: the
    // Core respec is bought through the Forge path, which routes to the same
    // rule, so the ranks stand until the price is paid.
    TestFalse(TEXT("Doctrine respec away from a Forge is rejected"), Progression->RespecAtForge(EBreakerPointCurrency::DoctrinePoints, false, Failure));
    TestFalse(TEXT("A Core respec past the free level with no wallet is refused"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("A refused Core respec leaves the ranks standing"), Progression->GetNodeRank(TEXT("Core.Precision.Sightline"), EBreakerPointCurrency::CorePoints), 1);
    // Pay the price the way the game pays it: through the owner's wallet.
    UBreakerEquipmentComponent* Wallet = NewObject<UBreakerEquipmentComponent>(Progression->GetOwner());
    Wallet->GrantForgeCurrency(BreakerCoreRespecCost(Progression->GetCharacterLevel()).Amount);
    TestTrue(TEXT("The Core respec succeeds once the price is held"), Progression->RespecCore(Failure));
    TestEqual(TEXT("The Core respec debits exactly its price"), Wallet->GetForgeWallet().Get(), 0);
    // Fifty, not the slice's twelve: this fixture now levels to the cap to
    // reach the doctrine benchmarks, and Core pays one per level on the way.
    TestEqual(TEXT("Core points are fully refunded"),
        Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints),
        UBreakerProgressionLibrary::CorePointCapLevel);
    TestEqual(TEXT("Core ranks are cleared"), Progression->GetNodeRank(TEXT("Core.Precision.Sightline"), EBreakerPointCurrency::CorePoints), 0);
    TestEqual(TEXT("Core effects are cleared"), Progression->GetNodeStats().CriticalChanceBonus, 0.0f, 0.0001f);
    TestEqual(TEXT("Class allocation survives a core respec"), Progression->GetNodeStats().SlideSpeedMultiplier, 1.24f, 0.0001f);

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
    // grant is mission entitlement settled against a counter, not commitment.
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

    // This isolated aggregation fixture supplies real authored rows directly;
    // paid reachability is tested above, separately from aggregation arithmetic.
    TArray<FBreakerNodeRank> Ranks;
    Ranks.Add({TEXT("Core.Precision.Angle"), 2});
    Ranks.Add({TEXT("Core.Precision.Sightline"), 1});
    Ranks.Add({TEXT("Core.Bulwark.Read"), 1});
    Ranks.Add({TEXT("Core.Bulwark.Evade"), 1});
    Ranks.Add({TEXT("Core.Aegis.Footing"), 1});
    Ranks.Add({TEXT("Core.Velocity.Grind"), 1});
    Ranks.Add({TEXT("Core.Affliction.Fester"), 1});
    Ranks.Add({TEXT("Swift.Kinetic.AirWork"), 1});
    Ranks.Add({TEXT("Swift.Marksman.LongLens"), 2});

    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    TestEqual(TEXT("Flat crit chance sums into a fraction"), Stats.CriticalChanceBonus, 0.04f, 0.0001f);
    TestEqual(TEXT("Flat crit damage scales with rank"), Stats.CriticalMultiplierBonus, 0.42f, 0.0001f);
    TestEqual(TEXT("Block chance converts to a fraction"), Stats.BlockChanceBonus, 0.05f, 0.0001f);
    TestEqual(TEXT("Dodge chance converts to a fraction"), Stats.DodgeChanceBonus, 0.03f, 0.0001f);
    TestEqual(TEXT("Health is a flat bonus"), Stats.BonusHealth, 60.0f, 0.0001f);
    TestEqual(TEXT("Increased move speed becomes a multiplier"), Stats.MoveSpeedMultiplier, 1.08f, 0.0001f);
    TestEqual(TEXT("Increased air control becomes a multiplier"), Stats.AirControlMultiplier, 1.12f, 0.0001f);
    TestEqual(TEXT("Increased DoT reaches the aggregate"), Stats.DamageOverTimeMultiplier, 1.25f, 0.0001f);
    // None of these Core rows authors shared Increased Damage. Long Lens
    // contributes its existing conditional lane only while aiming.
    TestEqual(TEXT("Unaimed shared damage remains neutral"), Stats.DamageMultiplier, 1.0f, 0.0001f);
    FBreakerBuildConditionState Aiming;
    Aiming.Set(EBreakerBuildCondition::Aiming, true);
    const FBreakerNodeStats Aimed = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks, nullptr, Aiming);
    TestEqual(TEXT("Increased damage stacks additively across ranks"), Aimed.DamageMultiplier, 1.06f, 0.0001f);
    TestEqual(TEXT("Untouched multipliers stay neutral"), Stats.SlideSpeedMultiplier, 1.0f, 0.0001f);

    TArray<FBreakerNodeRank> VerbRanks;
    VerbRanks.Add({TEXT("Core.Bulwark.Read"), 3});
    const FBreakerNodeStats ReadStats = UBreakerProgressionComponent::AggregateStats(Nodes, VerbRanks);
    TestFalse(TEXT("Read without Parry grants no verb"), ReadStats.GrantedTags.HasTag(BreakerNodeTags::Verb_Parry.GetTag()));
    TestEqual(TEXT("Read clamps to its single five-percent block rank"), ReadStats.BlockChanceBonus, 0.05f, 0.0001f);
    TestEqual(TEXT("Read no longer supplies an unrelated weapon-damage line"), ReadStats.DamageMultiplier, 1.0f, 0.0001f);
    VerbRanks.Add({TEXT("Core.Bulwark.Parry"), 1});
    const FBreakerNodeStats ParryStats = UBreakerProgressionComponent::AggregateStats(Nodes, VerbRanks);
    TestTrue(TEXT("Parry publishes its actual verb"), ParryStats.GrantedTags.HasTag(BreakerNodeTags::Verb_Parry.GetTag()));

    // Ranks beyond the node's cap cannot inflate the aggregate.
    TArray<FBreakerNodeRank> OverRanks;
    OverRanks.Add({TEXT("Swift.Marksman.LongLens"), 9});   // MaxRank 1, +36 crit damage (O272: single rank at the old total)
    const FBreakerNodeStats ClampedStats = UBreakerProgressionComponent::AggregateStats(Nodes, OverRanks);
    TestEqual(TEXT("Rank is clamped to the node's max"), ClampedStats.CriticalMultiplierBonus, 0.36f, 0.0001f);

    // Unknown ids in a loaded save are ignored, not fatal.
    TArray<FBreakerNodeRank> StaleRanks;
    StaleRanks.Add({TEXT("Core.Removed.Node"), 4});
    const FBreakerNodeStats StaleStats = UBreakerProgressionComponent::AggregateStats(Nodes, StaleRanks);
    TestEqual(TEXT("Unknown node ids contribute nothing"), StaleStats.CriticalChanceBonus, 0.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// Multi-rank More validator (owner ruling 2026-08-16). Rank never scales a
// More â€” AggregateStats refuses to multiply one by rank â€” so a node with
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

// ---------------------------------------------------------------------------
// Replacement Core has no legacy target-rider More. Its two conditional
// weapon Mores use selected source scopes in the joint strongest-three pool.
// Keep the existing test identity while checking both sides of that boundary.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOneHitTimeMoreTest,
    "RiorsEdge.Progression.TreeContent.OneHitTimeMore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOneHitTimeMoreTest::RunTest(const FString& Parameters)
{
    TArray<FString> Found, ScopedWeaponMores;
    int32 EffectsWalked = 0;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            for (const FBreakerNodeEffect& Effect : Node->Effects)
            {
                ++EffectsWalked;
                if (Effect.StatBucket == EBreakerNodeStatBucket::MorePercent
                    && (Effect.StatTarget == EBreakerNodeStatTarget::WeaponCriticalDamage || Effect.StatTarget == EBreakerNodeStatTarget::WeaponBeyondFirstDamage))
                {
                    ScopedWeaponMores.Add(Node->NodeId.ToString());
                    TestFalse(TEXT("Selected source scope is not an extra target rider"), Effect.RequiresTargetState());
                }
                if (Effect.StatBucket == EBreakerNodeStatBucket::MorePercent && Effect.RequiresTargetState())
                {
                    Found.Add(Node->NodeId.ToString());
                }
            }
        }
    }
    TestTrue(TEXT("The walk saw real content"), EffectsWalked > 100);
    TestEqual(TEXT("Replacement authors no legacy hit-time rider More"), Found.Num(), 0);
    TestEqual(TEXT("Exactly two selected conditional weapon scopes"), ScopedWeaponMores.Num(), 2);
    TestTrue(TEXT("Critical scope belongs to Fixate"), ScopedWeaponMores.Contains(TEXT("Core.Precision.Fixate")));
    TestTrue(TEXT("Later-target scope belongs to Splinter"), ScopedWeaponMores.Contains(TEXT("Core.Vector.Splinter")));
    return true;
}

#endif
