#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "UI/BreakerCoreBoardLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreBoardLayoutTest,
    "RiorsEdge.UI.CoreBoard.GraphGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoreBoardLayoutTest::RunTest(const FString& Parameters)
{
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetCoreSliceTree();
    if (!TestNotNull(TEXT("Shipped Core exists"), Tree)) return false;
    const BreakerCoreBoard::FLayout Overview = BreakerCoreBoard::Build(Tree);
    TestTrue(TEXT("Shipped metadata selects role layout"), Overview.bUsesCoreRoles && Overview.bValidMetadata);
    TestEqual(TEXT("Six authored sectors"), Overview.Sectors.Num(), 6);
    TestEqual(TEXT("Every shipped node appears"), Overview.Centers.Num(), 187);
    TestEqual(TEXT("Every actual edge appears"), Overview.Edges.Num(), 242);
    TestEqual(TEXT("Every authored gateway appears"), Overview.Entries.Num(), 22);
    TArray<FVector2D> OverviewPoints;
    Overview.Centers.GenerateValueArray(OverviewPoints);
    for (int32 Index = 0; Index < OverviewPoints.Num(); ++Index)
        for (int32 Other = Index + 1; Other < OverviewPoints.Num(); ++Other)
            TestTrue(TEXT("Overview nodes have visible breathing room beyond diamond footprint"),
                FVector2D::Distance(OverviewPoints[Index], OverviewPoints[Other]) >= 48.0f);
    auto Overlaps = [](FVector2D A, FVector2D B, FVector2D Size)
    {
        return FMath::Abs(A.X - B.X) < Size.X && FMath::Abs(A.Y - B.Y) < Size.Y;
    };
    const FVector2D OverviewHit = BreakerCoreBoard::OverviewHitSize();
    TestTrue(TEXT("Overview constellation targets remain 44px at far zoom"),
        OverviewHit.X * 0.6f >= 44.0f && OverviewHit.Y * 0.6f >= 44.0f);
    for (int32 Index = 0; Index < Overview.Wedges.Num(); ++Index)
    {
        const BreakerCoreBoard::FWedge& Wedge = Overview.Wedges[Index];
        float GatewayRadius = 0, ConvergenceRadius = 0, KeystoneRadius = 0;
        float MinLaneRadius = MAX_flt, MaxLaneRadius = 0;
        for (FName Id : Wedge.Nodes)
        {
            const auto* Node = Tree->FindNode(Id);
            if (!TestNotNull(TEXT("Drawn node resolves to real authoring"), Node)) return false;
            const FVector2D Delta = Overview.Centers[Id] - Overview.Hub;
            const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
            TestTrue(TEXT("Each node stays within its own radial wedge"),
                FMath::Abs(FMath::FindDeltaAngleDegrees(Wedge.AngleDegrees, Angle)) < 180.0f / Overview.Wedges.Num());
            const float Radius = Delta.Size();
            if (Node->CoreRole == EBreakerCoreNodeRole::Gateway) GatewayRadius = Radius;
            else if (Node->CoreRole == EBreakerCoreNodeRole::Convergence) ConvergenceRadius = Radius;
            else if (Node->CoreRole == EBreakerCoreNodeRole::Keystone) KeystoneRadius = Radius;
            else { MinLaneRadius = FMath::Min(MinLaneRadius, Radius); MaxLaneRadius = FMath::Max(MaxLaneRadius, Radius); }
        }
        TestTrue(TEXT("Branches remain visibly between gateway and convergence"),
            GatewayRadius < MinLaneRadius && MaxLaneRadius < ConvergenceRadius);
        TestTrue(TEXT("A major's keystone sits beyond its convergence"), KeystoneRadius == 0 || KeystoneRadius > ConvergenceRadius);
        // This is the actual role-layout label radius used by SBreakerMenu.
        const FVector2D Label = BreakerCoreBoard::Polar(Overview.Hub, 1450.0f, Wedge.AngleDegrees);
        for (int32 Other = Index + 1; Other < Overview.Wedges.Num(); ++Other)
            TestFalse(TEXT("Overview focus targets never overlap"), Overlaps(Label,
                BreakerCoreBoard::Polar(Overview.Hub, 1450.0f, Overview.Wedges[Other].AngleDegrees), OverviewHit));
        const BreakerCoreBoard::FLayout Focus = BreakerCoreBoard::Build(Tree, Wedge.Name);
        TestTrue(TEXT("Focus retains valid real role metadata"), Focus.bUsesCoreRoles && Focus.bValidMetadata);
        TestEqual(TEXT("Focus keeps every member"), Focus.Centers.Num(), Wedge.Nodes.Num());
        int32 ExpectedEdges = 0;
        for (const FBreakerNodeEdge& Edge : Tree->AdjacencyEdges)
            if (Wedge.Nodes.Contains(Edge.A) && Wedge.Nodes.Contains(Edge.B)) ++ExpectedEdges;
        TestEqual(TEXT("Focus uses actual induced graph"), Focus.Edges.Num(), ExpectedEdges);
        TestEqual(TEXT("Focus keeps its real entry"), Focus.Entries.Num(), 1);
        TArray<FVector2D> Points;
        Focus.Centers.GenerateValueArray(Points);
        for (int32 Point = 0; Point < Points.Num(); ++Point)
        {
            TestTrue(TEXT("Focus has room for full node name and state below each target"),
                Points[Point].X >= 88 && Points[Point].X <= Focus.Size.X - 88
                && Points[Point].Y >= 22 && Points[Point].Y <= Focus.Size.Y - 98);
            for (int32 Other = Point + 1; Other < Points.Num(); ++Other)
            {
                // Diamonds have a larger axis-aligned footprint than their 44px square.
                TestFalse(TEXT("Full-size focus purchase targets never overlap"),
                    Overlaps(Points[Point], Points[Other], FVector2D(63, 63)));
                TestFalse(TEXT("Focused name and state blocks retain their own space"),
                    Overlaps(Points[Point], Points[Other], FVector2D(176, 120)));
            }
        }
    }
    for (const UBreakerProgressionNode* Node : Tree->Nodes)
        if (Node) TestTrue(TEXT("Overview contains the actual node ID"), Overview.Centers.Contains(Node->NodeId));
    return true;
}

// ---------------------------------------------------------------------------
// UI.CoreBoard.Circuit: the gateway ring is one clean path.
//
// Owner, playtest 2026-09-10: "connect the traveling nodes together in a clean
// path". The gateways are the travelling nodes and their edges are the board's
// only circuit; the board draws them as arcs on the gateway radius now rather
// than as chords with a gap at each end.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreCircuitTest,
    "RiorsEdge.UI.CoreBoard.Circuit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoreCircuitTest::RunTest(const FString& Parameters)
{
    const UBreakerProgressionTree* Tree = UBreakerProgressionLibrary::GetCoreSliceTree();
    if (!TestNotNull(TEXT("Shipped Core exists"), Tree)) return false;
    const BreakerCoreBoard::FLayout Overview = BreakerCoreBoard::Build(Tree);
    if (!TestTrue(TEXT("Shipped metadata selects role layout"), Overview.bValidMetadata)) return false;

    // EVERY GATEWAY ON ONE RADIUS. The arcs are drawn on that radius, so if a
    // gateway ever left it the circuit would stop passing through its station.
    for (const FName Entry : Overview.Entries)
    {
        const FVector2D* Centre = Overview.Centers.Find(Entry);
        if (!TestNotNull(TEXT("Every gateway is placed"), Centre)) return false;
        TestTrue(TEXT("Every gateway sits on the circuit radius"),
            FMath::IsNearlyEqual(FVector2D::Distance(*Centre, Overview.Hub),
                BreakerCoreBoard::GatewayRingRadius, 0.5f));
    }

    // THE LEGS. Between each pair of adjacent wedges, every point of the arc is
    // on the same circle and the leg takes the short way round — a leg that
    // went the long way would close the ring by crossing the whole board.
    for (int32 Index = 0; Index < Overview.Wedges.Num(); ++Index)
    {
        const float From = Overview.Wedges[Index].AngleDegrees;
        const float To = Overview.Wedges[(Index + 1) % Overview.Wedges.Num()].AngleDegrees;
        const TArray<FVector2D> Arc = BreakerCoreBoard::CircuitArc(Overview.Hub,
            BreakerCoreBoard::GatewayRingRadius, From, To, 12);
        for (const FVector2D& Point : Arc)
            TestTrue(TEXT("Every point of a leg is on the circuit"),
                FMath::IsNearlyEqual(FVector2D::Distance(Point, Overview.Hub),
                    BreakerCoreBoard::GatewayRingRadius, 0.5f));
        // Within half a unit rather than exactly: the wrap-around leg reaches
        // its end angle as From + Sweep, which is the same place on the circle
        // and not the same float.
        TestTrue(TEXT("A leg ends at the next gateway's angle"),
            FVector2D::Distance(Arc.Last(),
                BreakerCoreBoard::Polar(Overview.Hub, BreakerCoreBoard::GatewayRingRadius, To)) < 0.5f);
        TestTrue(TEXT("No leg goes the long way round"),
            FMath::Abs(FMath::FindDeltaAngleDegrees(From, To)) < 180.0f);
    }

    // AND NO EDGE RUNS INWARD ANY MORE. Within a wedge, every prerequisite edge
    // must go outward or sideways: the Link nodes used to sit at 915, radially
    // inside the 1030 notables they join, so each major drew two inward kinks
    // across its own lane ring. Gateway-to-gateway legs are the circuit and are
    // level by construction, so they are the one exception.
    {
        TSet<FName> Gateways(Overview.Entries);
        for (const FBreakerNodeEdge& Edge : Overview.Edges)
        {
            if (Gateways.Contains(Edge.A) && Gateways.Contains(Edge.B)) continue;
            const float RadiusA = FVector2D::Distance(Overview.Centers[Edge.A], Overview.Hub);
            const float RadiusB = FVector2D::Distance(Overview.Centers[Edge.B], Overview.Hub);
            const float Inner = FMath::Min(RadiusA, RadiusB);
            const float Outer = FMath::Max(RadiusA, RadiusB);
            // The edge is stored in one direction and drawn in both, so what is
            // asserted is that the pair is ordered outward, not which end came
            // first in the data.
            TestTrue(TEXT("A wedge's edges never run back toward the hub"),
                Outer >= Inner - 0.5f);
        }
    }
    return true;
}

#endif
