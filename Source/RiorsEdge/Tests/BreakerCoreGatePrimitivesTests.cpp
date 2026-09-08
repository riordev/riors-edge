#include "Misc/AutomationTest.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreGatePrimitivesTest,
    "RiorsEdge.Progression.CoreGatePrimitives",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreGatePrimitivesTest::RunTest(const FString& Parameters)
{
    // Isolated authored schema fixture, not a replacement shipped Core roster.
    // Points come from the actual level curve; no rank list or point grant.
    auto* Progression = NewObject<UBreakerProgressionComponent>();
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(29, Progression->ExperienceCurve));
    const int32 Budget = Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints);
    if (!TestEqual(TEXT("Level-earned Core budget"), Budget, 29)) return false;
    auto* Tree = NewObject<UBreakerProgressionTree>();
    Tree->TreeId = TEXT("Test.Core.Gates");
    Tree->Currency = EBreakerPointCurrency::CorePoints;
    Tree->CornerstoneInvestmentGate = 0;
    auto Node = [&](const TCHAR* Id, int32 Ranks, int32 Cost, FName Wedge = FName(TEXT("Local")))
    {
        auto* Result = NewObject<UBreakerProgressionNode>(Tree);
        Result->NodeId = Id; Result->MaxRank = Ranks; Result->CostPerRank = Cost;
        Result->Currency = Tree->Currency; Result->Constellation = Wedge;
        Tree->Nodes.Add(Result);
        return Result;
    };
    auto Requirement = [](FName Id, int32 Rank)
    {
        FBreakerNodePrerequisite Result; Result.NodeId = Id; Result.RequiredRank = Rank; return Result;
    };
    auto* Gateway = Node(TEXT("Test.Gateway"), 1, 1);
    auto* Other = Node(TEXT("Test.Other"), 1, 3, TEXT("Other"));
    TArray<UBreakerProgressionNode*> Minors, Notables;
    for (int32 Lane = 0; Lane < 3; ++Lane)
    {
        auto* Minor = Node(*FString::Printf(TEXT("Test.Minor%d"), Lane), 3, 1);
        Minor->Prerequisites.Add(Requirement(Gateway->NodeId, 1));
        auto* Notable = Node(*FString::Printf(TEXT("Test.Notable%d"), Lane), 1, 2);
        Notable->Prerequisites.Add(Requirement(Minor->NodeId, 3));
        Minors.Add(Minor); Notables.Add(Notable);
    }
    auto* Convergence = Node(TEXT("Test.Convergence"), 1, 3);
    Convergence->Prerequisites.Add(Requirement(Gateway->NodeId, 1));
    FBreakerNodePrerequisiteGroup Group;
    Group.MinimumSatisfied = 2;
    for (auto* Notable : Notables) Group.Candidates.Add(Requirement(Notable->NodeId, 1));
    Group.Candidates.Add(Requirement(Notables[0]->NodeId, 1)); // Duplicate cannot count as another lane.
    Convergence->PrerequisiteGroups.Add(Group);
    auto* Link = Node(TEXT("Test.Link"), 1, 1);
    FBreakerNodePrerequisiteGroup AnyLane;
    AnyLane.Candidates = {Requirement(Notables[0]->NodeId, 1), Requirement(Notables[2]->NodeId, 1)};
    Link->PrerequisiteGroups.Add(AnyLane);
    auto* Keystone = Node(TEXT("Test.Keystone"), 1, 5);
    Keystone->RequiredConstellationInvestment = 18;
    Keystone->Prerequisites.Add(Requirement(Convergence->NodeId, 1));
    auto* Expensive = Node(TEXT("Test.BudgetRefusal"), 1, 4);
    FText Reason;
    auto Buy = [&](UBreakerProgressionNode* Value)
    {
        const bool Bought = Progression->PurchaseNode(Tree, Value->NodeId, Reason);
        TestTrue(FString::Printf(TEXT("Actual purchase %s: %s"), *Value->NodeId.ToString(), *Reason.ToString()), Bought);
        return Bought;
    };
    TestFalse(TEXT("Existing AND gate still blocks lane before gateway"), Progression->CanPurchaseNode(Tree, Minors[0]->NodeId, Reason));
    TestFalse(TEXT("Any-of requires one completed candidate"), Progression->CanPurchaseNode(Tree, Link->NodeId, Reason));
    if (!Buy(Other) || !Buy(Gateway)) return false;
    for (int32 Lane = 0; Lane < 2; ++Lane)
    {
        if (!Buy(Minors[Lane]) || !Buy(Minors[Lane])) return false;
        TestFalse(TEXT("Rank two cannot satisfy rank-three prerequisite"), Progression->CanPurchaseNode(Tree, Notables[Lane]->NodeId, Reason));
        if (!Buy(Minors[Lane])) return false;
        TestEqual(TEXT("Three actual one-point purchases retain rank three"), Progression->GetNodeRank(Minors[Lane]->NodeId, Tree->Currency), 3);
        TestFalse(TEXT("Fourth rank is refused"), Progression->CanPurchaseNode(Tree, Minors[Lane]->NodeId, Reason));
        if (!Buy(Notables[Lane])) return false;
        if (Lane == 0)
            TestFalse(TEXT("One completed lane plus duplicate ID is not two"), Progression->CanPurchaseNode(Tree, Convergence->NodeId, Reason));
    }
    TestTrue(TEXT("Any-of accepts an owned candidate without the other"), Progression->CanPurchaseNode(Tree, Link->NodeId, Reason));
    if (!Buy(Convergence)) return false;
    for (int32 Rank = 0; Rank < 3; ++Rank) if (!Buy(Minors[2])) return false;
    TestEqual(TEXT("Local investment counts paid ranks and costs"), Progression->GetConstellationInvestment(Tree, TEXT("Local")), 17);
    TestEqual(TEXT("Other wedge has its own investment"), Progression->GetConstellationInvestment(Tree, TEXT("Other")), 3);
    TestFalse(TEXT("Other spending and pending five-point cost cannot satisfy local eighteen"), Progression->CanPurchaseNode(Tree, Keystone->NodeId, Reason));
    if (!Buy(Link)) return false;
    TestEqual(TEXT("Exact local threshold"), Progression->GetConstellationInvestment(Tree, TEXT("Local")), 18);
    if (!Buy(Keystone)) return false;
    const int32 Remaining = Progression->GetUnspentPoints(Tree->Currency);
    TestEqual(TEXT("Exact total paid budget"), Remaining, Budget - 26);
    TestFalse(TEXT("Insufficient wallet refuses otherwise ungated node"), Progression->PurchaseNode(Tree, Expensive->NodeId, Reason));
    TestEqual(TEXT("Budget refusal does not debit"), Progression->GetUnspentPoints(Tree->Currency), Remaining);
    if (!TestTrue(TEXT("Actual below-threshold free Core respec"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec refunds exact paid rank costs"), Progression->GetUnspentPoints(Tree->Currency), Budget);
    TestEqual(TEXT("Respec clears local investment"), Progression->GetConstellationInvestment(Tree, TEXT("Local")), 0);
    TestFalse(TEXT("Respec removes completed lane eligibility"), Progression->CanPurchaseNode(Tree, Convergence->NodeId, Reason));
    return true;
}
#endif
