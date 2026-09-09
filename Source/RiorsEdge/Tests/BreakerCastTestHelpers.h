#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"

// O266: an ability with an authored wind-up no longer resolves on the frame it
// is activated, so a runtime test must advance the world past the cast before
// it may assert the effect. The number is READ FROM THE FILE rather than
// restated here — a retune of the wind-up must not silently un-prove a test.
inline float BreakerAuthoredCastSeconds(const TCHAR* AbilityId)
{
    const UBreakerAbilityDefinition* Definition = UBreakerAbilityDefinition::FindFallback(FName(AbilityId));
    return Definition ? Definition->GetCastTimeSeconds() : 0.0f;
}

// Advance a fixture's world by exactly the wind-up that is actually running
// and no further. Ticking a flat "long enough" interval would also advance
// every status, zone and cooldown these tests measure, so the loop watches the
// cast window itself and stops the moment the swing has landed. The cap is a
// runaway guard, not a duration.
inline bool BreakerAnyCastPending(class AActor* Caster)
{
    const UAbilitySystemComponent* ASC = Caster ? Caster->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
    if (!ASC) return false;
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        const UBreakerGameplayAbility* Ability = Cast<UBreakerGameplayAbility>(Spec.GetPrimaryInstance());
        if (!Ability) Ability = Cast<UBreakerGameplayAbility>(Spec.Ability);
        if (Ability && Ability->IsCasting()) return true;
    }
    return false;
}

// Advance a fixture's world by exactly the wind-up that is actually running
// and no further. Ticking a flat "long enough" interval would also advance
// every status, zone and cooldown these tests measure, so the loop asks the
// ABILITY whether it is still casting and stops the moment the swing lands.
//
// The ability is the authority rather than the cast window: the window needs a
// ticking state component to expire, and a fixture that never registers one
// would report no window at all and exit before the cast resolved. The cap is
// a runaway guard, not a duration.
inline void BreakerResolvePendingCast(class UWorld* World, class AActor* Caster, float MaxSeconds = 2.0f)
{
    // A fixture world that never had InitializeActorsForPlay called cannot be
    // ticked at all — doing so crashes inside the engine's tick group setup,
    // which is exactly how this guard was found. AreActorsInitialized is the
    // question that actually distinguishes them; HasBegunPlay is false in
    // every one of these fixtures and would silently disable the whole helper.
    if (!World || !World->AreActorsInitialized()) return;
    for (float T = 0.0f; T < MaxSeconds && BreakerAnyCastPending(Caster); T += .02f)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, .02f);
    }
}
