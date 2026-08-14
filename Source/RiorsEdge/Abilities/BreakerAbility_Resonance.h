#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "Combat/BreakerStatusConsumption.h"
#include "BreakerAbility_Resonance.generated.h"

// C6 Resonance (Class-Kits §2.2, Ability-Implementation-Spec §5.6): 40 Mana,
// no cooldown. "Detonates every status currently on the target for a burst of
// damage, consuming them. Damage scales with the NUMBER of distinct status
// types, not their stacks — an explicit anti-stacking rule."
//
// This is the ability that makes status a build axis rather than a
// damage-over-time footnote, and it is the reason UBreakerStatusComponent grew
// a consumption verb at all. All the arithmetic lives in
// UBreakerStatusConsumption so the §2.7.5 bound (6 statuses is at most 2.2x
// 2 statuses) is provable with no world and no actor.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Resonance : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Resonance();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance") FBreakerDetonationParams Detonation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance") EBreakerDetonationCurve Curve = EBreakerDetonationCurve::Linear;
    // MS8's rewrite: do not consume, halve the remaining durations instead.
    // A data field rather than a branch, so the node flips one value.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance") bool bConsumeStatuses = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0", ClampMax="1")) float DurationScalarWhenNotConsuming = 0.5f;
    // MS5 Payment: Mana per distinct status consumed. Zero at base.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0")) float RefundManaPerStatus = 0.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0")) float MaximumRangeCm = 4000.0f;
};
