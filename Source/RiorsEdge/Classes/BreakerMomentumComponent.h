#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerMomentumComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerCharacterMovementComponent;
class UBreakerProgressionComponent;

UENUM(BlueprintType)
enum class EBreakerMomentumState : uint8
{
    Settled,
    Running,
    Redline
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerMomentumStateChanged, EBreakerMomentumState, NewState);

// Swift's Momentum loop: purposeful movement fills the class resource and
// inaction drains it. Server-authority only; inert unless the owner's
// permanent class is Swift.
UCLASS(ClassGroup=Classes, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerMomentumComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerMomentumComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintPure, Category="Momentum") EBreakerMomentumState GetMomentumState() const { return CachedState; }
    UFUNCTION(BlueprintPure, Category="Momentum") bool IsActiveForOwner() const;
    UFUNCTION(BlueprintPure, Category="Momentum") float GetMomentum() const;
    UFUNCTION(BlueprintPure, Category="Momentum") float GetMomentumFraction() const;

    // Pure loop rules, exposed for tests and for the eventual DA_MomentumPolicy
    // asset that will own these numbers.
    static EBreakerMomentumState StateForFraction(float Fraction);
    static float GroundSpeedRate(float Speed, float ThresholdSpeed, float UpperSpeed, float RateAtThreshold, float RateAtUpper);
    static float ClampGeneration(float RequestedRate, float GlobalCap);
    static float DecayRateForSpeed(float Speed, float SettledSpeed, float ThresholdSpeed, float SettledDecay, float SlowDecay);

    UPROPERTY(BlueprintAssignable, Category="Momentum") FBreakerMomentumStateChanged OnMomentumStateChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundThresholdSpeed = 750.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundUpperSpeed = 1250.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundRateAtThreshold = 6.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundRateAtUpperSpeed = 10.0f;
    // Anti-farm: running into a wall must generate nothing, so ground credit
    // requires 3.0 m of world-space displacement per second.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GroundDisplacementPerSecond = 300.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float AirborneRate = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float AirborneCreditSeconds = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float SlideRate = 12.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WallRideRate = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WallRideCreditSeconds = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DashGrant = 10.0f;
    // Floor only: the real internal cooldown is the movement component's dash
    // cooldown, so refunded charges cannot be farmed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float DashGrantMinimumInterval = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WeakPointGrant = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float WeakPointInterval = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation") bool bWeakPointRequiresAirborneOrSlide = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Generation", meta=(ClampMin="0")) float GlobalGenerationCap = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float SettledSpeed = 400.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float SettledDecayRate = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float SlowDecayRate = 6.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Momentum|Decay", meta=(ClampMin="0")) float DecayGraceSeconds = 1.0f;

private:
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);

    UBreakerCharacterMovementComponent* GetBreakerMovement() const;
    bool IsInSafeZone() const;
    void ApplyMomentumDelta(float Delta);
    void RefreshState();

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    mutable TWeakObjectPtr<UBreakerCharacterMovementComponent> CachedMovement;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    EBreakerMomentumState CachedState = EBreakerMomentumState::Settled;
    FVector LastLocation = FVector::ZeroVector;
    bool bHasLastLocation = false;
    float AirborneCreditRemaining = 0.0f;
    float WallRideCreditRemaining = 0.0f;
    float SettledElapsed = 0.0f;
    float PendingGrants = 0.0f;
    double LastDashGrantTime = -1000.0;
    double LastWeakPointGrantTime = -1000.0;
    double LastObservedDashTime = -1000.0;
};
