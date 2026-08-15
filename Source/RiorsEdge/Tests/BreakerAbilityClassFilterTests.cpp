#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"

// Owner playtest: "im also able to have abilities from other classes
// equipped". UBreakerAbilityComponent::ValidateSelection (exercised by
// RiorsEdge.Abilities.Selection in BreakerAbilityTests.cpp) already refused a
// cross-class WRITE. The hole was on the READ side:
// UBreakerAbilityComponent::ResolveDefinition — the only function that turns
// a loadout id into an actually-granted GAS ability (RefreshGrants is its
// only caller) — checked CanOccupySlot but never checked ClassId. A loadout
// can carry a foreign-class id without ever passing through the equip write:
// UBreakerProgressionComponent::DevForceClass (Progression/) rewrites
// PermanentClass and ClassDefinition on a class switch but does not migrate
// State.AbilityLoadout, and a stale/hand-edited save can carry the same
// thing. This file exercises ResolveDefinition directly, which is pure and
// world-free (no UAbilitySystemComponent, no UBreakerProgressionComponent,
// no world) so it needs neither.
//
// What this file does NOT cover: that RefreshGrants actually calls
// ResolveDefinition with the live class and skips granting a null result, or
// that DevForceClass is the specific path that leaves a loadout unmigrated —
// both would need a world and a running ability/progression pair, which
// nothing in this suite constructs.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAbilityClassFilterResolveTest,
    "RiorsEdge.Abilities.ClassFilter.ResolveDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAbilityClassFilterResolveTest::RunTest(const FString& Parameters)
{
    const EBreakerAbilitySlot One = EBreakerAbilitySlot::ClassAbilityOne;
    const EBreakerAbilitySlot Ult = EBreakerAbilitySlot::Ultimate;

    // THE BUG, reproduced directly: a Swift ultimate sitting in a Caster's
    // loadout (exactly what a DevForceClass swap leaves behind) must not
    // resolve to itself.
    const UBreakerAbilityDefinition* CrossClassUltimate =
        UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Caster, Ult, TEXT("Swift.Overdrive"));
    TestTrue(TEXT("A foreign-class ultimate id does not resolve to itself"),
        !CrossClassUltimate || CrossClassUltimate->AbilityId != TEXT("Swift.Overdrive"));
    // And the archetype's own rule: no ability is granted with a class
    // mismatched to the character asking for it.
    TestTrue(TEXT("Whatever resolves for a Caster is a Caster ability"),
        !CrossClassUltimate || CrossClassUltimate->ClassId == EBreakerClassId::Caster);
    TestTrue(TEXT("A rejected foreign ultimate falls back to the class default rather than an empty slot"),
        CrossClassUltimate && CrossClassUltimate->AbilityId == TEXT("Caster.Unmake"));

    // Same shape, a class-ability slot rather than the ultimate: a Swift
    // starter in a Caster's ClassAbilityOne slot.
    const UBreakerAbilityDefinition* CrossClassAbility =
        UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Caster, One, TEXT("Swift.Skim"));
    TestTrue(TEXT("A foreign-class ability id does not resolve to itself"),
        !CrossClassAbility || CrossClassAbility->AbilityId != TEXT("Swift.Skim"));
    TestTrue(TEXT("The fallback for a rejected foreign ability is still this class's default"),
        CrossClassAbility && CrossClassAbility->ClassId == EBreakerClassId::Caster);

    // Control: the SAME ids resolve normally for their OWN class, so the fix
    // is a class filter and not an accidental ban on these ids everywhere.
    const UBreakerAbilityDefinition* OwnClassUltimate =
        UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, Ult, TEXT("Swift.Overdrive"));
    TestTrue(TEXT("Swift.Overdrive still resolves for a Swift character"),
        OwnClassUltimate && OwnClassUltimate->AbilityId == TEXT("Swift.Overdrive"));
    const UBreakerAbilityDefinition* OwnClassAbility =
        UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, One, TEXT("Swift.Skim"));
    TestTrue(TEXT("Swift.Skim still resolves for a Swift character"),
        OwnClassAbility && OwnClassAbility->AbilityId == TEXT("Swift.Skim"));

    // Untouched behaviour: nothing equipped still falls back to the class
    // default (the case BreakerAbilityTests.cpp's Selection test already
    // covers for Caster/ClassAbilityTwo; this checks the Swift side of it
    // stayed intact).
    const UBreakerAbilityDefinition* Defaulted =
        UBreakerAbilityComponent::ResolveDefinition(EBreakerClassId::Swift, One, NAME_None);
    TestTrue(TEXT("Nothing equipped still resolves to the class default"),
        Defaulted && Defaulted->AbilityId == TEXT("Swift.Skim"));

    return true;
}

#endif
