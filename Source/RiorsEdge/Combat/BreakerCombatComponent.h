#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCombatComponent.generated.h"

class UBreakerAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerDamageReceived, const FBreakerDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerDeathEvent);

UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerCombatComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") FBreakerDamageResult ReceiveDamage(const FBreakerDamageRequest& Request);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool SpendStamina(float Cost);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool SpendClassResource(float Cost);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void AddClassResource(float Amount);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void RestoreVitals();
    UFUNCTION(BlueprintPure, Category="Combat") bool IsDead() const;
    UFUNCTION(BlueprintPure, Category="Combat") float GetSecondsSinceDamage() const;

    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDamageReceived OnDamageReceived;
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDeathEvent OnDeath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina", meta=(ClampMin="0")) float StaminaRegenerationPerSecond = 20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina", meta=(ClampMin="0")) float StaminaRegenerationDelay = 1.2f;

private:
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    float TimeSinceStaminaSpend = 1000.0f;
    bool bDeathBroadcast = false;
    double LastDamageTime = -1000.0;
};
