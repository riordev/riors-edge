#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "BreakerAbility_Skim.generated.h"

// S3 Skim (Class-Kits §1.2). The proof ability for the Phase 0 chain: it
// grants from the loadout, binds to a slot, spends Momentum, starts a cooldown,
// and moves the character.
//
// Skim routes through UBreakerCharacterMovementComponent::TryRedirect: a pure
// rotation of existing horizontal velocity, no speed floor, no bonus, and no
// dash charge consumed (Class-Kits S3, "Not a dash ... It redirects").
//
// GAP: "usable airborne once per airtime" is not enforced yet — it needs the
// spec's ConsumeAirborneAction counter on the movement component, which K7
// Skim Discipline also raises to 2. Cost and cooldown bound the ability in the
// meantime.
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
