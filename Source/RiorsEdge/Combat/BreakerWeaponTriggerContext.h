#pragma once

#include "Combat/BreakerCombatTypes.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectKey.h"

// One accepted trigger, shared by all pellets and flying siblings. Object keys
// retain identity after destruction without extending an actor's lifetime.
struct FBreakerWeaponTriggerContext
{
    FBreakerDamageRequest Source;
    FObjectKey FirstTarget;
    bool bHasFirstTarget = false;

    bool ClaimTarget(const AActor* Target)
    {
        if (!Target) return false;
        const FObjectKey Key(Target);
        if (!bHasFirstTarget) { FirstTarget = Key; bHasFirstTarget = true; }
        return FirstTarget != Key;
    }
};
