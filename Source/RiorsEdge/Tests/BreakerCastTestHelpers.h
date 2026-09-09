#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerAbilityDefinition.h"

// O266: an ability with an authored wind-up no longer resolves on the frame it
// is activated, so a runtime test must advance the world past the cast before
// it may assert the effect. The number is READ FROM THE FILE rather than
// restated here — a retune of the wind-up must not silently un-prove a test.
inline float BreakerAuthoredCastSeconds(const TCHAR* AbilityId)
{
    const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(FName(AbilityId));
    return Definition ? Definition->GetCastTimeSeconds() : 0.0f;
}
