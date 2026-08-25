#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/BreakerCombatTypes.h"
#include "GameFramework/Actor.h"
#include "BreakerTargetDummy.generated.h"

class UAbilitySystemComponent;
class UBreakerAttributeSet;
class UBreakerCombatComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class USphereComponent;
class UBoxComponent;

UENUM(BlueprintType)
enum class EBreakerTargetProfile : uint8
{
    Health,
    Shielded,
    Armored,
    Moving
};

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerTargetDummy : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ABreakerTargetDummy();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UFUNCTION(BlueprintCallable, Category="Playtest") void ConfigureProfile(EBreakerTargetProfile NewProfile);
    UFUNCTION(BlueprintPure, Category="Playtest") EBreakerTargetProfile GetProfile() const { return Profile; }
    UFUNCTION(BlueprintPure, Category="Playtest") FString GetProfileLabel() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION() void HandleDeath();
    UFUNCTION() void RespawnDummy();
    // The dummy ANSWERS hits now (ruled): the same reaction component the
    // enemies carry — flash, death pop and crumple — without inheriting an
    // enemy, because a dummy with AI stops being a dummy.
    UFUNCTION() void HandleDamageReceived(const FBreakerDamageResult& Result);
    void HandleDeathBeatFinished();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCapsuleComponent> BodyCollision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BodyVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> BodyHitBox;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> WeakPoint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> WeakPointVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UBreakerHitReactionComponent> HitReaction;
    bool bLastHitWasWeakPoint = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playtest", meta=(ClampMin="0.1")) float RespawnDelay = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playtest") EBreakerTargetProfile Profile = EBreakerTargetProfile::Health;
    FVector MotionOrigin = FVector::ZeroVector;
    float MotionPhase = 0.0f;
};
