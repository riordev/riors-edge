#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerStatusComponent.generated.h"

class UBreakerCombatComponent;

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerActiveStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FBreakerStatusApplicationSpec Spec;
    UPROPERTY(BlueprintReadOnly) EBreakerDamageFamily DamageFamily = EBreakerDamageFamily::Physical;
    UPROPERTY(BlueprintReadOnly) int32 Stacks = 1;
    UPROPERTY(BlueprintReadOnly) float RemainingDuration = 0.0f;
    UPROPERTY(BlueprintReadOnly) float TimeUntilNextTick = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 TicksDelivered = 0;
    // Who applied this status. Weak: a DoT outliving its applier keeps
    // ticking, it just stops crediting anyone.
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Instigator = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerStatusEvent, const FBreakerActiveStatus&, Status);

// Runs active damage-over-time statuses on the owning actor. Bleed and
// Poison are physical, bypass shields, and take half armour mitigation —
// all of that is already encoded in the damage request each tick builds.
// Ticks use the snapshot taken at application; the tick interval is part of
// that snapshot, consistent with the DoT contract.
UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerStatusComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerStatusComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Reapplying a status the target already has adds stacks (capped) and
    // refreshes duration, but keeps the ORIGINAL snapshot: an application
    // either critically ticks for its whole lifetime or never does.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
    // Instigator is remembered weakly so every tick this application produces
    // credits the applier through the attacker-side hit events.
    void ApplyStatus(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator);

    UFUNCTION(BlueprintPure, Category="Combat|Status") const TArray<FBreakerActiveStatus>& GetActiveStatuses() const { return ActiveStatuses; }
    UFUNCTION(BlueprintPure, Category="Combat|Status") bool HasStatus(FGameplayTag StatusTag) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status", meta=(ClampMin="1")) int32 MaximumStacksPerStatus = 10;
    UPROPERTY(BlueprintAssignable, Category="Combat|Status") FBreakerStatusEvent OnStatusApplied;
    UPROPERTY(BlueprintAssignable, Category="Combat|Status") FBreakerStatusEvent OnStatusExpired;

private:
    UPROPERTY() TArray<FBreakerActiveStatus> ActiveStatuses;
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> Combat;
};
