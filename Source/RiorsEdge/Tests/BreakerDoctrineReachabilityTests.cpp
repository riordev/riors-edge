#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// EVERY TREE'S DEEPEST NODE IS BUYABLE ON THAT TREE'S SHIPPED GRANT.
//
// THE RULE UNDERNEATH THIS TEST: a gate and the budget it gates against are ONE
// NUMBER IN TWO PLACES. Twice now the budget moved and the gate keyed to it did
// not, and both times the result was content no player could buy while the
// suite stayed green:
//
//   Six branch keystones, for a milestone. Every test that touched them handed
//   itself points the shipped configuration does not produce.
//
//   All fifteen doctrine keystones, found 2026-08-23. CornerstoneInvestmentGate
//   was 8 points spent in the tree and the keystone cost 3, against O111's
//   8-point wallet — 11 needed out of 8, fifteen times over.
//
// Removing the doctrine gate fixes the second instance. It does not fix the
// CAUSE, which is that nothing checked. This test is the thing that checks, and
// it is deliberately written to cover trees that do not exist yet: Core's atlas
// will have its own gates and its own budget, and the assertion should be
// standing before the twelve wheels are, not written after they miss.
//
// Note the name. RiorsEdge.Abilities.KeystoneReachability already existed and
// asserts something different — that every keystone ABILITY TAG is granted by
// some cornerstone node. It was green throughout, because a tag can be sited on
// a node nobody can afford. Two kinds of reachability, and the word alone does
// not say which.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTreeDepthIsReachableTest,
    "RiorsEdge.Progression.TreeDepthIsReachable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    int32 BreakerGrantFor(const UBreakerProgressionTree* Tree)
    {
        return Tree->Currency == EBreakerPointCurrency::CorePoints
            ? UBreakerProgressionLibrary::CorePointBudget
            : UBreakerProgressionLibrary::DoctrinePointGrant;
    }

    // Points that must be spent inside this tree before the node may be bought:
    // its own investment gate, and its prerequisite closure. Whichever is
    // larger is the real floor — prerequisites COUNT toward the gate, so they
    // are not additive with it, and treating them as additive would report a
    // reachable node as unreachable.
    int32 BreakerPriorSpendFor(const UBreakerProgressionTree* Tree, const UBreakerProgressionNode* Node,
                               TSet<FName>& Visiting)
    {
        const int32 Gate = FMath::Max(Node->RequiredTreeInvestment,
            Node->bCornerstone ? Tree->CornerstoneInvestmentGate : 0);

        int32 Closure = 0;
        if (!Visiting.Contains(Node->NodeId))
        {
            Visiting.Add(Node->NodeId);
            for (const FBreakerNodePrerequisite& Prereq : Node->Prerequisites)
            {
                const UBreakerProgressionNode* Req = Tree->FindNode(Prereq.NodeId);
                if (!Req) continue;
                // The ranks the prerequisite demands, plus whatever THAT node
                // needed before it could be bought.
                Closure += Prereq.RequiredRank * Req->CostPerRank;
                Closure += BreakerPriorSpendFor(Tree, Req, Visiting);
            }
        }
        return FMath::Max(Gate, Closure);
    }
}

bool FBreakerTreeDepthIsReachableTest::RunTest(const FString& Parameters)
{
    int32 TreesWalked = 0;
    TArray<FString> Unreachable;

    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree) continue;
        ++TreesWalked;
        const int32 Grant = BreakerGrantFor(Tree);

        int32 Offered = 0;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (Node) Offered += FMath::Max(1, Node->MaxRank) * Node->CostPerRank;
        }

        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            TSet<FName> Visiting;
            const int32 Prior = BreakerPriorSpendFor(Tree, Node, Visiting);
            const int32 Needed = Prior + Node->CostPerRank;
            if (Needed > Grant)
            {
                Unreachable.Add(FString::Printf(TEXT("%s needs %d of %d"),
                    *Node->NodeId.ToString(), Needed, Grant));
            }
            // A gate the tree itself cannot fund is the same defect wearing a
            // different number: the budget could be infinite and the node would
            // still never unlock, because there is nothing left to spend on.
            else if (Prior > Offered - Node->CostPerRank)
            {
                Unreachable.Add(FString::Printf(TEXT("%s gates at %d but its tree only offers %d"),
                    *Node->NodeId.ToString(), Prior, Offered));
            }
        }
    }

    TestTrue(TEXT("There are trees to walk at all"), TreesWalked >= 16);
    TestEqual(*FString::Printf(TEXT("Every node in every tree is affordable on its own tree's grant: %s"),
        Unreachable.Num() ? *FString::Join(Unreachable, TEXT("; ")) : TEXT("all reachable")),
        Unreachable.Num(), 0);

    // ---- ROUTE TWO: the game's own purchase path --------------------------
    // The arithmetic above can be right while the purchase path refuses for a
    // reason the arithmetic does not model, so one tree is walked the way a
    // player walks it: commit, then buy toward the keystone, checking at every
    // step whether it has become affordable rather than spending the wallet
    // first and asking afterwards.
    const UBreakerProgressionTree* Kinetic = nullptr;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (Tree && Tree->TreeId == FName(TEXT("Doctrine.Swift.Kinetic"))) { Kinetic = Tree; break; }
    }
    if (!TestNotNull(TEXT("Doctrine.Swift.Kinetic resolves"), Kinetic)) return false;

    const UBreakerProgressionNode* Keystone = nullptr;
    for (const UBreakerProgressionNode* Node : Kinetic->Nodes)
    {
        if (Node && Node->bCornerstone) { Keystone = Node; break; }
    }
    if (!TestNotNull(TEXT("Kinetic carries a keystone"), Keystone)) return false;

    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    FBreakerProgressionState State;
    State.PermanentClass = EBreakerClassId::Swift;
    State.CommittedBranch = Kinetic->TreeId;
    State.UnspentDoctrinePoints = UBreakerProgressionLibrary::DoctrinePointGrant;
    Progression->LoadProgressionState(State);

    FText Reason;
    int32 Guard = 0;
    while (!Progression->CanPurchaseNode(Kinetic, Keystone->NodeId, Reason) && Guard++ < 64)
    {
        // Cheapest first, which is what a player racing a gate does and what
        // keeps the walk from stranding points it cannot spend.
        const UBreakerProgressionNode* Best = nullptr;
        for (const UBreakerProgressionNode* Node : Kinetic->Nodes)
        {
            if (!Node || Node->bCornerstone) continue;
            FText Ignored;
            if (!Progression->CanPurchaseNode(Kinetic, Node->NodeId, Ignored)) continue;
            if (!Best || Node->CostPerRank < Best->CostPerRank) Best = Node;
        }
        if (!Best) break;
        FText Ignored;
        Progression->PurchaseNode(Kinetic, Best->NodeId, Ignored);
    }

    const bool bCanBuy = Progression->CanPurchaseNode(Kinetic, Keystone->NodeId, Reason);
    TestTrue(*FString::Printf(
        TEXT("Walking Kinetic on its %d-point grant reaches the keystone (invested %d, unspent %d, refusal: '%s')"),
        UBreakerProgressionLibrary::DoctrinePointGrant, Progression->GetTreeInvestment(Kinetic),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), *Reason.ToString()),
        bCanBuy);

    // And it must still COST something to get there. A keystone affordable from
    // a standing start would mean the gate had been removed rather than
    // repriced, which is the opposite failure and just as quiet.
    TestTrue(TEXT("...and only after real investment, not from a standing start"),
        Progression->GetTreeInvestment(Kinetic) > 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
