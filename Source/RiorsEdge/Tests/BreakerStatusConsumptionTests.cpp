#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbility_Resonance.h"
#include "Abilities/BreakerAbility_Siphon.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerStatusConsumption.h"
#include "GameFramework/Actor.h"

// STATUS CONSUMPTION (Ability-Implementation-Spec §5.6). Before this the status
// component could be READ and nothing more — no ability could detonate,
// convert, strip or count a status. That verb is what makes status a build axis
// instead of a damage-over-time footnote.

namespace BreakerConsumptionTest
{
    // Prefixed against unity-build collisions with other test translation
    // units, which have bitten this project twice.
    static FBreakerStatusApplicationSpec BreakerMakeStatus(const TCHAR* TagName, float Duration)
    {
        FBreakerStatusApplicationSpec Spec;
        Spec.StatusTag = FGameplayTag::RequestGameplayTag(TagName, false);
        Spec.BaseDamagePerTick = 6.0f;
        Spec.Duration = Duration;
        Spec.TickInterval = 1.0f;
        return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStatusConsumptionTest,
    "RiorsEdge.Combat.Status.Consumption",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusConsumptionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerConsumptionTest;

    AActor* Target = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);

    // Consuming from an EMPTY component is a legal no-op, not an error.
    // Resonance and Cascade both fire into whatever is there, and "there is
    // nothing there" must not be a crash or a phantom payout.
    const TArray<FBreakerActiveStatus> Nothing = Status->ConsumeAllStatuses();
    TestEqual(TEXT("Consuming an empty status list returns nothing"), Nothing.Num(), 0);
    TestEqual(TEXT("An empty list has no distinct types"), Status->GetDistinctStatusTypeCount(), 0);

    bool bFound = true;
    Status->ConsumeStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false), bFound);
    TestFalse(TEXT("Consuming a status that is not present reports not found"), bFound);

    Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Bleed"), 4.0f), EBreakerDamageFamily::Physical, nullptr);
    Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Poison"), 4.0f), EBreakerDamageFamily::Physical, nullptr);
    TestEqual(TEXT("Two different statuses are two distinct types"), Status->GetDistinctStatusTypeCount(), 2);

    // DISTINCT TYPES, never stacks. This is the explicit anti-stacking rule:
    // without it the best Resonance build is "apply one status ten times".
    for (int32 Index = 0; Index < 5; ++Index)
    {
        Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Bleed"), 4.0f), EBreakerDamageFamily::Physical, nullptr);
    }
    TestEqual(TEXT("Stacking one status does not raise the distinct count"), Status->GetDistinctStatusTypeCount(), 2);
    TestTrue(TEXT("The stacks really did accumulate"), Status->GetActiveStatuses()[0].Stacks > 1);

    // Consumption returns what it took, so the detonator computes its damage
    // from the same data it destroyed rather than reading the list twice.
    const TArray<FBreakerActiveStatus> Consumed = Status->ConsumeAllStatuses();
    TestEqual(TEXT("Consumption returns what it removed"), Consumed.Num(), 2);
    TestEqual(TEXT("The target is left clean"), Status->GetActiveStatuses().Num(), 0);
    TestFalse(TEXT("A consumed status is gone"), Status->HasStatus(Consumed[0].Spec.StatusTag));

    // Single-type consumption, for conversion effects.
    Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Bleed"), 4.0f), EBreakerDamageFamily::Physical, nullptr);
    Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Poison"), 4.0f), EBreakerDamageFamily::Physical, nullptr);
    bool bTook = false;
    const FBreakerActiveStatus Took = Status->ConsumeStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false), bTook);
    TestTrue(TEXT("Consuming a present status reports found"), bTook);
    TestTrue(TEXT("It returns the status it took"), Took.Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false));
    TestEqual(TEXT("Only that status was taken"), Status->GetActiveStatuses().Num(), 1);
    TestTrue(TEXT("The other status survives"), Status->HasStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStatusDurationScalingTest,
    "RiorsEdge.Combat.Status.DurationScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusDurationScalingTest::RunTest(const FString& Parameters)
{
    using namespace BreakerConsumptionTest;

    AActor* Target = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Bleed"), 4.0f), EBreakerDamageFamily::Physical, nullptr);
    Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Poison"), 6.0f), EBreakerDamageFamily::Physical, nullptr);

    // MS8's rewrite: Resonance stops consuming and halves durations instead.
    Status->ScaleRemainingDurations(0.5f);
    TestEqual(TEXT("Both statuses survive the halving"), Status->GetActiveStatuses().Num(), 2);
    TestEqual(TEXT("Bleed is halved"), Status->GetActiveStatuses()[0].RemainingDuration, 2.0f);
    TestEqual(TEXT("Poison is halved"), Status->GetActiveStatuses()[1].RemainingDuration, 3.0f);

    // A scalar of zero means gone, and gone must mean the same thing whichever
    // route it took: it removes rather than leaving a zero-duration ghost that
    // never expires because expiry only runs on tick.
    Status->ScaleRemainingDurations(0.0f);
    TestEqual(TEXT("Scaling to zero removes"), Status->GetActiveStatuses().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerStatusStackCapDeltaTest,
    "RiorsEdge.Combat.Status.StackCapDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerStatusStackCapDeltaTest::RunTest(const FString& Parameters)
{
    using namespace BreakerConsumptionTest;

    AActor* Target = NewObject<AActor>();
    UBreakerStatusComponent* Status = NewObject<UBreakerStatusComponent>(Target);
    Status->MaximumStacksPerStatus = 3;
    TestEqual(TEXT("With no delta the cap is the authored cap"), Status->GetEffectiveStackCap(), 3);

    // Affliction A1 Deepen raises the cap.
    Status->SetStackCapDelta(2);
    TestEqual(TEXT("A positive delta raises the cap"), Status->GetEffectiveStackCap(), 5);
    for (int32 Index = 0; Index < 10; ++Index)
    {
        Status->ApplyStatus(BreakerMakeStatus(TEXT("Status.Bleed"), 4.0f), EBreakerDamageFamily::Physical, nullptr);
    }
    TestEqual(TEXT("Stacks cap at the raised cap"), Status->GetActiveStatuses()[0].Stacks, 5);

    // Lowering it trims live stacks immediately. A status carrying more stacks
    // than the cap allows keeps ticking at the old magnitude and reads as a
    // broken cap.
    Status->SetStackCapDelta(-1);
    TestEqual(TEXT("A negative delta lowers the cap"), Status->GetEffectiveStackCap(), 2);
    TestEqual(TEXT("Live stacks are trimmed to the new cap"), Status->GetActiveStatuses()[0].Stacks, 2);

    // Never below one: a cap of zero would create statuses that tick for
    // nothing and never expire early.
    Status->SetStackCapDelta(-50);
    TestEqual(TEXT("The cap floors at one"), Status->GetEffectiveStackCap(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDetonationCurveTest,
    "RiorsEdge.Combat.Status.DetonationCurve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDetonationCurveTest::RunTest(const FString& Parameters)
{
    FBreakerDetonationParams Params;

    // Detonating nothing pays nothing. This is the rule that stops Resonance
    // being free damage on a clean target.
    TestEqual(TEXT("Zero distinct statuses detonate for zero"),
        UBreakerStatusConsumption::DetonationDamage(0, Params, EBreakerDetonationCurve::Linear), 0.0f);

    const float Two = UBreakerStatusConsumption::DetonationDamage(2, Params, EBreakerDetonationCurve::Linear);
    const float Three = UBreakerStatusConsumption::DetonationDamage(3, Params, EBreakerDetonationCurve::Linear);
    TestTrue(TEXT("More distinct statuses detonate harder"), Three > Two);

    // MS9 Interference adds a flat bonus at the threshold and nothing below it.
    TestEqual(TEXT("Below the threshold MS9 matches the base curve"),
        UBreakerStatusConsumption::DetonationDamage(2, Params, EBreakerDetonationCurve::FixedPlusThreshold), Two);
    TestEqual(TEXT("At the threshold MS9 pays its flat bonus"),
        UBreakerStatusConsumption::DetonationDamage(3, Params, EBreakerDetonationCurve::FixedPlusThreshold),
        Three + Params.ThresholdFlatBonus);

    // Class-Kits §2.7.5, the BOUND this whole file exists to protect:
    // Resonance against 6 statuses is no more than 2.2x its damage against 2.
    // With no flat base term the linear ratio is exactly 3.0, which is why
    // FlatDamageIfAny defaults large — the bound is a property of the numbers,
    // so the numbers are checked and not just the shape.
    const float LinearRatio = UBreakerStatusConsumption::DetonationRatio(2, 6, Params, EBreakerDetonationCurve::Linear);
    const float MS9Ratio = UBreakerStatusConsumption::DetonationRatio(2, 6, Params, EBreakerDetonationCurve::FixedPlusThreshold);
    TestTrue(TEXT("The base curve honours the 2.2x bound"), LinearRatio <= 2.2f);
    TestTrue(TEXT("MS9 honours the 2.2x bound"), MS9Ratio <= 2.2f);
    TestTrue(TEXT("The bound is not honoured by making the curve flat"), LinearRatio > 1.0f);

    // ---- THE RELATIONSHIP, NOT JUST THE CEILING ---------------------------
    // O115, one system over, and the fourth instance of flatness-without-
    // magnitude. Both curves clear the 2.2x bound, so the two assertions above
    // pass and say nothing about whether MS9 does its job -- and it does not.
    // The enum header calls FixedPlusThreshold the anti-explosion rewrite,
    // "deliberately re-shaped AWAY from a count multiplier", while the
    // arithmetic is the LINEAR term plus a flat bonus at 3+: strictly additive
    // on top, so its 2-to-6 ratio is 1.87 against Linear's 1.70. MS9 is
    // STEEPER than the curve it exists to flatten, and a ceiling wide enough
    // for both could never notice.
    //
    // EXPECTED RED, and the condition that deletes it is a ruling rather than a
    // tuning pass: either FixedPlusThreshold stops reusing DamagePerDistinct-
    // Status so the per-status term really is fixed, or Class-Kits MS9 is
    // re-read and the header stops claiming a reshape the numbers do not do.
    // Do not close this by widening the bound, and do not close it by deleting
    // this assertion -- an anti-explosion rewrite that explodes faster than the
    // baseline is the finding.
    TestTrue(*FString::Printf(
        TEXT("MS9 is FLATTER than the curve it reshapes (MS9 %.3f vs linear %.3f)"),
        MS9Ratio, LinearRatio),
        MS9Ratio < LinearRatio);

    // The counted statuses are capped, so a seventh status type added later
    // cannot silently inflate an already-bounded ability.
    TestEqual(TEXT("Distinct count is capped"),
        UBreakerStatusConsumption::DetonationDamage(20, Params, EBreakerDetonationCurve::Linear),
        UBreakerStatusConsumption::DetonationDamage(Params.MaximumCountedStatuses, Params, EBreakerDetonationCurve::Linear));

    // MS5 Payment reads the same count the damage does.
    TestEqual(TEXT("Refund is per distinct status"),
        UBreakerStatusConsumption::RefundForConsumed(3, 5.0f, 6), 15.0f);
    TestEqual(TEXT("Refund is capped with the count"),
        UBreakerStatusConsumption::RefundForConsumed(20, 5.0f, 6), 30.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSiphonRulesTest,
    "RiorsEdge.Abilities.Siphon.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSiphonRulesTest::RunTest(const FString& Parameters)
{
    // The heal is a PORTION of damage landed, so it follows the caster's
    // offensive scaling instead of becoming irrelevant at level 50.
    TestEqual(TEXT("The heal is a fraction of the damage"), UBreakerAbility_Siphon::HealForTick(50.0f, 0.4f), 20.0f);
    // A dodged or fully mitigated tick landed nothing and pays nothing.
    TestEqual(TEXT("A tick that dealt nothing heals nothing"), UBreakerAbility_Siphon::HealForTick(0.0f, 0.4f), 0.0f);
    TestEqual(TEXT("A zero leech fraction heals nothing"), UBreakerAbility_Siphon::HealForTick(50.0f, 0.0f), 0.0f);

    // "Breaks on the caster taking damage ABOVE a threshold" — the boundary
    // does not break, matching Closequarter's at-or-below refund convention.
    TestFalse(TEXT("Damage below the threshold does not break"),
        UBreakerAbility_Siphon::ShouldBreakChannel(4.0f, 100.0f, 0.05f));
    TestFalse(TEXT("Damage exactly at the threshold does not break"),
        UBreakerAbility_Siphon::ShouldBreakChannel(5.0f, 100.0f, 0.05f));
    TestTrue(TEXT("Damage above the threshold breaks"),
        UBreakerAbility_Siphon::ShouldBreakChannel(5.1f, 100.0f, 0.05f));
    // VW6 Drain raises the threshold; it is a data row, so the same function
    // answers for both.
    TestFalse(TEXT("VW6's raised threshold tolerates the same hit"),
        UBreakerAbility_Siphon::ShouldBreakChannel(5.1f, 100.0f, 0.15f));
    TestFalse(TEXT("Taking no damage never breaks"),
        UBreakerAbility_Siphon::ShouldBreakChannel(0.0f, 100.0f, 0.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerResonanceDefaultsTest,
    "RiorsEdge.Abilities.Resonance.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerResonanceDefaultsTest::RunTest(const FString& Parameters)
{
    const UBreakerAbility_Resonance* Resonance = GetDefault<UBreakerAbility_Resonance>();
    if (!TestNotNull(TEXT("Resonance has a default object"), Resonance)) return false;
    TestTrue(TEXT("Resonance consumes at base; MS8 is what stops it"), Resonance->bConsumeStatuses);
    TestEqual(TEXT("MS8's rewrite halves durations"), Resonance->DurationScalarWhenNotConsuming, 0.5f);
    // MS5 Payment is a node, not base kit.
    TestEqual(TEXT("Resonance refunds nothing at base"), Resonance->RefundManaPerStatus, 0.0f);
    return true;
}

#endif
