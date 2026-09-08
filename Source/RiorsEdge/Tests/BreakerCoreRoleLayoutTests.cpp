#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "UI/BreakerCoreBoardLayout.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRoleLayoutTest, "RiorsEdge.UI.CoreRoleLayout187",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRoleLayoutTest::RunTest(const FString& Parameters)
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
    TestEqual(TEXT("Exact separate proposed fixture node count"),Tree->Nodes.Num(),187);
    TestEqual(TEXT("Exact offered rank cost"),Offered,429);
    auto CheckGeometry = [&](const BreakerCoreBoard::FLayout& Layout, FVector2D Bounds)
    {
        TestTrue(TEXT("Complete metadata accepted"),Layout.bValidMetadata);
        TArray<FVector2D> Points; Layout.Centers.GenerateValueArray(Points);
        for (int32 I=0; I<Points.Num(); ++I)
        {
            const FVector2D P=Points[I];
            TestTrue(TEXT("Draw bounds stay within board"),FMath::IsFinite(P.X)&&FMath::IsFinite(P.Y)
                && P.X>=Bounds.X*.5f && P.Y>=Bounds.Y*.5f && P.X+Bounds.X*.5f<=Layout.Size.X && P.Y+Bounds.Y*.5f<=Layout.Size.Y);
            for (int32 J=I+1; J<Points.Num(); ++J)
                TestFalse(TEXT("Node draw bounds do not overlap"),FMath::Abs(P.X-Points[J].X)<Bounds.X && FMath::Abs(P.Y-Points[J].Y)<Bounds.Y);
        }
        for (const auto& E:Layout.Edges) TestTrue(TEXT("Every drawn edge has actual endpoints"),Layout.Centers.Contains(E.A)&&Layout.Centers.Contains(E.B));
    };
    const auto Overview=BreakerCoreBoard::Build(Tree);
    TestTrue(TEXT("Explicit metadata selects radial path"),Overview.bUsesCoreRoles);
    TestEqual(TEXT("Overview includes every node"),Overview.Centers.Num(),187);
    TestEqual(TEXT("All clockwise wedges retained"),Overview.Wedges.Num(),22);
    TestEqual(TEXT("Six explicitly authored sectors"),Overview.Sectors.Num(),6);
    TestEqual(TEXT("All gateway entries retained"),Overview.Entries.Num(),22);
    TestEqual(TEXT("Overview preserves every actual graph edge"),Overview.Edges.Num(),Tree->AdjacencyEdges.Num());
    CheckGeometry(Overview,FVector2D(44,44)); // Overview icons; full labels are hover/focus details.
    for (FName Wedge:Tree->CoreWedgeOrder)
    {
        const auto Focus=BreakerCoreBoard::Build(Tree,Wedge);
        TestEqual(TEXT("Focused graph preserves distinct ranked nodes"),Focus.Centers.Num(),Majors.Contains(Wedge)?11:6);
        CheckGeometry(Focus,FVector2D(176,120));
    }
    Tree->Nodes[1]->CoreRole=EBreakerCoreNodeRole::Legacy;
    const auto Invalid=BreakerCoreBoard::Build(Tree);
    TestFalse(TEXT("Partial authored metadata is refused"),Invalid.bValidMetadata);
    TestEqual(TEXT("Invalid graph is not silently guessed"),Invalid.Centers.Num(),0);
    Tree->CoreWedgeOrder.Reset();
    TestFalse(TEXT("Empty opt-in preserves legacy path"),BreakerCoreBoard::Build(Tree).bUsesCoreRoles);
    return true;
}
#endif
