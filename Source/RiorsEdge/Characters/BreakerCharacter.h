#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "BreakerCharacter.generated.h"

class UAbilitySystemComponent;
class UBreakerAttributeSet;
class UBreakerInputConfig;
class UCameraComponent;
class UBreakerCharacterMovementComponent;
class UBreakerProgressionComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    ABreakerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const;
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const;
    UFUNCTION(BlueprintPure, Category="Movement") bool IsWallRiding() const;
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const;
    UFUNCTION(BlueprintPure, Category="Movement") UBreakerCharacterMovementComponent* GetBreakerMovement() const;
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryDash();
    UFUNCTION(BlueprintCallable, Category="Movement") void StartSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") void StopSlide();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> FirstPersonCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Abilities") TObjectPtr<UBreakerAttributeSet> Attributes;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Progression") TObjectPtr<UBreakerProgressionComponent> Progression;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UBreakerInputConfig> InputConfig;

    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnFireInput(bool bPressed);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnAimInput(bool bPressed);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnReloadInput();
    UFUNCTION(BlueprintImplementableEvent, Category="Movement") void OnDashPerformed();
    UFUNCTION(BlueprintImplementableEvent, Category="Movement") void OnSlideChanged(bool bNowSliding);

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void MoveForwardLegacy(float Value);
    void MoveRightLegacy(float Value);
    void TurnLegacy(float Value);
    void LookUpLegacy(float Value);
    void StartSprint();
    void StopSprint();
    void HandleDashInput();
    void HandleJumpInput();
    void StartFire();
    void StopFire();
    void StartAim();
    void StopAim();

};
