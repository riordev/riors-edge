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
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryDash(const FVector& RequestedDirection);
    UFUNCTION(BlueprintCallable, Category="Movement") bool BeginSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") void EndSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryWallJump();
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const { return bWantsToSprint; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const { return bSliding; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsWallRiding() const { return bWallRiding; }
    UFUNCTION(BlueprintPure, Category="Movement") FVector GetWallRideNormal() const { return WallRideNormal; }
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const { return Velocity.Size2D(); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float WalkSpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float SprintSpeed = 950.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedFloor = 1250.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedBonus = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashVerticalFloor = 80.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashCooldown = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntrySpeed = 750.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntryBoost = 120.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideExitSpeed = 450.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideGroundFriction = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideBrakingDeceleration = 350.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideSlopeAcceleration = 900.0f;

    // Reserved for the short, grounded wall-ride implementation. It will
    // preserve traversal flow without generating speed or replacing combat.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideMaxDuration = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideMinimumSpeed = 700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideGravityScale = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideTraceDistance = 85.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpAwaySpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpUpSpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideCooldown = 0.3f;

    UPROPERTY(BlueprintAssignable, Category="Movement|Wall Ride") FWallRideStateChanged OnWallRideStateChanged;

private:
    bool bWantsToSprint = false;
    bool bSliding = false;
    double LastDashTime = -1000.0;
    float SavedGroundFriction = 0.0f;
    float SavedBrakingDeceleration = 0.0f;
    bool bWallRiding = false;
    FVector WallRideNormal = FVector::ZeroVector;
    float WallRideElapsed = 0.0f;
    float SavedGravityScale = 1.0f;
    double LastWallRideEndTime = -1000.0;

    bool FindRunnableWall(FHitResult& OutHit) const;
    void BeginWallRide(const FHitResult& WallHit);
    void EndWallRide();
};
