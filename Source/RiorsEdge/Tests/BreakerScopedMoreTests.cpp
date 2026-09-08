#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "Combat/BreakerCombatComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "UI/BreakerSkillProjection.h"

#if WITH_DEV_AUTOMATION_TESTS
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::ElementalDamage) == 68);
static_assert(static_cast<uint8>(EBreakerNodeStatTarget::EffectiveHealth) == 74);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerScopedMoreTest, "RiorsEdge.Attributes.ScopedMoreCompetition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerScopedMoreTest::RunTest(const FString& Parameters)
{
    using Target = EBreakerNodeStatTarget;
    using Lane = EBreakerDamageMoreLane;
    TArray<const UBreakerProgressionNode*> Nodes;
    TArray<FBreakerNodeRank> Ranks;
    auto Add = [&](const TCHAR* Id, Target Stat, float Value, EBreakerNodeStatBucket Bucket)
    {
        UBreakerProgressionNode* Node = NewObject<UBreakerProgressionNode>();
        Node->NodeId = Id; Node->MaxRank = 1; Node->CostPerRank = 1;
        FBreakerNodeEffect Effect; Effect.StatTarget = Stat; Effect.StatBucket = Bucket; Effect.ValuePerRank = Value;
        Node->Effects.Add(Effect); Nodes.Add(Node); Ranks.Add({Node->NodeId, 1});
        return Node;
    };
    const auto More = EBreakerNodeStatBucket::MorePercent;
    UBreakerProgressionNode* Element = Add(TEXT("Test.Scope.Element"), Target::ElementalDamage, 22, More);
    Add(TEXT("Test.Scope.Void"), Target::VoidDamage, 18, More);
    Add(TEXT("Test.Scope.Reaction"), Target::ReactionDamage, 20, More);
    Add(TEXT("Test.Scope.Health"), Target::EffectiveHealth, 19, More);
    for (const UBreakerProgressionNode* Node : Nodes)
        TestTrue(TEXT("Each scoped single-rank author is legal"), UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Node));
    Element->MaxRank = 2;
    TestFalse(TEXT("Scoped More cannot author repeated ranks"), UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Element));
    Element->MaxRank = 1;
    FBreakerAttributeContribution Tree, Gear;
    const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks, &Tree);
    TestEqual(TEXT("All four offers count in one budget"), Stats.DamageMoreSourceCount, 4);
    FBreakerAttributeAggregator Fold;
    Fold.SetContribution(EBreakerAttributeContributor::Progression, Tree);
    TestEqual(TEXT("Weakest Void scope gets no private slot"), Fold.GetScopedMoreProduct(false,true,false,false), 1.0f);
    TestEqual(TEXT("Element and reaction overlap once each"), Fold.GetScopedMoreProduct(true,true,true,false), 1.22f*1.20f, 0.0001f);
    TestEqual(TEXT("Defensive winner pays same selected set"), Fold.GetScopedMoreProduct(false,false,false,true), 1.19f, 0.0001f);
    TestEqual(TEXT("Scopes do not leak into weapon delivery"), Fold.ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier), 1.0f);
    Gear.AddDamageMoreSource(TEXT("Gear.Stronger"), Lane::Weapon, 1.25f);
    Fold.SetContribution(EBreakerAttributeContributor::Equipment, Gear);
    TestEqual(TEXT("Equipment displaces defensive scope globally"), Fold.GetScopedMoreProduct(false,false,false,true), 1.0f);
    TestEqual(TEXT("Equipment and all scoped offers still select three"), Fold.GetSelectedDamageMoreSourceCount(), 3);
    TestEqual(TEXT("Selected gear pays delivery"), Fold.ComposedMoreProduct(EBreakerAggregatedAttribute::DamageMultiplier), 1.25f, 0.0001f);
    FBreakerSkillSnapshot Snapshot;
    Snapshot.Nodes = Nodes; Snapshot.Ranks = Ranks; Snapshot.Aggregator = Fold; Snapshot.bHasComposedAttributes = true;
    const TArray<FBreakerStatLine> Rows = BreakerSkillProjection::CurrentTotals(Snapshot);
    const FBreakerStatLine* HealthRow = Rows.FindByPredicate([](const FBreakerStatLine& Row) { return Row.Label == TEXT("EFFECTIVE HEALTH MORE"); });
    if (TestNotNull(TEXT("Projection exposes defensive scope"), HealthRow))
        TestEqual(TEXT("Projection respects gear displacing health"), HealthRow->Before, 1.0f);
    const FBreakerStatLine* ElementRow = Rows.FindByPredicate([](const FBreakerStatLine& Row) { return Row.Label == TEXT("ELEMENTAL MORE"); });
    if (TestNotNull(TEXT("Projection exposes elemental scope"), ElementRow))
        TestEqual(TEXT("Projection matches live selected element"), ElementRow->Before, 1.22f, 0.0001f);
    Fold.ClearContribution(EBreakerAttributeContributor::Equipment);
    TestEqual(TEXT("Unequip restores displaced defensive scope"), Fold.GetScopedMoreProduct(false,false,false,true), 1.19f, 0.0001f);
    Tree.Reset();
    Tree.AddDamageMoreSource(TEXT("Element"), Lane::Elemental, 1.8f);
    Tree.AddDamageMoreSource(TEXT("Void"), Lane::Void, 1.7f);
    Tree.AddDamageMoreSource(TEXT("Reaction"), Lane::Reaction, 1.6f);
    Fold.SetContribution(EBreakerAttributeContributor::Progression, Tree);
    TestEqual(TEXT("Overlapping scope maximum retains existing one ceiling"), Fold.GetScopedMoreProduct(true,true,true,false), FBreakerAttributeAggregator::ComposedMoreCeiling(), 0.0001f);

    Nodes.Reset(); Ranks.Reset();
    const auto Increased = EBreakerNodeStatBucket::IncreasedPercent;
    Add(TEXT("Test.Scope.Increased1"), Target::ElementalDamage, 10, Increased);
    Add(TEXT("Test.Scope.Increased2"), Target::ElementalDamage, 15, Increased);
    UBreakerProgressionNode* Rot = Add(TEXT("Test.Scope.Rot"), Target::RotDamage, 21, Increased);
    Add(TEXT("Test.Scope.VoidBurst"), Target::VoidBurstDamage, 24, Increased);
    Add(TEXT("Test.Scope.RiftBurst"), Target::RiftBurstDamage, 24, Increased);
    Add(TEXT("Test.Scope.ReactionDamage"), Target::ReactionDamage, 24, Increased);
    const FBreakerNodeStats Raw = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    TestEqual(TEXT("Element Increased adds raw percentage points"), Raw.ElementalDamageIncreasedPercent, 25.0f);
    TestEqual(TEXT("Rot raw scope"), Raw.RotDamageIncreasedPercent, 21.0f);
    TestEqual(TEXT("Void burst raw scope"), Raw.VoidBurstDamageIncreasedPercent, 24.0f);
    TestEqual(TEXT("Rift burst raw scope"), Raw.RiftBurstDamageIncreasedPercent, 24.0f);
    TestEqual(TEXT("Reaction raw scope"), Raw.ReactionDamageIncreasedPercent, 24.0f);
    TestTrue(TEXT("Rot Increased legal"), UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Rot));
    Rot->Effects[0].StatBucket = More;
    TestFalse(TEXT("Unsupported Rot More refuses authoring"), UBreakerProgressionComponent::IsNodeMoreAuthoringLegal(Rot));

    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>();
    Combat->PushOutgoingModifier(TEXT("Test.Scope.Window"), 0, 1.30f, 0);
    auto WindowRequest = []()
    {
        FBreakerDamageRequest Hit;
        Hit.BaseDamage = 10; Hit.bHasSourceSplit = true;
        Hit.Element = EBreakerElement::Void; Hit.ElementalFraction = 1;
        Hit.ElementSource.ElementalMoreProduct = 1.30f;
        Hit.ElementSource.VoidMoreProduct = 1.30f;
        Hit.ElementSource.ReactionMoreProduct = 1.30f;
        return Hit;
    };
    FBreakerDamageRequest VoidHit = WindowRequest();
    Combat->ApplyOutgoingModifiers(VoidHit);
    TestEqual(TEXT("Three relevant standing scopes leave no window headroom"), VoidHit.SourceMoreProduct, 1.0f, .0001f);
    FBreakerDamageRequest NoBuildup = WindowRequest(); NoBuildup.bCanApplyElementBuildup = false;
    Combat->ApplyOutgoingModifiers(NoBuildup);
    TestEqual(TEXT("Non-buildup hit reserves no reaction scope"), NoBuildup.SourceMoreProduct, 1.30f, .0001f);
    FBreakerDamageRequest EntropyHit = WindowRequest(); EntropyHit.Element = EBreakerElement::Entropy;
    Combat->ApplyOutgoingModifiers(EntropyHit);
    TestEqual(TEXT("Entropy does not reserve irrelevant Void scope"), EntropyHit.SourceMoreProduct, 1.30f, .0001f);
    FBreakerDamageRequest Physical = WindowRequest(); Physical.ElementalFraction = 0;
    Combat->ApplyOutgoingModifiers(Physical);
    TestEqual(TEXT("Physical hit receives window without elemental reservation"), Physical.SourceMoreProduct, 1.30f, .0001f);
    return true;
}
#endif
