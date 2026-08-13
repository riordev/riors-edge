#include "Classes/BreakerMomentumComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerMomentumComponent::UBreakerMomentumComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerMomentumComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
        {
            Attributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
        }
    }
    if (AActor* Owner = GetOwner())
    {
        CachedProgression = Owner->FindComponentByClass<UBreakerProgressionComponent>();
        if (UBreakerWeaponComponent* Weapon = Owner->FindComponentByClass<UBreakerWeaponComponent>())
        {
            Weapon->OnShot.AddDynamic(this, &UBreakerMomentumComponent::HandleShot);
        }
    }
    CachedState = StateForFraction(GetMomentumFraction());
}

EBreakerMomentumState UBreakerMomentumComponent::StateForFraction(float Fraction)
{
    if (Fraction >= 2.0f / 3.0f) return EBreakerMomentumState::Redline;
    if (Fraction >= 1.0f / 3.0f) return EBreakerMomentumState::Running;
    return EBreakerMomentumState::Settled;
}

float UBreakerMomentumComponent::GroundSpeedRate(float Speed, float ThresholdSpeed, float UpperSpeed, float RateAtThreshold, float RateAtUpper)
{
    if (Speed < ThresholdSpeed) return 0.0f;
    if (UpperSpeed <= ThresholdSpeed) return RateAtUpper;
    const float Alpha = FMath::Clamp((Speed - ThresholdSpeed) / (UpperSpeed - ThresholdSpeed), 0.0f, 1.0f);
    return FMath::Lerp(RateAtThreshold, RateAtUpper, Alpha);
}

float UBreakerMomentumComponent::ClampGeneration(float RequestedRate, float GlobalCap)
{
    return FMath::Clamp(RequestedRate, 0.0f, FMath::Max(0.0f, GlobalCap));
}

float UBreakerMomentumComponent::DecayRateForSpeed(float Speed, float SettledSpeed, float ThresholdSpeed, float SettledDecay, float SlowDecay)
{
    if (Speed < SettledSpeed) return SettledDecay;
    if (Speed < ThresholdSpeed) return SlowDecay;
    return 0.0f;
}

UBreakerCharacterMovementComponent* UBreakerMomentumComponent::GetBreakerMovement() const
{
    if (!CachedMovement.IsValid() && GetOwner())
    {
        CachedMovement = GetOwner()->FindComponentByClass<UBreakerCharacterMovementComponent>();
    }
    return CachedMovement.Get();
}

bool UBreakerMomentumComponent::IsActiveForOwner() const
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        const_cast<UBreakerMomentumComponent*>(this)->CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    return Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::Swift;
}

float UBreakerMomentumComponent::GetMomentum() const
{
    return Attributes ? Attributes->GetClassResource() : 0.0f;
}

float UBreakerMomentumComponent::GetMomentumFraction() const
{
    if (!Attributes) return 0.0f;
    const float Max = Attributes->GetMaxClassResource();
    return Max > 0.0f ? FMath::Clamp(Attributes->GetClassResource() / Max, 0.0f, 1.0f) : 0.0f;
}

bool UBreakerMomentumComponent::IsInSafeZone() const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !GetWorld()) return false;
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    return GameMode && GameMode->IsInSafeZone(Owner->GetActorLocation());
}

void UBreakerMomentumComponent::ApplyMomentumDelta(float Delta)
{
    if (!Attributes || FMath::IsNearlyZero(Delta)) return;
    const float Max = Attributes->GetMaxClassResource();
    Attributes->SetClassResource(FMath::Clamp(Attributes->GetClassResource() + Delta, 0.0f, Max));
}

void UBreakerMomentumComponent::RefreshState()
{
    const EBreakerMomentumState NewState = StateForFraction(GetMomentumFraction());
    if (NewState != CachedState)
    {
        CachedState = NewState;
        OnMomentumStateChanged.Broadcast(NewState);
    }
}

void UBreakerMomentumComponent::HandleShot(const FBreakerShotResult& Shot)
{
    if (!Shot.bWeakPoint || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    const UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    if (bWeakPointRequiresAirborneOrSlide && !(Movement && (Movement->IsFalling() || Movement->IsSliding()))) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastWeakPointGrantTime < WeakPointInterval) return;
    LastWeakPointGrantTime = Now;
    PendingGrants += WeakPointGrant;
}

void UBreakerMomentumComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !Attributes || DeltaTime <= 0.0f) return;
    if (!IsActiveForOwner())
    {
        PendingGrants = 0.0f;
        bHasLastLocation = false;
        return;
    }

    const UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    if (!Movement) return;

    const FVector Location = Owner->GetActorLocation();
    const float DisplacementRate = bHasLastLocation ? FVector::Dist2D(Location, LastLocation) / DeltaTime : 0.0f;
    LastLocation = Location;
    bHasLastLocation = true;

    const float Speed = Movement->GetHorizontalSpeed();
    const bool bAirborne = Movement->IsFalling();
    const bool bSliding = Movement->IsSliding();
    const bool bWallRiding = Movement->IsWallRiding();

    if (bAirborne) AirborneCreditRemaining = FMath::Max(0.0f, AirborneCreditRemaining - DeltaTime);
    else AirborneCreditRemaining = AirborneCreditSeconds;
    if (bWallRiding) WallRideCreditRemaining = FMath::Max(0.0f, WallRideCreditRemaining - DeltaTime);
    else WallRideCreditRemaining = WallRideCreditSeconds;

    // One-shot dash credit, gated by the movement component's own dash
    // cooldown so refunded charges cannot be farmed.
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float LastDashTime = Movement->GetLastDashTime();
    if (LastDashTime > LastObservedDashTime && Now - LastDashGrantTime >= FMath::Max(DashGrantMinimumInterval, Movement->DashCooldown))
    {
        LastObservedDashTime = LastDashTime;
        LastDashGrantTime = Now;
        PendingGrants += DashGrant;
    }
    else if (LastDashTime > LastObservedDashTime)
    {
        LastObservedDashTime = LastDashTime;
    }

    if (IsInSafeZone())
    {
        PendingGrants = 0.0f;
        RefreshState();
        return;
    }

    float Rate = 0.0f;
    if (!bAirborne && !bSliding && DisplacementRate >= GroundDisplacementPerSecond)
    {
        Rate += GroundSpeedRate(Speed, GroundThresholdSpeed, GroundUpperSpeed, GroundRateAtThreshold, GroundRateAtUpperSpeed);
    }
    if (bAirborne && AirborneCreditRemaining > 0.0f) Rate += AirborneRate;
    if (bSliding && Speed >= GroundThresholdSpeed) Rate += SlideRate;
    if (bWallRiding && WallRideCreditRemaining > 0.0f) Rate += WallRideRate;

    // Global cap applies to rates and one-shot grants together; grants queue
    // rather than being discarded when the budget is already spent.
    float Budget = ClampGeneration(GlobalGenerationCap, GlobalGenerationCap) * DeltaTime;
    float Generated = FMath::Min(ClampGeneration(Rate, GlobalGenerationCap) * DeltaTime, Budget);
    Budget -= Generated;
    if (PendingGrants > 0.0f && Budget > 0.0f)
    {
        const float Drawn = FMath::Min(PendingGrants, Budget);
        PendingGrants -= Drawn;
        Generated += Drawn;
    }

    if (Generated > 0.0f)
    {
        SettledElapsed = 0.0f;
        ApplyMomentumDelta(Generated);
        RefreshState();
        return;
    }

    const bool bDecayBlocked = bAirborne || bSliding || bWallRiding || Speed >= GroundThresholdSpeed;
    if (bDecayBlocked)
    {
        SettledElapsed = 0.0f;
        RefreshState();
        return;
    }

    SettledElapsed += DeltaTime;
    if (SettledElapsed >= DecayGraceSeconds)
    {
        ApplyMomentumDelta(-DecayRateForSpeed(Speed, SettledSpeed, GroundThresholdSpeed, SettledDecayRate, SlowDecayRate) * DeltaTime);
    }
    RefreshState();
}
