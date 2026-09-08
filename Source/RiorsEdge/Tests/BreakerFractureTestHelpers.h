#pragma once
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbility_Fracture.h"
#include "Engine/World.h"

// Advance actual GAS casting, stopping on the frame it emits. The bound catches
// a stuck phase; it is not a substitute cooldown or an assumed completion time.
inline bool BreakerWaitForFractureCast(UWorld* World, UAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle Handle)
{
    if (!World || !ASC) return false;
    auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
    const auto* Instance = Spec ? Cast<UBreakerAbility_Fracture>(Spec->GetPrimaryInstance()) : nullptr;
    if (!Instance) return false;
    const float Bound = FMath::Max(.1f, Instance->BaseCastSeconds) + .1f;
    for (float Elapsed = 0; Spec && Spec->IsActive() && Elapsed <= Bound; Elapsed += .005f)
    {
        ++GFrameCounter; World->Tick(LEVELTICK_All, .005f);
        Spec = ASC->FindAbilitySpecFromHandle(Handle);
    }
    return Spec && !Spec->IsActive();
}
