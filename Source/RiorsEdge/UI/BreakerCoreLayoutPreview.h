#pragma once
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
// Isolated presentation schema. Never registered in progression or saved.
namespace BreakerCoreLayoutPreview
{
inline UBreakerProgressionTree* Create()
{
    auto* Tree = NewObject<UBreakerProgressionTree>();
    const TCHAR* Names[] = {TEXT("Precision"),TEXT("Vector"),TEXT("Ballistics"),TEXT("Loadout"),
        TEXT("Arc"),TEXT("Tempo"),TEXT("Reservoir"),TEXT("Duration"),TEXT("Affliction"),TEXT("Entropy"),
        TEXT("Reaction"),TEXT("Rift"),TEXT("Void"),TEXT("Aegis"),TEXT("Bulwark"),TEXT("Constitution"),
        TEXT("Ward"),TEXT("Recovery"),TEXT("Velocity"),TEXT("Kinesis"),TEXT("Control"),TEXT("Threat")};
    const TSet<FName> Majors = {TEXT("Precision"),TEXT("Vector"),TEXT("Ballistics"),TEXT("Arc"),TEXT("Tempo"),
        TEXT("Affliction"),TEXT("Entropy"),TEXT("Reaction"),TEXT("Aegis"),TEXT("Bulwark"),TEXT("Velocity")};
    int32 Offered = 0;
    auto Edge = [&](FName A, FName B) { FBreakerNodeEdge E; E.A=A; E.B=B; Tree->AdjacencyEdges.Add(E); };
    int32 WedgeIndex = 0;
    for (const TCHAR* Name : Names)
    {
        const FName Wedge(Name); Tree->CoreWedgeOrder.Add(Wedge);
        Tree->CoreWedgeSectors.Add(Wedge, WedgeIndex<4 ? TEXT("Weapon") : WedgeIndex<8 ? TEXT("Ability")
            : WedgeIndex<13 ? TEXT("Status") : WedgeIndex<18 ? TEXT("Defence") : WedgeIndex<20 ? TEXT("Movement") : TEXT("Utility"));
        ++WedgeIndex;
        const bool Major = Majors.Contains(Wedge);
        auto Node = [&](EBreakerCoreNodeRole Role, int32 Lane, int32 Ranks, int32 Cost)
        {
            auto* N = NewObject<UBreakerProgressionNode>(Tree);
            N->NodeId = FName(*FString::Printf(TEXT("Test.Layout.%s.%d.%d"),Name,static_cast<int32>(Role),Lane));
            N->Constellation=Wedge; N->CoreRole=Role; N->CoreLaneIndex=Lane; N->MaxRank=Ranks; N->CostPerRank=Cost;
            N->DisplayName = FText::FromString(Role == EBreakerCoreNodeRole::Gateway ? FString(TEXT("Gateway"))
                : Role == EBreakerCoreNodeRole::Convergence ? FString(TEXT("Convergence"))
                : Role == EBreakerCoreNodeRole::Keystone ? FString(TEXT("Keystone"))
                : FString::Printf(TEXT("Lane %c %s"), TCHAR('A' + Lane), Role == EBreakerCoreNodeRole::LaneMinor ? TEXT("Minor") : Role == EBreakerCoreNodeRole::LaneNotable ? TEXT("Notable") : TEXT("Link")));
            N->Description = FText::FromString(TEXT("Structural preview only. No gameplay effect or purchase."));
            Tree->Nodes.Add(N); Offered += Ranks*Cost; return N->NodeId;
        };
        const FName Gateway=Node(EBreakerCoreNodeRole::Gateway,0,1,1); Tree->EntryNodeIds.Add(Gateway);
        TArray<FName> Notables;
        for (int32 Lane=0; Lane<(Major?3:2); ++Lane)
        {
            const FName Minor=Node(EBreakerCoreNodeRole::LaneMinor,Lane,3,1);
            const FName Notable=Node(EBreakerCoreNodeRole::LaneNotable,Lane,1,2);
            Edge(Gateway,Minor); Edge(Minor,Notable); Notables.Add(Notable);
        }
        if (Major) for (int32 Lane=0; Lane<2; ++Lane)
        {
            const FName Link=Node(EBreakerCoreNodeRole::Link,Lane,1,1);
            Edge(Notables[Lane],Link); Edge(Link,Notables[Lane+1]);
        }
        const FName Convergence=Node(EBreakerCoreNodeRole::Convergence,0,1,Major?3:2);
        for (FName Notable:Notables) Edge(Notable,Convergence);
        if (Major) Edge(Convergence,Node(EBreakerCoreNodeRole::Keystone,0,1,5));
    }
    for (int32 W=0; W<22; ++W) Edge(Tree->EntryNodeIds[W],Tree->EntryNodeIds[(W+1)%22]);
    Tree->TreeId = TEXT("Preview.Core.Structure");
    return Tree;
}
}
