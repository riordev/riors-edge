#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerManaComponent.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaGenerationTest,
    "RiorsEdge.Classes.ManaGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaGenerationTest::RunTest(const FString& Parameters)
{
    // Single-pellet weapons bank the flat weapon-hit gain.
    TestEqual(TEXT("A rifle hit banks the weapon-hit gain"), UBreakerManaComponent::HitGeneration(false, 1, 1, 1.5f, 4.0f, 1.0f), 1.5f);
    TestEqual(TEXT("A rifle weak point replaces the weapon-hit gain"), UBreakerManaComponent::HitGeneration(true, 1, 1, 1.5f, 4.0f, 1.0f), 4.0f);
    TestEqual(TEXT("A miss banks nothing"), UBreakerManaComponent::HitGeneration(false, 0, 1, 1.5f, 4.0f, 1.0f), 0.0f);

    // Anti-Multishot: each pellet is worth 1/n, so a full volley matches a rifle hit.
    TestEqual(TEXT("One pellet of eight banks an eighth"), UBreakerManaComponent::HitGeneration(false, 1, 8, 1.5f, 4.0f, 1.0f), 1.5f / 8.0f);
    TestEqual(TEXT("Half a volley banks half"), UBreakerManaComponent::HitGeneration(false, 4, 8, 1.5f, 4.0f, 1.0f), 0.75f);
    TestEqual(TEXT("A full volley banks exactly one rifle hit"), UBreakerManaComponent::HitGeneration(false, 8, 8, 1.5f, 4.0f, 1.0f), 1.5f);
    TestEqual(TEXT("Landed pellets cannot exceed the volley"), UBreakerManaComponent::HitGeneration(false, 99, 8, 1.5f, 4.0f, 1.0f), 1.5f);

    // A weak point pays for exactly one pellet; the rest stay at the hit rate.
    TestEqual(TEXT("A weak-point volley pays one pellet at the weak-point rate"), UBreakerManaComponent::HitGeneration(true, 8, 8, 1.5f, 4.0f, 1.0f), (4.0f + 1.5f * 7.0f) / 8.0f);
    TestEqual(TEXT("Weak point never stacks with the hit gain"), UBreakerManaComponent::HitGeneration(true, 1, 8, 1.5f, 4.0f, 1.0f), 4.0f / 8.0f);

    // Proc coefficient: a DoT tick carries 0 and must bank nothing.
    TestEqual(TEXT("A DoT tick banks nothing"), UBreakerManaComponent::HitGeneration(false, 1, 1, 1.5f, 4.0f, 0.0f), 0.0f);
    TestEqual(TEXT("A half-coefficient proc banks half"), UBreakerManaComponent::HitGeneration(false, 1, 1, 1.5f, 4.0f, 0.5f), 0.75f);

    TestEqual(TEXT("Generation clamps to the global cap"), UBreakerManaComponent::ClampGeneration(50.0f, 20.0f), 20.0f);
    TestEqual(TEXT("Generation below the cap passes through"), UBreakerManaComponent::ClampGeneration(12.0f, 20.0f), 12.0f);
    TestEqual(TEXT("Generation never goes negative"), UBreakerManaComponent::ClampGeneration(-4.0f, 20.0f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaOvercastTest,
    "RiorsEdge.Classes.ManaOvercast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaOvercastTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("An empty bank is not Overcast"), UBreakerManaComponent::IsOvercastValue(0.0f));
    TestFalse(TEXT("A positive bank is not Overcast"), UBreakerManaComponent::IsOvercastValue(35.0f));
    TestTrue(TEXT("A negative bank is Overcast"), UBreakerManaComponent::IsOvercastValue(-5.0f));

    TestEqual(TEXT("Generation is unchanged above zero"), UBreakerManaComponent::GenerationMultiplierForMana(10.0f, 2.0f), 1.0f);
    TestEqual(TEXT("Generation doubles while in debt"), UBreakerManaComponent::GenerationMultiplierForMana(-10.0f, 2.0f), 2.0f);
    TestEqual(TEXT("Doubling stops the instant the debt clears"), UBreakerManaComponent::GenerationMultiplierForMana(0.0f, 2.0f), 1.0f);

    // The bank is bounded by the Overcast floor below and MaxClassResource above.
    TestEqual(TEXT("Spending stops at the Overcast floor"), UBreakerManaComponent::ClampToBank(-45.0f, -20.0f, 100.0f), -20.0f);
    TestEqual(TEXT("Generation stops at the ceiling"), UBreakerManaComponent::ClampToBank(140.0f, -20.0f, 100.0f), 100.0f);
    TestEqual(TEXT("Values inside the bank pass through"), UBreakerManaComponent::ClampToBank(-7.5f, -20.0f, 100.0f), -7.5f);
    TestEqual(TEXT("A positive floor is treated as zero"), UBreakerManaComponent::ClampToBank(-5.0f, 10.0f, 100.0f), 0.0f);

    // Overcast is a debt, not a spiral.
    TestTrue(TEXT("An affordable cast is allowed"), UBreakerManaComponent::CanSpendFrom(40.0f, 30.0f, -20.0f));
    TestTrue(TEXT("A cast may drive the bank negative"), UBreakerManaComponent::CanSpendFrom(10.0f, 30.0f, -20.0f));
    TestTrue(TEXT("A cast may land exactly on the floor"), UBreakerManaComponent::CanSpendFrom(10.0f, 30.0f, -20.0f));
    TestFalse(TEXT("A cast may not pass the floor"), UBreakerManaComponent::CanSpendFrom(10.0f, 31.0f, -20.0f));
    TestFalse(TEXT("Nothing may be cast while already Overcast"), UBreakerManaComponent::CanSpendFrom(-1.0f, 5.0f, -20.0f));
    TestTrue(TEXT("A free cast is always allowed"), UBreakerManaComponent::CanSpendFrom(-1.0f, 0.0f, -20.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaInertTest,
    "RiorsEdge.Classes.ManaInert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaInertTest::RunTest(const FString& Parameters)
{
    UBreakerManaComponent* Mana = NewObject<UBreakerManaComponent>();
    TestFalse(TEXT("A componentless owner is inert"), Mana->IsActiveForOwner());
    TestFalse(TEXT("An inert component is never Overcast"), Mana->IsOvercast());
    TestEqual(TEXT("An inert component publishes no damage penalty"), Mana->GetOvercastIncomingDamageTaken(), 0.0f);
    TestFalse(TEXT("An inert component affords nothing"), Mana->CanAffordSpend(1.0f));
    TestEqual(TEXT("Defaults match the Class-Kits weapon-hit gain"), Mana->WeaponHitGain, 1.5f);
    TestEqual(TEXT("Defaults match the Class-Kits weak-point gain"), Mana->WeakPointGain, 4.0f);
    // The two numbers the 2026-08-14 inversion moved. Pinned so a silent revert
    // to the accumulating-bank weighting shows up as a failing test rather than
    // as a Caster who quietly stops needing to regenerate.
    TestEqual(TEXT("Passive regeneration is the primary recovery path"), Mana->PassiveRegenPerSecond, 6.0f);
    TestEqual(TEXT("The conditional-income cap is at par with regeneration"), Mana->GlobalGenerationCap, 6.0f);
    TestEqual(TEXT("Defaults match the Class-Kits Overcast floor"), Mana->GetOvercastFloor(), -20.0f);
    TestEqual(TEXT("Defaults match the Class-Kits Overcast damage penalty"), Mana->OvercastIncomingDamageTaken, 0.15f);

    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    TestFalse(TEXT("An unselected class is not Caster"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster);
    Progression->ChoosePermanentClassById(EBreakerClassId::Swift);
    TestFalse(TEXT("A Swift never runs the Mana loop"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster);
    Progression->DevForceClass(EBreakerClassId::Caster);
    TestTrue(TEXT("Caster runs the Mana loop"), Progression->GetProgressionState().PermanentClass == EBreakerClassId::Caster);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaIncomingDamageTest,
    "RiorsEdge.Classes.ManaIncomingDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaIncomingDamageTest::RunTest(const FString& Parameters)
{
    // Class-Kits §2.1: while Overcast the player takes 15% increased damage.
    TestEqual(TEXT("Not Overcast means no penalty"), UBreakerManaComponent::OvercastIncomingMultiplier(false, 0.15f), 1.0f);
    TestEqual(TEXT("Overcast means 1.15x incoming"), UBreakerManaComponent::OvercastIncomingMultiplier(true, 0.15f), 1.15f);
    // Overcast is a cost. A node that somehow supplied a negative penalty must
    // not turn the debt into a defensive buff.
    TestEqual(TEXT("A negative penalty never becomes mitigation"), UBreakerManaComponent::OvercastIncomingMultiplier(true, -0.5f), 1.0f);
    TestFalse(TEXT("The modifier key is real"), UBreakerManaComponent::OvercastDamageModifierKey().IsNone());

    // The combat-side chain the penalty rides on.
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>();
    TestEqual(TEXT("An untouched chain changes nothing"), Combat->GetComposedIncomingDamageMultiplier(), 1.0f);

    Combat->PushIncomingDamageModifier(UBreakerManaComponent::OvercastDamageModifierKey(), 1.15f);
    TestEqual(TEXT("The Overcast penalty composes in"), Combat->GetComposedIncomingDamageMultiplier(), 1.15f);
    // Re-pushing replaces rather than stacks: a debt re-entered mid-window must
    // not compound into 1.32x.
    Combat->PushIncomingDamageModifier(UBreakerManaComponent::OvercastDamageModifierKey(), 1.15f);
    TestEqual(TEXT("Re-pushing the same key replaces rather than stacks"), Combat->GetComposedIncomingDamageMultiplier(), 1.15f);

    // Independent keys compose multiplicatively, and each reverts alone.
    Combat->PushIncomingDamageModifier(TEXT("Test.Window"), 0.5f);
    TestEqual(TEXT("Independent modifiers compose"), Combat->GetComposedIncomingDamageMultiplier(), 1.15f * 0.5f);
    Combat->RemoveIncomingDamageModifier(TEXT("Test.Window"));
    TestEqual(TEXT("Removing one leaves the other"), Combat->GetComposedIncomingDamageMultiplier(), 1.15f);
    Combat->RemoveIncomingDamageModifier(UBreakerManaComponent::OvercastDamageModifierKey());
    TestEqual(TEXT("Clearing the debt clears the penalty"), Combat->GetComposedIncomingDamageMultiplier(), 1.0f);

    // Full immunity is expressible, and a nameless push is ignored.
    Combat->PushIncomingDamageModifier(TEXT("Test.Immune"), 0.0f);
    TestEqual(TEXT("A zero multiplier is immunity"), Combat->GetComposedIncomingDamageMultiplier(), 0.0f);
    Combat->RemoveIncomingDamageModifier(TEXT("Test.Immune"));
    Combat->PushIncomingDamageModifier(NAME_None, 2.0f);
    TestEqual(TEXT("A nameless modifier is refused"), Combat->GetComposedIncomingDamageMultiplier(), 1.0f);
    // Negative multipliers would heal the victim. Clamped at push.
    Combat->PushIncomingDamageModifier(TEXT("Test.Negative"), -3.0f);
    TestEqual(TEXT("A negative multiplier is clamped to immunity, never healing"), Combat->GetComposedIncomingDamageMultiplier(), 0.0f);
    return true;
}

namespace BreakerOvercastTestHelpers
{
    // A freshly constructed actor is ROLE_Authority, which is all the server
    // paths in the Mana component require; nothing here needs a world (the
    // safe-zone query answers "no" without one) or an ability system (the
    // attribute set falls back to writing its own data under the same clamp).
    struct FCasterRig
    {
        AActor* Owner = nullptr;
        UBreakerProgressionComponent* Progression = nullptr;
        UBreakerManaComponent* Mana = nullptr;
        UBreakerAttributeSet* Attributes = nullptr;
    };

    // StartingMana is set EXPLICITLY after the rig is built, because binding a
    // Caster fills the bar to maximum (owner ruling 2026-08-14, "the mana bar
    // should be full and go down when using spells"). Every test below asserts
    // one rule; the fill itself has its own test rather than being an implicit
    // precondition of all the others.
    FCasterRig MakeRig(EBreakerClassId ClassId, float StartingMana = 0.0f)
    {
        FCasterRig Rig;
        Rig.Owner = NewObject<AActor>();
        Rig.Progression = NewObject<UBreakerProgressionComponent>(Rig.Owner);
        Rig.Mana = NewObject<UBreakerManaComponent>(Rig.Owner);
        Rig.Attributes = NewObject<UBreakerAttributeSet>();
        Rig.Progression->DevForceClass(ClassId);
        Rig.Mana->BindAttributes(Rig.Attributes);
        Rig.Attributes->ApplyClassResource(StartingMana);
        return Rig;
    }

    // Runs the component's own tick loop. The generation budget is metered per
    // second, so a long window is spent in slices exactly as it is in play.
    void RunFor(UBreakerManaComponent* Mana, float Seconds, float Step = 0.1f)
    {
        for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += Step)
        {
            Mana->AdvanceLoop(Step);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOvercastReachableTest,
    "RiorsEdge.Classes.OvercastReachable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOvercastReachableTest::RunTest(const FString& Parameters)
{
    using namespace BreakerOvercastTestHelpers;

    // This is the test the whole ClassResourceFloor attribute exists for: until
    // it landed, the bank clamped at zero and Overcast — the Caster's defining
    // mechanic — could not be entered at all.
    FCasterRig Caster = MakeRig(EBreakerClassId::Caster);
    TestTrue(TEXT("The Mana loop runs for a Caster"), Caster.Mana->IsActiveForOwner());
    TestEqual(TEXT("Becoming a Caster opens the floor on the attribute set"), Caster.Attributes->GetClassResourceFloor(), -20.0f);
    TestEqual(TEXT("The published floor is the Overcast floor"), Caster.Mana->GetPublishedFloor(), -20.0f);

    // Bank 30, then spend 45: the cast is affordable because it lands inside
    // the debt allowance, and the bank genuinely goes negative.
    Caster.Attributes->ApplyClassResource(30.0f);
    TestEqual(TEXT("The bank holds what it was given"), Caster.Mana->GetMana(), 30.0f);
    TestFalse(TEXT("A full bank is not Overcast"), Caster.Mana->IsOvercast());

    TestTrue(TEXT("A cast that exceeds the bank is affordable inside the floor"), Caster.Mana->CanAffordSpend(45.0f));
    TestTrue(TEXT("The overdraft spend succeeds"), Caster.Mana->TrySpendMana(45.0f));
    TestEqual(TEXT("The bank is driven negative — Overcast is entered"), Caster.Mana->GetMana(), -15.0f);
    TestTrue(TEXT("The component reports Overcast"), Caster.Mana->IsOvercast());
    TestEqual(TEXT("The incoming-damage penalty is published"), Caster.Mana->GetOvercastIncomingDamageTaken(), 0.15f);

    // "No further ability may be cast until Mana is at or above zero."
    TestFalse(TEXT("Nothing may be cast while in debt"), Caster.Mana->CanAffordSpend(1.0f));
    TestFalse(TEXT("A spend attempted in debt is refused"), Caster.Mana->TrySpendMana(1.0f));
    TestEqual(TEXT("A refused spend leaves the bank untouched"), Caster.Mana->GetMana(), -15.0f);

    // A cast that would breach the floor is REFUSED, not truncated to the
    // floor: a partial spend would hand the player a silent discount.
    FCasterRig Deep = MakeRig(EBreakerClassId::Caster, 10.0f);
    TestFalse(TEXT("A cast past the floor is refused"), Deep.Mana->CanAffordSpend(31.0f));
    TestFalse(TEXT("The over-floor spend does not happen"), Deep.Mana->TrySpendMana(31.0f));
    TestEqual(TEXT("A refused over-floor cast costs nothing"), Deep.Mana->GetMana(), 10.0f);
    TestTrue(TEXT("A cast landing exactly on the floor is allowed"), Deep.Mana->TrySpendMana(30.0f));
    TestEqual(TEXT("The bank rests exactly on the floor"), Deep.Mana->GetMana(), -20.0f);
    // And the attribute clamp is the backstop: even a direct write past the
    // floor lands ON the floor, so no path can produce unbounded debt.
    Deep.Attributes->ApplyClassResource(-500.0f);
    TestEqual(TEXT("Nothing drives the bank below the floor"), Deep.Mana->GetMana(), -20.0f);

    // SB4/MS10 deepen the floor at runtime.
    Deep.Mana->SetOvercastFloor(-35.0f);
    TestEqual(TEXT("A deepened floor reaches the attribute set"), Deep.Attributes->GetClassResourceFloor(), -35.0f);
    TestTrue(TEXT("The deeper floor allows more rope"), Deep.Mana->GetPublishedFloor() < -20.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOvercastRecoveryTest,
    "RiorsEdge.Classes.OvercastRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOvercastRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace BreakerOvercastTestHelpers;

    // Doubled generation while in debt (Class-Kits §2.1). Now that the debt is
    // reachable, confirm the doubling actually runs rather than merely existing.
    FCasterRig Caster = MakeRig(EBreakerClassId::Caster, 30.0f);
    // Passive regeneration OFF for this test only. It is the primary recovery
    // path since the 2026-08-14 inversion and would swamp the thing under test,
    // which is that CONDITIONAL credit is doubled while the bank is negative.
    // Regeneration's own doubling has its own test below.
    Caster.Mana->PassiveRegenPerSecond = 0.0f;
    Caster.Mana->TrySpendMana(45.0f);
    TestEqual(TEXT("The bank starts the recovery in debt"), Caster.Mana->GetMana(), -15.0f);

    // Metered generation: 10 of credit, paid out through the per-second budget. The
    // exact landing point depends on which slice crosses zero (the doubling is
    // evaluated against the bank as it stands when each credit is paid), so
    // assert the property rather than a step-size-sensitive number: 10 of
    // credit moved the bank by MORE than 10 and carried it out of debt.
    const float DebtBefore = Caster.Mana->GetMana();
    Caster.Mana->GrantMana(10.0f, /*bIgnoreGlobalCap*/ false);
    RunFor(Caster.Mana, 2.0f);
    TestTrue(TEXT("Doubled generation climbs the bank out of debt"), Caster.Mana->GetMana() > 0.0f);
    TestTrue(TEXT("Ten of credit repaid more than ten of debt"), Caster.Mana->GetMana() - DebtBefore > 10.0f);
    TestFalse(TEXT("Clearing the debt leaves Overcast"), Caster.Mana->IsOvercast());
    TestEqual(TEXT("Leaving Overcast drops the damage penalty"), Caster.Mana->GetOvercastIncomingDamageTaken(), 0.0f);

    // Above zero the doubling stops: 10 more credit banks exactly 10.
    const float BankAfterRecovery = Caster.Mana->GetMana();
    Caster.Mana->GrantMana(10.0f, false);
    RunFor(Caster.Mana, 2.0f);
    TestEqual(TEXT("Generation is single rate once the debt is cleared"), Caster.Mana->GetMana(), BankAfterRecovery + 10.0f, 0.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOvercastFloorClosesTest,
    "RiorsEdge.Classes.OvercastFloorCloses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOvercastFloorClosesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerOvercastTestHelpers;

    // A class change must never strand a negative floor — or a bank sitting
    // below the floor that replaces it, which would be permanent debt.
    FCasterRig Rig = MakeRig(EBreakerClassId::Caster, 30.0f);
    Rig.Mana->TrySpendMana(45.0f);
    TestTrue(TEXT("The Caster is in debt before the change"), Rig.Mana->IsOvercast());

    // DevForceClass deliberately does NOT broadcast OnProgressionChanged, so
    // this exercises the tick-side poll, which is the real safety net.
    Rig.Progression->DevForceClass(EBreakerClassId::Swift);
    RunFor(Rig.Mana, 0.1f);

    TestFalse(TEXT("The Mana loop goes inert for a non-Caster"), Rig.Mana->IsActiveForOwner());
    TestEqual(TEXT("The floor closes on the class change"), Rig.Attributes->GetClassResourceFloor(), 0.0f);
    TestEqual(TEXT("The stranded debt is repaid to zero"), Rig.Attributes->GetClassResource(), 0.0f);
    TestFalse(TEXT("Nothing is left reporting Overcast"), Rig.Mana->IsOvercast());
    TestEqual(TEXT("No incoming-damage penalty survives the change"), Rig.Mana->GetOvercastIncomingDamageTaken(), 0.0f);

    // A respec broadcasts, so the bound handler covers that path.
    FCasterRig Respec = MakeRig(EBreakerClassId::Caster, 30.0f);
    Respec.Mana->TrySpendMana(45.0f);
    FText Failure;
    Respec.Progression->RespecAtForge(EBreakerPointCurrency::ClassPoints, true, Failure);
    TestEqual(TEXT("A respec keeps a Caster's floor open"), Respec.Attributes->GetClassResourceFloor(), -20.0f);
    TestTrue(TEXT("A respec does not silently repay the debt"), Respec.Mana->IsOvercast());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerMomentumUnaffectedByFloorTest,
    "RiorsEdge.Classes.MomentumUnaffectedByFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerMomentumUnaffectedByFloorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerOvercastTestHelpers;

    // The hard constraint on the whole feature: Swift is tuned and playtested,
    // so Momentum must behave exactly as it did before the floor existed.
    FCasterRig Swift = MakeRig(EBreakerClassId::Swift);
    UBreakerMomentumComponent* Momentum = NewObject<UBreakerMomentumComponent>(Swift.Owner);
    TestFalse(TEXT("The Mana loop is inert on a Swift"), Swift.Mana->IsActiveForOwner());
    TestEqual(TEXT("A Swift's floor is closed"), Swift.Attributes->GetClassResourceFloor(), 0.0f);
    TestEqual(TEXT("A Swift publishes no debt allowance"), Swift.Mana->GetPublishedFloor(), 0.0f);

    // With the floor closed the clamp GAS applies is the original [0, Max].
    float Value = -25.0f;
    Swift.Attributes->PreAttributeChange(UBreakerAttributeSet::GetClassResourceAttribute(), Value);
    TestEqual(TEXT("Momentum can never be driven below zero"), Value, 0.0f);
    Value = 130.0f;
    Swift.Attributes->PreAttributeChange(UBreakerAttributeSet::GetClassResourceAttribute(), Value);
    TestEqual(TEXT("Momentum still clamps at the maximum"), Value, 100.0f);

    // A Caster's floor is per-character state on that character's attribute
    // set, so it cannot leak onto a Swift.
    FCasterRig Caster = MakeRig(EBreakerClassId::Caster, 20.0f);
    Caster.Mana->TrySpendMana(35.0f);
    TestTrue(TEXT("The Caster is in debt"), Caster.Mana->IsOvercast());
    TestEqual(TEXT("The Swift's floor is untouched by another character"), Swift.Attributes->GetClassResourceFloor(), 0.0f);
    TestEqual(TEXT("The Swift's bank is untouched"), Swift.Attributes->GetClassResource(), 0.0f);
    TestEqual(TEXT("The Momentum component is still inert without a Swift-owned bar"), Momentum->GetMomentum(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaStartsFullTest,
    "RiorsEdge.Classes.ManaStartsFull",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaStartsFullTest::RunTest(const FString& Parameters)
{
    using namespace BreakerOvercastTestHelpers;

    // OWNER RULING 2026-08-14: "Caster's mana bar should be full and go down
    // when using spells." This is the inversion of Class-Kits §2.1's
    // accumulating bank, in which a fresh Caster started at zero and had to
    // shoot something before they could cast anything at all.
    AActor* Owner = NewObject<AActor>();
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>(Owner);
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Owner);
    UBreakerManaComponent* Mana = NewObject<UBreakerManaComponent>(Owner);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Combat->BindAttributes(Attributes);
    Mana->BindAttributes(Attributes);

    // Not yet a Caster: nothing has been filled, because filling is a Caster
    // rule and not an attribute-set default. A Swift's bar is earned.
    TestEqual(TEXT("A classless character has an empty bar"), Attributes->GetClassResource(), 0.0f);

    Progression->DevForceClass(EBreakerClassId::Caster);
    Mana->HandleProgressionChanged();
    TestEqual(TEXT("Becoming a Caster fills the bar"), Mana->GetMana(), Attributes->GetMaxClassResource());
    TestEqual(TEXT("A fresh Caster is at full fraction"), Mana->GetManaFraction(), 1.0f);
    TestTrue(TEXT("A fresh Caster can cast immediately"), Mana->CanAffordSpend(80.0f));

    // The fill is a TRANSITION, not a refresh. RefreshClassOwnership also runs
    // from the tick poll and from a respec; refilling on either would hand a
    // player a free bar mid-fight for standing still.
    Mana->TrySpendMana(60.0f);
    const float AfterCast = Mana->GetMana();
    Mana->HandleProgressionChanged();
    TestEqual(TEXT("A progression refresh does not refill the bar"), Mana->GetMana(), AfterCast);
    TestTrue(TEXT("The bar went DOWN when a spell was cast"), AfterCast < Attributes->GetMaxClassResource());

    // The reset and respawn path. RestoreVitals raises OnVitalsRestored and the
    // Mana component decides for itself; Combat/ never learns what a Caster is.
    Combat->RestoreVitals();
    TestEqual(TEXT("A vitals restore refills a Caster"), Mana->GetMana(), Attributes->GetMaxClassResource());

    // ...and does NOT fill a Swift, whose bar is the reward for moving.
    AActor* SwiftOwner = NewObject<AActor>();
    UBreakerProgressionComponent* SwiftProgression = NewObject<UBreakerProgressionComponent>(SwiftOwner);
    UBreakerCombatComponent* SwiftCombat = NewObject<UBreakerCombatComponent>(SwiftOwner);
    UBreakerManaComponent* SwiftMana = NewObject<UBreakerManaComponent>(SwiftOwner);
    UBreakerAttributeSet* SwiftAttributes = NewObject<UBreakerAttributeSet>();
    SwiftProgression->DevForceClass(EBreakerClassId::Swift);
    SwiftCombat->BindAttributes(SwiftAttributes);
    SwiftMana->BindAttributes(SwiftAttributes);
    SwiftCombat->RestoreVitals();
    TestEqual(TEXT("A reset never fills a Swift's Momentum"), SwiftAttributes->GetClassResource(), 0.0f);
    TestEqual(TEXT("A reset does restore a Swift's health"), SwiftAttributes->GetHealth(), SwiftAttributes->GetMaxHealth());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaRegenerationTest,
    "RiorsEdge.Classes.ManaRegeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaRegenerationTest::RunTest(const FString& Parameters)
{
    using namespace BreakerOvercastTestHelpers;

    // Regeneration is the PRIMARY recovery path under the inversion, so it is
    // worth proving it actually runs rather than existing as a property.
    FCasterRig Caster = MakeRig(EBreakerClassId::Caster, 40.0f);
    Caster.Mana->PassiveRegenPerSecond = 6.0f;

    RunFor(Caster.Mana, 2.0f);
    TestEqual(TEXT("Regeneration pays its authored rate"), Caster.Mana->GetMana(), 52.0f, 0.001f);

    // It stops at the ceiling rather than accumulating an invisible surplus.
    RunFor(Caster.Mana, 20.0f);
    TestEqual(TEXT("Regeneration stops at the maximum"), Caster.Mana->GetMana(), Caster.Attributes->GetMaxClassResource());

    // Doubled while in debt, exactly like conditional generation: climbing out
    // of Overcast is what the doubling is for.
    FCasterRig Debt = MakeRig(EBreakerClassId::Caster, 10.0f);
    Debt.Mana->PassiveRegenPerSecond = 6.0f;
    Debt.Mana->TrySpendMana(30.0f);
    TestEqual(TEXT("The bank is at the floor"), Debt.Mana->GetMana(), -20.0f);
    RunFor(Debt.Mana, 1.0f, 0.05f);
    // 1s at 6/s doubled is 12 of repayment, so the debt is -8 rather than -14.
    TestEqual(TEXT("Regeneration is doubled while in debt"), Debt.Mana->GetMana(), -8.0f, 0.001f);

    // Unmake suspends generation (Class-Kits §2.2), and regeneration is now
    // most of it — if it ran through the free window, the ultimate would refund
    // most of its own 80-Mana price while it was being spent.
    FCasterRig Unmaking = MakeRig(EBreakerClassId::Caster, 20.0f);
    Unmaking.Mana->PushGenerationSuspension(TEXT("Unmake"));
    RunFor(Unmaking.Mana, 3.0f);
    TestEqual(TEXT("Unmake suspends regeneration too"), Unmaking.Mana->GetMana(), 20.0f);
    Unmaking.Mana->PopGenerationSuspension(TEXT("Unmake"));
    RunFor(Unmaking.Mana, 1.0f);
    TestTrue(TEXT("Regeneration resumes when the window closes"), Unmaking.Mana->GetMana() > 20.0f);

    // Zeroing the rate is the "off" position and must be exactly inert, so the
    // owner can turn the whole path off in one place while tuning.
    FCasterRig Off = MakeRig(EBreakerClassId::Caster, 30.0f);
    Off.Mana->PassiveRegenPerSecond = 0.0f;
    RunFor(Off.Mana, 5.0f);
    TestEqual(TEXT("A zeroed rate regenerates nothing"), Off.Mana->GetMana(), 30.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerManaSuspensionTest,
    "RiorsEdge.Classes.ManaSuspension",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerManaSuspensionTest::RunTest(const FString& Parameters)
{
    // Class-Kits §2.2 UNMAKE: "Mana generation is suspended."
    UBreakerManaComponent* Mana = NewObject<UBreakerManaComponent>();
    TestFalse(TEXT("A fresh component generates normally"), Mana->IsGenerationSuspended());

    Mana->PushGenerationSuspension(TEXT("Unmake"));
    TestTrue(TEXT("Unmake suspends generation"), Mana->IsGenerationSuspended());

    // Overlapping sources each revert independently: popping one must not
    // resume generation while another still holds it.
    Mana->PushGenerationSuspension(TEXT("Test.Other"));
    Mana->PopGenerationSuspension(TEXT("Unmake"));
    TestTrue(TEXT("A second holder keeps generation suspended"), Mana->IsGenerationSuspended());
    Mana->PopGenerationSuspension(TEXT("Test.Other"));
    TestFalse(TEXT("Generation resumes when the last holder releases"), Mana->IsGenerationSuspended());

    // Popping something that was never pushed is a no-op, not an underflow.
    Mana->PopGenerationSuspension(TEXT("Never.Pushed"));
    TestFalse(TEXT("Popping an unheld key changes nothing"), Mana->IsGenerationSuspended());
    // A nameless push cannot silently freeze the class.
    Mana->PushGenerationSuspension(NAME_None);
    TestFalse(TEXT("A nameless suspension is refused"), Mana->IsGenerationSuspended());
    return true;
}

#endif
