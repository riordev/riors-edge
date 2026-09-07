#pragma once

#include "CoreMinimal.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// Presentation coordinates only. Purchasing still reads the original tree.
namespace BreakerDoctrineBoardLayout
{
    struct FLayout
    {
        FVector2D Size = FVector2D::ZeroVector;
        FVector2D Hub = FVector2D::ZeroVector;
        TMap<FName, FVector2D> Centers;
        TArray<FBreakerNodeEdge> Edges;
    };

    inline FLayout Build(const UBreakerProgressionTree* Tree)
    {
        FLayout Layout;
        if (!Tree) return Layout;
        TMap<FName, const UBreakerProgressionNode*> Nodes;
        TArray<FName> OrderedIds;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
            if (Node && !Node->NodeId.IsNone() && !Nodes.Contains(Node->NodeId))
            { Nodes.Add(Node->NodeId, Node); OrderedIds.Add(Node->NodeId); }
        if (OrderedIds.IsEmpty()) return Layout;

        TArray<FName> Roots;
        for (FName Id : OrderedIds)
        {
            bool bHasParent = false;
            for (const auto& Prerequisite : Nodes.FindChecked(Id)->Prerequisites)
                if (Nodes.Contains(Prerequisite.NodeId))
                {
                    bHasParent = true;
                    FBreakerNodeEdge Edge; Edge.A = Prerequisite.NodeId; Edge.B = Id;
                    Layout.Edges.Add(Edge);
                }
            if (!bHasParent) Roots.Add(Id);
        }
        // A malformed cyclic tree must remain drawable, without inventing a
        // dependency or recursively walking forever. Shipped trees are acyclic.
        if (Roots.IsEmpty()) Roots.Add(OrderedIds[0]);
        TFunction<int32(FName, TSet<FName>&)> ResolveBranch;
        ResolveBranch = [&](FName Id, TSet<FName>& Visiting) -> int32
        {
            const int32 Root = Roots.IndexOfByKey(Id);
            if (Root != INDEX_NONE) return Root;
            if (Visiting.Contains(Id)) return 0;
            Visiting.Add(Id);
            int32 Branch = MAX_int32;
            for (const auto& Prerequisite : Nodes.FindChecked(Id)->Prerequisites)
                if (Nodes.Contains(Prerequisite.NodeId)) Branch = FMath::Min(Branch, ResolveBranch(Prerequisite.NodeId, Visiting));
            Visiting.Remove(Id);
            return Branch == MAX_int32 ? 0 : Branch;
        };
        TMap<int32, TMap<int32, TArray<FName>>> Rings;
        for (FName Id : OrderedIds)
        {
            TSet<FName> Visiting;
            Rings.FindOrAdd(FMath::Max(1, Nodes.FindChecked(Id)->Tier)).FindOrAdd(ResolveBranch(Id, Visiting)).Add(Id);
        }
        TArray<int32> Tiers;
        Rings.GetKeys(Tiers); Tiers.Sort();
        // O2 PLACEHOLDER visual spacing: 64px markers retain at least32px air.
        constexpr double MinimumSpacing = 96;
        constexpr double RingSpacing = 104;
        constexpr double Margin = 64;
        const double Sector = 2.0 * UE_DOUBLE_PI / Roots.Num();
        double Radius = 8;
        for (const int32 Tier : Tiers)
        {
            TMap<FName, double> Angles;
            for (int32 Branch = 0; Branch < Roots.Num(); ++Branch)
                if (const auto* Siblings = Rings.FindChecked(Tier).Find(Branch))
                    for (int32 Index = 0; Index < Siblings->Num(); ++Index)
                    {
                        const double Fan = Siblings->Num() > 1 ? (static_cast<double>(Index) / (Siblings->Num() - 1) - .5) * Sector * .7 : 0;
                        Angles.Add((*Siblings)[Index], -UE_DOUBLE_PI / 2.0 + Branch * Sector + Fan);
                    }
            Radius += RingSpacing;
            TArray<double> SortedAngles;
            Angles.GenerateValueArray(SortedAngles); SortedAngles.Sort();
            if (SortedAngles.Num() > 1)
                for (int32 Index = 0; Index < SortedAngles.Num(); ++Index)
                {
                    const double Next = Index + 1 < SortedAngles.Num() ? SortedAngles[Index + 1] : SortedAngles[0] + 2.0 * UE_DOUBLE_PI;
                    const double Gap = Next - SortedAngles[Index];
                    if (Gap > UE_DOUBLE_SMALL_NUMBER)
                        Radius = FMath::Max(Radius, MinimumSpacing / (2.0 * FMath::Sin(Gap / 2.0)));
                }
            for (const auto& Entry : Angles)
                Layout.Centers.Add(Entry.Key, FVector2D(FMath::Cos(Entry.Value), FMath::Sin(Entry.Value)) * Radius);
        }
        Layout.Hub = FVector2D(Radius + Margin, Radius + Margin);
        Layout.Size = Layout.Hub * 2.0;
        for (auto& Entry : Layout.Centers) Entry.Value += Layout.Hub;
        return Layout;
    }
}
