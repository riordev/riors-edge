#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "BreakerEnemy.generated.h"

class UAbilitySystemComponent;
class UBreakerAttributeSet;
class UBreakerCombatComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class USphereComponent;
class UBoxComponent;

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerEnemy : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ABreakerEnemy();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="Enemy") void ConfigureEncounter(const FVector& NewLeashOrigin, float NewPatrolPhase);
    UFUNCTION(BlueprintPure, Category="Enemy") FString GetEnemyStateLabel() const;

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void HandleDeath();
    void RespawnEnemy();
    void PerformAttack(APawn* TargetPawn);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCapsuleComponent> BodyCollision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BodyVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> BodyHitBox;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> WeakPoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> WeakPointVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerCombatComponent> Combat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float DetectionRange = 2200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackRange = 260.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float MoveSpeed = 330.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackDamage = 14.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float AttackCooldown = 1.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy", meta=(ClampMin="0")) float RespawnDelay = 3.0f;

private:
    FVector LeashOrigin = FVector::ZeroVector;
    float PatrolPhase = 0.0f;
    double LastAttackTime = -1000.0;
    bool bDead = false;
    FString StateLabel = TEXT("PATROL");
};
