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
    TestEqual(TEXT("Five sector atlas"), Overview.Sectors.Num(), 5);
    TestEqual(TEXT("Every shipped node appears"), Overview.Centers.Num(), 117);
    TestEqual(TEXT("Every actual edge appears"), Overview.Edges.Num(), 171);
    TestEqual(TEXT("Every virtual hub entry appears"), Overview.Entries.Num(), 12);
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
        const FVector2D Cluster = BreakerCoreBoard::Polar(Overview.Hub, 520, Wedge.AngleDegrees);
        for (FName Id : Wedge.Nodes)
            TestTrue(TEXT("Each constellation stays a compact circular cluster"),
                FVector2D::Distance(Cluster, Overview.Centers[Id]) <= 110.01f);
        const FVector2D Label = BreakerCoreBoard::Polar(Overview.Hub, BreakerCoreBoard::OverviewLabelRadius, Wedge.AngleDegrees);
        for (int32 Other = Index + 1; Other < Overview.Wedges.Num(); ++Other)
            TestFalse(TEXT("Overview focus targets never overlap"), Overlaps(Label,
                BreakerCoreBoard::Polar(Overview.Hub, BreakerCoreBoard::OverviewLabelRadius, Overview.Wedges[Other].AngleDegrees), OverviewHit));
        const BreakerCoreBoard::FLayout Focus = BreakerCoreBoard::Build(Tree, Wedge.Name);
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

#endif
