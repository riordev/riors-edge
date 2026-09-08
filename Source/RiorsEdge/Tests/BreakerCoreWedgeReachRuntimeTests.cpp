#include "Misc/AutomationTest.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreWedgeReachRuntimeTest,"RiorsEdge.Progression.CoreWedgeReachRuntime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreWedgeReachRuntimeTest::RunTest(const FString& Parameters)
{
    for (int32 Case=0; Case<5; ++Case)
    {
        auto* Progression=NewObject<UBreakerProgressionComponent>();
        Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(10,Progression->ExperienceCurve));
        const int32 Budget=Progression->GetUnspentPoints(EBreakerPointCurrency::CorePoints);
        auto* Tree=NewObject<UBreakerProgressionTree>(); Tree->TreeId=TEXT("Test.WedgeReach");
        Tree->CoreWedgeOrder={TEXT("A"),TEXT("B"),TEXT("C"),TEXT("D")};
        auto Node=[](UBreakerProgressionTree* Owner,FName Id,FName Wedge)
        {
            auto* N=NewObject<UBreakerProgressionNode>(Owner); N->NodeId=Id; N->Constellation=Wedge;
            N->Currency=EBreakerPointCurrency::CorePoints; N->MaxRank=1; N->CostPerRank=1; Owner->Nodes.Add(N); return N;
        };
        for (FName Wedge:Tree->CoreWedgeOrder)
            Tree->EntryNodeIds.Add(Node(Tree,FName(*(TEXT("Test.Gateway.")+Wedge.ToString())),Wedge)->NodeId);
        const FName SourceWedge=Case==1?FName(TEXT("D")):Case==2?FName(TEXT("A")):FName(TEXT("C"));
        auto* Other=NewObject<UBreakerProgressionTree>(); Other->TreeId=TEXT("Test.WedgeReach.Foreign");
        // Earn the interior under a pre-metadata fixture, then enable the new
        // reach rule. This models restored interior ownership without forging
        // ranks, a wallet, or a free paid gateway in the source wedge.
        auto* Interior=Node(Case>=3?Other:Tree,TEXT("Test.Interior.Paid"),SourceWedge);
        FText Reason;
        if (!TestTrue(TEXT("Interior ownership is a real earned purchase"),Progression->PurchaseNode(Case>=3?Other:Tree,Interior->NodeId,Reason))) return false;
        if (Case==4)
        {
            auto* Wrong=Node(Tree,Interior->NodeId,SourceWedge); Wrong->Currency=EBreakerPointCurrency::DoctrinePoints;
        }
        Tree->bRestrictEntryToOwnedNeighbor=true;
        auto Can=[&](const TCHAR* Id) { return Progression->CanPurchaseNode(Tree,FName(Id),Reason); };
        if (Case==0)
        {
            TestTrue(TEXT("Interior C opens clockwise D gateway"),Can(TEXT("Test.Gateway.D")));
            TestTrue(TEXT("Interior C opens counterclockwise B gateway"),Can(TEXT("Test.Gateway.B")));
            TestFalse(TEXT("Interior C cannot open opposite A gateway"),Can(TEXT("Test.Gateway.A")));
            TestFalse(TEXT("Same wedge interior is not a neighboring wedge"),Can(TEXT("Test.Gateway.C")));
            if (!TestTrue(TEXT("Neighbor gateway actually debits earned wallet"),Progression->PurchaseNode(Tree,TEXT("Test.Gateway.D"),Reason))) return false;
            TestEqual(TEXT("Interior and neighbor cost two points"),Progression->GetUnspentPoints(Tree->Currency),Budget-2);
        }
        else if (Case==1) TestTrue(TEXT("Last wedge interior wraps to first gateway"),Can(TEXT("Test.Gateway.A")));
        else if (Case==2) TestTrue(TEXT("First wedge interior wraps to last gateway"),Can(TEXT("Test.Gateway.D")));
        else
        {
            TestFalse(TEXT("Foreign or wrong-currency ownership cannot open B"),Can(TEXT("Test.Gateway.B")));
            TestFalse(TEXT("Foreign or wrong-currency ownership cannot open D"),Can(TEXT("Test.Gateway.D")));
        }
        if (!TestTrue(TEXT("Native full respec clears reach"),Progression->RespecCore(Reason))) return false;
        TestEqual(TEXT("Respec restores earned wallet"),Progression->GetUnspentPoints(Tree->Currency),Budget);
        TestTrue(TEXT("Empty Core permits any first gateway again"),Can(TEXT("Test.Gateway.A")));
    }
    return true;
}
#endif
