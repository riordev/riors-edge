#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "BreakerAbility_Skim.generated.h"

// S3 Skim (Class-Kits §1.2). The proof ability for the Phase 0 chain: it
// grants from the loadout, binds to a slot, spends Momentum, starts a cooldown,
// and moves the character.
//
// DEVIATION: the design calls for a pure redirect with no speed gain, which
// needs UBreakerCharacterMovementComponent::TryRedirect (spec §4.3 MISSING
// HOOK). The movement component is owned elsewhere this pass, so Skim routes
// through the existing public TryDash. That means it currently carries the
// dash speed floor/bonus and its short boosted-speed window, which is wrong
// for Skim's design. Swap to TryRedirect when that hook lands.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Skim : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Skim();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Pure rule, exposed for tests: the horizontal impulse direction for a view
    // rotation. Returns a normalized, strictly horizontal vector; falls back to
    // world forward when the view is exactly vertical.
    static FVector HorizontalDirectionForView(const FRotator& ViewRotation);
};
