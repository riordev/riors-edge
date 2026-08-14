#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "GameFramework/Actor.h"

// Partial healing through the damage contract (Ability-Implementation-Spec
// §5.4). Before this existed the only healing path in the project was
// RestoreVitals, which sets both vitals to maximum, so Siphon, Leech, Rend and
// the whole Support Medic branch had nothing to build on.
//
// The invariant these tests protect is the one the design cares about: a heal
// is RESOLVED, not written. Overheal is reported rather than discarded, a heal
// never resurrects, and the arithmetic is pure.

namespace BreakerHealingTest
{
    // Prefixed: identical anonymous-namespace helper names in two translation
    // units have collided under this project's unity build before.
    static FBreakerVitalsState BreakerMakeVitals(float Health, float MaxHealth, float Shield, float MaxShield)
    {
        FBreakerVitalsState Vitals;
        Vitals.Health = Health;
        Vitals.MaxHealth = MaxHealth;
        Vitals.Shield = Shield;
        Vitals.MaxShield = MaxShield;
        return Vitals;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHealingResolutionTest,
    "RiorsEdge.Combat.Healing.Resolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHealingResolutionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHealingTest;

    FBreakerHealRequest Request;
    Request.Amount = 40.0f;

    // Partial: the whole point. A heal smaller than the missing health lands
    // entirely and overheals for nothing.
    const FBreakerHealResult Partial = UBreakerDamageLibrary::ResolveHealing(Request, BreakerMakeVitals(50.0f, 100.0f, 0.0f, 0.0f));
    TestEqual(TEXT("A partial heal lands in full"), Partial.HealthHealed, 40.0f);
    TestEqual(TEXT("A partial heal overheals for nothing"), Partial.Overheal, 0.0f);
    TestEqual(TEXT("Remaining health is the sum"), Partial.RemainingHealth, 90.0f);
    TestFalse(TEXT("A hurt target is not reported as full"), Partial.bWasAtFullHealth);

    // Overheal is REPORTED, not silently dropped. Support's Charge loop has
    // "overheal generates nothing" as a hard criterion, and it cannot honour a
    // number it never receives.
    const FBreakerHealResult Capped = UBreakerDamageLibrary::ResolveHealing(Request, BreakerMakeVitals(90.0f, 100.0f, 0.0f, 0.0f));
    TestEqual(TEXT("Healing is capped at maximum health"), Capped.HealthHealed, 10.0f);
    TestEqual(TEXT("The excess is reported as overheal"), Capped.Overheal, 30.0f);
    TestEqual(TEXT("Health never exceeds its maximum"), Capped.RemainingHealth, 100.0f);

    const FBreakerHealResult Full = UBreakerDamageLibrary::ResolveHealing(Request, BreakerMakeVitals(100.0f, 100.0f, 0.0f, 0.0f));
    TestEqual(TEXT("A full-health target heals for nothing"), Full.HealthHealed, 0.0f);
    TestEqual(TEXT("All of it is overheal"), Full.Overheal, 40.0f);
    TestTrue(TEXT("Full health is reported so a caller can tell why"), Full.bWasAtFullHealth);

    // A negative heal is not damage. Damage has exactly one entry point and
    // this is not it.
    FBreakerHealRequest Negative;
    Negative.Amount = -50.0f;
    const FBreakerHealResult Harmless = UBreakerDamageLibrary::ResolveHealing(Negative, BreakerMakeVitals(50.0f, 100.0f, 0.0f, 0.0f));
    TestEqual(TEXT("A negative heal does nothing at all"), Harmless.HealthHealed, 0.0f);
    TestEqual(TEXT("A negative heal never reduces health"), Harmless.RemainingHealth, 50.0f);

    // The healing multiplier is the twin of SourceDamageMultiplier: one place
    // for a future Increased Healing affix to land.
    FBreakerHealRequest Boosted;
    Boosted.Amount = 40.0f;
    Boosted.HealingMultiplier = 1.5f;
    const FBreakerHealResult Scaled = UBreakerDamageLibrary::ResolveHealing(Boosted, BreakerMakeVitals(0.0f, 100.0f, 0.0f, 0.0f));
    TestEqual(TEXT("The healing multiplier scales the heal"), Scaled.HealthHealed, 60.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHealingOverhealToShieldTest,
    "RiorsEdge.Combat.Healing.OverhealToShield",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHealingOverhealToShieldTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHealingTest;

    FBreakerHealRequest Leech;
    Leech.Amount = 60.0f;
    Leech.bOverhealToShield = true;

    const FBreakerHealResult Routed = UBreakerDamageLibrary::ResolveHealing(Leech, BreakerMakeVitals(80.0f, 100.0f, 0.0f, 50.0f));
    TestEqual(TEXT("Health fills first"), Routed.HealthHealed, 20.0f);
    TestEqual(TEXT("The remainder becomes shield"), Routed.ShieldGranted, 40.0f);
    TestEqual(TEXT("Overheal is still reported at full value"), Routed.Overheal, 40.0f);

    // Health FIRST, always. A target at full health with a broken shield must
    // still report the heal as overheal, or Charge generation reads a heal that
    // did nothing as a heal that did something.
    const FBreakerHealResult ShieldCapped = UBreakerDamageLibrary::ResolveHealing(Leech, BreakerMakeVitals(100.0f, 100.0f, 40.0f, 50.0f));
    TestEqual(TEXT("No health to heal"), ShieldCapped.HealthHealed, 0.0f);
    TestEqual(TEXT("Shield fills only to its maximum"), ShieldCapped.ShieldGranted, 10.0f);
    TestEqual(TEXT("Overheal is the whole heal regardless"), ShieldCapped.Overheal, 60.0f);

    // Without the flag, overheal is simply lost — Leech routes it, Siphon does
    // not, and that difference is a request field rather than two functions.
    FBreakerHealRequest Plain;
    Plain.Amount = 60.0f;
    const FBreakerHealResult Lost = UBreakerDamageLibrary::ResolveHealing(Plain, BreakerMakeVitals(80.0f, 100.0f, 0.0f, 50.0f));
    TestEqual(TEXT("Overheal does not become shield unless asked"), Lost.ShieldGranted, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHealingThroughContractTest,
    "RiorsEdge.Combat.Healing.ThroughContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHealingThroughContractTest::RunTest(const FString& Parameters)
{
    AActor* Target = NewObject<AActor>();
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Combat->BindAttributes(Attributes);

    Attributes->ApplyHealth(40.0f);
    TestEqual(TEXT("Test fixture starts hurt"), Attributes->GetHealth(), 40.0f);

    AActor* Healer = NewObject<AActor>();
    NewObject<UBreakerCombatComponent>(Healer);

    // NOT COVERED HERE: OnHealed and OnHealingDealt firing. Both are DYNAMIC
    // multicast delegates, so a listener has to be a UFUNCTION on a UObject and
    // a test .cpp cannot declare one without a header and a UHT pass. What is
    // covered is everything the events carry — the resolved result — and the
    // attribute write, which is what a listener would be reading anyway.
    const FBreakerHealResult Result = Combat->ApplyHealingAmount(25.0f, Healer, FGameplayTag());
    TestEqual(TEXT("The heal lands on the attribute"), Attributes->GetHealth(), 65.0f);
    TestEqual(TEXT("The result reports what landed"), Result.HealthHealed, 25.0f);

    // Healing is not revival. A heal landing on a corpse would resurrect it
    // without any of the state a real revive has to restore.
    Attributes->ApplyHealth(0.0f);
    const FBreakerHealResult OnCorpse = Combat->ApplyHealingAmount(50.0f, Healer, FGameplayTag());
    TestEqual(TEXT("A dead actor is not healed"), OnCorpse.HealthHealed, 0.0f);
    TestEqual(TEXT("A dead actor stays dead"), Attributes->GetHealth(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArmorReductionTest,
    "RiorsEdge.Combat.Armor.KeyedReduction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArmorReductionTest::RunTest(const FString& Parameters)
{
    AActor* Target = NewObject<AActor>();
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Target);
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Combat->BindAttributes(Attributes);
    Attributes->InitArmor(100.0f);

    TestEqual(TEXT("No strippers means armour is untouched"), Combat->GetEffectiveArmor(), 100.0f);

    Combat->PushArmorReduction(TEXT("Zone.Caster.Rot"), 40.0f);
    TestEqual(TEXT("A flat strip comes off"), Combat->GetEffectiveArmor(), 60.0f);

    // The bug this exists to prevent: two overlapping Rots stacking their
    // strips. They share a key, so the second REPLACES the first.
    Combat->PushArmorReduction(TEXT("Zone.Caster.Rot"), 40.0f);
    TestEqual(TEXT("Two zones of the same tag do not double-strip"), Combat->GetEffectiveArmor(), 60.0f);

    // A different source composes additively, as designed (VW7 adds a second
    // 40 against a target already affected by a DoT).
    Combat->PushArmorReduction(TEXT("Zone.Caster.Rot.Deepened"), 40.0f);
    TestEqual(TEXT("Distinct sources sum"), Combat->GetEffectiveArmor(), 20.0f);

    // And no combination may ever drive it negative: negative armour inverts
    // the mitigation formula into a damage BONUS.
    Combat->PushArmorReduction(TEXT("Disruptor"), 500.0f);
    TestEqual(TEXT("Effective armour floors at zero"), Combat->GetEffectiveArmor(), 0.0f);
    TestTrue(TEXT("The composed reduction itself is unbounded, only its effect is clamped"),
        Combat->GetComposedArmorReduction() > 500.0f);

    Combat->PopArmorReduction(TEXT("Disruptor"));
    Combat->PopArmorReduction(TEXT("Zone.Caster.Rot.Deepened"));
    Combat->PopArmorReduction(TEXT("Zone.Caster.Rot"));
    TestEqual(TEXT("Releasing every strip restores armour exactly"), Combat->GetEffectiveArmor(), 100.0f);
    return true;
}

#endif
