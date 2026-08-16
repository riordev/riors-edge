#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Classes/BreakerChargeComponent.h"
#include "Classes/BreakerGritComponent.h"
#include "Classes/BreakerScrapComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

// ---------------------------------------------------------------------------
// THE THREE DESIGNED-BUT-UNBUILT CLASSES — Gunsmith / Tank / Support
// ---------------------------------------------------------------------------
// What these tests cover: the PURE ARITHMETIC of the three new resource loops
// (generation, clamping, band thresholds, refusal to spend at the floor), the
// class gate refusing a non-owner, and the shape of the ability rows now in the
// fallback registry.
//
// UPDATE 2026-08-16 (owner authorization: "feel free to do all 5 classes"):
// the three kits now EXECUTE — every row carries a real AbilityClass, the
// class definitions are registered, and the assertions in this file that
// deliberately pinned the unbuilt state have been flipped to pin the built
// one. Tests/BreakerBuiltClassKitTests.cpp owns the new built-state coverage;
// this file keeps the loop arithmetic and the class gate.
//
// WHAT THESE TESTS DO NOT COVER, stated plainly rather than left to be
// discovered:
//
//  * **Branch trees are covered ELSEWHERE.** When this file was written none
//    of the three classes had an authored branch tree and the reachability
//    suite took its honest-emptiness arm; the branch layers landed later the
//    same day (2026-08-16, same authorization) and that arm no longer
//    applies. Tests/BreakerBuiltClassTreeTests.cpp owns the branch-tree
//    coverage; the keystone variant rows this file asserts to EXIST are now
//    also asserted REACHABLE there and in BreakerKeystoneReachabilityTests.
//  * **No deployable, minion, ally, party, mark, buff, aura, threat or stagger
//    behaviour is covered, because none exists** (O30 for the first group; the
//    party layer for the second). The Scrap loop's two deployable-facing
//    generation rules are tested as arithmetic and have no caller.
//  * **No integration through the tick.** `AdvanceLoop` is exercised only in
//    the class-gate direction (an inactive loop must stay inert). The metered
//    per-second budgets are proven through their pure `DrawFromBudget` /
//    `ClampGeneration` rules rather than by simulating frames, because the
//    frame-driven half needs an owner with authority and these components are
//    built with no world.
//  * **No value here is balance.** Every magnitude is O2 PLACEHOLDER, and the
//    default-pinning test below exists to catch a SILENT DRIFT, not to defend a
//    number. When wave mode reports, those expectations change with the
//    defaults in one edit.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// SCRAP — the ledger. Accumulates, never decays, never idles.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerScrapLoopTest,
    "RiorsEdge.Classes.Unbuilt.ScrapLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerScrapLoopTest::RunTest(const FString& Parameters)
{
    // Bands read as fractions of the maximum, so a Maximum Resource roll cannot
    // make Surplus cheaper to hold.
    TestTrue(TEXT("An empty bar is Dry"), UBreakerScrapComponent::StateForFraction(0.0f) == EBreakerScrapState::Dry);
    TestTrue(TEXT("Just below a quarter is still Dry"), UBreakerScrapComponent::StateForFraction(0.24f) == EBreakerScrapState::Dry);
    TestTrue(TEXT("A quarter is Stocked"), UBreakerScrapComponent::StateForFraction(0.25f) == EBreakerScrapState::Stocked);
    TestTrue(TEXT("Just below three fifths is still Stocked"), UBreakerScrapComponent::StateForFraction(0.59f) == EBreakerScrapState::Stocked);
    TestTrue(TEXT("Three fifths is Surplus"), UBreakerScrapComponent::StateForFraction(0.60f) == EBreakerScrapState::Surplus);
    TestTrue(TEXT("A full bar is Surplus"), UBreakerScrapComponent::StateForFraction(1.0f) == EBreakerScrapState::Surplus);

    // A kill credits at the KILLING INSTANCE'S proc coefficient, so a DoT tick
    // landing the kill cannot make Tick Frequency a Scrap engine.
    TestEqual(TEXT("A clean kill pays in full"), UBreakerScrapComponent::KillGeneration(12.0f, 1.0f), 12.0f);
    TestEqual(TEXT("A half-coefficient kill pays half"), UBreakerScrapComponent::KillGeneration(12.0f, 0.5f), 6.0f);
    TestEqual(TEXT("A zero-coefficient kill pays nothing"), UBreakerScrapComponent::KillGeneration(12.0f, 0.0f), 0.0f);

    // Reloading a magazine nothing has left credits nothing.
    TestEqual(TEXT("A real reload pays"), UBreakerScrapComponent::ReloadGeneration(true, 4.0f), 4.0f);
    TestEqual(TEXT("Topping off a full magazine pays nothing"), UBreakerScrapComponent::ReloadGeneration(false, 4.0f), 0.0f);
    TestEqual(TEXT("A full dump pays the magazine bonus"), UBreakerScrapComponent::MagazineDumpGeneration(true, 8.0f), 8.0f);
    TestEqual(TEXT("A partial magazine does not re-arm the dump bonus"), UBreakerScrapComponent::MagazineDumpGeneration(false, 8.0f), 0.0f);

    // REFUND, NOT PROFIT. The acceptance invariant is that placement has a
    // negative economic return; a refund at or above 1.0 inverts it and the
    // class farms itself.
    TestEqual(TEXT("Destruction refunds half the cost"), UBreakerScrapComponent::DestructionRefund(40.0f, 0.5f), 20.0f);
    TestTrue(TEXT("A refund is never profit"), UBreakerScrapComponent::DestructionRefund(40.0f, 0.5f) < 40.0f);
    TestEqual(TEXT("A refund fraction above one is clamped to the whole cost"), UBreakerScrapComponent::DestructionRefund(40.0f, 5.0f), 40.0f);
    TestEqual(TEXT("A negative cost refunds nothing"), UBreakerScrapComponent::DestructionRefund(-40.0f, 0.5f), 0.0f);

    // Deployable damage: overkill is the caller's business to trim, but the
    // divisor is the invariant, and a turret's whole lifetime output must not
    // out-earn its own cost.
    TestEqual(TEXT("Five hundred damage pays one Scrap"), UBreakerScrapComponent::DeployableDamageGeneration(500.0f, 500.0f), 1.0f);
    TestEqual(TEXT("Damage below the divisor pays a fraction"), UBreakerScrapComponent::DeployableDamageGeneration(250.0f, 500.0f), 0.5f);
    TestEqual(TEXT("A zero divisor pays nothing rather than dividing by zero"), UBreakerScrapComponent::DeployableDamageGeneration(500.0f, 0.0f), 0.0f);

    // Overflow is DISCARDED, not banked: no Gunsmith arrives at a boss with two
    // full deployable sets. The floor is a hard zero — broke, never in debt.
    TestEqual(TEXT("Generation stops at the ceiling"), UBreakerScrapComponent::ClampToBank(140.0f, 100.0f), 100.0f);
    TestEqual(TEXT("Spending stops at zero"), UBreakerScrapComponent::ClampToBank(-15.0f, 100.0f), 0.0f);
    TestEqual(TEXT("Values inside the bank pass through"), UBreakerScrapComponent::ClampToBank(37.5f, 100.0f), 37.5f);

    TestEqual(TEXT("Generation clamps to the global cap"), UBreakerScrapComponent::ClampGeneration(50.0f, 15.0f), 15.0f);
    TestEqual(TEXT("Generation below the cap passes through"), UBreakerScrapComponent::ClampGeneration(9.0f, 15.0f), 9.0f);
    TestEqual(TEXT("Generation never goes negative"), UBreakerScrapComponent::ClampGeneration(-4.0f, 15.0f), 0.0f);

    // The only failure state the loop has: there is no Overcast analogue to
    // borrow against, so an unaffordable placement is refused outright.
    TestTrue(TEXT("An affordable placement is allowed"), UBreakerScrapComponent::CanSpendFrom(60.0f, 40.0f));
    TestTrue(TEXT("Spending the exact balance is allowed"), UBreakerScrapComponent::CanSpendFrom(40.0f, 40.0f));
    TestFalse(TEXT("A placement one short is refused"), UBreakerScrapComponent::CanSpendFrom(39.0f, 40.0f));
    TestFalse(TEXT("An empty bar affords nothing"), UBreakerScrapComponent::CanSpendFrom(0.0f, 25.0f));
    TestTrue(TEXT("A free ability is always allowed"), UBreakerScrapComponent::CanSpendFrom(0.0f, 0.0f));

    // Zero is a legal generation multiplier and must survive composition — the
    // bug the Momentum loop shipped once, where "generation stops entirely"
    // silently became "generation is normal".
    TestEqual(TEXT("No overrides compose to unity"), UBreakerScrapComponent::ComposeGenerationMultipliers({}), 1.0f);
    TestEqual(TEXT("Overrides compose multiplicatively"), UBreakerScrapComponent::ComposeGenerationMultipliers({2.0f, 1.5f}), 3.0f);

    // Shipped defaults, pinned for their relationships. The refund fraction is
    // the one that matters most: at 1.0 placement becomes free and the density
    // cap is the only constraint left, which is the economy inverting.
    const UBreakerScrapComponent* Defaults = GetDefault<UBreakerScrapComponent>();
    TestTrue(TEXT("A destruction refund is strictly a partial refund"), Defaults->DestructionRefundFraction < 1.0f);
    TestTrue(TEXT("A destruction refund returns something"), Defaults->DestructionRefundFraction > 0.0f);
    TestTrue(TEXT("Surplus sits above Stocked"), Defaults->SurplusFraction > Defaults->StockedFraction);
    TestTrue(TEXT("Deployable damage pays through a real divisor"), Defaults->DeployableDamagePerScrap > 1.0f);
    // Gunsmith's cap is the tightest of the three new loops, because its
    // generation is kill- and damage-weighted and a dense pack would otherwise
    // refund a whole deployable set mid-wave.
    TestTrue(TEXT("Scrap's global cap is tighter than Grit's"),
        Defaults->GlobalGenerationCap < GetDefault<UBreakerGritComponent>()->GlobalGenerationCap);
    return true;
}

// ---------------------------------------------------------------------------
// GRIT — the banked state with a lapse timer.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGritLoopTest,
    "RiorsEdge.Classes.Unbuilt.GritLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGritLoopTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("An empty bar is Winded"), UBreakerGritComponent::BandForFraction(0.0f) == EBreakerGritBand::Winded);
    TestTrue(TEXT("A third is Braced"), UBreakerGritComponent::BandForFraction(0.34f) == EBreakerGritBand::Braced);
    TestTrue(TEXT("Two thirds is IRONCLAD"), UBreakerGritComponent::BandForFraction(0.67f) == EBreakerGritBand::Ironclad);
    TestTrue(TEXT("A full bar is IRONCLAD"), UBreakerGritComponent::BandForFraction(1.0f) == EBreakerGritBand::Ironclad);

    // THE MANDATORY RULE: generation is computed on POST-MITIGATION damage, and
    // in health FRACTIONS rather than raw damage. Two Tanks taking the same
    // fraction of their maximum health generate the same Grit whatever their
    // health pools are, which is what stops Armour and Maximum Health from
    // becoming Grit multipliers.
    const float BigTank = UBreakerGritComponent::DamageTakenGeneration(200.0f, 0.0f, 1000.0f, 1.0f, 0.02f, 0.5f, 1.0f);
    const float SmallTank = UBreakerGritComponent::DamageTakenGeneration(100.0f, 0.0f, 500.0f, 1.0f, 0.02f, 0.5f, 1.0f);
    TestEqual(TEXT("The same health fraction pays the same Grit at any health pool"), BigTank, SmallTank);
    TestEqual(TEXT("Twenty percent of maximum health pays ten Grit"), BigTank, 10.0f);

    // Shield absorption pays HALF: still post-mitigation damage, but it cost no
    // health. This halving is the guard that stops an overshield loop being a
    // generation engine as well as a survival one.
    TestEqual(TEXT("Shield damage pays half of what health damage pays"),
        UBreakerGritComponent::DamageTakenGeneration(0.0f, 200.0f, 1000.0f, 1.0f, 0.02f, 0.5f, 1.0f), 5.0f);
    TestEqual(TEXT("Health and shield damage sum"),
        UBreakerGritComponent::DamageTakenGeneration(200.0f, 200.0f, 1000.0f, 1.0f, 0.02f, 0.5f, 1.0f), 15.0f);

    // A DoT tick pays at its proc coefficient, or a stacked Bleed becomes the
    // cheapest Grit engine in the game.
    TestEqual(TEXT("A DoT tick at zero coefficient pays nothing"),
        UBreakerGritComponent::DamageTakenGeneration(200.0f, 0.0f, 1000.0f, 1.0f, 0.02f, 0.5f, 0.0f), 0.0f);
    TestEqual(TEXT("A half-coefficient tick pays half"),
        UBreakerGritComponent::DamageTakenGeneration(200.0f, 0.0f, 1000.0f, 1.0f, 0.02f, 0.5f, 0.5f), 5.0f);
    TestEqual(TEXT("A zero health pool generates nothing rather than dividing by zero"),
        UBreakerGritComponent::DamageTakenGeneration(200.0f, 0.0f, 0.0f, 1.0f, 0.02f, 0.5f, 1.0f), 0.0f);

    // Self-damage generates at a fraction and is never exempted. Reducing
    // self-damage reduces the Grit it produces, so rocket-jumping gets strictly
    // WORSE as a Grit engine the more the player invests in it.
    TestEqual(TEXT("Ordinary damage is unscaled"), UBreakerGritComponent::SelfDamageScalar(0.25f, false), 1.0f);
    TestEqual(TEXT("Self-damage generates at a quarter"), UBreakerGritComponent::SelfDamageScalar(0.25f, true), 0.25f);
    TestTrue(TEXT("Self-damage can never out-generate ordinary damage"),
        UBreakerGritComponent::SelfDamageScalar(5.0f, true) <= 1.0f);

    // COUNT-INDEPENDENCE. There is no overload taking a count, so this is a
    // property of the type and not of a clamp somebody could remove.
    TestEqual(TEXT("One enemy nearby generates the proximity rate"), UBreakerGritComponent::ProximityGeneration(true, 1.5f), 1.5f);
    TestEqual(TEXT("No enemy nearby generates nothing"), UBreakerGritComponent::ProximityGeneration(false, 1.5f), 0.0f);

    // The lapse window, and the decay that only runs outside it.
    TestTrue(TEXT("Contact holds the lapse window open"), UBreakerGritComponent::IsLapseOpen(0.0f, 6.0f));
    TestTrue(TEXT("The window is still open just before it lapses"), UBreakerGritComponent::IsLapseOpen(5.9f, 6.0f));
    TestFalse(TEXT("The window closes at the lapse time"), UBreakerGritComponent::IsLapseOpen(6.0f, 6.0f));
    TestEqual(TEXT("Grit banks inside the window"), UBreakerGritComponent::DecayRate(true, false, 5.0f), 0.0f);
    TestEqual(TEXT("Grit bleeds outside the window"), UBreakerGritComponent::DecayRate(false, false, 5.0f), 5.0f);
    TestEqual(TEXT("A suspending override stops decay outside the window too"), UBreakerGritComponent::DecayRate(false, true, 5.0f), 0.0f);

    // Per-source budgets. Damage capped at half the global cap is what makes
    // "being hit is necessary and never sufficient" structural rather than
    // aspirational: the bar cannot be filled at the global rate by taking
    // damage alone.
    float Budget = 10.0f;
    TestEqual(TEXT("A draw inside the budget pays in full"), UBreakerGritComponent::DrawFromBudget(4.0f, Budget), 4.0f);
    TestEqual(TEXT("The budget is reduced by what was drawn"), Budget, 6.0f);
    TestEqual(TEXT("A draw beyond the budget pays only the remainder"), UBreakerGritComponent::DrawFromBudget(20.0f, Budget), 6.0f);
    TestEqual(TEXT("An exhausted budget is exactly empty"), Budget, 0.0f);
    TestEqual(TEXT("An exhausted budget pays nothing"), UBreakerGritComponent::DrawFromBudget(5.0f, Budget), 0.0f);

    // The shipped defaults, pinned. Not a defence of any number — every one is
    // an O2 PLACEHOLDER — but the RELATIONSHIPS between them are the class
    // thesis, and a silent edit to any of the three would change what the class
    // is without changing a line of design. "Being hit is necessary and never
    // sufficient" is true only while the damage cap is strictly below the
    // global one; the self-damage cap must be the smallest of the three, or
    // rocket-jumping becomes drivable.
    const UBreakerGritComponent* Defaults = GetDefault<UBreakerGritComponent>();
    TestTrue(TEXT("Damage alone cannot reach the global generation cap"), Defaults->DamageRateCap < Defaults->GlobalGenerationCap);
    TestTrue(TEXT("Self-damage is the most tightly capped source"), Defaults->SelfDamageRateCap < Defaults->DamageRateCap);
    TestTrue(TEXT("Self-damage generates at a fraction of the ordinary rate"), Defaults->SelfDamageRate < 1.0f);
    TestTrue(TEXT("Shield absorption generates at less than the health rate"), Defaults->ShieldRateFraction < 1.0f);
    TestTrue(TEXT("The block proc is capped below the damage sources"), Defaults->BlockRateCap <= Defaults->DamageRateCap);
    TestTrue(TEXT("There is a combat-entry grant, so Winded is not a dead opening"), Defaults->CombatEntryGrant > 0.0f);
    TestTrue(TEXT("There is a lapse window, so Grit banks while engaged"), Defaults->LapseSeconds > 0.0f);

    TestEqual(TEXT("Spending stops at zero"), UBreakerGritComponent::ClampToBank(-15.0f, 100.0f), 0.0f);
    TestEqual(TEXT("Generation stops at the ceiling"), UBreakerGritComponent::ClampToBank(140.0f, 100.0f), 100.0f);
    TestTrue(TEXT("An affordable ability is allowed"), UBreakerGritComponent::CanSpendFrom(45.0f, 45.0f));
    TestFalse(TEXT("An ability one short is refused"), UBreakerGritComponent::CanSpendFrom(44.0f, 45.0f));
    TestFalse(TEXT("An empty bar affords nothing"), UBreakerGritComponent::CanSpendFrom(0.0f, 25.0f));
    return true;
}

// ---------------------------------------------------------------------------
// CHARGE — the event-driven bank with an out-of-combat clamp.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerChargeLoopTest,
    "RiorsEdge.Classes.Unbuilt.ChargeLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerChargeLoopTest::RunTest(const FString& Parameters)
{
    // Resonant starts at three quarters rather than at two thirds, because the
    // ultimate is at a full bar and the band that matters is the one
    // immediately below it. This asymmetry against Momentum and Grit is
    // deliberate and is pinned so it cannot be "tidied" into thirds.
    TestTrue(TEXT("An empty bar is Cold"), UBreakerChargeComponent::BandForFraction(0.0f) == EBreakerChargeBand::Cold);
    TestTrue(TEXT("A third is Attuned"), UBreakerChargeComponent::BandForFraction(0.34f) == EBreakerChargeBand::Attuned);
    TestTrue(TEXT("Two thirds is still only Attuned"), UBreakerChargeComponent::BandForFraction(0.67f) == EBreakerChargeBand::Attuned);
    TestTrue(TEXT("Three quarters is RESONANT"), UBreakerChargeComponent::BandForFraction(0.75f) == EBreakerChargeBand::Resonant);

    // THE NON-NEGOTIABLE CLAUSE: self-directed generation pays exactly what
    // ally-directed generation pays. There is no bSelfTargeted parameter on the
    // pure rule at all, which is the strongest possible statement of it — the
    // rate CANNOT differ, because there is nothing to differ on.
    const float AllyHeal = UBreakerChargeComponent::HealingGeneration(300.0f, 1000.0f, 0.03f, 1.0f);
    const float SelfHeal = UBreakerChargeComponent::HealingGeneration(300.0f, 1000.0f, 0.03f, 1.0f);
    TestEqual(TEXT("Healing yourself pays exactly what healing an ally pays"), SelfHeal, AllyHeal);
    TestEqual(TEXT("Thirty percent of maximum health pays ten Charge"), AllyHeal, 10.0f);

    // Percentage-of-TARGET, not absolute: healing a Tank is not a Charge engine.
    TestEqual(TEXT("The same fraction of a bigger target pays the same"),
        UBreakerChargeComponent::HealingGeneration(600.0f, 2000.0f, 0.03f, 1.0f), 10.0f);

    // Overheal pays nothing — it never reaches the rule, which is why there is
    // no overheal term to forget to subtract.
    TestEqual(TEXT("A heal that moved nothing pays nothing"),
        UBreakerChargeComponent::HealingGeneration(0.0f, 1000.0f, 0.03f, 1.0f), 0.0f);
    // A HoT tick pays at the healing source's proc coefficient, so ten ticks do
    // not out-generate one instant heal of the same total.
    TestEqual(TEXT("A half-coefficient heal tick pays half"),
        UBreakerChargeComponent::HealingGeneration(300.0f, 1000.0f, 0.03f, 0.5f), 5.0f);
    TestEqual(TEXT("A zero-health target generates nothing rather than dividing by zero"),
        UBreakerChargeComponent::HealingGeneration(300.0f, 0.0f, 0.03f, 1.0f), 0.0f);

    // The shield-side twin, and the offensive conversion path.
    TestEqual(TEXT("Shielding pays at the healing rate"),
        UBreakerChargeComponent::ShieldingGeneration(300.0f, 1000.0f, 0.03f), 10.0f);
    TestEqual(TEXT("Over-shield pays nothing"),
        UBreakerChargeComponent::ShieldingGeneration(0.0f, 1000.0f, 0.03f), 0.0f);
    TestEqual(TEXT("Marked damage pays on the target's own health fraction"),
        UBreakerChargeComponent::MarkedDamageGeneration(200.0f, 1000.0f, 0.02f), 10.0f);
    TestTrue(TEXT("Marked damage pays faster per fraction than healing does"),
        UBreakerChargeComponent::MarkedDamageGeneration(100.0f, 1000.0f, 0.02f)
        > UBreakerChargeComponent::HealingGeneration(100.0f, 1000.0f, 0.03f, 1.0f));

    // Cleansing a clean target pays nothing.
    TestEqual(TEXT("Two statuses removed pay twice"), UBreakerChargeComponent::CleanseGeneration(2, 4.0f), 8.0f);
    TestEqual(TEXT("Cleansing nothing pays nothing"), UBreakerChargeComponent::CleanseGeneration(0, 4.0f), 0.0f);
    TestEqual(TEXT("A negative count pays nothing"), UBreakerChargeComponent::CleanseGeneration(-3, 4.0f), 0.0f);

    // COUNT-INDEPENDENCE, cutting both ways: five allies pay what one self pays,
    // and five stacked buffs pay what one does. A bool is the whole rule.
    TestEqual(TEXT("A live buff pays the uptime rate"), UBreakerChargeComponent::BuffUptimeGeneration(true, true, 2.0f), 2.0f);
    TestEqual(TEXT("No live buff pays nothing"), UBreakerChargeComponent::BuffUptimeGeneration(false, true, 2.0f), 0.0f);
    // ...and it does not credit outside combat, which together with the absence
    // of passive regeneration means there is nothing to farm before a fight.
    TestEqual(TEXT("Buff uptime pays nothing out of combat"), UBreakerChargeComponent::BuffUptimeGeneration(true, false, 2.0f), 0.0f);

    // The out-of-combat clamp: falls TO the ceiling and holds there. Never past
    // it, never to zero. A Support may open a fight strong and never with a free
    // ultimate.
    TestEqual(TEXT("A full bar falls toward the ceiling"), UBreakerChargeComponent::OutOfCombatDecay(100.0f, 60.0f, 10.0f, 1.0f), 90.0f);
    TestEqual(TEXT("The clamp stops exactly at the ceiling"), UBreakerChargeComponent::OutOfCombatDecay(65.0f, 60.0f, 10.0f, 1.0f), 60.0f);
    TestEqual(TEXT("A bar already under the ceiling is untouched"), UBreakerChargeComponent::OutOfCombatDecay(30.0f, 60.0f, 10.0f, 1.0f), 30.0f);
    TestEqual(TEXT("The clamp never reaches zero"), UBreakerChargeComponent::OutOfCombatDecay(100.0f, 60.0f, 1000.0f, 10.0f), 60.0f);

    // The self-heal sub-cap: the Life-on-Hit guard. It meters the RATE and
    // never the rate-per-event, so gear supplements the loop instead of
    // becoming it.
    float SelfBudget = 6.0f;
    TestEqual(TEXT("Self-heal credit inside the sub-cap pays in full"), UBreakerChargeComponent::DrawFromBudget(4.0f, SelfBudget), 4.0f);
    TestEqual(TEXT("Self-heal credit beyond the sub-cap pays only the remainder"), UBreakerChargeComponent::DrawFromBudget(50.0f, SelfBudget), 2.0f);
    TestEqual(TEXT("A spent self-heal sub-cap pays nothing more"), UBreakerChargeComponent::DrawFromBudget(50.0f, SelfBudget), 0.0f);

    // Shipped defaults, pinned for their relationships rather than their values.
    const UBreakerChargeComponent* Defaults = GetDefault<UBreakerChargeComponent>();
    TestTrue(TEXT("The self-heal sub-cap is strictly below the global cap"), Defaults->SelfHealRateCap < Defaults->GlobalGenerationCap);
    TestTrue(TEXT("The out-of-combat ceiling is below a full bar, so there is no free ultimate"), Defaults->OutOfCombatCeiling < 100.0f);
    TestTrue(TEXT("The out-of-combat ceiling is high enough to open a fight strong"), Defaults->OutOfCombatCeiling > 0.0f);
    TestTrue(TEXT("Marked damage pays per a smaller health fraction than healing does"),
        Defaults->MarkedHealthFractionPerCharge < Defaults->HealthFractionPerCharge);
    TestTrue(TEXT("Resonant sits above Attuned"), Defaults->ResonantFraction > Defaults->AttunedFraction);
    // The asymmetry against Momentum's and Grit's even thirds, pinned so it is
    // not "corrected" into consistency by someone reading the five loops
    // together. Support reaches its top band and spends it; Swift holds its own.
    TestTrue(TEXT("Resonant starts higher than an even-thirds top band would"), Defaults->ResonantFraction > 2.0f / 3.0f);

    TestEqual(TEXT("Generation clamps to the global cap"), UBreakerChargeComponent::ClampGeneration(50.0f, 18.0f), 18.0f);
    TestEqual(TEXT("Spending stops at zero"), UBreakerChargeComponent::ClampToBank(-15.0f, 100.0f), 0.0f);
    TestTrue(TEXT("An affordable ability is allowed"), UBreakerChargeComponent::CanSpendFrom(20.0f, 20.0f));
    TestFalse(TEXT("An ability one short is refused"), UBreakerChargeComponent::CanSpendFrom(19.0f, 20.0f));
    // Mark is the loop's ignition and must be affordable from a low bar, which
    // is why it is the cheapest ability in the class.
    TestTrue(TEXT("A Support at a quarter bar can still ignite the loop"), UBreakerChargeComponent::CanSpendFrom(25.0f, 20.0f));
    return true;
}

// ---------------------------------------------------------------------------
// THE CLASS GATE — each loop is inert for anyone but its own class.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerUnbuiltClassGateTest,
    "RiorsEdge.Classes.Unbuilt.ClassGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerUnbuiltClassGateTest::RunTest(const FString& Parameters)
{
    // No owner at all: every loop is inert and affords nothing. This is the
    // state a component is in before anything wires it up, and it must be safe
    // rather than merely unlikely.
    UBreakerScrapComponent* Scrap = NewObject<UBreakerScrapComponent>();
    UBreakerGritComponent* Grit = NewObject<UBreakerGritComponent>();
    UBreakerChargeComponent* Charge = NewObject<UBreakerChargeComponent>();

    TestFalse(TEXT("A componentless owner runs no Scrap loop"), Scrap->IsActiveForOwner());
    TestFalse(TEXT("A componentless owner runs no Grit loop"), Grit->IsActiveForOwner());
    TestFalse(TEXT("A componentless owner runs no Charge loop"), Charge->IsActiveForOwner());
    TestFalse(TEXT("An inert Scrap loop affords nothing"), Scrap->CanAffordSpend(1.0f));
    TestFalse(TEXT("An inert Grit loop affords nothing"), Grit->CanAffordSpend(1.0f));
    TestFalse(TEXT("An inert Charge loop affords nothing"), Charge->CanAffordSpend(1.0f));
    TestFalse(TEXT("An inert Scrap loop refuses to spend"), Scrap->TrySpendScrap(1.0f));
    TestFalse(TEXT("An inert Grit loop refuses to spend"), Grit->TrySpendGrit(1.0f));
    TestFalse(TEXT("An inert Charge loop refuses to spend"), Charge->TrySpendCharge(1.0f));

    // An attribute set with a full bar changes nothing while the class gate is
    // shut: a non-owner may not read, spend, or generate the resource. This is
    // the check that catches a loop left running after a class swap — the
    // failure the Mana component's own class poll exists as a backstop for.
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    Attributes->ApplyClassResource(100.0f);
    Scrap->BindAttributes(Attributes);
    Grit->BindAttributes(Attributes);
    Charge->BindAttributes(Attributes);
    TestFalse(TEXT("A bound but classless Scrap loop is still inert"), Scrap->IsActiveForOwner());
    TestFalse(TEXT("A bound but classless Scrap loop still affords nothing"), Scrap->CanAffordSpend(1.0f));
    TestFalse(TEXT("A bound but classless Grit loop still affords nothing"), Grit->CanAffordSpend(1.0f));
    TestFalse(TEXT("A bound but classless Charge loop still affords nothing"), Charge->CanAffordSpend(1.0f));

    // Advancing an inactive loop must be a no-op, not a slow leak. Each of the
    // three has a different reason it could leak — Scrap has queued grants,
    // Grit has decay, Charge has the out-of-combat clamp — so all three are
    // driven rather than one standing in for the others.
    const float Before = Attributes->GetClassResource();
    Scrap->AdvanceLoop(1.0f);
    Grit->AdvanceLoop(1.0f);
    Charge->AdvanceLoop(1.0f);
    TestEqual(TEXT("An inactive loop never writes the class resource"), Attributes->GetClassResource(), Before);

    // The gate is the PERMANENT CLASS and nothing else. Each component answers
    // for exactly one class, and a Gunsmith must not run the Grit loop however
    // the components ended up on the same actor.
    UBreakerProgressionComponent* Progression = NewObject<UBreakerProgressionComponent>();
    TestFalse(TEXT("An unselected character is not a Gunsmith"),
        Progression->GetProgressionState().PermanentClass == EBreakerClassId::Gunsmith);

    // O39's gate now ADMITS all three (kits implemented 2026-08-16): the lock
    // succeeds once and — class choice being permanent — refuses everything
    // after, which is the same one-way rule Swift and Caster live under.
    TestTrue(TEXT("Locking into Gunsmith is allowed (kit implemented 2026-08-16)"), Progression->ChoosePermanentClassById(EBreakerClassId::Gunsmith));
    TestFalse(TEXT("A second lock is refused: class choice is permanent"), Progression->ChoosePermanentClassById(EBreakerClassId::Tank));
    TestFalse(TEXT("A third lock is refused too"), Progression->ChoosePermanentClassById(EBreakerClassId::Support));
    TestTrue(TEXT("The lock actually landed"),
        Progression->GetProgressionState().PermanentClass == EBreakerClassId::Gunsmith);
    // D11: the lock seeds the starter loadout, so a fresh character's picker
    // shows what the HUD fires instead of an empty selection.
    TestEqual(TEXT("The lock seeded slot one with the first starter"),
        Progression->GetProgressionState().AbilityLoadout.ClassAbilityOne, FName(TEXT("Gunsmith.SidearmRig")));
    TestEqual(TEXT("The lock seeded the ultimate"),
        Progression->GetProgressionState().AbilityLoadout.Ultimate, FName(TEXT("Gunsmith.FieldAssembly")));

    Progression->DevForceClass(EBreakerClassId::Gunsmith);
    TestTrue(TEXT("The dev path still lands on a Gunsmith for testing"),
        Progression->GetProgressionState().PermanentClass == EBreakerClassId::Gunsmith);
    TestFalse(TEXT("A Gunsmith is not a Tank"),
        Progression->GetProgressionState().PermanentClass == EBreakerClassId::Tank);
    TestFalse(TEXT("A Gunsmith is not a Support"),
        Progression->GetProgressionState().PermanentClass == EBreakerClassId::Support);
    return true;
}

// ---------------------------------------------------------------------------
// THE ABILITY DATA — registered, and deliberately not executable.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerUnbuiltClassAbilityDataTest,
    "RiorsEdge.Classes.Unbuilt.AbilityData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerUnbuiltClassAbilityDataTest::RunTest(const FString& Parameters)
{
    struct FExpected
    {
        const TCHAR* Id;
        EBreakerClassId ClassId;
        float Cost;
        float Cooldown;
    };

    // Quoted from the three class-kit documents, each of which is under a
    // blanket O2 PLACEHOLDER banner. This table's job is to catch a SILENT
    // EDIT, not to defend a number: when wave mode reports, these move with the
    // registry in one commit.
    static const FExpected Expected[] =
    {
        { TEXT("Gunsmith.SidearmRig"),    EBreakerClassId::Gunsmith,   0.0f, 10.0f },
        { TEXT("Gunsmith.Overhaul"),      EBreakerClassId::Gunsmith,   0.0f, 18.0f },
        { TEXT("Gunsmith.Turret"),        EBreakerClassId::Gunsmith,  40.0f,  0.0f },
        { TEXT("Gunsmith.AmmoCrate"),     EBreakerClassId::Gunsmith,  30.0f,  0.0f },
        { TEXT("Gunsmith.MineCluster"),   EBreakerClassId::Gunsmith,  35.0f,  0.0f },
        { TEXT("Gunsmith.Disruptor"),     EBreakerClassId::Gunsmith,  45.0f,  0.0f },
        { TEXT("Gunsmith.FieldAssembly"), EBreakerClassId::Gunsmith, 100.0f,  0.0f },
        { TEXT("Tank.Rend"),              EBreakerClassId::Tank,      25.0f,  6.0f },
        { TEXT("Tank.Bloodline"),         EBreakerClassId::Tank,      40.0f, 12.0f },
        { TEXT("Tank.AnchorPoint"),       EBreakerClassId::Tank,      30.0f, 10.0f },
        { TEXT("Tank.Provoke"),           EBreakerClassId::Tank,      35.0f, 12.0f },
        { TEXT("Tank.BreachCharge"),      EBreakerClassId::Tank,      30.0f,  8.0f },
        { TEXT("Tank.GroundZero"),        EBreakerClassId::Tank,      45.0f, 10.0f },
        { TEXT("Tank.Hold"),              EBreakerClassId::Tank,     100.0f,  0.0f },
        { TEXT("Support.Patch"),          EBreakerClassId::Support,   25.0f,  6.0f },
        { TEXT("Support.Purge"),          EBreakerClassId::Support,   30.0f, 10.0f },
        { TEXT("Support.Cadence"),        EBreakerClassId::Support,   30.0f,  8.0f },
        { TEXT("Support.Metronome"),      EBreakerClassId::Support,   35.0f,  9.0f },
        { TEXT("Support.Mark"),           EBreakerClassId::Support,   20.0f,  5.0f },
        { TEXT("Support.Suppress"),       EBreakerClassId::Support,   40.0f, 10.0f },
        { TEXT("Support.Conduit"),        EBreakerClassId::Support,  100.0f,  0.0f },
    };

    for (const FExpected& Row : Expected)
    {
        const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(Row.Id);
        if (!Definition)
        {
            AddError(FString::Printf(TEXT("%s is missing from the fallback registry"), Row.Id));
            continue;
        }
        TestTrue(*FString::Printf(TEXT("%s belongs to its class"), Row.Id), Definition->ClassId == Row.ClassId);
        TestEqual(*FString::Printf(TEXT("%s costs what its kit document says"), Row.Id), Definition->GetResourceCost(), Row.Cost);
        TestEqual(*FString::Printf(TEXT("%s has the cooldown its kit document says"), Row.Id), Definition->GetCooldownSeconds(), Row.Cooldown);
        // FLIPPED 2026-08-16: the O39 conversation happened (owner: "feel free
        // to do all 5 classes") and every row now executes. The old TestFalse
        // was the alarm for a row acquiring an AbilityClass without that
        // conversation; it fired exactly as designed, and the assertion now
        // pins the built state so a row LOSING its class is equally loud.
        TestTrue(*FString::Printf(TEXT("%s executes (implemented 2026-08-16)"), Row.Id), Definition->IsImplemented());
    }

    // The class's ergonomic, asserted rather than described: Gunsmith runs on
    // two clocks. Personal abilities cost nothing and have a cooldown;
    // deployables cost Scrap and have none. A cooldown appearing on a deployable
    // deletes the thing that makes a Gunsmith loadout a decision.
    for (const UBreakerAbilityDefinition* Definition : UBreakerAbilityDefinition::GetFallbackRegistry())
    {
        if (!Definition || Definition->ClassId != EBreakerClassId::Gunsmith) continue;
        const bool bCostsScrap = Definition->GetResourceCost() > 0.0f;
        TestFalse(TEXT("No Gunsmith ability is gated by both a cost and a cooldown"),
            bCostsScrap && Definition->HasCooldown());
        TestTrue(TEXT("Every Gunsmith ability is gated by exactly one of cost or cooldown"),
            bCostsScrap || Definition->HasCooldown());
    }

    // Every Tank ability carries BOTH a cost and a cooldown, and the cooldown
    // band is the longest in the game because the invulnerability audit leans on
    // cooldown length as its primary guard. The ultimate is the exception on
    // purpose: its cost IS its cooldown.
    for (const UBreakerAbilityDefinition* Definition : UBreakerAbilityDefinition::GetFallbackRegistry())
    {
        if (!Definition || Definition->ClassId != EBreakerClassId::Tank) continue;
        TestTrue(TEXT("Every Tank ability costs Grit"), Definition->GetResourceCost() > 0.0f);
        if (Definition->IsUltimate())
        {
            TestFalse(TEXT("The Tank ultimate is cost-gated, not cooldown-gated"), Definition->HasCooldown());
            continue;
        }
        TestTrue(TEXT("Every non-ultimate Tank ability has a cooldown of at least 5s"), Definition->GetCooldownSeconds() >= 5.0f);
        TestTrue(TEXT("No Tank cooldown exceeds 12s"), Definition->GetCooldownSeconds() <= 12.0f);
    }

    // Support's band, and the ignition rule: Mark is the cheapest and shortest
    // ability in the class, because a loop whose ignition is expensive stalls at
    // low Charge.
    const UBreakerAbilityDefinition* Mark = UBreakerAbilityDefinition::FindFallback(TEXT("Support.Mark"));
    for (const UBreakerAbilityDefinition* Definition : UBreakerAbilityDefinition::GetFallbackRegistry())
    {
        if (!Definition || Definition->ClassId != EBreakerClassId::Support || Definition->IsUltimate()) continue;
        TestTrue(TEXT("Every Support ability costs Charge and has a cooldown"),
            Definition->GetResourceCost() > 0.0f && Definition->HasCooldown());
        if (Mark && Definition != Mark)
        {
            TestTrue(TEXT("Mark is the cheapest Support ability"), Definition->GetResourceCost() >= Mark->GetResourceCost());
            TestTrue(TEXT("Mark has the shortest Support cooldown"), Definition->GetCooldownSeconds() >= Mark->GetCooldownSeconds());
        }
    }

    // Each ultimate costs a full bar and is cost-gated, so a keystone rewrite
    // can never be chain-cast off a cooldown that happens to be ready.
    for (const TCHAR* UltimateId : { TEXT("Gunsmith.FieldAssembly"), TEXT("Tank.Hold"), TEXT("Support.Conduit") })
    {
        const UBreakerAbilityDefinition* Ultimate = UBreakerAbilityDefinition::FindFallback(UltimateId);
        if (!Ultimate) continue;
        TestTrue(*FString::Printf(TEXT("%s occupies the ultimate slot"), UltimateId), Ultimate->IsUltimate());
        TestEqual(*FString::Printf(TEXT("%s costs a full bar"), UltimateId), Ultimate->GetResourceCost(), 100.0f);
        TestFalse(*FString::Printf(TEXT("%s is cost-gated, not cooldown-gated"), UltimateId), Ultimate->HasCooldown());
        // One base row plus three keystone rewrites. A missing row would mean a
        // keystone silently falling through to base behaviour, which is how a
        // build-defining pick becomes inert without anything failing.
        TestEqual(*FString::Printf(TEXT("%s authors a base row and three keystone rewrites"), UltimateId),
            Ultimate->Variants.Num(), 4);
        int32 KeystoneRows = 0;
        for (const FBreakerAbilityVariant& Variant : Ultimate->Variants)
        {
            if (Variant.KeystoneTag.IsValid()) ++KeystoneRows;
        }
        TestEqual(*FString::Printf(TEXT("%s authors exactly three keystone tags"), UltimateId), KeystoneRows, 3);
    }

    // Conduit is the one ultimate of the three whose keystones differ
    // PARAMETRICALLY: two of the three replace casting rather than enabling it,
    // and therefore stop the free-cast window. Pinned because it is the only
    // behavioural difference in the three variant tables that is expressible in
    // data today, so it is the only one a test can defend.
    if (const UBreakerAbilityDefinition* Conduit = UBreakerAbilityDefinition::FindFallback(TEXT("Support.Conduit")))
    {
        FGameplayTagContainer Triage;
        // Requested by STRING rather than through a C++ symbol on purpose: the
        // string is what a granted GameplayEffect and a save actually key off,
        // and this is the assertion that survives the tags being promoted out of
        // BreakerAbilityDefinition.cpp into BreakerAbilityTags.h.
        Triage.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Keystone.Support.Triage")), false));
        TestEqual(TEXT("Base Conduit makes Support abilities free"),
            Conduit->ResolveVariant(FGameplayTagContainer()).AbilityCostMultiplier, 0.0f);
        TestEqual(TEXT("Triage replaces the free casts rather than adding to them"),
            Conduit->ResolveVariant(Triage).AbilityCostMultiplier, 1.0f);
    }

    return true;
}

// ---------------------------------------------------------------------------
// O39 — THE HONESTY GATE. This is the test that must not go quiet.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerUnbuiltClassHonestyTest,
    "RiorsEdge.Classes.Unbuilt.SliceClassHonesty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerUnbuiltClassHonestyTest::RunTest(const FString& Parameters)
{
    // O39: outside dev mode the class screen offers only classes with
    // IMPLEMENTED kits. Registering ability DATA for three unbuilt classes made
    // "the registry has rows for this class" stop being a safe proxy for that,
    // and this is the assertion that keeps the two apart.
    TestTrue(TEXT("Swift has an implemented kit"), UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId::Swift));
    TestTrue(TEXT("Caster has an implemented kit"), UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId::Caster));
    // FLIPPED 2026-08-16: "the abilities landed (good, and the class screen
    // needs telling)" — the case the old comment predicted. The class screen
    // derives from this very predicate, so it was told automatically (O39's
    // gate is ClassHasImplementedKit, not a list).
    TestTrue(TEXT("Gunsmith has an implemented kit (2026-08-16)"), UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId::Gunsmith));
    TestTrue(TEXT("Tank has an implemented kit (2026-08-16)"), UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId::Tank));
    TestTrue(TEXT("Support has an implemented kit (2026-08-16)"), UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId::Support));
    TestFalse(TEXT("No class is not a class"), UBreakerAbilityDefinition::ClassHasImplementedKit(EBreakerClassId::None));

    TestTrue(TEXT("Gunsmith has a catalogue to enumerate"),
        UBreakerAbilityDefinition::GetClassAbilityIds(EBreakerClassId::Gunsmith, EBreakerAbilitySlot::ClassAbilityOne).Num() > 0);
    TestTrue(TEXT("Tank has a catalogue to enumerate"),
        UBreakerAbilityDefinition::GetClassAbilityIds(EBreakerClassId::Tank, EBreakerAbilitySlot::ClassAbilityOne).Num() > 0);
    TestTrue(TEXT("Support has a catalogue to enumerate"),
        UBreakerAbilityDefinition::GetClassAbilityIds(EBreakerClassId::Support, EBreakerAbilitySlot::ClassAbilityOne).Num() > 0);

    // The progression layer's independent gate on the same decision agrees
    // (T7 step 4: the definitions were registered AFTER the abilities ran).
    TestNotNull(TEXT("Gunsmith has a class definition (2026-08-16)"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Gunsmith));
    TestNotNull(TEXT("Tank has a class definition (2026-08-16)"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Tank));
    TestNotNull(TEXT("Support has a class definition (2026-08-16)"), UBreakerProgressionLibrary::GetFallbackClassDefinition(EBreakerClassId::Support));

    // A class ability may not be equipped by a class that does not grant it,
    // whatever route the id took to get into the request.
    TestTrue(TEXT("Gunsmith grants its own Turret"), UBreakerAbilityDefinition::ClassGrantsAbility(EBreakerClassId::Gunsmith, TEXT("Gunsmith.Turret")));
    TestFalse(TEXT("A Swift may not equip a Turret"), UBreakerAbilityDefinition::ClassGrantsAbility(EBreakerClassId::Swift, TEXT("Gunsmith.Turret")));
    TestFalse(TEXT("A Tank may not equip a Support Mark"), UBreakerAbilityDefinition::ClassGrantsAbility(EBreakerClassId::Tank, TEXT("Support.Mark")));
    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
