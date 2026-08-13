#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// Ability-layer tags are declared natively rather than added to
// Config/DefaultGameplayTags.ini so that the ability infrastructure owns its
// own vocabulary and content authoring cannot silently drop a tag the C++
// fallback registry depends on.
namespace BreakerAbilityTags
{
    // SetByCaller magnitude channels (Ability-Implementation-Spec D3/SI-6).
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_AbilityCost);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_AbilityCooldown);

    // Swift, the vertical-slice class (Class-Kits §1.2).
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Class_Swift_Skim);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Class_Swift_Lead);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Class_Swift_Overdrive);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Class_Swift_Skim);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Class_Swift_Lead);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Class_Swift_Overdrive);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ability_Skim);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ability_Lead);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ultimate_Overdrive);

    // Branch keystones (spec D1). A keystone node grants a passive GE whose
    // only job is to add one of these to the owner; the ultimate reads its own
    // owner's container at activation and selects a variant row. No keystone
    // ever grants, replaces, or blocks an ability.
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Keystone_Swift_Bloodrhythm);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Keystone_Swift_TerminalVelocity);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Keystone_Swift_StandingWave);

    // Caster (Class-Kits §2.2). Deliberately NO Cooldown.* tags: every Caster
    // ability is cost-gated and Mana *is* the cooldown (Class-Kits §2.1), so
    // authoring cooldown tags here would let the HUD render a phantom
    // "cooldown of zero" (spec D3).
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Class_Caster_Cleave);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Class_Caster_Closequarter);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Class_Caster_Unmake);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ability_Cleave);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ability_Closequarter);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ultimate_Unmake);

    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Keystone_Caster_Edgework);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Keystone_Caster_LongDark);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Keystone_Caster_Cascade);

    // Source tag on melee damage requests. Class-Kits §2.3 gives the Spellblade
    // branch a 1.30x melee More and Item-Foundation gives Melee Damage % a class
    // home; both need to recognise a melee hit from the request alone.
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Melee);
}
