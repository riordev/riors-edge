#include "Misc/AutomationTest.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "UI/BreakerDoctrineBoardLayout.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerDoctrineBoardLayoutTest, "RiorsEdge.UI.DoctrineBoardLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerDoctrineBoardLayoutTest::RunTest(const FString& Parameters)
{
    const TArray<UBreakerProgressionTree*> Trees = {
        UBreakerProgressionLibrary::GetSwiftKineticTree(), UBreakerProgressionLibrary::GetSwiftMarksmanTree(), UBreakerProgressionLibrary::GetSwiftFrenzyTree(),
        UBreakerProgressionLibrary::GetCasterSpellbladeTree(), UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), UBreakerProgressionLibrary::GetCasterMultispellTree(),
        UBreakerProgressionLibrary::GetGunsmithArmoryTree(), UBreakerProgressionLibrary::GetGunsmithFieldTechTree(), UBreakerProgressionLibrary::GetGunsmithTinkererTree(),
        UBreakerProgressionLibrary::GetTankLeechTree(), UBreakerProgressionLibrary::GetTankBastionTree(), UBreakerProgressionLibrary::GetTankDemolitionistTree(),
        UBreakerProgressionLibrary::GetSupportMedicTree(), UBreakerProgressionLibrary::GetSupportConductorTree(), UBreakerProgressionLibrary::GetSupportWardenTree() };
    for (const auto* Tree : Trees)
    {
        if (!TestNotNull(TEXT("shipped Doctrine tree"), Tree)) return false;
        const FString Label = Tree->TreeId.ToString();
        const auto Layout = BreakerDoctrineBoardLayout::Build(Tree);
        const auto Repeat = BreakerDoctrineBoardLayout::Build(Tree);
        TestEqual(*(Label + TEXT(" contains every authored node, no synthetic hub node")), Layout.Centers.Num(), Tree->Nodes.Num());
        TArray<FName> Ids;
        Layout.Centers.GetKeys(Ids);
        int32 PrerequisiteCount = 0;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            const auto* Center = Layout.Centers.Find(Node->NodeId);
            if (!TestNotNull(*(Label + TEXT(" has node coordinate")), Center)) return false;
            TestTrue(*(Label + TEXT(" node coordinate is finite")), !Center->ContainsNaN());
            TestTrue(*(Label + TEXT(" marker bounds fit within board")), Center->X >= 32 && Center->Y >= 32 && Center->X + 32 <= Layout.Size.X && Center->Y + 32 <= Layout.Size.Y);
            TestTrue(*(Label + TEXT(" decorative hub does not overlap a marker")), FVector2D::Distance(*Center, Layout.Hub) >= 90);
            TestTrue(*(Label + TEXT(" layout is deterministic")), Center->Equals(Repeat.Centers.FindChecked(Node->NodeId), .001));
            for (const auto& Prerequisite : Node->Prerequisites)
            {
                ++PrerequisiteCount;
                TestTrue(*(Label + TEXT(" actual prerequisite edge is present")), Layout.Edges.ContainsByPredicate([&](const FBreakerNodeEdge& Edge) { return Edge.A == Prerequisite.NodeId && Edge.B == Node->NodeId; }));
            }
        }
        TestEqual(*(Label + TEXT(" contains no invented edges")), Layout.Edges.Num(), PrerequisiteCount);
        for (int32 A = 0; A < Ids.Num(); ++A)
            for (int32 B = A + 1; B < Ids.Num(); ++B)
                TestTrue(*(Label + TEXT(" all marker centers have comfortable spacing")), FVector2D::Distance(Layout.Centers.FindChecked(Ids[A]), Layout.Centers.FindChecked(Ids[B])) >= 90);
        AddInfo(FString::Printf(TEXT("DOCTRINE BOARD %s nodes%d edges%d size%.0fx%.0f"), *Label, Layout.Centers.Num(), Layout.Edges.Num(), Layout.Size.X, Layout.Size.Y));
    }
    TestEqual(TEXT("null tree has no fictional node"), BreakerDoctrineBoardLayout::Build(nullptr).Centers.Num(), 0);
    return true;
}
#endif
