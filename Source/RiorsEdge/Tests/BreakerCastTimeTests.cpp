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
    for (const TCHAR* Instant : { TEXT("Caster.Closequarter"), TEXT("Swift.Slipcut"),
        TEXT("Swift.HardStop"), TEXT("Swift.CadenceBreak"), TEXT("Tank.AnchorPoint") })
    {
        TestEqual(*FString::Printf(TEXT("%s stays instant"), Instant), Authored(Instant), 0.0f, 0.0001f);
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
