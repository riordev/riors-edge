#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// EVERY DOCTRINE KEYSTONE IS BUYABLE WITH THE POINTS THE GAME ACTUALLY GRANTS.
//
// This project has shipped unpurchasable keystones once already — six of them,
// for a milestone, with a green suite the whole time — because every test that
// touched them handed itself points the shipped configuration does not produce.
// CLAUDE.md states the rule that came out of it: never grant a test more than
// the game grants. This is that rule, asserted.
//
// The arithmetic is three shipped numbers and nothing else:
//
//   CornerstoneInvestmentGate   points that must already be SPENT IN THE TREE
//                               (BreakerProgressionTree.h:25, default 8)
//   keystone CostPerRank        what the keystone itself costs (3)
//   DoctrinePointGrant          the whole doctrine wallet (O111, 8)
//
// A keystone is reachable only when gate + cost <= grant. Nothing else in the
// purchase path can rescue it: the gate counts rank x CostPerRank summed over
// the tree (BreakerProgressionComponent.cpp:669), so the points spent reaching
// the gate are the same points that would have paid for the keystone.
//
// TWO ROUTES, DELIBERATELY. The arithmetic can be right while the purchase path
// refuses for some other reason, and the purchase path can be walked wrongly by
// a test. So this asserts the shipped constants AND drives the component's own
// CanPurchaseNode with a wallet holding exactly the grant.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDoctrineKeystoneReachableTest,
    "RiorsEdge.Progression.Doctrine.KeystoneIsReachable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDoctrineKeystoneReachableTest::RunTest(const FString& Parameters)
{
    const int32 Grant = UBreakerProgressionLibrary::DoctrinePointGrant;

    int32 Keystones = 0;
    TArray<FString> Unreachable;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (!Tree || Tree->Currency != EBreakerPointCurrency::DoctrinePoints) continue;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node || !Node->bCornerstone) continue;
            ++Keystones;

            // The gate the purchase path will actually apply, read the same way
            // CanPurchaseNode reads it rather than assumed to be the default.
            const int32 Gate = FMath::Max(Node->RequiredTreeInvestment, Tree->CornerstoneInvestmentGate);
            const int32 Needed = Gate + Node->CostPerRank;
            if (Needed > Grant)
            {
                Unreachable.Add(FString::Printf(TEXT("%s needs %d (gate %d + cost %d)"),
                    *Node->NodeId.ToString(), Needed, Gate, Node->CostPerRank));
            }
        }
    }

    // Fifteen doctrines, one keystone each. Asserted so an empty walk cannot
    // pass this test by having found nothing to check.
    TestEqual(TEXT("Every doctrine carries exactly one keystone"), Keystones, 15);
    TestEqual(*FString::Printf(TEXT("Every doctrine keystone is affordable inside the %d-point grant: %s"),
        Grant, Unreachable.Num() ? *FString::Join(Unreachable, TEXT("; ")) : TEXT("all reachable")),
        Unreachable.Num(), 0);

    // ---- ROUTE TWO: the game's own purchase path --------------------------
    // One doctrine, walked the way a player walks it: commit, then spend the
    // whole wallet on the cheapest legal nodes, then ask whether the keystone
    // can be bought. This is the assertion that would have caught the six
    // unpurchasable keystones, because it never grants itself a point the
    // commitment did not pay.
    const UBreakerProgressionTree* Kinetic = nullptr;
    for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
    {
        if (Tree && Tree->TreeId == FName(TEXT("Doctrine.Swift.Kinetic"))) { Kinetic = Tree; break; }
    }
    if (!TestNotNull(TEXT("Doctrine.Swift.Kinetic resolves"), Kinetic)) return false;

    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    FBreakerProgressionState State;
    State.PermanentClass = EBreakerClassId::Swift;
    State.CommittedBranch = Kinetic->TreeId;
    State.UnspentDoctrinePoints = Grant;
    Progression->LoadProgressionState(State);

    const UBreakerProgressionNode* Keystone = nullptr;
    for (const UBreakerProgressionNode* Node : Kinetic->Nodes)
    {
        if (Node && Node->bCornerstone) { Keystone = Node; break; }
    }
    if (!TestNotNull(TEXT("Kinetic carries a keystone"), Keystone)) return false;

    // Spend toward the gate on the cheapest nodes available, exactly as a
    // player racing to the keystone would. Whatever is left is what the
    // keystone has to be bought with.
    int32 Guard = 0;
    while (Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints) > 0 && Guard++ < 64)
    {
        bool bBought = false;
        for (const UBreakerProgressionNode* Node : Kinetic->Nodes)
        {
            if (!Node || Node->bCornerstone) continue;
            FText Ignored;
            if (Progression->PurchaseNode(Kinetic, Node->NodeId, Ignored)) { bBought = true; break; }
        }
        if (!bBought) break;
    }

    FText Reason;
    const bool bCanBuy = Progression->CanPurchaseNode(Kinetic, Keystone->NodeId, Reason);
    TestTrue(*FString::Printf(
        TEXT("After spending the whole %d-point grant toward it, the keystone is buyable (invested %d, unspent %d, refusal: '%s')"),
        Grant, Progression->GetTreeInvestment(Kinetic),
        Progression->GetUnspentPoints(EBreakerPointCurrency::DoctrinePoints), *Reason.ToString()),
        bCanBuy);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
