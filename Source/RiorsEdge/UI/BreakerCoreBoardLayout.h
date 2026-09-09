#pragma once

#include "CoreMinimal.h"
#include "Progression/BreakerCoreWheelMath.h"

// Presentation only. Every point and edge names shipped tree data; no purchase
// rules or synthetic travel nodes live in the layout.
namespace BreakerCoreBoard
{
    constexpr float FocusHitSize = 44.0f;
    constexpr float OverviewLabelRadius = 720.0f; // O2 presentation.
    // Role-overview name ring. Twenty-two names on ONE radius leave 414 canvas
    // units of arc each, and the board draws at roughly a fifth of its canvas,
    // so that is an 87-pixel budget for CONSTITUTION — the reason the wheel
    // shipped anonymous. Alternating two radii doubles each name's arc.
    constexpr float RoleLabelNearRadius = 1400.0f; // O2 presentation.
    constexpr float RoleLabelFarRadius = 1530.0f;  // O2 presentation. Its box stays inside the 1600 half-canvas.
    // Inside the gateway ring (620) and outside the sector spokes' 100.
    constexpr float RoleSectorLabelRadius = 380.0f; // O2 presentation.
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
        FVector2D Size = FVector2D(1680.0f, 1680.0f);
        FVector2D Hub = FVector2D(840.0f, 840.0f);
        TMap<FName, FVector2D> Centers;
        TArray<FBreakerNodeEdge> Edges;
        TArray<FName> Entries;
        TArray<FWedge> Wedges;
        TArray<FName> Sectors;
        bool bUsesCoreRoles = false;
        bool bValidMetadata = true;
    };

    inline FVector2D Polar(const FVector2D& Center, float Radius, float Degrees)
    {
        const float Angle = FMath::DegreesToRadians(Degrees);
        return Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
    }

    inline FVector2D SectorAngles(const FLayout& Layout, FName Sector)
    {
        float First = TNumericLimits<float>::Max(), Last = TNumericLimits<float>::Lowest();
        for (const FWedge& Wedge : Layout.Wedges) if (Wedge.Sector == Sector)
        { First = FMath::Min(First, Wedge.AngleDegrees); Last = FMath::Max(Last, Wedge.AngleDegrees); }
        if (First > Last) return FVector2D::ZeroVector;
        // X: label center angle, Y: leading boundary angle.
        return FVector2D((First + Last) * .5f, First - 180.0f / FMath::Max(1, Layout.Wedges.Num()));
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

    inline FLayout BuildRoleLayout(const UBreakerProgressionTree* Tree, FName Focus)
    {
        FLayout Layout; Layout.bUsesCoreRoles = true;
        Layout.Size = Focus.IsNone() ? FVector2D(3200, 3200) : FVector2D(1480, 1200);
        Layout.Hub = Focus.IsNone() ? FVector2D(1600, 1600) : FVector2D(700, 600);
        TSet<FName> SeenWedges, SeenNodes;
        for (int32 W = 0; W < Tree->CoreWedgeOrder.Num(); ++W)
        {
            const FName Name = Tree->CoreWedgeOrder[W];
            if (Name.IsNone() || SeenWedges.Contains(Name)) { Layout.bValidMetadata = false; break; }
            SeenWedges.Add(Name);
            FWedge Wedge; Wedge.Name = Name; Wedge.Sector = Tree->CoreWedgeSectors.FindRef(Name);
            if (Wedge.Sector.IsNone()) { Layout.bValidMetadata = false; break; }
            Wedge.AngleDegrees = -90 + W * 360.0f / Tree->CoreWedgeOrder.Num();
            TMap<int32, const UBreakerProgressionNode*> Slots;
            bool bMajor = false;
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (!Node || Node->Constellation != Name) continue;
                const auto Role = Node->CoreRole;
                int32 Slot = -1;
                switch (Role)
                {
                case EBreakerCoreNodeRole::Gateway: Slot = 0; break;
                case EBreakerCoreNodeRole::LaneMinor: if (Node->CoreLaneIndex >= 0 && Node->CoreLaneIndex < 3) Slot = 1 + Node->CoreLaneIndex; break;
                case EBreakerCoreNodeRole::LaneNotable: if (Node->CoreLaneIndex >= 0 && Node->CoreLaneIndex < 3) Slot = 4 + Node->CoreLaneIndex; break;
                case EBreakerCoreNodeRole::Link: if (Node->CoreLaneIndex >= 0 && Node->CoreLaneIndex < 2) Slot = 7 + Node->CoreLaneIndex; break;
                case EBreakerCoreNodeRole::Convergence: Slot = 9; break;
                case EBreakerCoreNodeRole::Keystone: Slot = 10; bMajor = true; break;
                default: break;
                }
                if (Slot < 0 || Slots.Contains(Slot) || Node->NodeId.IsNone() || SeenNodes.Contains(Node->NodeId))
                { Layout.bValidMetadata = false; break; }
                Slots.Add(Slot, Node); SeenNodes.Add(Node->NodeId); Wedge.Nodes.Add(Node->NodeId);
            }
            const int32 Lanes = bMajor ? 3 : 2;
            if (!Slots.Contains(0) || !Slots.Contains(9) || Slots.Num() != (bMajor ? 11 : 6)) Layout.bValidMetadata = false;
            for (int32 Lane = 0; Lane < Lanes; ++Lane)
                if (!Slots.Contains(1 + Lane) || !Slots.Contains(4 + Lane)) Layout.bValidMetadata = false;
            if (bMajor && (!Slots.Contains(7) || !Slots.Contains(8))) Layout.bValidMetadata = false;
            if (!bMajor && (Slots.Contains(3) || Slots.Contains(6) || Slots.Contains(7) || Slots.Contains(8))) Layout.bValidMetadata = false;
            if (!Layout.bValidMetadata) break;
            Layout.Wedges.Add(Wedge); Layout.Sectors.AddUnique(Wedge.Sector);
            if (!Focus.IsNone() && Focus != Name) continue;
            for (const auto& Entry : Slots)
            {
                const int32 Slot = Entry.Key;
                FVector2D Position;
                if (Focus.IsNone())
                {
                    float Radius = 620, Offset = 0;
                    const float LaneStep = 100.0f / Tree->CoreWedgeOrder.Num();
                    if (Slot >= 1 && Slot <= 6)
                    {
                        const int32 Lane = (Slot - 1) % 3;
                        Offset = (Lane - (Lanes - 1) * .5f) * LaneStep;
                        Radius = Slot <= 3 ? 800 : 1030;
                    }
                    else if (Slot == 7 || Slot == 8) { Radius = 915; Offset = (Slot == 7 ? -.5f : .5f) * LaneStep; }
                    else if (Slot == 9) Radius = 1190;
                    else if (Slot == 10) Radius = 1340;
                    Position = Polar(Layout.Hub, Radius, Wedge.AngleDegrees + Offset);
                }
                else
                {
                    if (Slot == 0) Position = FVector2D(100,600);
                    else if (Slot == 9) Position = FVector2D(1100,600);
                    else if (Slot == 10) Position = FVector2D(1350,600);
                    else if (Slot == 7 || Slot == 8) Position = FVector2D(600,Slot == 7 ? 450 : 750);
                    else
                    {
                        const int32 Lane = (Slot - 1) % 3;
                        const float Y = bMajor ? 300 + Lane * 300 : 380 + Lane * 440;
                        Position = FVector2D(Slot <= 3 ? 420 : 760,Y);
                    }
                }
                Layout.Centers.Add(Entry.Value->NodeId, Position);
            }
        }
        if (SeenNodes.Num() != Tree->Nodes.Num() || (!Focus.IsNone() && !SeenWedges.Contains(Focus))) Layout.bValidMetadata = false;
        if (!Layout.bValidMetadata)
        {
            // Do not silently guess a partially authored replacement graph.
            Layout.Centers.Reset(); Layout.Wedges.Reset(); Layout.Sectors.Reset(); return Layout;
        }
        for (const FBreakerNodeEdge& Edge : Tree->AdjacencyEdges)
            if (Layout.Centers.Contains(Edge.A) && Layout.Centers.Contains(Edge.B)) Layout.Edges.Add(Edge);
        for (FName Id : Tree->EntryNodeIds) if (Layout.Centers.Contains(Id)) Layout.Entries.Add(Id);
        return Layout;
    }
    inline FLayout Build(const UBreakerProgressionTree* Tree, FName Focus = NAME_None)
    {
        FLayout Layout;
        if (!Tree) return Layout;
        if (!Tree->CoreWedgeOrder.IsEmpty()) return BuildRoleLayout(Tree, Focus);
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
        // O2 presentation: equal cluster allotments, grouped by authored sector.
        // Weapon has four clusters; its sector earns twice a two-cluster sector's arc.
        int32 Ordinal = 0;
        const float ClusterAngle = 360.0f / FMath::Max(1, Layout.Wedges.Num());
        for (const FName Sector : Layout.Sectors)
            for (FWedge& Wedge : Layout.Wedges)
                if (Wedge.Sector == Sector) Wedge.AngleDegrees = -105.0f + ClusterAngle * Ordinal++;
        if (!Focus.IsNone())
        {
            Layout.Size = FVector2D(1320.0f, 1080.0f);
            Layout.Hub = FVector2D(60.0f, 480.0f);
        }
        for (const FWedge& Wedge : Layout.Wedges)
        {
            if (!Focus.IsNone() && Wedge.Name != Focus) continue;
            const TArray<FName> Rims = RimOrder(Tree, Wedge);
            const FVector2D FocusCenter(660.0f, 480.0f);
            const FVector2D ClusterCenter = Focus.IsNone() ? Polar(Layout.Hub, 520.0f, Wedge.AngleDegrees) : FocusCenter;
            const float Radius = Focus.IsNone() ? 110.0f : 400.0f; // O2 presentation; full labels fit between circles.
            for (int32 Index = 0; Index < Rims.Num(); ++Index)
            {
                // Entry faces the shared hub in overview; the focus entry is left.
                const float Start = Focus.IsNone() ? Wedge.AngleDegrees + 180.0f : 180.0f;
                Layout.Centers.Add(Rims[Index], Polar(ClusterCenter, Radius,
                    Start + 360.0f * Index / FMath::Max(1, Rims.Num())));
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
                FVector2D Mean = FVector2D::ZeroVector;
                int32 Count = 0;
                for (const FBreakerNodePrerequisite& Prerequisite : Tree->FindNode(Inner[Index])->Prerequisites)
                    if (const FVector2D* Point = Layout.Centers.Find(Prerequisite.NodeId)) { Mean += *Point; ++Count; }
                const FVector2D Position = Count > 0 ? FMath::Lerp(ClusterCenter, Mean / Count, 0.74f)
                    : Polar(ClusterCenter, Radius * .55f, 120.0f * Index);
                Layout.Centers.Add(Inner[Index], Position);
            }
            for (int32 Index = 0; Index < Outer.Num(); ++Index)
                Layout.Centers.Add(Outer[Index], ClusterCenter + FVector2D(Index * Radius * .3f, 0));
        }
        for (const FBreakerNodeEdge& Edge : Tree->AdjacencyEdges)
            if (Layout.Centers.Contains(Edge.A) && Layout.Centers.Contains(Edge.B)) Layout.Edges.Add(Edge);
        for (const FName Id : Tree->EntryNodeIds) if (Layout.Centers.Contains(Id)) Layout.Entries.Add(Id);
        return Layout;
    }
}
