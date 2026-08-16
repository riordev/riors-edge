#include "Abilities/BreakerAbilityTags.h"

namespace BreakerAbilityTags
{
    UE_DEFINE_GAMEPLAY_TAG(Data_AbilityCost, "Data.AbilityCost");
    UE_DEFINE_GAMEPLAY_TAG(Data_AbilityCooldown, "Data.AbilityCooldown");

    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Swift_Skim, "Ability.Class.Swift.Skim");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Swift_Lead, "Ability.Class.Swift.Lead");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Swift_CadenceBreak, "Ability.Class.Swift.CadenceBreak");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Swift_Overdrive, "Ability.Class.Swift.Overdrive");

    UE_DEFINE_GAMEPLAY_TAG(Cooldown_Class_Swift_Skim, "Cooldown.Class.Swift.Skim");
    UE_DEFINE_GAMEPLAY_TAG(Cooldown_Class_Swift_Lead, "Cooldown.Class.Swift.Lead");
    UE_DEFINE_GAMEPLAY_TAG(Cooldown_Class_Swift_CadenceBreak, "Cooldown.Class.Swift.CadenceBreak");
    UE_DEFINE_GAMEPLAY_TAG(Cooldown_Class_Swift_Overdrive, "Cooldown.Class.Swift.Overdrive");

    UE_DEFINE_GAMEPLAY_TAG(State_Ability_Skim, "State.Ability.Skim");
    UE_DEFINE_GAMEPLAY_TAG(State_Ability_Lead, "State.Ability.Lead");
    // Spec §4.2: ActivationOwnedTags State.Ability.CadenceBreak.
    UE_DEFINE_GAMEPLAY_TAG(State_Ability_CadenceBreak, "State.Ability.CadenceBreak");
    UE_DEFINE_GAMEPLAY_TAG(State_Ultimate_Overdrive, "State.Ultimate.Overdrive");

    UE_DEFINE_GAMEPLAY_TAG(Keystone_Swift_Bloodrhythm, "Keystone.Swift.Bloodrhythm");
    UE_DEFINE_GAMEPLAY_TAG(Keystone_Swift_TerminalVelocity, "Keystone.Swift.TerminalVelocity");
    UE_DEFINE_GAMEPLAY_TAG(Keystone_Swift_StandingWave, "Keystone.Swift.StandingWave");

    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Caster_Cleave, "Ability.Class.Caster.Cleave");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Caster_Closequarter, "Ability.Class.Caster.Closequarter");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Caster_Rot, "Ability.Class.Caster.Rot");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Caster_Siphon, "Ability.Class.Caster.Siphon");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Caster_Fracture, "Ability.Class.Caster.Fracture");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Caster_Resonance, "Ability.Class.Caster.Resonance");
    UE_DEFINE_GAMEPLAY_TAG(Ability_Class_Caster_Unmake, "Ability.Class.Caster.Unmake");

    UE_DEFINE_GAMEPLAY_TAG(State_Ability_Cleave, "State.Ability.Cleave");
    UE_DEFINE_GAMEPLAY_TAG(State_Ability_Closequarter, "State.Ability.Closequarter");
    UE_DEFINE_GAMEPLAY_TAG(State_Ability_Siphon, "State.Ability.Siphon");
    UE_DEFINE_GAMEPLAY_TAG(State_Ultimate_Unmake, "State.Ultimate.Unmake");

    UE_DEFINE_GAMEPLAY_TAG(Zone_Caster_Rot, "Zone.Caster.Rot");

    UE_DEFINE_GAMEPLAY_TAG(Keystone_Caster_Edgework, "Keystone.Caster.Edgework");
    UE_DEFINE_GAMEPLAY_TAG(Keystone_Caster_LongDark, "Keystone.Caster.LongDark");
    UE_DEFINE_GAMEPLAY_TAG(Keystone_Caster_Cascade, "Keystone.Caster.Cascade");

    UE_DEFINE_GAMEPLAY_TAG(Damage_Melee, "Damage.Melee");
}
