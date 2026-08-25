#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <initializer_list>
#include "GameFramework/Actor.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTypes.h"

// ---------------------------------------------------------------------------
// The Core-atlas lane pass (Phase 4, step 1): IncomingDamageReduction,
// RecoilRecovery, WeaponSpread, StatusChance, StatusDuration. Five
// single-bidder lanes on FBreakerNodeStats.
//
// World-free by the suite's construction: the DECISION math — the register,
// the aggregation shapes, the floors, condition gating — is pinned here over
// AggregateStats, which is a static over nodes and ranks. The consumer reads
// (TickRecoil's settle, the two spread call sites, ApplyBleedOnHit's roll,
// ReceiveDamage's family bucket) are the thin world-touching shell, exercised
// by playtest like every other trace in this project — except ApplyStatus,
// which the component tests can reach, so its door is pinned below.
// ---------------------------------------------------------------------------

namespace BreakerAtlasLaneTest
{
    // Prefixed: identical anonymous-namespace helper names in two translation
    // units have collided under this project's unity build before.
    struct FBreakerAtlasEffectRow
    {
        EBreakerNodeStatTarget Target;
        EBreakerNodeStatBucket Bucket;
        float Value;
        EBreakerBuildCondition Condition = EBreakerBuildCondition::Always;
    };

    static UBreakerProgressionNode* BreakerAtlasMakeNode(FName NodeId, std::initializer_list<FBreakerAtlasEffectRow> Rows)
    {
        UBreakerProgressionNode* Node = NewObject<UBreakerProgressionNode>();
        Node->NodeId = NodeId;
        Node->MaxRank = 1;
        for (const FBreakerAtlasEffectRow& Row : Rows)
        {
            FBreakerNodeEffect Effect;
            Effect.StatTarget = Row.Target;
            Effect.StatBucket = Row.Bucket;
            Effect.ValuePerRank = Row.Value;
            Effect.Condition = Row.Condition;
            Node->Effects.Add(Effect);
        }
        return Node;
    }

    static TArray<FBreakerNodeRank> BreakerAtlasOwn(std::initializer_list<FName> NodeIds)
    {
        TArray<FBreakerNodeRank> Ranks;
        for (const FName& NodeId : NodeIds) Ranks.Add({NodeId, 1});
        return Ranks;
    }
}

// ---------------------------------------------------------------------------
// The register flips in the same commit as the lane — this is that commit,
// and these five are the lanes. (The overall count pin lives in
// BreakerConditionVocabularyTests and moved 26 -> 31 with this pass.)
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAtlasLaneRegisterTest,
    "RiorsEdge.Progression.AtlasLanes.Register",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAtlasLaneRegisterTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("IncomingDamageReduction has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::IncomingDamageReduction));
    TestTrue(TEXT("RecoilRecovery has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::RecoilRecovery));
    TestTrue(TEXT("WeaponSpread has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::WeaponSpread));
    TestTrue(TEXT("StatusChance has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::StatusChance));
    TestTrue(TEXT("StatusDuration has its lane"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::StatusDuration));
    // Lifesteal is the one O30 target deliberately left unwired: no bidder in
    // either layer, owed a lane or a retirement as its own ruling — not a
    // quiet sixth line in somebody else's pass.
    TestFalse(TEXT("Lifesteal stays unwired pending its own ruling"), BreakerStatTargetHasAggregationLane(EBreakerNodeStatTarget::Lifesteal));
    return true;
}

// ---------------------------------------------------------------------------
// The aggregation shapes: one Flat points lane, three 1.0-based multipliers,
// one divisor — with their floors, and with the bucket discipline that drops
// a row authored in the wrong bucket instead of paying it somewhere.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAtlasLaneCompositionTest,
    "RiorsEdge.Progression.AtlasLanes.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAtlasLaneCompositionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAtlasLaneTest;

    // Two IDR sources sum as points — the additive bucket ReceiveDamage folds
    // into gear's family reduction. A negative line (a downside node) drags
    // the sum; the consumer, not this sum, owns the floor.
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        Nodes.Add(BreakerAtlasMakeNode(TEXT("AtlasTest.IdrA"), {{EBreakerNodeStatTarget::IncomingDamageReduction, EBreakerNodeStatBucket::Flat, 8.0f}}));
        Nodes.Add(BreakerAtlasMakeNode(TEXT("AtlasTest.IdrB"), {{EBreakerNodeStatTarget::IncomingDamageReduction, EBreakerNodeStatBucket::Flat, 4.0f}}));
        Nodes.Add(BreakerAtlasMakeNode(TEXT("AtlasTest.IdrDown"), {{EBreakerNodeStatTarget::IncomingDamageReduction, EBreakerNodeStatBucket::Flat, -3.0f}}));
        const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(
            Nodes, BreakerAtlasOwn({TEXT("AtlasTest.IdrA"), TEXT("AtlasTest.IdrB"), TEXT("AtlasTest.IdrDown")}));
        TestEqual(TEXT("IDR points sum raw across sources"), Stats.IncomingDamageReductionPercent, 9.0f, 0.0001f);
    }

    // The three Increased multipliers and the divisor, each from one +N line.
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        Nodes.Add(BreakerAtlasMakeNode(TEXT("AtlasTest.Four"), {
            {EBreakerNodeStatTarget::RecoilRecovery, EBreakerNodeStatBucket::IncreasedPercent, 30.0f},
            {EBreakerNodeStatTarget::WeaponSpread, EBreakerNodeStatBucket::IncreasedPercent, 25.0f},
            {EBreakerNodeStatTarget::StatusChance, EBreakerNodeStatBucket::IncreasedPercent, 40.0f},
            {EBreakerNodeStatTarget::StatusDuration, EBreakerNodeStatBucket::IncreasedPercent, 50.0f}}));
        const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, BreakerAtlasOwn({TEXT("AtlasTest.Four")}));
        TestEqual(TEXT("+30 recoil recovery composes to x1.30"), Stats.RecoilRecoveryMultiplier, 1.30f, 0.0001f);
        TestEqual(TEXT("+25 weapon spread composes to the 1.25 divisor"), Stats.WeaponSpreadReduction, 1.25f, 0.0001f);
        TestEqual(TEXT("+40 status chance composes to x1.40"), Stats.StatusChanceMultiplier, 1.40f, 0.0001f);
        TestEqual(TEXT("+50 status duration composes to x1.50"), Stats.StatusDurationMultiplier, 1.50f, 0.0001f);
    }

    // Floors: the three multipliers land on 0 (a real state — "never
    // recovers", "never procs", "no duration"), the divisor stops just above
    // it so no cone is ever divided by zero.
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        Nodes.Add(BreakerAtlasMakeNode(TEXT("AtlasTest.Floors"), {
            {EBreakerNodeStatTarget::RecoilRecovery, EBreakerNodeStatBucket::IncreasedPercent, -150.0f},
            {EBreakerNodeStatTarget::WeaponSpread, EBreakerNodeStatBucket::IncreasedPercent, -150.0f},
            {EBreakerNodeStatTarget::StatusChance, EBreakerNodeStatBucket::IncreasedPercent, -150.0f},
            {EBreakerNodeStatTarget::StatusDuration, EBreakerNodeStatBucket::IncreasedPercent, -150.0f}}));
        const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, BreakerAtlasOwn({TEXT("AtlasTest.Floors")}));
        TestEqual(TEXT("recoil recovery floors at 0"), Stats.RecoilRecoveryMultiplier, 0.0f, 0.0001f);
        TestEqual(TEXT("the spread divisor floors just above 0"), Stats.WeaponSpreadReduction, 0.01f, 0.0001f);
        TestEqual(TEXT("status chance floors at 0"), Stats.StatusChanceMultiplier, 0.0f, 0.0001f);
        TestEqual(TEXT("status duration floors at 0"), Stats.StatusDurationMultiplier, 0.0f, 0.0001f);
    }

    // Bucket discipline: IDR is Flat-only and the four multipliers are
    // Increased-only. A row in the other bucket contributes nothing — dropped
    // by the dispatch, exactly like any other laneless line — rather than
    // paying into a bucket the consumer never reads.
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        Nodes.Add(BreakerAtlasMakeNode(TEXT("AtlasTest.WrongBucket"), {
            {EBreakerNodeStatTarget::IncomingDamageReduction, EBreakerNodeStatBucket::IncreasedPercent, 20.0f},
            {EBreakerNodeStatTarget::StatusDuration, EBreakerNodeStatBucket::Flat, 20.0f},
            {EBreakerNodeStatTarget::WeaponSpread, EBreakerNodeStatBucket::Flat, 20.0f}}));
        const FBreakerNodeStats Stats = UBreakerProgressionComponent::AggregateStats(Nodes, BreakerAtlasOwn({TEXT("AtlasTest.WrongBucket")}));
        TestEqual(TEXT("an Increased row against IDR pays nothing"), Stats.IncomingDamageReductionPercent, 0.0f, 0.0001f);
        TestEqual(TEXT("a Flat row against StatusDuration pays nothing"), Stats.StatusDurationMultiplier, 1.0f, 0.0001f);
        TestEqual(TEXT("a Flat row against WeaponSpread pays nothing"), Stats.WeaponSpreadReduction, 1.0f, 0.0001f);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Conditions gate the new lanes exactly as they gate every older one — the
// atlas's wheels author conditional lines against these targets, so the
// gating is part of the lane, not a later favour.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAtlasLaneConditionalTest,
    "RiorsEdge.Progression.AtlasLanes.ConditionalComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAtlasLaneConditionalTest::RunTest(const FString& Parameters)
{
    using namespace BreakerAtlasLaneTest;

    TArray<const UBreakerProgressionNode*> Nodes;
    Nodes.Add(BreakerAtlasMakeNode(TEXT("AtlasTest.AirborneDuration"), {
        {EBreakerNodeStatTarget::StatusDuration, EBreakerNodeStatBucket::IncreasedPercent, 50.0f, EBreakerBuildCondition::Airborne}}));
    const TArray<FBreakerNodeRank> Ranks = BreakerAtlasOwn({TEXT("AtlasTest.AirborneDuration")});

    const FBreakerNodeStats Grounded = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks);
    TestEqual(TEXT("grounded, the airborne line pays nothing"), Grounded.StatusDurationMultiplier, 1.0f, 0.0001f);

    FBreakerBuildConditionState Airborne;
    Airborne.Set(EBreakerBuildCondition::Airborne, true);
    const FBreakerNodeStats Live = UBreakerProgressionComponent::AggregateStats(Nodes, Ranks, nullptr, Airborne);
    TestEqual(TEXT("airborne, the line pays x1.50"), Live.StatusDurationMultiplier, 1.50f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// The one consumer the world-free suite can reach: ApplyStatus's door. An
// instigator with no progression component — every enemy, and every test that
// passes nullptr — scales duration by exactly 1, so the lane's default is
// bit-identical to the world before it existed.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAtlasLaneStatusDoorTest,
    "RiorsEdge.Combat.AtlasLanes.StatusDurationDoor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAtlasLaneStatusDoorTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Owner);

    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    Spec.Duration = 5.0f;
    Spec.TickInterval = 1.0f;
    Spec.BaseDamagePerTick = 1.0f;

    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, nullptr);
    TestEqual(TEXT("a null instigator lands"), Status->GetActiveStatuses().Num(), 1);
    if (Status->GetActiveStatuses().Num() == 1)
    {
        TestEqual(TEXT("and its duration is unscaled"), Status->GetActiveStatuses()[0].RemainingDuration, 5.0f, 0.0001f);
    }

    AActor* BareInstigator = NewObject<AActor>();
    UBreakerStatusComponent* Second = NewObject<UBreakerStatusComponent>(NewObject<AActor>());
    Second->ApplyStatus(Spec, EBreakerDamageFamily::Physical, BareInstigator);
    TestEqual(TEXT("an instigator with no progression component lands"), Second->GetActiveStatuses().Num(), 1);
    if (Second->GetActiveStatuses().Num() == 1)
    {
        TestEqual(TEXT("at exactly the authored duration"), Second->GetActiveStatuses()[0].RemainingDuration, 5.0f, 0.0001f);
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
