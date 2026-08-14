#include "Classes/BreakerMomentumComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "TimerManager.h"
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
        if (UBreakerProgressionComponent* Progression = Owner->FindComponentByClass<UBreakerProgressionComponent>())
        {
            CachedProgression = Progression;
            Progression->OnProgressionChanged.AddDynamic(this, &UBreakerMomentumComponent::HandleProgressionChanged);
        }
        if (UBreakerWeaponComponent* Weapon = Owner->FindComponentByClass<UBreakerWeaponComponent>())
        {
            Weapon->OnShot.AddDynamic(this, &UBreakerMomentumComponent::HandleShot);
        }
        if (UBreakerCombatComponent* Combat = Owner->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->OnDamageReceived.AddDynamic(this, &UBreakerMomentumComponent::HandleDamageReceived);
        }
    }
    HandleProgressionChanged();
    CachedState = StateForFraction(GetMomentumFraction());
}

void UBreakerMomentumComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    HandleProgressionChanged();
    CachedState = StateForFraction(GetMomentumFraction());
}

EBreakerMomentumState UBreakerMomentumComponent::StateForFraction(float Fraction)
{
    if (Fraction >= 2.0f / 3.0f) return EBreakerMomentumState::Redline;
    if (Fraction >= 1.0f / 3.0f) return EBreakerMomentumState::Running;
    return EBreakerMomentumState::Settled;
}

float UBreakerMomentumComponent::ComposeGenerationMultipliers(const TArray<float>& Multipliers)
{
    float Composed = 1.0f;
    for (const float Multiplier : Multipliers)
    {
        if (Multiplier > 0.0f) Composed *= Multiplier;
    }
    return Composed;
}

bool UBreakerMomentumComponent::IsLoopOverrideExpired(double ExpiryTime, double Now)
{
    return ExpiryTime >= 0.0 && ExpiryTime <= Now;
}

void UBreakerMomentumComponent::PushLoopOverride(FName Key, bool bSuspendDecay, float GenerationMultiplier, float Duration)
{
    if (Key.IsNone()) return;
    FLoopOverrideEntry Entry;
    Entry.bSuspendDecay = bSuspendDecay;
    Entry.GenerationMultiplier = GenerationMultiplier > 0.0f ? GenerationMultiplier : 1.0f;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    // Re-pushing the same key replaces rather than stacks: a re-cast refreshes.
    LoopOverrides.Add(Key, Entry);
}

void UBreakerMomentumComponent::PopLoopOverride(FName Key)
{
    LoopOverrides.Remove(Key);
}

void UBreakerMomentumComponent::PruneLoopOverrides() const
{
    const UWorld* World = GetWorld();
    if (!World || LoopOverrides.Num() == 0) return;
    const double Now = World->GetTimeSeconds();
    for (auto It = LoopOverrides.CreateIterator(); It; ++It)
    {
        if (IsLoopOverrideExpired(It.Value().ExpiryTime, Now)) It.RemoveCurrent();
    }
}

bool UBreakerMomentumComponent::IsDecaySuspended() const
{
    PruneLoopOverrides();
    for (const TPair<FName, FLoopOverrideEntry>& Pair : LoopOverrides)
    {
        if (Pair.Value.bSuspendDecay) return true;
    }
    return false;
}

float UBreakerMomentumComponent::GetGenerationMultiplier() const
{
    PruneLoopOverrides();
    TArray<float> Active;
    Active.Reserve(LoopOverrides.Num());
    for (const TPair<FName, FLoopOverrideEntry>& Pair : LoopOverrides)
    {
        Active.Add(Pair.Value.GenerationMultiplier);
    }
    return ComposeGenerationMultipliers(Active);
}

int32 UBreakerMomentumComponent::GetActiveLoopOverrideCount() const
{
    PruneLoopOverrides();
    return LoopOverrides.Num();
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

void UBreakerMomentumComponent::HandleProgressionChanged()
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    bIsSwift = Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::Swift;
    if (!bIsSwift) PendingGrants = 0.0f;
}

bool UBreakerMomentumComponent::IsActiveForOwner() const
{
    return bIsSwift;
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
    // ApplyClassResource, not the generated SetClassResource — the same reason
    // the Mana loop and UBreakerCombatComponent's resource helpers use it: the
    // generated setter ensure()s with no owning AbilitySystemComponent, so the
    // whole generation loop was unexercisable in automation and Momentum was
    // proven only by the pure StateForFraction/decay maths either side of this
    // write. The explicit [0, Max] clamp stays: Swift's floor is 0, so this is
    // bit-identical to what it replaced, and stating it here means a Momentum
    // component that somehow found itself on an Overcasting bank still cannot
    // generate into a debt it does not own.
    Attributes->ApplyClassResource(FMath::Clamp(Attributes->GetClassResource() + Delta, 0.0f, Max));
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

void UBreakerMomentumComponent::HandleDamageReceived(const FBreakerDamageResult& Result)
{
    // The passive evade proc, not a dodge input: the player cannot time this
    // source (Class-Kits 1.1, O1). Queued like every other one-shot grant so
    // it is spent against the same global generation budget.
    if (!Result.bDodged) return;
    // Phantom Step is a Core node, not a Swift one: it runs before the Swift
    // gate below and grants no Momentum of its own.
    TryPhantomStep();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastDodgeGrantTime < DodgeProcInterval) return;
    LastDodgeGrantTime = Now;
    PendingGrants += DodgeProcGrant;
}

void UBreakerMomentumComponent::TryPhantomStep()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World || !Owner->HasAuthority() || PhantomStepWindowSeconds <= 0.0f) return;
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    if (!Progression || !Progression->HasNodeTag(BreakerNodeTags::Node_PhantomStep.GetTag())) return;

    const double Now = World->GetTimeSeconds();
    if (Now - LastPhantomStepTime < PhantomStepCooldownSeconds) return;

    UBreakerCombatComponent* Combat = Owner->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat) return;
    LastPhantomStepTime = Now;

    // "Brief invulnerability" expressed through the only primitive Combat/
    // already owns: a guaranteed passive dodge for the window. The previous
    // value is restored on a timer, and the 2.0s internal cooldown is strictly
    // longer than the window, so the windows can never overlap and the saved
    // value can never be a value this function itself wrote.
    const float PreviousDodgeChance = Combat->DodgeChance;
    Combat->DodgeChance = 1.0f;
    TWeakObjectPtr<UBreakerCombatComponent> WeakCombat(Combat);
    FTimerHandle RestoreHandle;
    World->GetTimerManager().SetTimer(RestoreHandle, FTimerDelegate::CreateWeakLambda(this, [WeakCombat, PreviousDodgeChance]()
    {
        if (UBreakerCombatComponent* Restored = WeakCombat.Get()) Restored->DodgeChance = PreviousDodgeChance;
    }), PhantomStepWindowSeconds, false);
}

void UBreakerMomentumComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceLoop(DeltaTime);
}

void UBreakerMomentumComponent::AdvanceLoop(float DeltaTime)
{
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

    // An active loop override (Overdrive) multiplies both the generated rate
    // and the per-second cap, so doubling generation is not silently eaten by
    // the cap it was meant to raise.
    // FLAGGED: Class-Kits §1.2 quotes "cap raised to 40/s" for the 2x case,
    // where scaling the 25/s cap by the same 2x gives 50/s. Scaling is the
    // rule here because a second override must not need a second cap knob;
    // the exact ceiling is an O2 tuning value either way.
    const float GenerationMultiplier = GetGenerationMultiplier();
    const float EffectiveCap = FMath::Max(0.0f, GlobalGenerationCap * GenerationMultiplier);
    Rate *= GenerationMultiplier;

    // Global cap applies to rates and one-shot grants together; grants queue
    // rather than being discarded when the budget is already spent.
    float Budget = ClampGeneration(EffectiveCap, EffectiveCap) * DeltaTime;
    float Generated = FMath::Min(ClampGeneration(Rate, EffectiveCap) * DeltaTime, Budget);
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

    const bool bDecayBlocked = IsDecaySuspended() || bAirborne || bSliding || bWallRiding || Speed >= GroundThresholdSpeed;
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
