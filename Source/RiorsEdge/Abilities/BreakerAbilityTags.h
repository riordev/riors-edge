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
}
