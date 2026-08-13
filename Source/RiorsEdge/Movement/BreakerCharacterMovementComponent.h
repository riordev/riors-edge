#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BreakerCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWallRideStateChanged, bool, bNowWallRiding);

UCLASS(ClassGroup=Movement, BlueprintType)
class RIORSEDGE_API UBreakerCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UBreakerCharacterMovementComponent();

    virtual float GetMaxSpeed() const override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category="Movement") void SetSprinting(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Movement") void SetSlideRequested(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryDash(const FVector& RequestedDirection);
    // Ability-Implementation-Spec §4.3: rotates existing horizontal velocity onto
    // Direction with NO magnitude gain. Skim's verb. It owns no cooldown of its
    // own — the ability pays cost and cooldown — and it never consumes the dash
    // charge. Returns false when there is not enough horizontal speed to
    // redirect (below walk speed), so a standing player cannot use it as a
    // free reposition.
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryRedirect(const FVector& Direction);
    // Tag/key-keyed temporary speed multiplier, composed multiplicatively with
    // the gear and tree multipliers (Ability-Implementation-Spec §6, the shape
    // Gunsmith's Disruptor and Swift's Overdrive both need). Lazily expired:
    // no timers, no tick cost when nothing is pushed.
    UFUNCTION(BlueprintCallable, Category="Movement") void PushSpeedMultiplier(FName Key, float Multiplier, float Duration);
    UFUNCTION(BlueprintCallable, Category="Movement") void PopSpeedMultiplier(FName Key);
    UFUNCTION(BlueprintPure, Category="Movement") float GetSpeedMultiplier() const;
    UFUNCTION(BlueprintCallable, Category="Movement") bool BeginSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") void PrepareSlideJump();
    UFUNCTION(BlueprintCallable, Category="Movement") void EndSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryWallJump();
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const { return bWantsToSprint; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const { return bSliding; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSlideRequested() const { return bSlideRequested; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsWallRiding() const { return bWallRiding; }
    UFUNCTION(BlueprintPure, Category="Movement") FVector GetWallRideNormal() const { return WallRideNormal; }
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const { return Velocity.Size2D(); }
    // World time of the last consumed dash charge; class loops watch it to
    // credit a dash exactly once.
    UFUNCTION(BlueprintPure, Category="Movement") float GetLastDashTime() const { return static_cast<float>(LastDashTime); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float WalkSpeed = 700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float SprintSpeed = 1100.0f;

    // Forgiving Source-style air steering: turns existing momentum toward
    // input without increasing its magnitude or allowing a free reversal.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Air Movement", meta=(ClampMin="0")) float AirSteerRate = 4.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Air Movement", meta=(ClampMin="-1", ClampMax="1")) float AirSteerMinimumAlignment = -0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedFloor = 1500.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedBonus = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashVerticalFloor = 80.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashCooldown = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float MomentumHardCap = 4200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntrySpeed = 550.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntryBoost = 120.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntryBoostDuration = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideBoostCooldown = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideExitSpeed = 450.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideGroundFriction = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideBrakingDeceleration = 350.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideSlopeAcceleration = 900.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideMaxDuration = 1.0f;

    // Short wall ride: preserves traversal flow without generating speed or
    // replacing combat.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideMaxDuration = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideMinimumSpeed = 700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideGravityScale = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideTraceDistance = 85.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpAwaySpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpUpSpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideCooldown = 0.3f;

    UPROPERTY(BlueprintAssignable, Category="Movement|Wall Ride") FWallRideStateChanged OnWallRideStateChanged;

    // Pure rules, exposed for world-free tests.
    // A redirect is legal only when there is real horizontal speed to turn.
    static bool CanRedirect(float HorizontalSpeed, float MinimumSpeed);
    // Rotates HorizontalVelocity onto Direction. The output magnitude is the
    // input magnitude, floored at MinimumSpeed so a redirect never dead-stops,
    // and never above the input magnitude (Master 5.4: no self-acceleration).
    static FVector RedirectHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float MinimumSpeed);
    // Multiplicative composition of the active temporary multipliers.
    static float ComposeSpeedMultipliers(const TArray<float>& Multipliers);

private:
    struct FSpeedMultiplierEntry
    {
        float Multiplier = 1.0f;
        // Negative = no expiry; popped explicitly.
        double ExpiryTime = -1.0;
    };
    // Mutable: GetMaxSpeed() is const and is the natural place to drop expired
    // entries, which is what "lazy expiry" means here.
    mutable TMap<FName, FSpeedMultiplierEntry> SpeedMultipliers;
    void PruneSpeedMultipliers() const;

    // Gear-rolled movement multipliers, read from the owner's equipment
    // component: move/slide speed, air control, dash cooldown.
    class UBreakerEquipmentComponent* GetEquipment() const;
    float GearMoveSpeedMultiplier() const;
    float GearSlideSpeedMultiplier() const;
    float GearAirControlMultiplier() const;
    float GearDashCooldownMultiplier() const;
    mutable TWeakObjectPtr<class UBreakerEquipmentComponent> CachedEquipment;
    // Tree-node movement multipliers compose multiplicatively with gear.
    class UBreakerProgressionComponent* GetProgression() const;
    mutable TWeakObjectPtr<class UBreakerProgressionComponent> CachedProgression;

    bool bWantsToSprint = false;
    bool bSlideRequested = false;
    bool bSlideRequestConsumed = false;
    bool bSliding = false;
    double LastDashTime = -1000.0;
    double LastSlideBoostTime = -1000.0;
    float BoostedSpeedCeiling = 0.0f;
    float SavedGroundFriction = 0.0f;
    float SavedBrakingDeceleration = 0.0f;
    float SlideElapsed = 0.0f;
    float SlideEntryBoostRemaining = 0.0f;
    bool bWallRiding = false;
    FVector WallRideNormal = FVector::ZeroVector;
    float WallRideElapsed = 0.0f;
    float SavedGravityScale = 1.0f;
    double LastWallRideEndTime = -1000.0;

    bool FindRunnableWall(FHitResult& OutHit) const;
    void ApplyAirSteering(float DeltaTime);
    void BeginWallRide(const FHitResult& WallHit);
    void EndWallRide();
};
