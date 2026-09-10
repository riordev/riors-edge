#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerGameplayAbility.h"

// ---------------------------------------------------------------------------
// O266. The wind-up rule, and the shipped configuration it is authored into.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCastTimeRuleTest,
    "RiorsEdge.Abilities.CastTime.Rule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCastTimeRuleTest::RunTest(const FString& Parameters)
{
    using FAbility = UBreakerGameplayAbility;

    // The DIVISOR convention every rate lane in this project already uses: a
    // multiplier of 1.25 is a 20% SHORTER cast, never a 25% longer one.
    TestEqual(TEXT("a neutral multiplier changes nothing"), FAbility::EffectiveCastSeconds(0.80f, 1.0f), 0.80f, 0.0001f);
    TestEqual(TEXT("1.25 divides to a 20% shorter cast"), FAbility::EffectiveCastSeconds(0.80f, 1.25f), 0.64f, 0.0001f);
    TestEqual(TEXT("2.0 halves it"), FAbility::EffectiveCastSeconds(0.90f, 2.0f), 0.45f, 0.0001f);

    // An ability that authors no wind-up has none, whatever the multiplier —
    // this is what keeps movement and defensive verbs instant by ruling rather
    // than by anybody remembering to skip them.
    TestEqual(TEXT("no authored cast stays instant"), FAbility::EffectiveCastSeconds(0.0f, 2.0f), 0.0f, 0.0001f);
    TestEqual(TEXT("and cannot be made negative"), FAbility::EffectiveCastSeconds(-1.0f, 1.0f), 0.0f, 0.0001f);
    // A malformed multiplier is floored rather than dividing by zero.
    TestTrue(TEXT("a zero multiplier is floored, never a division by zero"),
        FMath::IsFinite(FAbility::EffectiveCastSeconds(0.80f, 0.0f)));

    // The window key is per-ability, so two pending casts can never share one
    // countdown, and carries the prefix the HUD's window bar already filters on.
    const FName Key = FAbility::CastWindowKey(FName(TEXT("Caster.Cleave")));
    TestTrue(TEXT("a cast window is a HUD window"), Key.ToString().StartsWith(TEXT("Window.")));
    TestTrue(TEXT("and names its own ability"), Key.ToString().Contains(TEXT("Caster.Cleave")));
    TestNotEqual(TEXT("two abilities do not share a countdown"), Key, FAbility::CastWindowKey(FName(TEXT("Caster.Rot"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCastTimeShippedTest,
    "RiorsEdge.Abilities.CastTime.Shipped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCastTimeShippedTest::RunTest(const FString& Parameters)
{
    auto Authored = [this](const TCHAR* Id) -> float
    {
        const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(FName(Id));
        if (!TestNotNull(*FString::Printf(TEXT("%s is registered"), Id), Definition)) return -1.0f;
        return Definition->GetCastTimeSeconds();
    };

    // Cleave is the owner's own report — "cleave has instant cast speed" — and
    // is the ability this rule ships on.
    TestEqual(TEXT("Cleave winds up before it swings"), Authored(TEXT("Caster.Cleave")), 0.35f, 0.0001f);

    // Movement and defensive verbs stay instant, OWNER-RULED: a dodge with a
    // wind-up is not a dodge. Closequarter is a blink, so it is movement.
    // THE INSTANT FOUR, and the roster is asserted CLOSED: an ability added
    // later without a wind-up is a red, not a silent exemption.
    const TSet<FName> Instant = {
        FName(TEXT("Swift.Slipcut")),        // a tempo state, snapped on mid-fight
        FName(TEXT("Swift.HardStop")),       // "cancels all velocity INSTANTLY", its own text
        FName(TEXT("Caster.Closequarter")),  // a blink is movement
        FName(TEXT("Tank.GroundZero")),      // an airborne commit already in flight
        // AND the abilities that ALREADY OWN THEIR TIMING. A generic wind-up
        // on top of a bespoke one casts the ability twice, which is how these
        // three were found: Fracture has its own cast phase (see
        // FractureCastPhaseRuntime), Siphon is a 5s channel, and Breach
        // Charge's 1.2s fuse is the delay it already carries.
        FName(TEXT("Caster.Fracture")),
        FName(TEXT("Caster.Siphon")),
        FName(TEXT("Tank.BreachCharge")),
        // UNMAKE AND RESONANCE HAVE LEFT THIS SET, owner-ruled, and both were
        // parked here on real measurements rather than hunches — so what moved
        // is the mechanism each measurement was about, not the number.
        //
        // Unmake was instant because its window suspends Mana generation and a
        // wind-up let the bank regenerate for the length of the cast BEFORE the
        // suspension started, deep enough to clear OverreachRuntime's debt and
        // hand back a second free Unmake. The suspension now starts at the CAST
        // (UBreakerAbility_Unmake::OnCastBegan), so there is no generating
        // window to exploit and that fixture's interaction stands untouched.
        //
        // Resonance was instant because it is paid out of the target's
        // remaining status budget and a wind-up burned that budget before the
        // detonation collected it — 405 -> 270. It now SNAPSHOTS the count at
        // cast start, the way DoT sources already snapshot, which was named
        // here as the ruling this needed. What is consumed is still whatever is
        // live at the landing; only the count the damage scales by is frozen.
    };
    for (const FName Id : Instant)
    {
        TestEqual(*FString::Printf(TEXT("%s stays instant"), *Id.ToString()), Authored(*Id.ToString()), 0.0f, 0.0001f);
    }

    // The two that left the set, pinned by VALUE rather than only by the
    // sweep below: each one's wind-up is the thing a ruling bought, so a
    // silent return to zero is a regression and not a retune.
    TestEqual(TEXT("Resonance winds up before it detonates"), Authored(TEXT("Caster.Resonance")), 0.35f, 0.0001f);
    TestEqual(TEXT("Unmake winds up before the window opens"), Authored(TEXT("Caster.Unmake")), 0.4f, 0.0001f);
    TestTrue(TEXT("The ultimate is the most committal cast in the class"),
        Authored(TEXT("Caster.Unmake")) > Authored(TEXT("Caster.Cleave"))
        && Authored(TEXT("Caster.Unmake")) > Authored(TEXT("Caster.Resonance")));
    for (const UBreakerAbilityDefinition* Definition : UBreakerAbilityDefinition::GetFallbackRegistry())
    {
        if (!Definition || Instant.Contains(Definition->AbilityId)) continue;
        TestTrue(*FString::Printf(TEXT("%s winds up"), *Definition->AbilityId.ToString()), Definition->HasCastTime());
    }

    // Every registry row carries the key, so a row that forgot it is a load
    // error rather than a silent zero.
    for (const UBreakerAbilityDefinition* Definition : UBreakerAbilityDefinition::GetFallbackRegistry())
    {
        if (!Definition) continue;
        TestTrue(*FString::Printf(TEXT("%s authors a finite, non-negative wind-up"), *Definition->AbilityId.ToString()),
            FMath::IsFinite(Definition->CastTimeSeconds) && Definition->CastTimeSeconds >= 0.0f);
        // HasCastTime and the number agree, so no caller can disagree with the
        // gate about whether an ability winds up.
        TestEqual(*FString::Printf(TEXT("%s agrees with its own predicate"), *Definition->AbilityId.ToString()),
            Definition->HasCastTime(), Definition->CastTimeSeconds > 0.0f);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
