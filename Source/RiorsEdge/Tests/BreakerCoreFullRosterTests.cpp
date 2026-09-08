#include "Misc/AutomationTest.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerWorldPoints.h"
#include "Save/BreakerQuestJournal.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace BreakerFullCoreFixture
{
    const TArray<FName> Order={TEXT("Precision"),TEXT("Vector"),TEXT("Ballistics"),TEXT("Loadout"),
        TEXT("Aegis"),TEXT("Bulwark"),TEXT("Constitution"),TEXT("Ward"),TEXT("Recovery"),
        TEXT("Arc"),TEXT("Tempo"),TEXT("Reservoir"),TEXT("Duration"),
        TEXT("Affliction"),TEXT("Entropy"),TEXT("Reaction"),TEXT("Rift"),TEXT("Void"),
        TEXT("Velocity"),TEXT("Kinesis"),TEXT("Control"),TEXT("Threat")};
    bool IsMajor(FName Wedge)
    {
        return Wedge==TEXT("Precision") || Wedge==TEXT("Vector") || Wedge==TEXT("Ballistics")
            || Wedge==TEXT("Arc") || Wedge==TEXT("Tempo") || Wedge==TEXT("Affliction")
            || Wedge==TEXT("Entropy") || Wedge==TEXT("Reaction") || Wedge==TEXT("Aegis")
            || Wedge==TEXT("Bulwark") || Wedge==TEXT("Velocity");
    }
    const UBreakerProgressionNode* Find(const UBreakerProgressionTree* Tree,FName Wedge,EBreakerCoreNodeRole Role,int32 Lane=0)
    {
        for (const UBreakerProgressionNode* N:Tree->Nodes)
            if (N->Constellation==Wedge && N->CoreRole==Role && N->CoreLaneIndex==Lane) return N;
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterAuthoringTest,"RiorsEdge.Progression.CoreRoster.Authoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterAuthoringTest::RunTest(const FString&)
{
    using namespace BreakerFullCoreFixture;
    FString Error; auto* Tree=BreakerCoreRoster::BuildCandidate(GetTransientPackage(),Error);
    if (!TestNotNull(TEXT("Complete candidate builds"),Tree)) return false;
    TestTrue(TEXT("Exact accepted clockwise wedge order"),Tree->CoreWedgeOrder==Order);
    TestEqual(TEXT("Full candidate has 187 nodes"),Tree->Nodes.Num(),187);
    TestEqual(TEXT("Full candidate has 22 entry nodes"),Tree->EntryNodeIds.Num(),22);
    TestEqual(TEXT("Full candidate has 242 authored graph edges"),Tree->AdjacencyEdges.Num(),242);
    TSet<FName> Ids; int32 Offered=0,Majors=0;
    for (FName Wedge:Order)
    {
        const bool bMajor=IsMajor(Wedge); Majors+=bMajor?1:0;
        int32 Count=0,Cost=0;
        for (const UBreakerProgressionNode* N:Tree->Nodes) if (N->Constellation==Wedge)
        {
            ++Count; Cost+=N->CostPerRank*N->MaxRank;
            TestFalse(TEXT("Unique full node ID"),Ids.Contains(N->NodeId)); Ids.Add(N->NodeId);
            TestTrue(TEXT("Node ID retains named wedge"),N->NodeId.ToString().StartsWith(FString(TEXT("Core."))+Wedge.ToString()+TEXT(".")));
            TestFalse(TEXT("Literal name present"),N->DisplayName.IsEmpty()); TestFalse(TEXT("Description present"),N->Description.IsEmpty());
            TestTrue(TEXT("Every authored node has an effect or actual rule grant"),!N->Effects.IsEmpty() || !N->GrantedTags.IsEmpty() || !N->GrantedAbilities.IsEmpty());
            TestTrue(TEXT("Core remains generic across classes"),N->RequiredClass==EBreakerClassId::None);
            TestTrue(TEXT("Only Core wallet"),N->Currency==EBreakerPointCurrency::CorePoints);
            TestEqual(TEXT("No hidden global investment gate"),N->RequiredTreeInvestment,0);
            const bool bRanked=N->CoreRole==EBreakerCoreNodeRole::LaneMinor;
            TestEqual(TEXT("Exactly lane minors retain three ranks"),N->MaxRank,bRanked?3:1);
            const int32 Price=N->CoreRole==EBreakerCoreNodeRole::Keystone?5:
                N->CoreRole==EBreakerCoreNodeRole::Convergence?(bMajor?3:2):N->CoreRole==EBreakerCoreNodeRole::LaneNotable?2:1;
            TestEqual(TEXT("Role price matches accepted shape"),N->CostPerRank,Price);
            TestEqual(TEXT("Only keystones require eighteen local points"),N->RequiredConstellationInvestment,N->CoreRole==EBreakerCoreNodeRole::Keystone?18:0);
            for (const auto& P:N->Prerequisites) TestEqual(TEXT("Direct routes open at rank one"),P.RequiredRank,1);
            for (const auto& Group:N->PrerequisiteGroups)
                for (const auto& P:Group.Candidates) TestEqual(TEXT("Grouped routes open at rank one"),P.RequiredRank,1);
        }
        TestEqual(TEXT("Named wedge node count"),Count,bMajor?11:6);
        TestEqual(TEXT("Named wedge offered cost"),Cost,bMajor?26:13); Offered+=Cost;
        const auto* Gateway=Find(Tree,Wedge,EBreakerCoreNodeRole::Gateway);
        TestTrue(TEXT("Actual gateway is declared entry"),Gateway && Tree->EntryNodeIds.Contains(Gateway->NodeId));
    }
    TestEqual(TEXT("Eleven majors retained"),Majors,11);
    TestEqual(TEXT("Complete candidate offers 429 points"),Offered,429);
    TSet<FString> Edges;
    for (const auto& E:Tree->AdjacencyEdges)
    {
        TestTrue(TEXT("Edge endpoints exist"),Ids.Contains(E.A)&&Ids.Contains(E.B));
        TestTrue(TEXT("No self edge"),E.A!=E.B);
        FString A=E.A.ToString(),B=E.B.ToString(); if (A.Compare(B)>0) Swap(A,B);
        const FString Key=A+TEXT("|")+B; TestFalse(TEXT("No duplicate undirected edge"),Edges.Contains(Key)); Edges.Add(Key);
    }
    for (int32 I=0;I<Order.Num();++I)
    {
        const auto* A=Find(Tree,Order[I],EBreakerCoreNodeRole::Gateway);
        const auto* B=Find(Tree,Order[(I+1)%Order.Num()],EBreakerCoreNodeRole::Gateway);
        if (!A||!B) continue;
        TestTrue(TEXT("Neighbor gateways form closed ring including final wrap"),Tree->AdjacencyEdges.ContainsByPredicate([&](const FBreakerNodeEdge& E)
            {return (E.A==A->NodeId&&E.B==B->NodeId)||(E.B==A->NodeId&&E.A==B->NodeId);}));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreRosterRingReachabilityTest,"RiorsEdge.Progression.CoreRoster.RingReachability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreRosterRingReachabilityTest::RunTest(const FString&)
{
    using namespace BreakerFullCoreFixture;
    FString Error; auto* Tree=BreakerCoreRoster::BuildCandidate(GetTransientPackage(),Error);
    if (!TestNotNull(TEXT("Full candidate builds"),Tree)) return false;
    if (!TestTrue(TEXT("Full ring is assembled before validation"),Tree->CoreWedgeOrder==Order)) return false;
    // Cap entitlement: fifty level points plus fifteen idempotent world-source
    // claims. This exercises the grant API, not those sources' encounter triggers.
    auto Fund = [&](UBreakerProgressionComponent* P)
    {
        P->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(50,P->ExperienceCurve));
        auto* Journal=NewObject<UBreakerQuestJournal>();
        for (const auto& Source:UBreakerWorldPointLibrary::GetSources())
            if (!TestTrue(TEXT("Native world-source entitlement"),P->GrantWorldPoint(Source.SourceId,Journal))) return false;
        return TestEqual(TEXT("Full cap entitlement is exactly sixty-five"),P->GetUnspentPoints(Tree->Currency),65);
    };
    for (int32 Start=0;Start<Order.Num();++Start)
    {
        auto* P=NewObject<UBreakerProgressionComponent>();
        if (!Fund(P)) return false;
        FText Reason;
        for (int32 I=0;I<Order.Num();++I)
        {
            const auto* G=Find(Tree,Order[I],EBreakerCoreNodeRole::Gateway);
            if (!TestNotNull(TEXT("Named gateway exists"),G)) return false;
            TestTrue(TEXT("Every gateway is a legal first choice"),P->CanPurchaseNode(Tree,G->NodeId,Reason));
        }
        const auto* G=Find(Tree,Order[Start],EBreakerCoreNodeRole::Gateway);
        const auto* Minor=Find(Tree,Order[Start],EBreakerCoreNodeRole::LaneMinor);
        const auto* Notable=Find(Tree,Order[Start],EBreakerCoreNodeRole::LaneNotable);
        if (!G||!Minor||!Notable) return false;
        if (!TestTrue(TEXT("Earned initial gateway purchase"),P->PurchaseNode(Tree,G->NodeId,Reason))) return false;
        for (int32 I=0;I<Order.Num();++I) if (I!=Start)
        {
            const bool Neighbor=I==(Start+1)%Order.Num() || I==(Start+Order.Num()-1)%Order.Num();
            TestEqual(TEXT("Only actual neighbors open from the first wedge"),P->CanPurchaseNode(Tree,Find(Tree,Order[I],EBreakerCoreNodeRole::Gateway)->NodeId,Reason),Neighbor);
        }
        TestFalse(TEXT("Rank zero still refuses notable"),P->CanPurchaseNode(Tree,Notable->NodeId,Reason));
        if (!P->PurchaseNode(Tree,Minor->NodeId,Reason)) return false;
        TestTrue(TEXT("One purchased rank opens onward lane"),P->CanPurchaseNode(Tree,Notable->NodeId,Reason));
        if (!P->PurchaseNode(Tree,Notable->NodeId,Reason)) return false;
        TestTrue(TEXT("Optional second rank remains available"),P->CanPurchaseNode(Tree,Minor->NodeId,Reason));
        for (int32 Step=1;Step<Order.Num();++Step)
        {
            const FName Next=Order[(Start+Step)%Order.Num()];
            const auto* NextGateway=Find(Tree,Next,EBreakerCoreNodeRole::Gateway);
            if (!TestTrue(TEXT("Clockwise route buys every neighbor with earned points"),P->PurchaseNode(Tree,NextGateway->NodeId,Reason))) return false;
        }
        TestEqual(TEXT("Every possible start reaches all 22 wedges for 25 points including one lane"),P->GetUnspentPoints(Tree->Currency),40);
        for (FName Wedge:Order) TestEqual(TEXT("Each named gateway is actually owned"),P->GetNodeRank(Find(Tree,Wedge,EBreakerCoreNodeRole::Gateway)->NodeId,Tree->Currency),1);
    }
    // Adjacent majors provide a cheapest-route witness without inventing
    // travel taxes: two keystones fit, while a third costs 69 of 65 points.
    auto* Budget=NewObject<UBreakerProgressionComponent>();
    if (!Fund(Budget)) return false;
    FText Reason;
    for (int32 I=0;I<3;++I)
    {
        const FName Wedge=Order[I];
        auto Buy=[&](EBreakerCoreNodeRole Role,int32 Lane=0)
        {
            const auto* N=Find(Tree,Wedge,Role,Lane);
            return N && TestTrue(TEXT("Cheapest major route uses earned purchases"),Budget->PurchaseNode(Tree,N->NodeId,Reason));
        };
        if (!Buy(EBreakerCoreNodeRole::Gateway)) return false;
        for (int32 Lane=0;Lane<3;++Lane)
        {
            for (int32 Rank=0;Rank<(Lane==2?2:3);++Rank) if (!Buy(EBreakerCoreNodeRole::LaneMinor,Lane)) return false;
            if (!Buy(EBreakerCoreNodeRole::LaneNotable,Lane)) return false;
        }
        if (!Buy(EBreakerCoreNodeRole::Convergence)) return false;
        TestEqual(TEXT("Cheapest keystone route reaches eighteen local points"),Budget->GetConstellationInvestment(Tree,Wedge),18);
        if (I<2) {if (!Buy(EBreakerCoreNodeRole::Keystone)) return false;}
        else TestFalse(TEXT("Third keystone is four points beyond the actual budget"),Budget->CanPurchaseNode(Tree,Find(Tree,Wedge,EBreakerCoreNodeRole::Keystone)->NodeId,Reason));
    }
    TestEqual(TEXT("Two keystones plus third eighteen-point gate spend sixty-four"),Budget->GetUnspentPoints(Tree->Currency),1);
    return true;
}
#endif
