#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "BreakerAbility_Fracture.generated.h"

class ABreakerProjectileBase;

// C5 Fracture (Class-Kits §2.2, Ability-Implementation-Spec §5.5): 30 Mana, no
// cooldown. "Projectile that applies one status, cycling deterministically
// through the caster's available status types on each cast. The sequencing
// enabler; the cycle order is visible on the HUD."
//
// Two systems carry it and neither is Caster-specific: ABreakerProjectileBase
// (travel, replication, impact through the ordinary damage contract) and
// UBreakerStatusCycleComponent (what the next cast applies). Fracture itself is
// an aim solve, a payload and a cycle advance.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Fracture : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Fracture();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // MS7's upgrade applies TWO cycle positions per cast. Data, not a branch.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="1", ClampMax="4")) int32 CyclePositionsPerCast = 1;
    // Every value below is O2 PLACEHOLDER: Class-Kits gives Fracture a cost and
    // a behaviour and no numbers at all.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="0")) float ImpactDamage = 30.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="0")) float ProjectileSpeed = 4000.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="0")) float MuzzleForwardCm = 80.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture") TSubclassOf<ABreakerProjectileBase> ProjectileClass;
};
