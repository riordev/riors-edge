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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") FBreakerDamageResult ReceiveDamage(const FBreakerDamageRequest& Request);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool SpendStamina(float Cost);
    // Block is a stance: hold to gain a frontal block chance, each blocked
    // hit spends stamina; the stance drops when stamina cannot cover it.
    UFUNCTION(BlueprintCallable, Category="Combat|Defense") void SetBlocking(bool bNewBlocking);
    // Dodge is an instant action: spend stamina for a short full-negation
    // window; a dodged hit refunds a little class resource.
    UFUNCTION(BlueprintCallable, Category="Combat|Defense") bool TryDodge();
    UFUNCTION(BlueprintPure, Category="Combat|Defense") bool IsBlocking() const { return bBlocking; }
    UFUNCTION(BlueprintPure, Category="Combat|Defense") bool IsDodgeInvulnerable() const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool SpendClassResource(float Cost);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void AddClassResource(float Amount);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void RestoreVitals();
    UFUNCTION(BlueprintPure, Category="Combat") bool IsDead() const;
    UFUNCTION(BlueprintPure, Category="Combat") float GetSecondsSinceDamage() const;

    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDamageReceived OnDamageReceived;
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDeathEvent OnDeath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina", meta=(ClampMin="0")) float StaminaRegenerationPerSecond = 20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina", meta=(ClampMin="0")) float StaminaRegenerationDelay = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float BlockChance = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float BlockMitigation = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0")) float BlockStaminaCostPerHit = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float DodgeChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0")) float DodgeStaminaCost = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0")) float DodgeWindowSeconds = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0")) float DodgeResourceRefund = 5.0f;

protected:
    UFUNCTION(Server, Reliable) void ServerSetBlocking(bool bNewBlocking);
    UFUNCTION(Server, Reliable) void ServerTryDodge();

private:
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY(Replicated) bool bBlocking = false;
    double DodgeWindowEndTime = -1000.0;
    float TimeSinceStaminaSpend = 1000.0f;
    bool bDeathBroadcast = false;
    double LastDamageTime = -1000.0;
};
