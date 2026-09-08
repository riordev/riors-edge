#include "Misc/AutomationTest.h"
#include "Progression/BreakerCoreRoster.h"
#include "Progression/BreakerCoreTree.h"
#include "Progression/BreakerExperience.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreStatusRosterTest, "RiorsEdge.Progression.CoreRoster.Status",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreStatusRosterTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto* Tree = BreakerCoreRoster::BuildCandidate(GetTransientPackage(), Error);
    if (!TestNotNull(*Error, Tree)) return false;
    TArray<FName> StatusOrder;
    for (FName Wedge : Tree->CoreWedgeOrder)
        if (Tree->CoreWedgeSectors.FindRef(Wedge) == TEXT("Status")) StatusOrder.Add(Wedge);
    const TArray<FName> ExpectedOrder{TEXT("Affliction"), TEXT("Entropy"), TEXT("Reaction"), TEXT("Rift"), TEXT("Void")};
    TestTrue(TEXT("Status preserves all five accepted wedges in order"), StatusOrder == ExpectedOrder);
    int32 Nodes = 0, Offered = 0, Ranked = 0, Keys = 0;
    for (const UBreakerProgressionNode* N : Tree->Nodes)
    {
        if (!ExpectedOrder.Contains(N->Constellation)) continue;
        ++Nodes; Offered += N->MaxRank * N->CostPerRank;
        TestFalse(TEXT("Authored Status identity has descriptive text"), N->Description.IsEmpty());
        if (N->CoreRole == EBreakerCoreNodeRole::LaneMinor)
        {
            ++Ranked;
            TestEqual(TEXT("Status minors retain three ranks"), N->MaxRank, 3);
            TestEqual(TEXT("Each minor rank costs one"), N->CostPerRank, 1);
        }
        if (N->CoreRole == EBreakerCoreNodeRole::Keystone)
        {
            ++Keys;
            TestEqual(TEXT("Status keystones require eighteen prior local points"), N->RequiredConstellationInvestment, 18);
            TestEqual(TEXT("Status keystones cost five"), N->CostPerRank, 5);
        }
    }
    TestEqual(TEXT("Status authored identities"), Nodes, 45);
    TestEqual(TEXT("Status fully ranked offered points"), Offered, 104);
    TestEqual(TEXT("Status three-rank nodes"), Ranked, 13);
    TestEqual(TEXT("Status major keystones"), Keys, 3);
    auto CheckEffect = [&](const TCHAR* Id, EBreakerNodeStatTarget Target, EBreakerNodeStatBucket Bucket, float Value)
    {
        const auto* N = Tree->FindNode(FName(Id));
        if (!TestNotNull(Id, N)) return false;
        if (!TestEqual(TEXT("Exact single authored effect"), N->Effects.Num(), 1)) return false;
        TestTrue(TEXT("Exact target and bucket"), N->Effects[0].StatTarget == Target && N->Effects[0].StatBucket == Bucket);
        TestEqual(TEXT("Exact per-rank magnitude"), N->Effects[0].ValuePerRank, Value);
        return true;
    };
    using Target = EBreakerNodeStatTarget;
    using Bucket = EBreakerNodeStatBucket;
    if (!CheckEffect(TEXT("Core.Affliction.Compound"), Target::DamageOverTime, Bucket::MorePercent, 24)
        || !CheckEffect(TEXT("Core.Entropy.Cascade"), Target::ElementalDamage, Bucket::MorePercent, 22)
        || !CheckEffect(TEXT("Core.Reaction.Resonance"), Target::ReactionDamage, Bucket::MorePercent, 20)
        || !CheckEffect(TEXT("Core.Void.NothingLeft"), Target::VoidDamage, Bucket::MorePercent, 18)
        || !CheckEffect(TEXT("Core.Reaction.Residue"), Target::ReactionResiduePercent, Bucket::Flat, 10)
        || !CheckEffect(TEXT("Core.Entropy.Threshold"), Target::ElementalThresholdReduction, Bucket::Flat, 15)
        || !CheckEffect(TEXT("Core.Entropy.Penetrance"), Target::ElementalBuildupPenetration, Bucket::Flat, 10)) return false;
    const auto* Chain = Tree->FindNode(TEXT("Core.Reaction.Chain"));
    if (!TestNotNull(TEXT("Chain notable"), Chain)) return false;
    TestTrue(TEXT("Chain selects the funded native reaction consumer"), Chain->GrantedTags.HasTagExact(
        FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Reaction.Chain"))));
    // These assertions describe authored rule identities, not implemented behavior.
    // Their missing consumers remain explicit at each production authoring site.
    for (const TCHAR* Suffix : {TEXT("SecondOrder"), TEXT("Overlap"), TEXT("Sympathetic")})
    {
        const auto* N = Tree->FindNode(FName(*(FString(TEXT("Core.Reaction.")) + Suffix)));
        if (!TestNotNull(Suffix, N)) return false;
        TestTrue(TEXT("Pending rule has a registered ownership identity"), N->GrantedTags.HasTagExact(
            FGameplayTag::RequestGameplayTag(FName(*(FString(TEXT("Progression.Node.Core.Reaction.")) + Suffix)))));
        TestTrue(TEXT("Pending rule has no fabricated substitute stat"), N->Effects.IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerCoreStatusPaidRouteTest, "RiorsEdge.Progression.CoreRoster.StatusPaidRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerCoreStatusPaidRouteTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto* Tree = BreakerCoreRoster::BuildCandidate(GetTransientPackage(), Error);
    if (!TestNotNull(*Error, Tree)) return false;
    auto* Progression = NewObject<UBreakerProgressionComponent>();
    auto* Class = NewObject<UBreakerClassDefinition>();
    Class->ClassId = EBreakerClassId::Caster; Class->BranchTrees.Add(Tree);
    if (!TestTrue(TEXT("Candidate is the selected class Core"), Progression->ChoosePermanentClass(Class))) return false;
    Progression->AwardExperience(UBreakerExperienceLibrary::TotalXpToReachLevel(29, Progression->ExperienceCurve));
    const int32 Budget = Progression->GetUnspentPoints(Tree->Currency);
    FText Reason;
    auto Buy = [&](const TCHAR* Id)
    { return TestTrue(FString(TEXT("Real candidate purchase: ")) + Id, Progression->PurchaseNode(Tree, FName(Id), Reason)); };
    if (!Buy(TEXT("Core.Entropy.Attunement"))) return false;
    TestFalse(TEXT("Entropy cannot directly enter non-neighbor Void"), Progression->CanPurchaseNode(Tree, TEXT("Core.Void.Erase"), Reason));
    TestTrue(TEXT("Entropy can enter Affliction"), Progression->CanPurchaseNode(Tree, TEXT("Core.Affliction.OpenWound"), Reason));
    TestTrue(TEXT("Entropy can enter Reaction"), Progression->CanPurchaseNode(Tree, TEXT("Core.Reaction.Catalysis"), Reason));
    if (!Buy(TEXT("Core.Entropy.Catalyst")) || !Buy(TEXT("Core.Entropy.Threshold"))) return false;
    TestFalse(TEXT("One lane does not unlock Cascade"), Progression->CanPurchaseNode(Tree, TEXT("Core.Entropy.Cascade"), Reason));
    if (!Buy(TEXT("Core.Entropy.Decay")) || !Buy(TEXT("Core.Entropy.Density")) || !Buy(TEXT("Core.Entropy.Cascade"))) return false;
    TestEqual(TEXT("Two rank-one lanes reach Cascade in ten points"), Progression->GetConstellationInvestment(Tree, TEXT("Entropy")), 10);
    TestFalse(TEXT("Cheap convergence cannot buy Long Dark"), Progression->CanPurchaseNode(Tree, TEXT("Core.Entropy.LongDark"), Reason));
    TestTrue(TEXT("Purchased Density reaches existing aggregator"), Progression->GetNodeStats().bRotDensity);
    TestTrue(TEXT("Purchased threshold reduces threshold fifteen percent"), FMath::IsNearlyEqual(Progression->GetNodeStats().ElementalThresholdMultiplier, .85f));
    TestEqual(TEXT("Rank-one Decay increases Rot by seven"), Progression->GetNodeStats().RotDamageIncreasedPercent, 7.0f);
    for (int32 Rank = 0; Rank < 2; ++Rank)
        if (!Buy(TEXT("Core.Entropy.Catalyst")) || !Buy(TEXT("Core.Entropy.Decay"))) return false;
    for (int32 Rank = 0; Rank < 3; ++Rank)
        if (!Buy(TEXT("Core.Entropy.Conductive"))) return false;
    TestEqual(TEXT("Optional depth reaches seventeen prior local points"), Progression->GetConstellationInvestment(Tree, TEXT("Entropy")), 17);
    TestFalse(TEXT("Seventeen still refuses the keystone"), Progression->CanPurchaseNode(Tree, TEXT("Core.Entropy.LongDark"), Reason));
    if (!Buy(TEXT("Core.Entropy.Penetrance")) || !Buy(TEXT("Core.Entropy.LongDark"))) return false;
    TestEqual(TEXT("Actual candidate keystone costs twenty-three"), Progression->GetUnspentPoints(Tree->Currency), Budget - 23);
    TestTrue(TEXT("Long Dark ownership reaches persistent Rot primitive"), Progression->GetNodeStats().bLongDark);
    TestEqual(TEXT("Three paid Decay ranks contribute twenty-one"), Progression->GetNodeStats().RotDamageIncreasedPercent, 21.0f);
    TestFalse(TEXT("Fourth Decay rank refused"), Progression->CanPurchaseNode(Tree, TEXT("Core.Entropy.Decay"), Reason));
    if (!TestTrue(TEXT("Native Core respec"), Progression->RespecCore(Reason))) return false;
    TestEqual(TEXT("Respec refunds every paid rank"), Progression->GetUnspentPoints(Tree->Currency), Budget);
    TestFalse(TEXT("Respec releases Long Dark"), Progression->GetNodeStats().bLongDark);
    TestFalse(TEXT("Respec releases Density"), Progression->GetNodeStats().bRotDensity);
    if (!Buy(TEXT("Core.Reaction.Catalysis")) || !Buy(TEXT("Core.Reaction.Ignition")) || !Buy(TEXT("Core.Reaction.Chain"))) return false;
    for (int32 Rank = 0; Rank < 3; ++Rank)
        if (!Buy(TEXT("Core.Reaction.Residue"))) return false;
    TestEqual(TEXT("Chain plus three Residue ranks costs seven"), Progression->GetUnspentPoints(Tree->Currency), Budget - 7);
    TestEqual(TEXT("Residue rank values aggregate once"), Progression->GetNodeStats().ReactionResiduePercent, 30.0f);
    TestTrue(TEXT("Earned Chain exposes native reaction permission"), Progression->HasNodeTag(
        FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Reaction.Chain"))));
    TestFalse(TEXT("Three Residue ranks still do not fabricate a second completed lane"), Progression->CanPurchaseNode(Tree, TEXT("Core.Reaction.Resonance"), Reason));
    return true;
}
#endif
