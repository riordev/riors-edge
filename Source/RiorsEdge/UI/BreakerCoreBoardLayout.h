#pragma once

#include "CoreMinimal.h"
#include "Progression/BreakerCoreWheelMath.h"

// Presentation only. Every point and edge names shipped tree data; no purchase
// rules or synthetic travel nodes live in the layout.
namespace BreakerCoreBoard
{
    constexpr float FocusHitSize = 44.0f;
    constexpr float OverviewLabelRadius = 550.0f;
    inline FVector2D OverviewHitSize() { return FVector2D(190.0f, 74.0f); }
    struct FWedge
    {
        FName Name;
        FName Sector;
        float AngleDegrees = 0.0f;
        TArray<FName> Nodes;
    };

    struct FLayout
    {
        FVector2D Size = FVector2D(1600.0f, 1600.0f);
        FVector2D Hub = FVector2D(800.0f, 800.0f);
        TMap<FName, FVector2D> Centers;
        TArray<FBreakerNodeEdge> Edges;
        TArray<FName> Entries;
        TArray<FWedge> Wedges;
        TArray<FName> Sectors;
    };

    inline FVector2D Polar(const FVector2D& Center, float Radius, float Degrees)
    {
        const float Angle = FMath::DegreesToRadians(Degrees);
        return Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
    }

    inline TArray<FName> RimOrder(const UBreakerProgressionTree* Tree, const FWedge& Wedge)
    {
        TArray<FName> Rims;
        int32 FirstTier = MAX_int32;
        for (const FName Id : Wedge.Nodes) FirstTier = FMath::Min(FirstTier, Tree->FindNode(Id)->Tier);
        for (const FName Id : Wedge.Nodes) if (Tree->FindNode(Id)->Tier == FirstTier) Rims.Add(Id);
        Rims.Sort(FNameLexicalLess());
        TArray<FName> Ordered;
        for (const FName Id : Tree->EntryNodeIds) if (Rims.Contains(Id)) { Ordered.Add(Id); break; }
        if (Ordered.IsEmpty() && !Rims.IsEmpty()) Ordered.Add(Rims[0]);
        while (Ordered.Num() < Rims.Num())
        {
            TArray<FName> Next;
            for (const FBreakerNodeEdge& Edge : Tree->AdjacencyEdges)
            {
                const FName Neighbor = Edge.A == Ordered.Last() ? Edge.B : (Edge.B == Ordered.Last() ? Edge.A : NAME_None);
                if (Rims.Contains(Neighbor) && !Ordered.Contains(Neighbor)) Next.AddUnique(Neighbor);
            }
            Next.Sort(FNameLexicalLess());
            if (!Next.IsEmpty()) Ordered.Add(Next[0]);
            else for (const FName Id : Rims) if (!Ordered.Contains(Id)) { Ordered.Add(Id); break; }
        }
        return Ordered;
    }

    inline FLayout Build(const UBreakerProgressionTree* Tree, FName Focus = NAME_None)
    {
        FLayout Layout;
        if (!Tree) return Layout;
        Layout.Sectors = { BreakerCoreWheel::SectorMovement, BreakerCoreWheel::SectorWeapon,
            BreakerCoreWheel::SectorDefence, BreakerCoreWheel::SectorAbility, BreakerCoreWheel::SectorElements };
        TArray<FName> Names;
        for (const UBreakerProgressionNode* Node : Tree->Nodes) if (Node) Names.AddUnique(Node->Constellation);
        Names.Sort(FNameLexicalLess());
        for (const FName Name : Names)
        {
            FWedge Wedge;
            Wedge.Name = Name;
            Wedge.Sector = BreakerCoreSectorOf(Name);
            if (!Layout.Sectors.Contains(Wedge.Sector)) Layout.Sectors.Add(Wedge.Sector);
            for (const UBreakerProgressionNode* Node : Tree->Nodes) if (Node && Node->Constellation == Name) Wedge.Nodes.Add(Node->NodeId);
            Wedge.Nodes.Sort(FNameLexicalLess());
            Layout.Wedges.Add(MoveTemp(Wedge));
        }
        const float SectorAngle = 360.0f / FMath::Max(1, Layout.Sectors.Num());
        for (int32 SectorIndex = 0; SectorIndex < Layout.Sectors.Num(); ++SectorIndex)
        {
            TArray<int32> WedgeIndices;
            for (int32 Index = 0; Index < Layout.Wedges.Num(); ++Index)
                if (Layout.Wedges[Index].Sector == Layout.Sectors[SectorIndex]) WedgeIndices.Add(Index);
            for (int32 Index = 0; Index < WedgeIndices.Num(); ++Index)
            {
                Layout.Wedges[WedgeIndices[Index]].AngleDegrees = -90.0f + SectorIndex * SectorAngle
                    - SectorAngle * 0.5f + SectorAngle * (Index + 0.5f) / WedgeIndices.Num();
            }
        }
        if (!Focus.IsNone())
        {
            Layout.Size = FVector2D(1000.0f, 720.0f);
            Layout.Hub = FVector2D(40.0f, 340.0f);
        }
        for (const FWedge& Wedge : Layout.Wedges)
        {
            if (!Focus.IsNone() && Wedge.Name != Focus) continue;
            const TArray<FName> Rims = RimOrder(Tree, Wedge);
            const FVector2D FocusCenter(500.0f, 340.0f);
            const FVector2D FocusRims[] = { {140,340}, {340,120}, {660,120}, {860,340}, {660,560}, {340,560} };
            const FVector2D OverviewRims[] = { {240,0}, {320,-26}, {400,-26}, {480,0}, {400,26}, {320,26} };
            const float Angle = FMath::DegreesToRadians(Wedge.AngleDegrees);
            const FVector2D Radial(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D Tangent(-Radial.Y, Radial.X);
            for (int32 Index = 0; Index < Rims.Num(); ++Index)
            {
                const FVector2D Offset = Rims.Num() == UE_ARRAY_COUNT(OverviewRims) ? OverviewRims[Index]
                    : FVector2D(240.0f + 70.0f * Index, 0.0f);
                const FVector2D Position = Focus.IsNone() ? Layout.Hub + Radial * Offset.X + Tangent * Offset.Y
                    : (Rims.Num() == UE_ARRAY_COUNT(FocusRims) ? FocusRims[Index]
                        : Polar(FocusCenter, 240.0f, 180.0f + 360.0f * Index / FMath::Max(1, Rims.Num())));
                Layout.Centers.Add(Rims[Index], Position);
            }
            TArray<FName> Inner;
            TArray<FName> Outer;
            for (const FName Id : Wedge.Nodes)
            {
                if (Rims.Contains(Id)) continue;
                const UBreakerProgressionNode* Node = Tree->FindNode(Id);
                bool bTouchesRim = false;
                for (const FBreakerNodePrerequisite& Prerequisite : Node->Prerequisites) bTouchesRim |= Rims.Contains(Prerequisite.NodeId);
                (bTouchesRim ? Inner : Outer).Add(Id);
            }
            for (int32 Index = 0; Index < Inner.Num(); ++Index)
            {
                FVector2D Position;
                if (Focus.IsNone())
                {
                    const float Side = Index - (Inner.Num() - 1) * 0.5f;
                    Position = Layout.Hub + Radial * (Side == 0.0f ? 595.0f : 540.0f) + Tangent * (Side * 48.0f);
                }
                else
                {
                    FVector2D Mean = FVector2D::ZeroVector;
                    int32 Count = 0;
                    for (const FBreakerNodePrerequisite& Prerequisite : Tree->FindNode(Inner[Index])->Prerequisites)
                        if (const FVector2D* Point = Layout.Centers.Find(Prerequisite.NodeId)) { Mean += *Point; ++Count; }
                    Position = Count > 0 ? FMath::Lerp(FocusCenter, Mean / Count, 0.74f)
                        : FVector2D(280.0f + Index * 220.0f, 360.0f);
                }
                Layout.Centers.Add(Inner[Index], Position);
            }
            for (int32 Index = 0; Index < Outer.Num(); ++Index)
                Layout.Centers.Add(Outer[Index], Focus.IsNone()
                    ? Layout.Hub + Radial * (665.0f + Index * 60.0f)
                    : FocusCenter + FVector2D(Index * 100.0f, 0.0f));
        }
        if (Focus.IsNone())
            for (TPair<FName, FVector2D>& Point : Layout.Centers)
                Point.Value = Layout.Hub + (Point.Value - Layout.Hub) * 0.74f;
        for (const FBreakerNodeEdge& Edge : Tree->AdjacencyEdges)
            if (Layout.Centers.Contains(Edge.A) && Layout.Centers.Contains(Edge.B)) Layout.Edges.Add(Edge);
        for (const FName Id : Tree->EntryNodeIds) if (Layout.Centers.Contains(Id)) Layout.Entries.Add(Id);
        return Layout;
    }
}
