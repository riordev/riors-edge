#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <initializer_list>
#include "Abilities/BreakerAbilityTags.h"
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
// Most tests here author node effects by CONSTRUCTION (NewObject nodes in a
// hand-built tree), not from BreakerProgressionLibrary, so the machinery is
// proven independently of content. The first library authoring pass has now
// landed — Open Wound (TargetBleeding), Tunnel Vision (TargetElite) and
// Culling (TargetLowHealth) — and FBreakerTargetRiderLibraryAuthoringTest at
// the foot of this file verifies those three real nodes publish through the
// same BuildTargetConditionRiders path. All values are O2 PLACEHOLDER
// structure-only numbers; magnitude is not under test.
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

    // O141 rewrote §3.3's half-rule: a target-gated MorePercent on a
    // delivered damage pool now PUBLISHES as a hit-time More row — the sort
    // never sees it, the combat site pays it under the one ceiling — and the
    // row carries the authored percent UNSCALED BY RANK, the same rule the
    // aggregation side holds for every More.
    {
        TArray<const UBreakerProgressionNode*> Nodes = { BreakerMakeRiderNode(
            TEXT("Test.Table.MoreRider"), EBreakerBuildCondition::TargetMultiStatus, {}, 25.0f,
            EBreakerNodeStatBucket::MorePercent) };
        TArray<FBreakerNodeRank> Ranks = { {TEXT("Test.Table.MoreRider"), 2} };
        const TArray<FBreakerTargetConditionRider> MoreRows =
            UBreakerProgressionComponent::BuildTargetConditionRiders(Nodes, Ranks);
        TestEqual(TEXT("a target-gated MorePercent publishes one hit-time More row (O141)"), MoreRows.Num(), 1);
        if (MoreRows.Num() == 1)
        {
            TestEqual(TEXT("the row carries the More half, unscaled by rank"), MoreRows[0].MorePercent, 25.0f, 0.0001f);
            TestEqual(TEXT("the row's Increased half is empty"), MoreRows[0].Percent, 0.0f, 0.0001f);
        }
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
// O98 — the first TAG-KEYED rider: MeleeDamage rows key on Damage.Melee and
// ride the weapon lane. The slice is selected by what the hit says it IS (the
// request's SourceTags — the same tag Cleave and the Tank sweep already
// stamp), never by what triggered it. The tag requirement is what defers an
// unconditional line to the hit, exactly as a Target* condition defers a
// conditional one — which is why the same table and the same recomposition
// carry both, and why this file is where the slice is proven.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSourceTagRiderMeleeTest,
    "RiorsEdge.Combat.TargetRiders.SourceTagMelee",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSourceTagRiderMeleeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerTargetRiderTest;

    // An UNCONDITIONAL MeleeDamage Increased line publishes a rider row
    // carrying the native tag. Condition Always is an honest shape here: the
    // tag is the gate.
    UBreakerProgressionNode* Node = BreakerMakeRiderNode(TEXT("Test.Rider.Melee"),
        EBreakerBuildCondition::Always, {}, 30.0f,
        EBreakerNodeStatBucket::IncreasedPercent, EBreakerNodeStatTarget::MeleeDamage);
    AActor* Attacker = BreakerMakeRiderAttacker(Node, 1);

    const UBreakerProgressionComponent* Progression = Attacker->FindComponentByClass<UBreakerProgressionComponent>();
    if (!TestEqual(TEXT("a MeleeDamage line publishes exactly one rider row"),
        Progression->GetTargetConditionRiders().Num(), 1))
    {
        return true;
    }
    TestTrue(TEXT("the row keys on the native Damage.Melee tag, not a restated string"),
        Progression->GetTargetConditionRiders()[0].RequiredSourceTag == BreakerAbilityTags::Damage_Melee.GetTag());

    // A melee-tagged, weapon-delivered hit — the exact request shape Cleave
    // and the Tank sweep submit: +30 joins the source's +20 in the ADDITIVE
    // bucket. 100 x (1 + (20+30)/100) = 150, never 100 x 1.20 x 1.30.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        FBreakerDamageRequest Request = BreakerMakeSplitRequest(Attacker, 20.0f, 1.0f);
        Request.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
        TestEqual(TEXT("a melee-tagged weapon hit pays the slice additively (150)"),
            Victim.Combat->ReceiveDamage(Request).RawDamage, 150.0f, 0.001f);
    }

    // The same attacker firing a BULLET (untagged): the slice pays nothing.
    // This is the whole line between a slice and a lane — fold the 30 in here
    // and MeleeDamage has quietly become the weapon pool.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        TestEqual(TEXT("an untagged weapon hit resolves without the slice (120)"),
            Victim.Combat->ReceiveDamage(BreakerMakeSplitRequest(Attacker, 20.0f, 1.0f)).RawDamage, 120.0f, 0.001f);
    }

    // Tagged but ABILITY-delivered: refused by the lane rule. No shipped melee
    // submits as Ability (O55 makes swinging the equipped weapon
    // weapon-delivered), so this pins the refusal, not a live case.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        FBreakerDamageRequest Request = BreakerMakeSplitRequest(Attacker, 20.0f, 1.0f);
        Request.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
        Request.Delivery = EBreakerDamageDelivery::Ability;
        TestEqual(TEXT("a melee-tagged ability-delivered hit is refused by the lane rule (120)"),
            Victim.Combat->ReceiveDamage(Request).RawDamage, 120.0f, 0.001f);
    }

    // A melee line may still carry a condition, and the requirements AND: the
    // tag on the request, the condition on the pair of actors (both rigs sit
    // at the origin, so TargetAtCloseRange is genuinely satisfied).
    {
        AActor* Conditional = BreakerMakeRiderAttacker(BreakerMakeRiderNode(TEXT("Test.Rider.MeleeClose"),
            EBreakerBuildCondition::TargetAtCloseRange, {}, 25.0f,
            EBreakerNodeStatBucket::IncreasedPercent, EBreakerNodeStatTarget::MeleeDamage), 1);
        {
            FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
            FBreakerDamageRequest Request = BreakerMakeSplitRequest(Conditional, 0.0f, 1.0f);
            Request.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
            TestEqual(TEXT("tag AND condition satisfied together pay (125)"),
                Victim.Combat->ReceiveDamage(Request).RawDamage, 125.0f, 0.001f);
        }
        {
            FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
            TestEqual(TEXT("condition satisfied but tag absent pays nothing (100)"),
                Victim.Combat->ReceiveDamage(BreakerMakeSplitRequest(Conditional, 0.0f, 1.0f)).RawDamage, 100.0f, 0.001f);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The first library authoring pass: the three rider lines exist ON REAL NODES
// and publish through the real table build (no hand-built tree, no injection).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTargetRiderLibraryAuthoringTest,
    "RiorsEdge.Combat.TargetRiders.LibraryAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTargetRiderLibraryAuthoringTest::RunTest(const FString& Parameters)
{
    // A Swift with the three rider-carrying nodes: Open Wound and Tunnel
    // Vision are Core (class-agnostic), Culling is Marksman. Loaded state, so
    // the rows are produced by the real RecalculateStats path off the real
    // fallback trees — LoadProgressionState resolves the Swift class
    // definition itself.
    AActor* Attacker = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Attacker);
    FBreakerProgressionState State;
    State.PermanentClass = EBreakerClassId::Swift;
    State.CoreNodeRanks.Add({TEXT("Core.Affliction.OpenWound"), 1});
    State.CoreNodeRanks.Add({TEXT("Core.Precision.TunnelVision"), 1});
    State.DoctrineNodeRanks.Add({TEXT("Swift.Marksman.Culling"), 1});
    Progression->LoadProgressionState(State);

    const TArray<FBreakerTargetConditionRider>& Riders = Progression->GetTargetConditionRiders();
    TestEqual(TEXT("The three authored library lines publish exactly three rider rows"), Riders.Num(), 3);

    auto FindRider = [&Riders](EBreakerBuildCondition Condition) -> const FBreakerTargetConditionRider*
    {
        return Riders.FindByPredicate([Condition](const FBreakerTargetConditionRider& Rider) { return Rider.Condition == Condition; });
    };
    for (const EBreakerBuildCondition Condition : { EBreakerBuildCondition::TargetBleeding,
        EBreakerBuildCondition::TargetElite, EBreakerBuildCondition::TargetLowHealth })
    {
        const FBreakerTargetConditionRider* Rider = FindRider(Condition);
        if (!TestNotNull(TEXT("Each authored condition has its row"), Rider)) continue;
        // Structure, not magnitude (O2): the row is Damage-target,
        // rank-scaled to something positive, and composed with no
        // AlsoRequires — each line is a single honest condition.
        TestEqual(TEXT("Rider rows are Damage-target (the one lane ReceiveDamage consumes)"),
            Rider->StatTarget, EBreakerNodeStatTarget::Damage);
        TestTrue(TEXT("Rider rows carry a positive rank-scaled percent"), Rider->Percent > 0.0f);
        TestEqual(TEXT("The authored lines compose no extra requirements"), Rider->AlsoRequires.Num(), 0);
    }

    // The unconditional halves of the same purchases still pay the ordinary
    // way: Tunnel Vision's crit damage and Culling's unconditional More reach
    // the aggregate exactly as before the rider lines were added — the rider
    // is ADDITIVE authoring, not a re-route of what the nodes already did.
    const FBreakerNodeStats& Stats = Progression->GetNodeStats();
    TestTrue(TEXT("Tunnel Vision's flat crit-damage line still aggregates"), Stats.CriticalMultiplierBonus > 0.0f);
    // O95: Culling authors no More at all now -- a doctrine authors none, and
    // every slot lives in Core. This build holds NO Core node, so the composed
    // More product is exactly 1.0 and that is the assertion: the doctrine
    // contributes nothing to the multiplier layer, by rule.
    TestEqual(TEXT("A doctrine-only build composes no More at all (O95)"), Stats.DamageMoreMultiplier, 1.0f, 0.0001f);

    // A build without the nodes publishes no rows at all — the whole
    // pre-existing population is untouched by the authoring pass.
    AActor* Bare = NewObject<AActor>();
    UBreakerProgressionComponent* BareProgression = NewObject<UBreakerProgressionComponent>(Bare);
    FBreakerProgressionState BareState;
    BareState.PermanentClass = EBreakerClassId::Swift;
    BareProgression->LoadProgressionState(BareState);
    TestEqual(TEXT("A Swift with none of the nodes publishes no riders"), BareProgression->GetTargetConditionRiders().Num(), 0);
    return true;
}

// ---------------------------------------------------------------------------
// TargetBandBroken, end to end: the previous-hit bit written at the foot of
// ReceiveDamage (BreakerHealthBands::IndexOf on pre/post health), read by the
// NEXT hit's rider resolution. The one-hit-late shape is the design — "this
// hit will cross a band" is a fixpoint and cannot be built — so the sequence
// below is the contract: the breaking hit itself pays no rider, the hit after
// it does, and any landed hit that crosses nothing takes the bit away.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTargetBandBrokenRiderTest,
    "RiorsEdge.Combat.TargetRiders.BandBroken",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTargetBandBrokenRiderTest::RunTest(const FString& Parameters)
{
    using namespace BreakerTargetRiderTest;

    // A plain-actor victim carries no rank, so the write site uses the
    // Trash-equivalent segment count — 4 bands of 25 on the default 100 pool.
    AActor* Attacker = BreakerMakeRiderAttacker(
        BreakerMakeRiderNode(TEXT("Test.Rider.BandBroken"), EBreakerBuildCondition::TargetBandBroken, {}, 25.0f), 1);
    FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();

    // The shared helper's BaseDamage would kill the pool in one hit; the
    // sequence below needs the victim alive across four, so sizes are local.
    auto MakeSizedRequest = [&](float BaseDamage)
    {
        FBreakerDamageRequest Request = BreakerMakeSplitRequest(Attacker, 0.0f, 1.0f);
        Request.BaseDamage = BaseDamage;
        return Request;
    };

    TestFalse(TEXT("an unhit victim reports no band break"), Victim.Combat->WasBandBrokenByPreviousHit());

    // Hit 1 crosses the 75 boundary (100 -> 70). The BREAKING hit itself pays
    // no rider — the bit it reads is still false — then sets the bit.
    {
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(MakeSizedRequest(30.0f));
        TestEqual(TEXT("the breaking hit itself pays no rider"), Result.RawDamage, 30.0f, 0.001f);
        TestTrue(TEXT("crossing a boundary sets the bit"), Victim.Combat->WasBandBrokenByPreviousHit());
    }

    // Hit 2 rides the break: 10 x (1 + 25/100) = 12.5. It lands 70 -> 57.5,
    // inside one band, so it also takes the bit away — the previous-hit
    // lifetime, pinned.
    {
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(MakeSizedRequest(10.0f));
        TestEqual(TEXT("the hit after a break pays the rider"), Result.RawDamage, 12.5f, 0.001f);
        TestFalse(TEXT("a landed hit that crosses nothing clears the bit"), Victim.Combat->WasBandBrokenByPreviousHit());
    }

    // Hit 3 gets no rider (the bit is gone) and happens to cross the 50
    // boundary (57.5 -> 47.5), re-arming it.
    {
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(MakeSizedRequest(10.0f));
        TestEqual(TEXT("no rider without the bit"), Result.RawDamage, 10.0f, 0.001f);
        TestTrue(TEXT("the next crossing re-arms it"), Victim.Combat->WasBandBrokenByPreviousHit());
    }

    // The pool's revive checklist calls this so a reused body cannot hand its
    // first attacker a rider the fresh enemy never earned.
    Victim.Combat->ClearBandBreakTracking();
    {
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(MakeSizedRequest(5.0f));
        TestEqual(TEXT("a cleared bit pays nothing"), Result.RawDamage, 5.0f, 0.001f);
    }

    return true;
}

// ---------------------------------------------------------------------------
// O141 — the ONE hit-time More, end to end. A target-gated MorePercent rides
// the rider table and pays by multiplying the request's standing More product
// under the one O34 ceiling: headroom, never a slot. The three points that
// define the law are each asserted: full payment where headroom exists,
// exact ceiling where the product would pass it, and nothing at saturation —
// plus the recomposition identity with the Increased half, and the gate that
// an unsatisfied condition pays nothing.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHitTimeMoreRiderTest,
    "RiorsEdge.Combat.TargetRiders.HitTimeMore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHitTimeMoreRiderTest::RunTest(const FString& Parameters)
{
    using namespace BreakerTargetRiderTest;

    AActor* Attacker = BreakerMakeRiderAttacker(BreakerMakeRiderNode(TEXT("Test.Rider.HitTimeMore"),
        EBreakerBuildCondition::TargetBandBroken, {}, 30.0f,
        EBreakerNodeStatBucket::MorePercent, EBreakerNodeStatTarget::Damage), 1);

    const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();

    // One case per standing product: a fresh victim, armed by a 30-damage
    // first hit (100 -> 70 crosses the 75 boundary on the default 4-band
    // pool), then the measured hit with that standing More in its split.
    auto MeasureRiddenMore = [&](float StandingMore) -> float
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        FBreakerDamageRequest Arm = BreakerMakeSplitRequest(Attacker, 0.0f, 1.0f);
        Arm.BaseDamage = 30.0f;
        Victim.Combat->ReceiveDamage(Arm);
        FBreakerDamageRequest Hit = BreakerMakeSplitRequest(Attacker, 0.0f, StandingMore);
        Hit.BaseDamage = 4.0f;
        const FBreakerDamageResult Result = Victim.Combat->ReceiveDamage(Hit);
        return Result.RawDamage / 4.0f;   // the effective composed multiplier
    };

    // Headroom: a bare product takes the whole authored x1.30.
    TestEqual(TEXT("with headroom the rider pays its full authored More"),
        MeasureRiddenMore(1.0f), 1.30f, 0.001f);
    // Partial: 1.9349 standing (the weapon build's product) clamps the total
    // to exactly the ceiling — the rider delivered x1.1355, not x1.30.
    TestEqual(TEXT("a near-saturated product clamps the total to the one ceiling"),
        MeasureRiddenMore(1.9349f), Ceiling, 0.001f);
    // Saturation: a product already at the ceiling gains exactly nothing.
    TestEqual(TEXT("a saturated product gains nothing from the rider"),
        MeasureRiddenMore(Ceiling), Ceiling, 0.001f);

    // The recomposition identity with the Increased half intact:
    // (1 + 20/100) x 1.0 x 1.30 = 1.56.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        FBreakerDamageRequest Arm = BreakerMakeSplitRequest(Attacker, 0.0f, 1.0f);
        Arm.BaseDamage = 30.0f;
        Victim.Combat->ReceiveDamage(Arm);
        FBreakerDamageRequest Hit = BreakerMakeSplitRequest(Attacker, 20.0f, 1.0f);
        Hit.BaseDamage = 10.0f;
        TestEqual(TEXT("the Increased half recomposes beside the rider More"),
            Victim.Combat->ReceiveDamage(Hit).RawDamage, 15.6f, 0.01f);
    }

    // The gate: no band break, no payment — the composed value resolves
    // exactly as authored.
    {
        FBreakerRiderVictimRig Victim = BreakerMakeRiderVictim();
        FBreakerDamageRequest Hit = BreakerMakeSplitRequest(Attacker, 0.0f, 1.0f);
        Hit.BaseDamage = 10.0f;
        TestEqual(TEXT("an unsatisfied More rider changes nothing"),
            Victim.Combat->ReceiveDamage(Hit).RawDamage, 10.0f, 0.001f);
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
