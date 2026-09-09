#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerAbility_Fracture.generated.h"

class ABreakerProjectileBase;
class UBreakerCombatComponent;
class UBreakerStatusCycleComponent;

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

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="0")) float BaseCastSeconds {}; // O246: authored in Data/abilities.json. // O2 PLACEHOLDER; authored in Data/abilities.json

    // MS7's upgrade applies TWO cycle positions per cast. Data, not a branch.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="1", ClampMax="4")) int32 CyclePositionsPerCast {}; // O246: authored in Data/abilities.json.
    // Every value below is O2 PLACEHOLDER: Class-Kits gives Fracture a cost and
    // a behaviour and no numbers at all. ImpactDamage is the item-level-1
    // number and rides the equipped weapon's item-level scalar (O35).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="0")) float ImpactDamage {}; // O246: authored in Data/abilities.json.   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="0")) float ProjectileSpeed {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture", meta=(ClampMin="0")) float MuzzleForwardCm {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fracture") TSubclassOf<ABreakerProjectileBase> ProjectileClass;
private:
    void CompleteCast();
    void ClearPendingCast();
    UFUNCTION() void CancelPendingCast();
    FTimerHandle CastTimer;
    uint64 CastGeneration = 0;
    TWeakObjectPtr<UBreakerCombatComponent> CastCombat;
    TWeakObjectPtr<UBreakerStatusCycleComponent> CastCycle;
    UPROPERTY() FBreakerDamageRequest PendingDamage;
    UPROPERTY() TArray<FBreakerCarriedStatus> PendingStatuses;
    UPROPERTY() TSubclassOf<ABreakerProjectileBase> PendingProjectileClass;
    FVector PendingMuzzle = FVector::ZeroVector;
    FVector PendingDirection = FVector::ForwardVector;
    FLinearColor PendingColor = FLinearColor::White;
    float PendingSpeed = 0;
    int32 PendingPositions = 0;
    bool bPendingAdvanceOnHit = false;
    bool bHasPendingColor = false;
};
