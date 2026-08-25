#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbility_CadenceBreak.h"
#include "Abilities/BreakerAbility_HardStop.h"
#include "Abilities/BreakerAbility_Sightline.h"
#include "Abilities/BreakerAbility_Skim.h"
#include "Abilities/BreakerAbility_Slipcut.h"
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

    // Spend to Live's cost half: "it costs twice the Momentum" (node text).
    // Since O177 the payer is the standalone Hard Stop, at S4's authored 30 —
    // the doubling is the doc's own 30 -> 60.
    TestEqual(TEXT("Without the node Hard Stop costs its authored price"),
        UBreakerAbility_HardStop::CostMultiplier(false), 1.0f);
    TestEqual(TEXT("With the node Hard Stop costs twice the Momentum"),
        UBreakerAbility_HardStop::CostMultiplier(true), 2.0f);

    // The protective window (§1.2 S4 base, §1.4 K10 rewrite): a reduction
    // becomes the incoming chain's own 0.0 — immunity — with the node.
    TestEqual(TEXT("Base Hard Stop reduces incoming damage for the window"),
        UBreakerAbility_HardStop::IncomingMultiplier(false, 0.30f), 0.70f);
    TestEqual(TEXT("Spend to Live turns the window into true immunity"),
        UBreakerAbility_HardStop::IncomingMultiplier(true, 0.30f), 0.0f);
    TestEqual(TEXT("A nonsense fraction clamps instead of inverting"),
        UBreakerAbility_HardStop::IncomingMultiplier(false, 2.0f), 0.0f);

    // Structural guards: the window must expire before the next Hard Stop can
    // exist (its cooldown is 6s), or the timed removal could strip a newer
    // window's chain entry.
    TestTrue(TEXT("The window is shorter than Hard Stop's cooldown"),
        UBreakerAbility_HardStop::WindowSeconds < 6.0f);
    TestFalse(TEXT("The window's chain key is real"), UBreakerAbility_HardStop::IncomingModifierKey().IsNone());
    TestFalse(TEXT("The HUD window key is real"), UBreakerAbility_HardStop::WindowKey().IsNone());
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
    State.DoctrineNodeRanks.Add({FName(TEXT("Swift.Kinetic.MomentumShield")), 1});
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

// The three O175 rows, each with §1.2's quoted numbers and its real ability
// class — the same shape as the CadenceBreak row test above, because a row
// whose id or class drifts resolves to nothing at the one grant site.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSwiftRosterCompleteTest,
    "RiorsEdge.Abilities.SwiftRosterComplete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSwiftRosterCompleteTest::RunTest(const FString& Parameters)
{
    const UBreakerAbilityDefinition* Slipcut = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Slipcut"));
    if (TestNotNull(TEXT("Swift.Slipcut is a registry row"), Slipcut))
    {
        TestEqual(TEXT("S1 costs 20 Momentum"), Slipcut->ResourceCost, 20.0f);
        TestEqual(TEXT("S1 cools down for 4s"), Slipcut->CooldownSeconds, 4.0f);
        TestEqual(TEXT("S1's window is 0.4s"), Slipcut->WindowDuration, 0.4f);
        TestEqual(TEXT("S1 belongs to Swift"), Slipcut->ClassId, EBreakerClassId::Swift);
        TestTrue(TEXT("S1 resolves to the real ability class"),
            Slipcut->AbilityClass == UBreakerAbility_Slipcut::StaticClass());
    }

    const UBreakerAbilityDefinition* HardStop = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.HardStop"));
    if (TestNotNull(TEXT("Swift.HardStop is a registry row"), HardStop))
    {
        TestEqual(TEXT("S4 costs 30 Momentum"), HardStop->ResourceCost, 30.0f);
        TestEqual(TEXT("S4 cools down for 6s"), HardStop->CooldownSeconds, 6.0f);
        TestEqual(TEXT("S4's window is 0.6s"), HardStop->WindowDuration, 0.6f);
        TestEqual(TEXT("S4 belongs to Swift"), HardStop->ClassId, EBreakerClassId::Swift);
        TestTrue(TEXT("S4 resolves to the real ability class"),
            HardStop->AbilityClass == UBreakerAbility_HardStop::StaticClass());
    }

    const UBreakerAbilityDefinition* Sightline = UBreakerAbilityDefinition::FindFallback(TEXT("Swift.Sightline"));
    if (TestNotNull(TEXT("Swift.Sightline is a registry row"), Sightline))
    {
        TestEqual(TEXT("S5 costs 25 Momentum"), Sightline->ResourceCost, 25.0f);
        TestEqual(TEXT("S5 cools down for 6s"), Sightline->CooldownSeconds, 6.0f);
        TestEqual(TEXT("S5's window is 2s"), Sightline->WindowDuration, 2.0f);
        TestEqual(TEXT("S5 belongs to Swift"), Sightline->ClassId, EBreakerClassId::Swift);
        TestTrue(TEXT("S5 resolves to the real ability class"),
            Sightline->AbilityClass == UBreakerAbility_Sightline::StaticClass());
    }

    // The catalogue, derived: Swift now offers six class abilities and one
    // ultimate, which is the design's 6+1 (Docs/spec/classes-and-abilities.md:
    // "Two abilities plus one ultimate are equipped, from six per class").
    TestEqual(TEXT("Swift's class-slot catalogue is six"),
        UBreakerAbilityDefinition::GetClassAbilityIds(EBreakerClassId::Swift, EBreakerAbilitySlot::ClassAbilityOne).Num(), 6);
    TestEqual(TEXT("Swift's ultimate catalogue is one"),
        UBreakerAbilityDefinition::GetClassAbilityIds(EBreakerClassId::Swift, EBreakerAbilitySlot::Ultimate).Num(), 1);
    return true;
}

// Slipcut's window rule (S1 base, F7 Slipcut Mastery extension) and the
// structural guards on its keys.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSlipcutWindowRuleTest,
    "RiorsEdge.Abilities.SlipcutWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSlipcutWindowRuleTest::RunTest(const FString& Parameters)
{
    using FSlipcut = UBreakerAbility_Slipcut;
    // Base rule: the authored window, untouched — with any number of
    // cooldowns running. The node is the only thing that reads them.
    TestEqual(TEXT("Without Mastery the window is the authored window"),
        FSlipcut::WindowSeconds(0.4f, 3, false), 0.4f);
    // F7: +0.15s per running ability cooldown (deleted §1.3's figure, O2).
    TestEqual(TEXT("Mastery with no cooldowns running adds nothing"),
        FSlipcut::WindowSeconds(0.4f, 0, true), 0.4f);
    TestEqual(TEXT("Mastery widens by 0.15s per running cooldown"),
        FSlipcut::WindowSeconds(0.4f, 2, true), 0.7f);
    TestEqual(TEXT("A negative count cannot narrow the window"),
        FSlipcut::WindowSeconds(0.4f, -3, true), 0.4f);

    // The cadence rewrite is a rate statement: 2x, and the window is shorter
    // than the cooldown so two windows can never overlap one keyed push.
    TestEqual(TEXT("S1 fires at 2x rate"), FSlipcut::CadenceMultiplier, 2.0f);
    TestTrue(TEXT("The window is shorter than Slipcut's cooldown"), 0.4f < 4.0f);
    TestFalse(TEXT("The cadence key is real"), FSlipcut::CadenceKey().IsNone());
    TestNotEqual(TEXT("The window key is distinct from the cadence key"),
        FSlipcut::WindowKey(), FSlipcut::CadenceKey());

    // Sightline's structural guards ride here: one granted-rule pierce count,
    // finite ("all targets in a clear line", not an overflow), and real keys.
    TestTrue(TEXT("Sightline's pierce grant is finite and large"),
        UBreakerAbility_Sightline::AllTargetsPierceCount >= 16 && UBreakerAbility_Sightline::AllTargetsPierceCount < 1000);
    TestFalse(TEXT("Sightline's channel key is real"), UBreakerAbility_Sightline::ChannelKey().IsNone());
    TestNotEqual(TEXT("Sightline's window key is distinct from its channel key"),
        UBreakerAbility_Sightline::WindowKey(), UBreakerAbility_Sightline::ChannelKey());
    return true;
}

#endif
