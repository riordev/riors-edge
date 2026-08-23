#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerBuildConditions.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Abilities/BreakerAbility_Cleave.h"
#include "Abilities/BreakerAbility_Rot.h"
#include "Abilities/BreakerGameplayAbility.h"

// ---------------------------------------------------------------------------
// The loop valve (ClassResourceDecay -> PushLoopOverride) and the ability
// geometry seam (AbilityArea / AbilityDuration / AbilityCooldown), 2026-08-16.
//
// These are the two consumers that unblocked the largest groups of silent
// tree nodes. The tests here cover four layers, from pure math to a real
// component: the aggregation lanes compose the authored lines correctly; the
// re-authored nodes carry exactly the Class-Kits-transcribed values; the
// progression -> resource-component bridge lands a keyed override on a real
// Momentum component and the decay change is observable through AdvanceLoop;
// and the geometry accessors scale each subclass's differently-named numbers.
// ---------------------------------------------------------------------------

namespace BreakerLoopValveTestHelpers
{
    TArray<const UBreakerProgressionNode*> BreakerAllFallbackNodes()
    {
        TArray<const UBreakerProgressionNode*> Nodes;
        for (const UBreakerProgressionTree* Tree : UBreakerProgressionLibrary::GetAllFallbackTrees())
        {
            for (const UBreakerProgressionNode* Node : Tree->Nodes) Nodes.Add(Node);
        }
        return Nodes;
    }

    bool BreakerBuyToMax(FAutomationTestBase& Test, UBreakerProgressionComponent* Progression,
        const UBreakerProgressionTree* Tree, FName NodeId)
    {
        const UBreakerProgressionNode* Node = Tree->FindNode(NodeId);
        if (!Node) return false;
        for (int32 Rank = 0; Rank < Node->MaxRank; ++Rank)
        {
            FText Failure;
            if (!Test.TestTrue(*FString::Printf(TEXT("%s purchases rank %d (%s)"),
                *NodeId.ToString(), Rank + 1, *Failure.ToString()),
                Progression->PurchaseNode(Tree, NodeId, Failure)))
            {
                return false;
            }
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
// Lane math: the ClassResourceDecay bucket composes the three consuming
// nodes' authored lines under their conditions, sign convention included.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLoopValveLaneMathTest,
    "RiorsEdge.Progression.LoopValve.LaneMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLoopValveLaneMathTest::RunTest(const FString& Parameters)
{
    using namespace BreakerLoopValveTestHelpers;
    const TArray<const UBreakerProgressionNode*> Nodes = BreakerAllFallbackNodes();

    // No Safety: F11's "decay is doubled" is unconditional, so it pays on the
    // empty state — POSITIVE means FASTER, the lane's sign convention.
    TArray<FBreakerNodeRank> NoSafety;
    NoSafety.Add({TEXT("Swift.Frenzy.NoSafety"), 1});
    const FBreakerNodeStats NoSafetyStats = UBreakerProgressionComponent::AggregateStats(Nodes, NoSafety);
    TestEqual(TEXT("No Safety doubles decay (F11)"), NoSafetyStats.ClassResourceDecayMultiplier, 2.0f, 0.0001f);
    // The other F11 half rides the pre-existing AbilityCost lane, visible in
    // the attribute contribution rather than here; the branch-content tests
    // already prove that lane reaches ResourceCostMultiplier.

    // No Ground: +50 while Grounded, -100 while Airborne, nothing standing in
    // neither state (a ladder, mid-teleport).
    TArray<FBreakerNodeRank> NoGround;
    NoGround.Add({TEXT("Swift.Kinetic.NoGround"), 1});
    const FBreakerNodeStats NoGroundNeither = UBreakerProgressionComponent::AggregateStats(Nodes, NoGround);
    TestEqual(TEXT("No Ground is neutral in neither state"), NoGroundNeither.ClassResourceDecayMultiplier, 1.0f, 0.0001f);

    FBreakerBuildConditionState Grounded;
    Grounded.Set(EBreakerBuildCondition::Grounded, true);
    const FBreakerNodeStats NoGroundGrounded = UBreakerProgressionComponent::AggregateStats(Nodes, NoGround, nullptr, Grounded);
    TestEqual(TEXT("No Ground raises grounded decay by 50% (K11)"), NoGroundGrounded.ClassResourceDecayMultiplier, 1.5f, 0.0001f);

    FBreakerBuildConditionState Airborne;
    Airborne.Set(EBreakerBuildCondition::Airborne, true);
    const FBreakerNodeStats NoGroundAirborne = UBreakerProgressionComponent::AggregateStats(Nodes, NoGround, nullptr, Airborne);
    TestEqual(TEXT("No Ground suspends airborne decay outright (K11)"), NoGroundAirborne.ClassResourceDecayMultiplier, 0.0f, 0.0001f);

    // Reserve: -100 while Aiming composes to exactly zero — "does not decay
    // while ADS" (M9) — and pays nothing off ADS.
    TArray<FBreakerNodeRank> Reserve;
    Reserve.Add({TEXT("Swift.Marksman.Reserve"), 1});
    const FBreakerNodeStats ReserveIdle = UBreakerProgressionComponent::AggregateStats(Nodes, Reserve);
    TestEqual(TEXT("Reserve is neutral off ADS"), ReserveIdle.ClassResourceDecayMultiplier, 1.0f, 0.0001f);
    FBreakerBuildConditionState Aiming;
    Aiming.Set(EBreakerBuildCondition::Aiming, true);
    const FBreakerNodeStats ReserveAiming = UBreakerProgressionComponent::AggregateStats(Nodes, Reserve, nullptr, Aiming);
    TestEqual(TEXT("Reserve suspends decay while ADS (M9)"), ReserveAiming.ClassResourceDecayMultiplier, 0.0f, 0.0001f);

    // ONE ADDITIVE BUCKET, THE LOCKED LAW, STATED WHERE IT SURPRISES: a build
    // holding BOTH Reserve and No Safety while ADS composes 1 + (100-100)/100
    // = exactly normal decay — the doubled drain and the ADS hold tug the one
    // bucket rather than one absolutely overriding the other. If the owner
    // wants Reserve's hold to be absolute, that is a composition ruling, and
    // this assertion is where it gets changed deliberately.
    TArray<FBreakerNodeRank> Both = Reserve;
    Both.Add({TEXT("Swift.Frenzy.NoSafety"), 1});
    const FBreakerNodeStats BothAiming = UBreakerProgressionComponent::AggregateStats(Nodes, Both, nullptr, Aiming);
    TestEqual(TEXT("Reserve + No Safety while ADS meet in one additive bucket"), BothAiming.ClassResourceDecayMultiplier, 1.0f, 0.0001f);
    // And the floor holds: the multiplier can never go negative however many
    // suspensions stack.
    TArray<FBreakerNodeRank> DoubleSuspend;
    DoubleSuspend.Add({TEXT("Swift.Marksman.Reserve"), 1});
    DoubleSuspend.Add({TEXT("Swift.Kinetic.NoGround"), 1});
    const FBreakerNodeStats Floored = UBreakerProgressionComponent::AggregateStats(
        Nodes, DoubleSuspend, nullptr, FBreakerBuildConditionState::All());
    TestTrue(TEXT("Stacked suspensions floor at zero, never negative"), Floored.ClassResourceDecayMultiplier >= 0.0f);

    // Redirect: the AbilityCooldown lane, divisor convention. Two ranks at
    // +20%/rank while Airborne read 1.40 — a 40%-shorter cooldown — and
    // exactly 1.0 grounded.
    TArray<FBreakerNodeRank> Redirect;
    Redirect.Add({TEXT("Swift.Kinetic.Redirect"), 2});
    const FBreakerNodeStats RedirectIdle = UBreakerProgressionComponent::AggregateStats(Nodes, Redirect);
    TestEqual(TEXT("Redirect pays nothing grounded"), RedirectIdle.AbilityCooldownReduction, 1.0f, 0.0001f);
    const FBreakerNodeStats RedirectAir = UBreakerProgressionComponent::AggregateStats(Nodes, Redirect, nullptr, Airborne);
    TestEqual(TEXT("Redirect composes the cooldown divisor airborne"), RedirectAir.AbilityCooldownReduction, 1.40f, 0.0001f);

    // Lingering: the AbilityDuration lane, two ranks at +15%/rank.
    TArray<FBreakerNodeRank> Lingering;
    Lingering.Add({TEXT("Caster.VoidWhisperer.Lingering"), 2});
    const FBreakerNodeStats LingeringStats = UBreakerProgressionComponent::AggregateStats(Nodes, Lingering);
    TestEqual(TEXT("Lingering composes the duration multiplier"), LingeringStats.AbilityDurationMultiplier, 1.30f, 0.0001f);
    TestEqual(TEXT("Lingering leaves area alone"), LingeringStats.AbilityAreaMultiplier, 1.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// The valve on the component: a keyed override's decay multiplier scales the
// one place decay is paid, zero survives the fold, and the stack composes.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLoopValveOverrideStackTest,
    "RiorsEdge.Classes.LoopValve.OverrideStack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLoopValveOverrideStackTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Owner);

    TestEqual(TEXT("No overrides is neutral"), Momentum->GetDecayRateMultiplier(), 1.0f, 0.0001f);
    Momentum->PushLoopOverride(TEXT("Test.Doubled"), false, 1.0f, 0.0f, 2.0f);
    TestEqual(TEXT("A doubled-decay override reads back"), Momentum->GetDecayRateMultiplier(), 2.0f, 0.0001f);
    // Zero must SURVIVE: "decay stops" is a real request (Reserve), and the
    // generation fold's skip-non-positive rule must not apply here.
    Momentum->PushLoopOverride(TEXT("Test.Suspend"), false, 1.0f, 0.0f, 0.0f);
    TestEqual(TEXT("A zero suspension survives the fold"), Momentum->GetDecayRateMultiplier(), 0.0f, 0.0001f);
    Momentum->PopLoopOverride(TEXT("Test.Suspend"));
    TestEqual(TEXT("Popping the suspension restores the product"), Momentum->GetDecayRateMultiplier(), 2.0f, 0.0001f);
    // Re-pushing a key replaces rather than stacks, the seam's existing rule.
    Momentum->PushLoopOverride(TEXT("Test.Doubled"), false, 1.0f, 0.0f, 1.5f);
    TestEqual(TEXT("Re-pushing a key replaces its multiplier"), Momentum->GetDecayRateMultiplier(), 1.5f, 0.0001f);
    // A caller that never mentions decay (Overdrive's 4-argument form) is
    // neutral on this lane — the defaulted parameter is what keeps every
    // pre-existing caller bit-identical.
    Momentum->PushLoopOverride(TEXT("Test.Legacy"), true, 2.0f, 0.0f);
    TestEqual(TEXT("A legacy push is decay-neutral"), Momentum->GetDecayRateMultiplier(), 1.5f, 0.0001f);

    // Grit carries the identical seam.
    UBreakerGritComponent* Grit = NewObject<UBreakerGritComponent>(Owner);
    Grit->PushLoopOverride(TEXT("Test.Doubled"), false, 1.0f, 0.0f, 2.0f);
    TestEqual(TEXT("Grit's decay lane composes identically"), Grit->GetDecayRateMultiplier(), 2.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// The bridge, end to end on a real rig: buy No Safety, the override lands on
// the Momentum component, the decay change is observable through AdvanceLoop,
// and a respec pops it.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLoopValveBridgeTest,
    "RiorsEdge.Classes.LoopValve.BridgeReachesMomentum",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLoopValveBridgeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerLoopValveTestHelpers;

    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Owner);
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();

    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Progression->BindAttributes(Attributes);
    Progression->DevForceClass(EBreakerClassId::Swift);
    Momentum->HandleProgressionChanged();
    Momentum->BindAttributes(Attributes);
    TestTrue(TEXT("The Momentum loop runs for this rig"), Momentum->IsActiveForOwner());

    Progression->GrantPlaytestPoints(40, 0);
    UBreakerProgressionTree* Frenzy = UBreakerProgressionLibrary::GetSwiftFrenzyTree();

    // The tier-4 gate is 6 points of branch investment plus the Short Leash
    // prerequisite — paid the way a player would.
    TestTrue(TEXT("Trigger Discipline buys"), BreakerBuyToMax(*this, Progression, Frenzy, TEXT("Swift.Frenzy.TriggerDiscipline")));
    TestTrue(TEXT("Loaded buys"), BreakerBuyToMax(*this, Progression, Frenzy, TEXT("Swift.Frenzy.Loaded")));
    TestTrue(TEXT("Short Leash buys"), BreakerBuyToMax(*this, Progression, Frenzy, TEXT("Swift.Frenzy.ShortLeash")));
    TestEqual(TEXT("Before No Safety the valve is idle"), Momentum->GetActiveLoopOverrideCount(), 0);
    TestTrue(TEXT("No Safety buys"), BreakerBuyToMax(*this, Progression, Frenzy, TEXT("Swift.Frenzy.NoSafety")));

    // THE BRIDGE: the purchase recalculated, PushLoopValveOverrides ran, and
    // the composed multiplier is now a keyed override on the real component.
    TestEqual(TEXT("The bridge lands one keyed override"), Momentum->GetActiveLoopOverrideCount(), 1);
    TestEqual(TEXT("No Safety's doubled decay reaches the Momentum component"), Momentum->GetDecayRateMultiplier(), 2.0f, 0.0001f);
    TestFalse(TEXT("The valve does not suspend decay as a side effect"), Momentum->IsDecaySuspended());
    TestEqual(TEXT("The valve does not touch generation"), Momentum->GetGenerationMultiplier(), 1.0f, 0.0001f);

    // OBSERVABLE: a settled, grounded, slow rig past the decay grace loses
    // Momentum at exactly twice the settled rate. 1.5s at 15/s doubled = 45.
    Attributes->ApplyClassResource(100.0f);
    Momentum->AdvanceLoop(1.5f);
    TestEqual(TEXT("Doubled decay is observable through the loop"),
        Attributes->GetClassResource(), 100.0f - Momentum->SettledDecayRate * 2.0f * 1.5f, 0.01f);

    // A respec pops the override rather than leaving a stale 1.0 entry.
    FText Failure;
    TestTrue(TEXT("Respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Respec pops the valve override"), Momentum->GetActiveLoopOverrideCount(), 0);
    TestEqual(TEXT("Decay is back to authored after respec"), Momentum->GetDecayRateMultiplier(), 1.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// Ability geometry: each subclass's differently-named numbers respond to the
// seam, and a Caster's no-cooldown rule survives any divisor.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityGeometrySeamTest,
    "RiorsEdge.Abilities.GeometrySeam",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityGeometrySeamTest::RunTest(const FString& Parameters)
{
    using namespace BreakerLoopValveTestHelpers;

    // --- Pure rules --------------------------------------------------------
    // Cleave's arc: SB8's 180-degree rule replaces the base, the area
    // multiplier scales whichever base applies, and nothing sweeps past 360.
    TestEqual(TEXT("Authored arc with no ranks"), UBreakerAbility_Cleave::EffectiveArcFor(false, 120.0f, 1.0f), 120.0f, 0.0001f);
    TestEqual(TEXT("Edge widens to 180 (SB8)"), UBreakerAbility_Cleave::EffectiveArcFor(true, 120.0f, 1.0f), 180.0f, 0.0001f);
    TestEqual(TEXT("Area ranks scale the authored arc"), UBreakerAbility_Cleave::EffectiveArcFor(false, 120.0f, 1.25f), 150.0f, 0.0001f);
    TestEqual(TEXT("Edge and area ranks compose"), UBreakerAbility_Cleave::EffectiveArcFor(true, 120.0f, 1.5f), 270.0f, 0.0001f);
    TestEqual(TEXT("No arc sweeps past a full circle"), UBreakerAbility_Cleave::EffectiveArcFor(true, 120.0f, 3.0f), 360.0f, 0.0001f);

    // The cooldown divisor: DashCooldownReduction's convention, and the
    // Caster invariance — an authored cooldown of zero stays zero under ANY
    // divisor, so the lane cannot invent a cooldown for a class that has none
    // by design (T8: Mana IS the cooldown).
    TestEqual(TEXT("Neutral divisor changes nothing"), UBreakerGameplayAbility::ScaledCooldownSeconds(8.0f, 1.0f), 8.0f, 0.0001f);
    TestEqual(TEXT("1.25 divides to a 20% shorter cooldown"), UBreakerGameplayAbility::ScaledCooldownSeconds(8.0f, 1.25f), 6.4f, 0.0001f);
    TestEqual(TEXT("A Caster's zero cooldown survives any divisor"), UBreakerGameplayAbility::ScaledCooldownSeconds(0.0f, 2.0f), 0.0f, 0.0001f);
    TestEqual(TEXT("A malformed divisor is floored, never a division by zero"), UBreakerGameplayAbility::ScaledCooldownSeconds(8.0f, 0.0f), 800.0f, 0.01f);

    // Null-safety: no owner means every multiplier is exactly 1.0, so every
    // authored number is bit-identical for a rig with no progression at all.
    TestEqual(TEXT("Area multiplier is 1.0 with no owner"), UBreakerGameplayAbility::AbilityAreaMultiplierFor(nullptr), 1.0f, 0.0001f);
    TestEqual(TEXT("Duration multiplier is 1.0 with no owner"), UBreakerGameplayAbility::AbilityDurationMultiplierFor(nullptr), 1.0f, 0.0001f);
    TestEqual(TEXT("Cooldown reduction is 1.0 with no owner"), UBreakerGameplayAbility::AbilityCooldownReductionFor(nullptr), 1.0f, 0.0001f);

    // --- On a rigged Caster ------------------------------------------------
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Progression->IncreasedDamagePerSpentPoint = 0.0f;
    Progression->BindAttributes(Attributes);
    Progression->DevForceClass(EBreakerClassId::Caster);
    Progression->GrantPlaytestPoints(40, 0);

    UBreakerAbility_Rot* Rot = NewObject<UBreakerAbility_Rot>();
    UBreakerAbility_Cleave* Cleave = NewObject<UBreakerAbility_Cleave>();

    // Before any purchase, the Class-Kits numbers exactly: 4 m, 6 s, 120°.
    TestEqual(TEXT("Rot's authored radius with no ranks"), Rot->ComputeEffectiveRadiusCm(Owner), 400.0f, 0.0001f);
    TestEqual(TEXT("Rot's authored duration with no ranks"), Rot->ComputeEffectiveDurationSeconds(Owner), 6.0f, 0.0001f);
    TestEqual(TEXT("Cleave's authored arc with no ranks"), Cleave->ComputeEffectiveArcDegrees(Owner), 120.0f, 0.0001f);
    TestEqual(TEXT("Cleave's authored range with no ranks"), Cleave->ComputeEffectiveRangeCm(Owner), 300.0f, 0.0001f);

    // Lingering (VW tree): two ranks of +15% duration reach Rot's zone.
    UBreakerProgressionTree* VoidWhisperer = UBreakerProgressionLibrary::GetCasterVoidWhispererTree();
    TestTrue(TEXT("Standing Water buys"), BreakerBuyToMax(*this, Progression, VoidWhisperer, TEXT("Caster.VoidWhisperer.StandingWater")));
    TestTrue(TEXT("Lingering buys"), BreakerBuyToMax(*this, Progression, VoidWhisperer, TEXT("Caster.VoidWhisperer.Lingering")));
    TestEqual(TEXT("Lingering lengthens Rot's zone (6s -> 7.8s)"), Rot->ComputeEffectiveDurationSeconds(Owner), 7.8f, 0.0001f);
    TestEqual(TEXT("Lingering does not widen Rot's radius"), Rot->ComputeEffectiveRadiusCm(Owner), 400.0f, 0.0001f);

    // Edge (Spellblade tree): the tag consumer — Cleave's arc becomes SB8's
    // 180 the moment the node is owned, and narrows back on respec.
    UBreakerProgressionTree* Spellblade = UBreakerProgressionLibrary::GetCasterSpellbladeTree();
    TestTrue(TEXT("Close buys"), BreakerBuyToMax(*this, Progression, Spellblade, TEXT("Caster.Spellblade.Close")));
    TestTrue(TEXT("Bloodprice buys"), BreakerBuyToMax(*this, Progression, Spellblade, TEXT("Caster.Spellblade.Bloodprice")));
    TestTrue(TEXT("Edge buys"), BreakerBuyToMax(*this, Progression, Spellblade, TEXT("Caster.Spellblade.Edge")));
    TestEqual(TEXT("Edge widens Cleave's swing to the full sweep"), Cleave->ComputeEffectiveArcDegrees(Owner), 180.0f, 0.0001f);

    FText Failure;
    TestTrue(TEXT("Respec succeeds"), Progression->RespecAtForge(EBreakerPointCurrency::CorePoints, true, Failure));
    TestEqual(TEXT("Respec narrows the swing back"), Cleave->ComputeEffectiveArcDegrees(Owner), 120.0f, 0.0001f);
    TestEqual(TEXT("Respec shortens the zone back"), Rot->ComputeEffectiveDurationSeconds(Owner), 6.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// Node-level content pins for everything this pass made live, in the
// tree-content style: exact targets, buckets, values and conditions, so the
// transcribed Class-Kits numbers cannot drift silently.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLoopValveNodeContentTest,
    "RiorsEdge.Progression.LoopValve.NodeContent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLoopValveNodeContentTest::RunTest(const FString& Parameters)
{
    struct FBreakerExpectedEffect
    {
        EBreakerNodeStatTarget Target;
        float Value;
        EBreakerBuildCondition Condition;
    };
    struct FBreakerExpectedNode
    {
        const UBreakerProgressionTree* Tree;
        const TCHAR* NodeId;
        TArray<FBreakerExpectedEffect> Effects;
    };

    const TArray<FBreakerExpectedNode> Expected = {
        // K11: grounded +50 (Class-Kits §1.4), airborne suspension.
        { UBreakerProgressionLibrary::GetSwiftKineticTree(), TEXT("Swift.Kinetic.NoGround"),
            {{EBreakerNodeStatTarget::ClassResourceDecay, 50.0f, EBreakerBuildCondition::Grounded},
             {EBreakerNodeStatTarget::ClassResourceDecay, -100.0f, EBreakerBuildCondition::Airborne}} },
        // M9: no decay while ADS (Class-Kits §1.5).
        { UBreakerProgressionLibrary::GetSwiftMarksmanTree(), TEXT("Swift.Marksman.Reserve"),
            {{EBreakerNodeStatTarget::ClassResourceDecay, -100.0f, EBreakerBuildCondition::Aiming}} },
        // F11: decay doubled, abilities 40% cheaper (Class-Kits §1.3).
        { UBreakerProgressionLibrary::GetSwiftFrenzyTree(), TEXT("Swift.Frenzy.NoSafety"),
            {{EBreakerNodeStatTarget::ClassResourceDecay, 100.0f, EBreakerBuildCondition::Always},
             {EBreakerNodeStatTarget::AbilityCost, 40.0f, EBreakerBuildCondition::Always}} },
        // Redirect: the cooldown lane while airborne (O2 PLACEHOLDER value).
        { UBreakerProgressionLibrary::GetSwiftKineticTree(), TEXT("Swift.Kinetic.Redirect"),
            {{EBreakerNodeStatTarget::AbilityCooldown, 20.0f, EBreakerBuildCondition::Airborne}} },
        // VW4 Lingering: the duration lane (O2 PLACEHOLDER value).
        { UBreakerProgressionLibrary::GetCasterVoidWhispererTree(), TEXT("Caster.VoidWhisperer.Lingering"),
            {{EBreakerNodeStatTarget::AbilityDuration, 15.0f, EBreakerBuildCondition::Always}} },
    };

    for (const FBreakerExpectedNode& Row : Expected)
    {
        const UBreakerProgressionNode* Node = Row.Tree ? Row.Tree->FindNode(Row.NodeId) : nullptr;
        if (!TestNotNull(*FString::Printf(TEXT("%s is authored"), Row.NodeId), Node)) continue;
        const FString Context = Node->NodeId.ToString();
        TestEqual(*(Context + TEXT(" authors exactly the expected lines")), Node->Effects.Num(), Row.Effects.Num());
        for (int32 Index = 0; Index < FMath::Min(Node->Effects.Num(), Row.Effects.Num()); ++Index)
        {
            const FBreakerNodeEffect& Effect = Node->Effects[Index];
            const FBreakerExpectedEffect& Want = Row.Effects[Index];
            TestEqual(*(Context + TEXT(" line target")), static_cast<int32>(Effect.StatTarget), static_cast<int32>(Want.Target));
            TestEqual(*(Context + TEXT(" line bucket is IncreasedPercent")),
                static_cast<int32>(Effect.StatBucket), static_cast<int32>(EBreakerNodeStatBucket::IncreasedPercent));
            TestEqual(*(Context + TEXT(" line value")), Effect.ValuePerRank, Want.Value, 0.0001f);
            TestEqual(*(Context + TEXT(" line condition")), static_cast<int32>(Effect.Condition), static_cast<int32>(Want.Condition));
        }
        // Every one of these keeps its identity tag — the ids, tags, costs
        // and gates were required to stay stable through the re-authoring.
        TestTrue(*(Context + TEXT(" keeps its rule tag")), Node->GrantedTags.Num() > 0);
    }

    // Edge stays a pure rule tag — SB8 is explicit that it carries no
    // percentage — and its consumer is Cleave's geometry accessor, proven in
    // the seam test above.
    const UBreakerProgressionNode* Edge = UBreakerProgressionLibrary::GetCasterSpellbladeTree()->FindNode(TEXT("Caster.Spellblade.Edge"));
    if (TestNotNull(TEXT("Edge is authored"), Edge))
    {
        TestEqual(TEXT("Edge authors no stat line (SB8: rule change only)"), Edge->Effects.Num(), 0);
        TestTrue(TEXT("Edge keeps its rule tag"), Edge->GrantedTags.HasTag(BreakerNodeTags::Node_SB_Edge.GetTag()));
    }
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
