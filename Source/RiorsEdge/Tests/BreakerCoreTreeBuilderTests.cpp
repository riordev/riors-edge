#include "Misc/AutomationTest.h"
#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreTreeBuilderTest,"RiorsEdge.Progression.CoreTreeBuilder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreTreeBuilderTest::RunTest(const FString& Parameters)
{
    // Four caller-supplied fixture wedges prove construction, not shipped
    // roster/effect completeness. Real level-earned purchases test its gates.
    TArray<FBreakerCoreWedgeDefinition> Wedges;
    for (int32 I=0; I<4; ++I)
    {
        FBreakerCoreWedgeDefinition W; W.Id=FName(*FString::Printf(TEXT("Fixture%d"),I)); W.Sector=TEXT("Fixture"); W.bMajor=I==0;
        auto Node = [&](const FString& Suffix)
        {
            auto* N=NewObject<UBreakerProgressionNode>();
            N->NodeId=FName(*(W.Id.ToString()+TEXT(".")+Suffix)); N->DisplayName=FText::FromString(Suffix);
            N->Description=FText::FromString(TEXT("Caller-authored fixture"));
            // An existing supported effect verifies copying; it is not created
            // by the builder and does not stand in for any unimplemented rule.
            FBreakerNodeEffect E; E.StatTarget=EBreakerNodeStatTarget::Health; E.StatBucket=EBreakerNodeStatBucket::Flat; E.ValuePerRank=1;
            N->Effects.Add(E); N->RequiredTreeInvestment=99; return N;
        };
        W.Gateway=Node(TEXT("Gateway")); W.Convergence=Node(TEXT("Convergence"));
        for (int32 Lane=0; Lane<(W.bMajor?3:2); ++Lane)
            W.Lanes.Add({Node(FString::Printf(TEXT("Minor%d"),Lane)),Node(FString::Printf(TEXT("Notable%d"),Lane))});
        if (W.bMajor)
        { W.Links={Node(TEXT("Link0")),Node(TEXT("Link1"))}; W.Keystone=Node(TEXT("Keystone")); }
        Wedges.Add(W);
    }
    FString Error;
    auto* Tree=BreakerCoreTree::Build(GetTransientPackage(),TEXT("Test.Core.Builder"),FText::FromString(TEXT("Fixture")),Wedges,Error);
    if (!TestNotNull(TEXT("Valid definitions build"),Tree)) return false;
    TestEqual(TEXT("Major plus three minor node count"),Tree->Nodes.Num(),29);
    int32 Offered=0;
    for (const UBreakerProgressionNode* N:Tree->Nodes)
    {
        Offered+=N->CostPerRank*N->MaxRank;
        TestEqual(TEXT("Visual tiers add no global spend gate"),N->RequiredTreeInvestment,0);
        TestEqual(TEXT("Authored effect copied unchanged"),N->Effects.Num(),1);
        TestEqual(TEXT("Authored description retained"),N->Description.ToString(),FString(TEXT("Caller-authored fixture")));
        TestTrue(TEXT("Output node owned by output tree"),N->GetOuter()==Tree);
        for (const auto& Prerequisite : N->Prerequisites)
            TestEqual(TEXT("Builder paths require only rank one"),Prerequisite.RequiredRank,1);
        for (const auto& Group : N->PrerequisiteGroups)
            for (const auto& Candidate : Group.Candidates)
                TestEqual(TEXT("Builder lane groups require only rank one"),Candidate.RequiredRank,1);
    }
    TestEqual(TEXT("Shape-derived offered cost"),Offered,65);
    TestEqual(TEXT("Builder does not mutate caller node"),Wedges[0].Gateway->RequiredTreeInvestment,99);
    auto* Progression=NewObject<UBreakerProgressionComponent>();
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(29,Progression->ExperienceCurve));
    const int32 Budget=Progression->GetUnspentPoints(Tree->Currency);
    FText Reason;
    auto Buy=[&](const TCHAR* Id) { return TestTrue(FString(TEXT("Earned purchase "))+Id,Progression->PurchaseNode(Tree,FName(Id),Reason)); };
    if (!Buy(TEXT("Fixture0.Gateway"))) return false;
    TestFalse(TEXT("Opposite gateway requires a neighbor"),Progression->CanPurchaseNode(Tree,TEXT("Fixture2.Gateway"),Reason));
    TestTrue(TEXT("Clockwise gateway reachable"),Progression->CanPurchaseNode(Tree,TEXT("Fixture1.Gateway"),Reason));
    TestTrue(TEXT("Counterclockwise gateway reachable"),Progression->CanPurchaseNode(Tree,TEXT("Fixture3.Gateway"),Reason));
    for (int32 Lane=0; Lane<2; ++Lane)
    {
        const FString Minor=FString::Printf(TEXT("Fixture0.Minor%d"),Lane), Notable=FString::Printf(TEXT("Fixture0.Notable%d"),Lane);
        TestFalse(TEXT("Unowned minor still blocks notable"),Progression->CanPurchaseNode(Tree,FName(*Notable),Reason));
        if (!Buy(*Minor)) return false;
        TestTrue(TEXT("Rank one unlocks onward travel"),Progression->CanPurchaseNode(Tree,FName(*Notable),Reason));
        if (!Buy(*Notable)) return false;
        if (Lane==0) TestFalse(TEXT("One complete lane does not unlock convergence"),Progression->CanPurchaseNode(Tree,TEXT("Fixture0.Convergence"),Reason));
    }
    if (!Buy(TEXT("Fixture0.Convergence"))) return false;
    TestEqual(TEXT("Two rank-one lanes reach major convergence for ten points"),Progression->GetConstellationInvestment(Tree,TEXT("Fixture0")),10);
    TestFalse(TEXT("Cheap convergence route cannot bypass eighteen-point keystone gate"),Progression->CanPurchaseNode(Tree,TEXT("Fixture0.Keystone"),Reason));
    for (int32 Lane=0; Lane<2; ++Lane)
    {
        const FString Minor=FString::Printf(TEXT("Fixture0.Minor%d"),Lane);
        if (!Buy(*Minor)||!Buy(*Minor)) return false;
        TestEqual(TEXT("Optional investment still reaches rank three"),Progression->GetNodeRank(FName(*Minor),Tree->Currency),3);
        TestFalse(TEXT("Fourth minor rank remains refused"),Progression->CanPurchaseNode(Tree,FName(*Minor),Reason));
    }
    for (int32 Rank=0;Rank<3;++Rank) if (!Buy(TEXT("Fixture0.Minor2"))) return false;
    TestEqual(TEXT("Seventeen local points before link"),Progression->GetConstellationInvestment(Tree,TEXT("Fixture0")),17);
    TestFalse(TEXT("Keystone cannot count its own five-point cost toward gate"),Progression->CanPurchaseNode(Tree,TEXT("Fixture0.Keystone"),Reason));
    if (!Buy(TEXT("Fixture0.Link0"))||!Buy(TEXT("Fixture0.Keystone"))) return false;
    TestEqual(TEXT("Keystone route costs twenty-three"),Progression->GetUnspentPoints(Tree->Currency),Budget-23);
    if (!TestTrue(TEXT("Real free low-level respec"),Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("All rank costs refunded"),Progression->GetUnspentPoints(Tree->Currency),Budget);
    auto Invalid=Wedges; Invalid[1].Gateway=Invalid[0].Gateway;
    TestNull(TEXT("Duplicate input identity refuses"),BreakerCoreTree::Build(GetTransientPackage(),TEXT("Bad"),{},Invalid,Error));
    TestFalse(TEXT("Refusal explains defect"),Error.IsEmpty());
    Invalid=Wedges; Invalid[0].Lanes.Pop();
    TestNull(TEXT("Malformed major shape refuses"),BreakerCoreTree::Build(GetTransientPackage(),TEXT("Bad"),{},Invalid,Error));
    return true;
}
#endif
