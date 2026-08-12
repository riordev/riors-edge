#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerCharacter.generated.h"

class UAbilitySystemComponent;
class UBreakerAttributeSet;
class UBreakerInputConfig;
class UCameraComponent;
class UBreakerCharacterMovementComponent;
class UBreakerProgressionComponent;
class UBreakerCombatComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UBreakerPlaytestComponent;
class SBreakerMenu;
struct FInputActionValue;

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    ABreakerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const;
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const;
    UFUNCTION(BlueprintPure, Category="Movement") bool IsWallRiding() const;
    UFUNCTION(BlueprintPure, Category="Movement") bool IsMantling() const { return bMantling; }
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const;
    UFUNCTION(BlueprintPure, Category="Movement") UBreakerCharacterMovementComponent* GetBreakerMovement() const;
    UFUNCTION(BlueprintPure, Category="Combat") UBreakerAttributeSet* GetAttributes() const { return Attributes; }
    UFUNCTION(BlueprintPure, Category="Combat") UBreakerCombatComponent* GetCombat() const { return Combat; }
    UFUNCTION(BlueprintPure, Category="Weapon") UBreakerWeaponComponent* GetWeapon() const { return Weapon; }
    UFUNCTION(BlueprintPure, Category="Playtest") UBreakerPlaytestComponent* GetPlaytest() const { return Playtest; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetLookSensitivity() const { return LookSensitivity; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetCurrentFOV() const;
    UFUNCTION(BlueprintPure, Category="UI") bool IsLookInverted() const { return bInvertLookY; }
    UFUNCTION(BlueprintPure, Category="UI") bool IsMenuOpen() const { return MenuWidget.IsValid(); }
    void ApplyMenuSettings(float NewSensitivity, float NewFOV, bool bNewInvertLookY);
    void ResumeFromMenu();
    void ReturnToTitleMenu();
    void QuitFromMenu();
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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UBreakerWeaponComponent> Weapon;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> PrototypeWeaponVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> PrototypeWeaponBarrel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UStaticMeshComponent> PrototypeWeaponSight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon") TObjectPtr<UPointLightComponent> PrototypeMuzzleFlash;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> LeftArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMeshComponent> RightArmVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Playtest") TObjectPtr<UBreakerPlaytestComponent> Playtest;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UBreakerInputConfig> InputConfig;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Mantle", meta=(ClampMin="0")) float MantleReach = 90.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Mantle", meta=(ClampMin="0")) float MantleMinimumHeight = 35.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Mantle", meta=(ClampMin="0")) float MantleMaximumHeight = 150.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement|Mantle", meta=(ClampMin="0.05")) float MantleDuration = 0.20f;

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
    void HandleDashInput();
    void HandleJumpInput();
    bool TryMantle();
    void StartFire();
    void StopFire();
    void StartAim();
    void StopAim();
    void HandleReloadInput();
    void EquipPrimaryWeapon();
    void EquipSecondaryWeapon();
    void ApplyWeaponPresentation();
    void ResetPlaytest();
    void CopyPlaytestReport();
    void TogglePlaytestDiagnostics();
    void IncreaseFOV();
    void DecreaseFOV();
    void IncreaseSensitivity();
    void DecreaseSensitivity();
    void SavePlaytestSettings() const;
    void TogglePauseMenu();
    void ShowInitialMenu();
    void OpenMenu(bool bInitialMenu);
    UFUNCTION() void HandleShotCosmetics(const FBreakerShotResult& Shot);
    UFUNCTION() void HandlePlayerDeath();
    void EndShotCosmetics();

    FTransform PlaytestSpawnTransform;
    float LookSensitivity = 1.0f;
    bool bInvertLookY = false;
    bool bShowingInitialMenu = false;
    TSharedPtr<SBreakerMenu> MenuWidget;
    FTimerHandle ShotCosmeticTimer;
    bool bMantling = false;
    float MantleElapsed = 0.0f;
    FVector MantleStart = FVector::ZeroVector;
    FVector MantleTarget = FVector::ZeroVector;
    FVector MantleExitVelocity = FVector::ZeroVector;

};
