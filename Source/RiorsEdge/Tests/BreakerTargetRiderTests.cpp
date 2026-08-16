#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <initializer_list>
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"

// ---------------------------------------------------------------------------
// STAGE 6 — target-side conditional damage (Hook-And-Condition-Vocabulary
// §3.2-§3.3), and the O34 §4a conformance test its canon row demands:
// "a new lane requires a canon row plus a conformance test before it may
// merge". The canon row lives in Docs/Design/Damage-Pipeline.md §4a
// ("Target-conditional Increased (riders)"); this file is the toll.
//
// The lane under test: FBreakerDamageRequest carries the source split
// (SourceIncreasedPercent / SourceMoreProduct alongside the composed
// SourceDamageMultiplier), UBreakerProgressionComponent publishes a rider
// table of target-conditional node effects, and
// UBreakerCombatComponent::ReceiveDamage — the one site that knows both
// actors — recomposes (1 + (Increased + Riders)/100) x MoreProduct ONLY when
// a rider fired and the split is present. Every other request must resolve
// bit-identically to a build where none of this exists; that pin is half of
// this file.
//
// These tests author node effects by CONSTRUCTION (NewObject nodes in a
// hand-built tree), not from BreakerProgressionLibrary: no library node
// targets a Target* condition yet — that authoring pass is listed in the
// wave report, and the machinery must be proven before content leans on it.
// All values are O2 PLACEHOLDER structure-only numbers; magnitude is not
// under test.
// ---------------------------------------------------------------------------

namespace BreakerTargetRiderTest
{
    // Prefixed, per the unity-build house rule about anonymous-namespace
    // helper collisions.

    struct FBreakerRiderVictimRig
    {
        AActor* Owner = nullptr;
        UBreakerCombatComponent* Combat = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
    };

    // A freshly constructed actor is ROLE_Authority, which is all
    // ReceiveDamage requires; components with an actor outer register into
    // OwnedComponents so FindComponentByClass works.
    static FBreakerRiderVictimRig BreakerMakeRiderVictim()
    {
        FBreakerRiderVictimRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Combat = NewObject<UBreakerCombatComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Combat->BindAttributes(Rig.Attributes);
        return Rig;
    }

    // A node whose one effect requires the given conditions.
    static UBreakerProgressionNode* BreakerMakeRiderNode(FName NodeId, EBreakerBuildCondition Condition,
        std::initializer_list<EBreakerBuildCondition> AlsoRequires, float PercentPerRank,
        EBreakerNodeStatBucket Bucket = EBreakerNodeStatBucket::IncreasedPercent,
        EBreakerNodeStatTarget Target = EBreakerNodeStatTarget::Damage)
    {
        UBreakerProgressionNode* Node = NewObject<UBreakerProgressionNode>();
        Node->NodeId = NodeId;
        Node->MaxRank = 2;
        Node->Currency = EBreakerPointCurrency::CorePoints;
        FBreakerNodeEffect Effect;
        Effect.StatTarget = Target;
        Effect.StatBucket = Bucket;
        Effect.ValuePerRank = PercentPerRank;   // O2 PLACEHOLDER
        Effect.Condition = Condition;
        for (const EBreakerBuildCondition Also : AlsoRequires) Effect.AlsoRequires.Add(Also);
        Node->Effects.Add(Effect);
        return Node;
    }

    // An attacker actor whose progression component genuinely OWNS the given
    // node — built tree, class definition, loaded state — so the rider table
    // is produced by the real RecalculateStats path, not injected.
    static AActor* BreakerMakeRiderAttacker(UBreakerProgressionNode* Node, int32 Rank)
    {
        AActor* Attacker = NewObject<AActor>();
        UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Attacker);

        UBreakerProgressionTree* Tree = NewObject<UBreakerProgressionTree>();
        Tree->TreeId = TEXT("Test.RiderTree");
        Tree->Currency = EBreakerPointCurrency::CorePoints;
        Tree->RequiredClass = EBreakerClassId::None;   // class-agnostic, so no class lock is needed
        Tree->Nodes.Add(Node);

        UBreakerClassDefinition* Definition = NewObject<UBreakerClassDefinition>();
        Definition->BranchTrees.Add(Tree);
        Progression->ClassDefinition = Definition;

        FBreakerProgressionState State;
        State.CoreNodeRanks.Add({Node->NodeId, Rank});
        Progression->LoadProgressionState(State);
        return Attacker;
    }

    static FBreakerDamageRequest BreakerMakeSplitRequest(AActor* Attacker, float IncreasedPercent, float MoreProduct)
    {
        FBreakerDamageRequest Request;
        Request.BaseDamage = 100.0f;
        Request.bCanCritical = false;
        Request.SetInstigator(Attacker);
        Request.SourceIncreasedPercent = IncreasedPercent;
        Request.SourceMoreProduct = MoreProduct;
        Request.SourceDamageMultiplier = (1.0f + IncreasedPercent / 100.0f) * MoreProduct;
        Request.bHasSourceSplit = true;
        return Request;
    }
}

// ---------------------------------------------------------------------------
// The recomposition is ADDITIVE-BUCKET, and the More product is untouched.
// This is the whole reason the split fields exist (§3.3): folding the rider
// into the composed value would have made it a second More.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTargetRiderConformanceTest,
    "RiorsEdge.Combat.TargetRiders.Conformance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTargetRiderConformanceTest::RunTest(const FString& Parameters)
{
    using namespace BreakerTargetRiderTest;

    // TargetAtCloseRange: both bare actors sit at the origin, so the distance
    // is zero and the condition is genuinely satisfied through
    // SupplyTargetState's own read — no condition state is hand-fed anywhere
    // in this file.
    UBreakerProgressionNode* Node = BreakerMakeRiderNode(TEXT("Test.Rider.CloseRange"),
        EBreakerBuildCondition::TargetAtCloseRange, {}, 30.0f);
    AActor* Attacker = BreakerMakeRiderAttacker(Node, 1);

    // The attacker's component actually published the row.
    const UBreakerProgressionComponent* Progression = Attacker->FindComponentByClass<UBreakerProgressionComponent>();
    TestEqual(TEXT("the owned target-conditional node publishes exactly one rider row"),
        Progression->GetTargetConditionRiders().Num(), 1);

    // Source split: +20% Increased, no More. Rider +30% must JOIN the additive
    // bucket: 100 x (1 + (20+30)/100) = 150 — and the multiplicative wrong
    // answer, 100 x 1.20 x 1.30 = 156, must not appear.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(BreakerMakeSplitRequest(Attacker, 20.0f, 1.0f));
        TestEqual(TEXT("a satisfied rider joins the additive Increased bucket (150, not 156)"),
            Result.RawDamage, 150.0f, 0.001f);
    }

    // With a More in the split, the rider still lands in the Increased half
    // and the More is reapplied on top, unchanged:
    // 100 x (1 + (20+30)/100) x 1.25 = 187.5.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(BreakerMakeSplitRequest(Attacker, 20.0f, 1.25f));
        TestEqual(TEXT("the More product passes through the recomposition untouched"),
            Result.RawDamage, 187.5f, 0.001f);
    }

    // Rank scales the row: the same node at rank 2 pays 60, not 30.
    {
        AActor* RankTwoAttacker = BreakerMakeRiderAttacker(
            BreakerMakeRiderNode(TEXT("Test.Rider.CloseRangeR2"), EBreakerBuildCondition::TargetAtCloseRange, {}, 30.0f), 2);
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(BreakerMakeSplitRequest(RankTwoAttacker, 0.0f, 1.0f));
        TestEqual(TEXT("a rank-2 rider pays twice its per-rank percent"), Result.RawDamage, 160.0f, 0.001f);
    }
    return true;
}

// ---------------------------------------------------------------------------
// The bit-identity pins: every request that is not (split present AND rider
// satisfied) resolves exactly as it did before Stage 6 existed.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTargetRiderBitIdentityTest,
    "RiorsEdge.Combat.TargetRiders.BitIdentical",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTargetRiderBitIdentityTest::RunTest(const FString& Parameters)
{
    using namespace BreakerTargetRiderTest;

    // (a) Split present, rider NOT satisfied: TargetBleeding against a victim
    // carrying no status component is an honest "asked and the answer was no"
    // — no recomposition, no warning storm, the composed value resolves as-is.
    {
        AActor* Attacker = BreakerMakeRiderAttacker(
            BreakerMakeRiderNode(TEXT("Test.Rider.Bleeding"), EBreakerBuildCondition::TargetBleeding, {}, 30.0f), 1);
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(BreakerMakeSplitRequest(Attacker, 20.0f, 1.0f));
        TestEqual(TEXT("an unsatisfied rider changes nothing"), Result.RawDamage, 120.0f, 0.001f);
    }

    // (b) Rider WOULD be satisfied, but the request carries no split — the
    // ability-path shape today. The composed value must resolve untouched:
    // without the halves, recomposing would be a guess, and a guess is a
    // second More by accident.
    {
        AActor* Attacker = BreakerMakeRiderAttacker(
            BreakerMakeRiderNode(TEXT("Test.Rider.NoSplit"), EBreakerBuildCondition::TargetAtCloseRange, {}, 30.0f), 1);
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        FBreakerDamageRequest Composed;
        Composed.BaseDamage = 100.0f;
        Composed.bCanCritical = false;
        Composed.SetInstigator(Attacker);
        Composed.SourceDamageMultiplier = 1.2f;   // composed-only, as every ability submits today
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(Composed);
        TestEqual(TEXT("a composed-only request resolves exactly as before Stage 6"),
            Result.RawDamage, 120.0f, 0.001f);
    }

    // (c) Split present, no rider table at all (an attacker with no
    // progression component — every enemy).
    {
        AActor* PlainAttacker = NewObject<AActor>();
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(BreakerMakeSplitRequest(PlainAttacker, 20.0f, 1.0f));
        TestEqual(TEXT("a riderless attacker's split request resolves unchanged"), Result.RawDamage, 120.0f, 0.001f);
    }

    // (d) No instigator at all (a hazard, a destroyed shooter): the weak
    // pointer answers null and the request resolves as today.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        FBreakerDamageRequest Request = BreakerMakeSplitRequest(nullptr, 20.0f, 1.0f);
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(Request);
        TestEqual(TEXT("an instigator-less split request resolves unchanged"), Result.RawDamage, 120.0f, 0.001f);
    }
    return true;
}

// ---------------------------------------------------------------------------
// The rider table itself: what is published, what is dropped, and how loudly.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTargetRiderTableTest,
    "RiorsEdge.Combat.TargetRiders.Table",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTargetRiderTableTest::RunTest(const FString& Parameters)
{
    using namespace BreakerTargetRiderTest;

    // A self-only conditional effect is NOT a rider — it composes at the
    // source exactly as it always has.
    {
        TArray<const UBreakerProgressionNode*> Nodes = { BreakerMakeRiderNode(
            TEXT("Test.Table.SelfOnly"), EBreakerBuildCondition::Airborne, {}, 10.0f) };
        TArray<FBreakerNodeRank> Ranks = { {TEXT("Test.Table.SelfOnly"), 1} };
        TestEqual(TEXT("a self-only conditional publishes no rider row"),
            UBreakerProgressionComponent::BuildTargetConditionRiders(Nodes, Ranks).Num(), 0);
    }

    // A mixed requirement (self AND target) travels whole: the row carries
    // both halves, so ReceiveDamage can hold it to the full requirement.
    {
        TArray<const UBreakerProgressionNode*> Nodes = { BreakerMakeRiderNode(
            TEXT("Test.Table.Mixed"), EBreakerBuildCondition::Airborne, {EBreakerBuildCondition::TargetBleeding}, 15.0f) };
        TArray<FBreakerNodeRank> Ranks = { {TEXT("Test.Table.Mixed"), 1} };
        const TArray<FBreakerTargetConditionRider> Riders =
            UBreakerProgressionComponent::BuildTargetConditionRiders(Nodes, Ranks);
        TestEqual(TEXT("a mixed self+target requirement is one rider row"), Riders.Num(), 1);
        if (Riders.Num() == 1)
        {
            TestEqual(TEXT("the row keeps its primary condition"),
                static_cast<int32>(Riders[0].Condition), static_cast<int32>(EBreakerBuildCondition::Airborne));
            TestEqual(TEXT("the row keeps its composed requirement"), Riders[0].AlsoRequires.Num(), 1);
            TestEqual(TEXT("the row's percent is rank-scaled"), Riders[0].Percent, 15.0f, 0.001f);
        }
    }

    // An unowned node (rank 0) publishes nothing.
    {
        TArray<const UBreakerProgressionNode*> Nodes = { BreakerMakeRiderNode(
            TEXT("Test.Table.Unowned"), EBreakerBuildCondition::TargetElite, {}, 25.0f) };
        TArray<FBreakerNodeRank> Ranks = { {TEXT("Test.Table.Unowned"), 0} };
        TestEqual(TEXT("an unowned node publishes no rider row"),
            UBreakerProgressionComponent::BuildTargetConditionRiders(Nodes, Ranks).Num(), 0);
    }

    // The doc rule (§3.3): a target-conditional MorePercent is NOT supported —
    // it would need the strongest-three More selection re-run per event per
    // target. Warn-and-drop, exactly like the aggregator's other unpaid Mores.
    AddExpectedError(TEXT("target-side lines are Increased-bucket Damage only"), EAutomationExpectedErrorFlags::Contains, 0);
    {
        TArray<const UBreakerProgressionNode*> Nodes = { BreakerMakeRiderNode(
            TEXT("Test.Table.MoreRider"), EBreakerBuildCondition::TargetMultiStatus, {}, 25.0f,
            EBreakerNodeStatBucket::MorePercent) };
        TArray<FBreakerNodeRank> Ranks = { {TEXT("Test.Table.MoreRider"), 1} };
        TestEqual(TEXT("a target-conditional MorePercent is dropped, never published"),
            UBreakerProgressionComponent::BuildTargetConditionRiders(Nodes, Ranks).Num(), 0);
    }

    // Same drop for a bucket/target with no target-side lane (a Flat rider).
    {
        TArray<const UBreakerProgressionNode*> Nodes = { BreakerMakeRiderNode(
            TEXT("Test.Table.FlatRider"), EBreakerBuildCondition::TargetLowHealth, {}, 25.0f,
            EBreakerNodeStatBucket::Flat) };
        TArray<FBreakerNodeRank> Ranks = { {TEXT("Test.Table.FlatRider"), 1} };
        TestEqual(TEXT("a Flat target-conditional effect is dropped, never published"),
            UBreakerProgressionComponent::BuildTargetConditionRiders(Nodes, Ranks).Num(), 0);
    }
    return true;
}

// ---------------------------------------------------------------------------
// WHAT THIS FILE DOES NOT COVER, stated plainly (the vocabulary tests' rule)
// ---------------------------------------------------------------------------
//  1. Status-driven target conditions end to end. TargetBleeding is exercised
//     here only as "asked and the answer was no"; a live bleed requires the
//     status component's application path, which needs world time. The
//     condition read itself (HasStatus) is covered by the status suite.
//  2. The weapon fill sites' split values against a live aggregator stack —
//     the identity (1 + Increased/100) x More == composed is enforced by
//     construction (the split is derived by division at the fill site), and
//     the ceiling tests already pin the composed value's budget.
//  3. Magnitudes. O2: every percent above is structure, not balance.

#endif  // WITH_DEV_AUTOMATION_TESTS
