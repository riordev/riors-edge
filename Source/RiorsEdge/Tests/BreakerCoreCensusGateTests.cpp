#include "Misc/AutomationTest.h"
#include "Data/BreakerCensus.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreCensusGateTest, "RiorsEdge.Data.Census.CoreGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreCensusGateTest::RunTest(const FString& Parameters)
{
    auto* Tree = NewObject<UBreakerProgressionTree>();
    Tree->TreeId = TEXT("Test.Census.Gates");
    Tree->EntryNodeIds = { TEXT("GateA"), TEXT("GateB") };
    Tree->AdjacencyEdges.Add({ TEXT("GateA"), TEXT("GateB") });
    Tree->bRestrictEntryToOwnedNeighbor = true;
    auto* Node = NewObject<UBreakerProgressionNode>(Tree);
    Node->NodeId = TEXT("Key");
    Node->RequiredConstellationInvestment = 18;
    Node->RequiredTreeInvestment = 20;
    Node->Constellation = TEXT("Vector"); Node->CoreRole = EBreakerCoreNodeRole::Keystone;
    Tree->CoreWedgeOrder = {TEXT("Vector")}; Tree->CoreWedgeSectors.Add(TEXT("Vector"), TEXT("Weapon"));
    FBreakerNodePrerequisiteGroup Group;
    Group.MinimumSatisfied = 2;
    Group.Candidates.Add({ TEXT("LaneA"), 3 });
    Group.Candidates.Add({ TEXT("LaneB"), 1 });
    Group.Candidates.Add({ TEXT("LaneC"), 1 });
    Node->PrerequisiteGroups.Add(Group);
    Tree->Nodes.Add(Node);
    const TSharedRef<FJsonObject> Census = BreakerCensus::Export({ Tree });
    const auto& Trees = Census->GetArrayField(TEXT("trees"));
    if (!TestEqual(TEXT("Fixture tree exported"), Trees.Num(), 1)) return false;
    const auto T = Trees[0]->AsObject();
    TestTrue(TEXT("Entry policy exported"), T->GetBoolField(TEXT("restrictEntryToOwnedNeighbor")));
    TestEqual(TEXT("Explicit wedge order exported"), T->GetArrayField(TEXT("coreWedgeOrder"))[0]->AsString(), FString(TEXT("Vector")));
    TestEqual(TEXT("New wedge sector uses authored map"), T->GetArrayField(TEXT("constellations"))[0]->AsObject()->GetStringField(TEXT("sector")), FString(TEXT("Weapon")));
    TestEqual(TEXT("Entry roster exported"), T->GetArrayField(TEXT("entryNodes")).Num(), 2);
    const auto& Edges = T->GetArrayField(TEXT("adjacencyEdges"));
    if (!TestEqual(TEXT("Actual adjacency exported"), Edges.Num(), 1)) return false;
    TestEqual(TEXT("Edge source retained"), Edges[0]->AsObject()->GetStringField(TEXT("a")), FString(TEXT("GateA")));
    TestEqual(TEXT("Edge destination retained"), Edges[0]->AsObject()->GetStringField(TEXT("b")), FString(TEXT("GateB")));
    const auto& Nodes = T->GetArrayField(TEXT("nodes"));
    if (!TestEqual(TEXT("Node exported"), Nodes.Num(), 1)) return false;
    const auto N = Nodes[0]->AsObject();
    TestEqual(TEXT("Role exported without tier inference"), N->GetStringField(TEXT("coreRole")), FString(TEXT("Keystone")));
    TestEqual(TEXT("Local gate kept separate from tree gate"), N->GetNumberField(TEXT("requiredConstellationInvestment")), 18.0);
    TestEqual(TEXT("Tree gate retained"), N->GetNumberField(TEXT("requiredTreeInvestment")), 20.0);
    const auto& Groups = N->GetArrayField(TEXT("prerequisiteGroups"));
    if (!TestEqual(TEXT("Counted group exported"), Groups.Num(), 1)) return false;
    const auto G = Groups[0]->AsObject();
    TestEqual(TEXT("Required completed lane count retained"), G->GetNumberField(TEXT("minimumSatisfied")), 2.0);
    const auto& Candidates = G->GetArrayField(TEXT("candidates"));
    if (!TestEqual(TEXT("All lane candidates exported"), Candidates.Num(), 3)) return false;
    TestEqual(TEXT("Rank requirement is not flattened to ownership"), Candidates[0]->AsObject()->GetNumberField(TEXT("rank")), 3.0);
    return true;
}
#endif
