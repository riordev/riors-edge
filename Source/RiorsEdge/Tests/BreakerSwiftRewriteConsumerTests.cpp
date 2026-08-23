#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "GameFramework/Actor.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"

// ---------------------------------------------------------------------------
// SWIFT'S WAITING KINETIC/FRENZY REWRITES PAY (2026-08-16). One contract
// throughout, the same as BreakerBranchNodeConsumerTests: buy the node -> an
// observable rule change; without it, bit-identical behaviour. Skim Discipline
// (both halves), Spend to Live, Momentum Shield, and Second Wind's Cadence
// Break host are the nodes under test.
// ---------------------------------------------------------------------------

// Skim's airtime ceiling and Spend to Live's two halves, as the pure rules the
// ability activates through.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSkimAirtimeAndSpendToLiveTest,
    "RiorsEdge.Abilities.SkimAirtimeAndSpendToLive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkimAirtimeAndSpendToLiveTest::RunTest(const FString& Parameters)
{
    // Class-Kits §1.2 S3: once per airtime. §1.4 K7: twice with the node.
    TestEqual(TEXT("Base kit skims once per airtime"), UBreakerAbility_Skim::MaxAirborneUses(false), 1);
    TestEqual(TEXT("Skim Discipline raises the ceiling to two"), UBreakerAbility_Skim::MaxAirborneUses(true), 2);

    // Spend to Live's cost half: "it costs twice the Momentum" (node text) —
    // and ONLY on a cast that resolves as Hard Stop. Everything else is the
    // bit-identity half.
    TestEqual(TEXT("Without the node a Hard Stop costs its authored price"),
        UBreakerAbility_Skim::HardStopCostMultiplier(true, false), 1.0f);
    TestEqual(TEXT("With the node a plain redirect is untouched"),
        UBreakerAbility_Skim::HardStopCostMultiplier(false, true), 1.0f);
    TestEqual(TEXT("With the node a Hard Stop costs twice the Momentum"),
        UBreakerAbility_Skim::HardStopCostMultiplier(true, true), 2.0f);

    // The protective window (§1.2 S4 base, §1.4 K10 rewrite): a reduction
    // becomes the incoming chain's own 0.0 — immunity — with the node.
    TestEqual(TEXT("Base Hard Stop reduces incoming damage for the window"),
        UBreakerAbility_Skim::HardStopIncomingMultiplier(false, 0.30f), 0.70f);
    TestEqual(TEXT("Spend to Live turns the window into true immunity"),
        UBreakerAbility_Skim::HardStopIncomingMultiplier(true, 0.30f), 0.0f);
    TestEqual(TEXT("A nonsense fraction clamps instead of inverting"),
        UBreakerAbility_Skim::HardStopIncomingMultiplier(false, 2.0f), 0.0f);

    // Structural guards: the window must expire before the next Hard Stop can
    // exist (Skim's cooldown is 3s), or the timed removal could strip a newer
    // window's chain entry.
    TestTrue(TEXT("The window is shorter than Skim's cooldown"),
        UBreakerAbility_Skim::HardStopWindowSeconds < 3.0f);
    TestFalse(TEXT("The window's chain key is real"), UBreakerAbility_Skim::HardStopWindowKey().IsNone());
    return true;
}

// K9 Momentum Shield on the house world-less rig: the loop pushes a keyed
// incoming-damage reduction exactly while Redline and grounded, and a rig
// without the node never touches the chain.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumShieldLoopTest,
    "RiorsEdge.Classes.MomentumShieldLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumShieldLoopTest::RunTest(const FString& Parameters)
{
    // The pure rule first: 1.0 (bit-identical) everywhere except the node's
    // one named condition.
    TestEqual(TEXT("Without the node the chain sees nothing"),
        UBreakerMomentumComponent::MomentumShieldIncomingMultiplier(false, EBreakerMomentumState::Redline, true, 0.25f), 1.0f);
    TestEqual(TEXT("Off Redline the chain sees nothing"),
        UBreakerMomentumComponent::MomentumShieldIncomingMultiplier(true, EBreakerMomentumState::Running, true, 0.25f), 1.0f);
    TestEqual(TEXT("Airborne is the affix's posture, not the node's"),
        UBreakerMomentumComponent::MomentumShieldIncomingMultiplier(true, EBreakerMomentumState::Redline, false, 0.25f), 1.0f);
    TestEqual(TEXT("Redline and grounded pays the reduction"),
        UBreakerMomentumComponent::MomentumShieldIncomingMultiplier(true, EBreakerMomentumState::Redline, true, 0.25f), 0.75f);

    // The rig: Swift, node owned, bank seeded to Redline BEFORE the bind so
    // the cached band is current on the first tick.
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerCharacterMovementComponent* Movement = NewObject<UBreakerCharacterMovementComponent>(Owner);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Owner);
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    TestNotNull(TEXT("The movement component attaches to the rig"), Movement);
    Progression->BindAttributes(Attributes);
    FBreakerProgressionState State;
    State.PermanentClass = EBreakerClassId::Swift;
    State.CoreNodeRanks.Add({FName(TEXT("Swift.Kinetic.MomentumShield")), 1});
    Progression->LoadProgressionState(State);
    Combat->BindAttributes(Attributes);
    Attributes->ApplyClassResource(90.0f);
    Momentum->BindAttributes(Attributes);
    TestEqual(TEXT("The seeded bank reads as Redline"), Momentum->GetMomentumState(), EBreakerMomentumState::Redline);

    // Grounded tick (the rig's movement default is not falling): the shield
    // lands on the chain at the placeholder fraction.
    Momentum->AdvanceLoop(0.05f);
    TestEqual(TEXT("At Redline, grounded, the shield holds the chain"),
        Combat->GetComposedIncomingDamageMultiplier(), 1.0f - Momentum->MomentumShieldReductionFraction);

    // Dropping out of Redline removes it (one tick to refresh the band, one
    // for the shield to follow — the loop's own one-frame tolerance).
    Attributes->ApplyClassResource(10.0f);
    Momentum->AdvanceLoop(0.05f);
    Momentum->AdvanceLoop(0.05f);
    TestEqual(TEXT("Below Redline the chain entry is removed"),
        Combat->GetComposedIncomingDamageMultiplier(), 1.0f);

    // Bit-identity: the same rig without the node never touches the chain.
    AActor* BareOwner = NewObject<AActor>();
    UBreakerProgressionComponent* BareProgression = NewObject<UBreakerProgressionComponent>(BareOwner);
    UBreakerCharacterMovementComponent* BareMovement = NewObject<UBreakerCharacterMovementComponent>(BareOwner);
    UBreakerCombatComponent* BareCombat = NewObject<UBreakerCombatComponent>(BareOwner);
    UBreakerMomentumComponent* BareMomentum = NewObject<UBreakerMomentumComponent>(BareOwner);
    UBreakerAttributeSet* BareAttributes = NewObject<UBreakerAttributeSet>();
    TestNotNull(TEXT("The bare rig's movement component attaches too"), BareMovement);
    BareProgression->DevForceClass(EBreakerClassId::Swift);
    BareCombat->BindAttributes(BareAttributes);
    BareAttributes->ApplyClassResource(90.0f);
    BareMomentum->BindAttributes(BareAttributes);
    BareMomentum->AdvanceLoop(0.05f);
    TestEqual(TEXT("Without the node a Redline Swift's chain is untouched"),
        BareCombat->GetComposedIncomingDamageMultiplier(), 1.0f);
    return true;
}

// S2 Cadence Break exists as its own registry row (closing the gap Slipcut
// Mastery and Second Wind record), with Class-Kits §1.2 S2's quoted numbers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCadenceBreakRegistryTest,
    "RiorsEdge.Abilities.CadenceBreakRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCadenceBreakRegistryTest::RunTest(const FString& Parameters)
{
    const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.CadenceBreak"));
    if (!TestNotNull(TEXT("Swift.CadenceBreak is now a fallback registry row"), Definition))
    {
        return false;
    }
    TestEqual(TEXT("S2 costs 35 Momentum"), Definition->ResourceCost, 35.0f);
    TestEqual(TEXT("S2 cools down for 8s"), Definition->CooldownSeconds, 8.0f);
    TestEqual(TEXT("S2's state lasts 3s"), Definition->WindowDuration, 3.0f);
    TestEqual(TEXT("S2 belongs to Swift"), Definition->ClassId, EBreakerClassId::Swift);
    TestTrue(TEXT("The row resolves to the real ability class"),
        Definition->AbilityClass == UBreakerAbility_CadenceBreak::StaticClass());
    TestNotEqual(TEXT("The window key is distinct from the modifier key"),
        UBreakerAbility_CadenceBreak::WindowKey(), UBreakerAbility_CadenceBreak::ModifierKey());
    return true;
}

// The stacking rule, base against F9 Second Wind. Base: resets on miss or
// target swap (§1.2 S2). Second Wind: "Only a full second without a hit
// resets it" (Swift.Frenzy.SecondWind node text; Class-Kits §1.3 F9).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCadenceBreakStackRuleTest,
    "RiorsEdge.Abilities.CadenceBreakStacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCadenceBreakStackRuleTest::RunTest(const FString& Parameters)
{
    using FCadence = UBreakerAbility_CadenceBreak;
    constexpr float Quick = 0.2f;   // well inside the fire cadence
    constexpr float Gap = 1.5f;     // a full second without a hit, and then some

    // Base rule.
    TestEqual(TEXT("The first hit opens the stack at 1"), FCadence::NextStackCount(0, true, false, Quick, false), 1);
    TestEqual(TEXT("A same-target hit increments"), FCadence::NextStackCount(4, true, true, Quick, false), 5);
    TestEqual(TEXT("A miss zeroes the stack"), FCadence::NextStackCount(7, false, false, Quick, false), 0);
    TestEqual(TEXT("A target swap restarts at the new target's first hit"), FCadence::NextStackCount(7, true, false, Quick, false), 1);
    TestEqual(TEXT("The 11th consecutive hit does not exceed 10 stacks"), FCadence::NextStackCount(10, true, true, Quick, false), 10);

    // Second Wind: the swap and the miss stop mattering; the hit clock is the
    // one reset. Bit-for-bit divergence from the base rule is the node.
    TestEqual(TEXT("Second Wind carries the stack through a swap"), FCadence::NextStackCount(7, true, false, Quick, true), 8);
    TestEqual(TEXT("Second Wind carries the stack through a miss"), FCadence::NextStackCount(7, false, false, Quick, true), 7);
    TestEqual(TEXT("A full second without a hit resets to the arriving hit"), FCadence::NextStackCount(7, true, true, Gap, true), 1);
    TestEqual(TEXT("Second Wind still caps at 10"), FCadence::NextStackCount(10, true, false, Quick, true), 10);
    // Exactly one second is NOT "a full second without a hit" exceeded — the
    // reset needs the second to have fully passed.
    TestEqual(TEXT("The boundary second itself still continues the stack"),
        FCadence::NextStackCount(3, true, true, FCadence::SecondWindResetSeconds, true), 4);
    return true;
}

#endif
