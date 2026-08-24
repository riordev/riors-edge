#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Combat/BreakerCombatComponent.h"
#include "Classes/BreakerManaComponent.h"

// ---------------------------------------------------------------------------
// TWO OVERCAST RULES, TWO KEYS, AND THEY MUST COMPOSE RATHER THAN OVERWRITE.
//
// SB11 Overreach and VW11 Long Debt both raise the damage taken while Mana is
// negative — 15% becomes 30% for one and 25% for the other. Both were about to
// be written the obvious way: assign OvercastIncomingDamageTaken, which is one
// BlueprintReadWrite field pushed under ONE key,
// UBreakerManaComponent::OvercastDamageModifierKey(). Two rules writing one
// field under one key do not compose; the second silently replaces the first,
// and a player holding both doctrines' keystones would get whichever ran last.
//
// The incoming-damage layer already composes correctly — it multiplies over a
// KEYED MAP, so distinct keys stack and a repeated key replaces. That is the
// right shape and it is what makes the fix a key rather than a rewrite. This
// test pins that property BEFORE either reader exists, because the reader that
// gets written second is the one that would quietly break the first, and by
// then the assertion is a regression rather than a design note.
//
// Nothing here reads a node tag: neither node is authored yet. What is asserted
// is the mechanism both will use.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerOvercastModifierKeysTest,
    "RiorsEdge.Combat.Incoming.KeyedModifiersCompose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerOvercastModifierKeysTest::RunTest(const FString& Parameters)
{
    AActor* Owner = NewObject<AActor>();
    UBreakerCombatComponent* Combat = NewObject<UBreakerCombatComponent>(Owner);
    if (!TestNotNull(TEXT("A combat component is constructible"), Combat)) return false;

    TestEqual(TEXT("Nothing pushed composes to identity"),
        Combat->GetComposedIncomingDamageMultiplier(), 1.0f, 0.0001f);

    // The key the Mana component already owns. SB11's half rides this one,
    // because Overcast's base penalty is the thing it rewrites.
    const FName OvercastKey = UBreakerManaComponent::OvercastDamageModifierKey();
    Combat->PushIncomingDamageModifier(OvercastKey, 1.30f);
    TestEqual(TEXT("One modifier composes to itself"),
        Combat->GetComposedIncomingDamageMultiplier(), 1.30f, 0.0001f);

    // THE SAME KEY REPLACES. This is the half that would have bitten: a second
    // rule assigning the same field under the same key is not additive and not
    // multiplicative, it is a silent overwrite.
    Combat->PushIncomingDamageModifier(OvercastKey, 1.25f);
    TestEqual(TEXT("The same key REPLACES rather than stacking"),
        Combat->GetComposedIncomingDamageMultiplier(), 1.25f, 0.0001f);

    // A DISTINCT KEY COMPOSES. VW11 takes its own key for exactly this reason,
    // so a character holding both keystones gets both rules.
    const FName LongDebtKey(TEXT("Caster.Overcast.LongDebt"));
    Combat->PushIncomingDamageModifier(LongDebtKey, 1.30f);
    TestEqual(TEXT("A second KEY composes with the first, multiplicatively"),
        Combat->GetComposedIncomingDamageMultiplier(), 1.25f * 1.30f, 0.0001f);

    // And each is independently removable, so one doctrine's rule ending does
    // not take the other's with it.
    Combat->PushIncomingDamageModifier(LongDebtKey, 1.0f);
    TestEqual(TEXT("Neutralising one key leaves the other standing"),
        Combat->GetComposedIncomingDamageMultiplier(), 1.25f, 0.0001f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
