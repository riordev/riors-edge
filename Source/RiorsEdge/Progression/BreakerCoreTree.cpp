#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

UBreakerProgressionTree* BreakerCoreTree::Build(UObject* Outer, FName TreeId,
    const FText& Title, const TArray<FBreakerCoreWedgeDefinition>& Wedges, FString& Error)
{
    Error.Reset();
    if (!Outer || TreeId.IsNone() || Wedges.IsEmpty())
    { Error = TEXT("Core tree requires an outer, identity and wedges"); return nullptr; }
    TSet<FName> Ids, WedgeIds;
    for (const auto& Wedge : Wedges)
    {
        if (Wedge.Id.IsNone() || Wedge.Sector.IsNone() || WedgeIds.Contains(Wedge.Id)
            || Wedge.Lanes.Num() != (Wedge.bMajor ? 3 : 2)
            || Wedge.Links.Num() != (Wedge.bMajor ? 2 : 0)
            || (Wedge.Keystone != nullptr) != Wedge.bMajor)
        { Error = FString::Printf(TEXT("Invalid Core wedge shape: %s"), *Wedge.Id.ToString()); return nullptr; }
        WedgeIds.Add(Wedge.Id);
        TArray<const UBreakerProgressionNode*> Nodes{Wedge.Gateway, Wedge.Convergence};
        for (const auto& Lane : Wedge.Lanes) { Nodes.Add(Lane.Minor); Nodes.Add(Lane.Notable); }
        Nodes.Append(Wedge.Links);
        if (Wedge.Keystone) Nodes.Add(Wedge.Keystone);
        for (const auto* Node : Nodes)
        {
            if (!Node || Node->NodeId.IsNone() || Ids.Contains(Node->NodeId))
            { Error = FString::Printf(TEXT("Missing or duplicate Core node in %s"), *Wedge.Id.ToString()); return nullptr; }
            Ids.Add(Node->NodeId);
        }
    }
    auto* Tree = NewObject<UBreakerProgressionTree>(Outer);
    Tree->TreeId = TreeId; Tree->DisplayName = Title;
    Tree->Currency = EBreakerPointCurrency::CorePoints; Tree->RequiredClass = EBreakerClassId::None;
    Tree->CornerstoneInvestmentGate = 0; Tree->bRestrictEntryToOwnedNeighbor = true;
    auto Edge = [&](FName A, FName B)
    {
        if (A == B || Tree->AdjacencyEdges.ContainsByPredicate([&](const FBreakerNodeEdge& E)
            { return (E.A == A && E.B == B) || (E.A == B && E.B == A); })) return;
        FBreakerNodeEdge E; E.A = A; E.B = B; Tree->AdjacencyEdges.Add(E);
    };
    auto Requirement = [](FName Id, int32 Rank = 1)
    { FBreakerNodePrerequisite P; P.NodeId = Id; P.RequiredRank = Rank; return P; };
    for (const auto& Wedge : Wedges)
    {
        Tree->CoreWedgeOrder.Add(Wedge.Id); Tree->CoreWedgeSectors.Add(Wedge.Id,Wedge.Sector);
        auto Copy = [&](const UBreakerProgressionNode* Source, EBreakerCoreNodeRole Role, int32 Lane, int32 Ranks, int32 Cost, int32 Tier)
        {
            auto* Node = DuplicateObject<UBreakerProgressionNode>(Source,Tree);
            Node->Currency = Tree->Currency; Node->RequiredClass = EBreakerClassId::None;
            Node->Constellation = Wedge.Id; Node->CoreRole = Role; Node->CoreLaneIndex = Lane;
            Node->MaxRank = Ranks; Node->CostPerRank = Cost; Node->Tier = Tier;
            Node->RequiredTreeInvestment = 0; Node->RequiredConstellationInvestment = 0;
            Node->bCornerstone = false; Node->Prerequisites.Reset(); Node->PrerequisiteGroups.Reset(); Node->MutuallyExclusiveNodeIds.Reset();
            Tree->Nodes.Add(Node); return Node;
        };
        auto* Gateway = Copy(Wedge.Gateway,EBreakerCoreNodeRole::Gateway,0,1,1,1);
        Tree->EntryNodeIds.Add(Gateway->NodeId);
        TArray<UBreakerProgressionNode*> Notables;
        for (int32 Index = 0; Index < Wedge.Lanes.Num(); ++Index)
        {
            const auto& Lane = Wedge.Lanes[Index];
            auto* Minor = Copy(Lane.Minor,EBreakerCoreNodeRole::LaneMinor,Index,3,1,2);
            Minor->Prerequisites.Add(Requirement(Gateway->NodeId)); Edge(Gateway->NodeId,Minor->NodeId);
            auto* Notable = Copy(Lane.Notable,EBreakerCoreNodeRole::LaneNotable,Index,1,2,3);
            Notable->Prerequisites.Add(Requirement(Minor->NodeId)); Edge(Minor->NodeId,Notable->NodeId);
            Notables.Add(Notable);
        }
        for (int32 Index = 0; Index < Wedge.Links.Num(); ++Index)
        {
            auto* Link = Copy(Wedge.Links[Index],EBreakerCoreNodeRole::Link,Index,1,1,3);
            FBreakerNodePrerequisiteGroup Group; Group.MinimumSatisfied = 1;
            Group.Candidates = {Requirement(Notables[Index]->NodeId),Requirement(Notables[Index+1]->NodeId)};
            Link->PrerequisiteGroups.Add(Group);
            Edge(Notables[Index]->NodeId,Link->NodeId); Edge(Link->NodeId,Notables[Index+1]->NodeId);
        }
        auto* Convergence = Copy(Wedge.Convergence,EBreakerCoreNodeRole::Convergence,0,1,Wedge.bMajor ? 3 : 2,4);
        Convergence->Prerequisites.Add(Requirement(Gateway->NodeId));
        FBreakerNodePrerequisiteGroup Group; Group.MinimumSatisfied = 2;
        for (const auto* Notable : Notables)
        { Group.Candidates.Add(Requirement(Notable->NodeId)); Edge(Notable->NodeId,Convergence->NodeId); }
        Convergence->PrerequisiteGroups.Add(Group);
        if (Wedge.bMajor)
        {
            auto* Keystone = Copy(Wedge.Keystone,EBreakerCoreNodeRole::Keystone,0,1,5,5);
            Keystone->RequiredConstellationInvestment = 18;
            Keystone->Prerequisites.Add(Requirement(Convergence->NodeId)); Edge(Convergence->NodeId,Keystone->NodeId);
        }
    }
    for (int32 Index = 0; Index < Tree->EntryNodeIds.Num(); ++Index)
        Edge(Tree->EntryNodeIds[Index],Tree->EntryNodeIds[(Index+1)%Tree->EntryNodeIds.Num()]);
    return Tree;
}
