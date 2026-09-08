#include "Misc/AutomationTest.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreGatewayRuntimeTest,
    "RiorsEdge.Progression.CoreGatewayRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreGatewayRuntimeTest::RunTest(const FString& Parameters)
{
    auto* Progression = NewObject<UBreakerProgressionComponent>();
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10, Progression->ExperienceCurve));
    const int32 Budget = Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints);
    TestEqual(TEXT("Actual earned budget"), Budget, 10);
    auto* Tree = NewObject<UBreakerProgressionTree>();
    Tree->TreeId = TEXT("Test.Core.Gateways");
    TestFalse(TEXT("Existing trees opt out by default"), Tree->bRestrictEntryToOwnedNeighbor);
    Tree->bRestrictEntryToOwnedNeighbor = true;
    auto MakeNode = [](UBreakerProgressionTree* Owner, const TCHAR* Id)
    {
        auto* Node = NewObject<UBreakerProgressionNode>(Owner);
        Node->NodeId = Id; Node->Currency = EBreakerPointCurrency::CorePoints;
        Node->MaxRank = 2; Node->CostPerRank = 1;
        Owner->Nodes.Add(Node); return Node;
    };
    auto* A = MakeNode(Tree, TEXT("Test.Gateway.A"));
    auto* B = MakeNode(Tree, TEXT("Test.Gateway.B"));
    auto* C = MakeNode(Tree, TEXT("Test.Gateway.C"));
    auto* D = MakeNode(Tree, TEXT("Test.Gateway.D"));
    auto* Interior = MakeNode(Tree, TEXT("Test.Gateway.Interior"));
    Tree->EntryNodeIds = {A->NodeId, B->NodeId, C->NodeId, D->NodeId};
    auto Edge = [&](FName First, FName Second)
    { FBreakerNodeEdge Value; Value.A = First; Value.B = Second; Tree->AdjacencyEdges.Add(Value); };
    Edge(A->NodeId, B->NodeId); Edge(B->NodeId, C->NodeId);
    Edge(C->NodeId, D->NodeId); Edge(D->NodeId, A->NodeId);
    Edge(C->NodeId, Interior->NodeId); Edge(Interior->NodeId, A->NodeId);
    FText Reason;
    auto Buy = [&](UBreakerProgressionTree* InTree, UBreakerProgressionNode* Node)
    {
        const bool Result = Progression->PurchaseNode(InTree, Node->NodeId, Reason);
        TestTrue(FString::Printf(TEXT("Actual purchase %s: %s"), *Node->NodeId.ToString(), *Reason.ToString()), Result);
        return Result;
    };
    if (!Buy(Tree, C)) return false;
    TestFalse(TEXT("Opposite gateway cannot be a second free start"), Progression->CanPurchaseNode(Tree, A->NodeId, Reason));
    TestTrue(TEXT("Clockwise neighbor reachable"), Progression->CanPurchaseNode(Tree, B->NodeId, Reason));
    TestTrue(TEXT("Counterclockwise neighbor reachable"), Progression->CanPurchaseNode(Tree, D->NodeId, Reason));
    if (!Buy(Tree, C) || !Buy(Tree, Interior)) return false;
    TestEqual(TEXT("Owned gateway can gain another rank without neighboring gateway"), Progression->GetNodeRank(C->NodeId, Tree->Currency), 2);
    TestFalse(TEXT("Owned interior cannot substitute for gateway adjacency"), Progression->CanPurchaseNode(Tree, A->NodeId, Reason));
    if (!Buy(Tree, D) || !Buy(Tree, A)) return false;
    TestEqual(TEXT("Wrap edge D to A permits purchase"), Progression->GetNodeRank(A->NodeId, Tree->Currency), 1);
    if (!TestTrue(TEXT("Actual free low-level full respec"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec restores exact earned budget"), Progression->GetUnspentPoints(Tree->Currency), Budget);
    if (!Buy(Tree, A)) return false;
    TestFalse(TEXT("Former first gateway no longer bypasses reach after reset"), Progression->CanPurchaseNode(Tree, C->NodeId, Reason));
    if (!Progression->RespecCore(Reason)) return false;
    auto* OtherTree = NewObject<UBreakerProgressionTree>();
    OtherTree->TreeId = TEXT("Test.Core.Other");
    auto* Other = MakeNode(OtherTree, TEXT("Test.Other.CoreSpend"));
    if (!Buy(OtherTree, Other)) return false;
    TestFalse(TEXT("Other Core tree spending prevents a new free gateway"), Progression->CanPurchaseNode(Tree, C->NodeId, Reason));
    Tree->bRestrictEntryToOwnedNeighbor = false;
    if (!Buy(Tree, C) || !Buy(Tree, A)) return false;
    TestEqual(TEXT("Default entry behavior still allows distant starts"), Progression->GetNodeRank(A->NodeId, Tree->Currency), 1);
    return true;
}
#endif
