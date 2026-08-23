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
            // Dry Fire (Class-Kits §1.3 F5) rides the magazine-economy event
            // the weapon already broadcasts for Scrap: the last round LEAVING
            // the magazine, never the reload.
            Weapon->OnMagazineEmptied.AddDynamic(this, &UBreakerMomentumComponent::HandleMagazineEmptied);
        }
        if (UBreakerCombatComponent* Combat = Owner->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->OnDamageReceived.AddDynamic(this, &UBreakerMomentumComponent::HandleDamageReceived);
            // Feed (Class-Kits §1.3 F6) rides the attacker-side kill event.
            Combat->OnKillDealt.AddDynamic(this, &UBreakerMomentumComponent::HandleKillDealt);
        }
    }
    HandleProgressionChanged();
    CachedState = StateForFraction(GetMomentumFraction());
    // Baseline the spend observer at whatever the bar holds now, so the first
    // observation after BeginPlay cannot misread starting resource as a spend.
    LastKnownResource = GetMomentum();
}

void UBreakerMomentumComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    HandleProgressionChanged();
    CachedState = StateForFraction(GetMomentumFraction());
    LastKnownResource = GetMomentum();
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

void UBreakerMomentumComponent::PushLoopOverride(FName Key, bool bSuspendDecay, float GenerationMultiplier, float Duration, float DecayRateMultiplier)
{
    if (Key.IsNone()) return;
    FLoopOverrideEntry Entry;
    Entry.bSuspendDecay = bSuspendDecay;
    // ZERO IS A LEGAL MULTIPLIER AND USED TO BE SILENTLY DISCARDED. The guard
    // read `> 0.0f`, so a caller asking for "generation stops entirely" was
    // handed 1.0 — full normal generation — with no warning: the exact opposite
    // of what it requested. Standing Wave (Class-Kits M12) is the first caller
    // to ask for it, and its "no gain, no loss" freeze would have shipped as
    // "no loss, normal gain" and read as the keystone simply not working. Only
    // a NEGATIVE multiplier is meaningless, and that one is now loud.
    if (GenerationMultiplier < 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("PushLoopOverride('%s'): negative generation multiplier %.3f is meaningless; falling back to 1.0."),
            *Key.ToString(), GenerationMultiplier);
    }
    Entry.GenerationMultiplier = GenerationMultiplier >= 0.0f ? GenerationMultiplier : 1.0f;
    // Same rule for the decay lane: zero is a legal, meaningful suspension
    // (Reserve's while-ADS line composes to exactly 0), only negative is
    // nonsense, and it is loud rather than silently promoted.
    if (DecayRateMultiplier < 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("PushLoopOverride('%s'): negative decay-rate multiplier %.3f is meaningless; falling back to 1.0."),
            *Key.ToString(), DecayRateMultiplier);
    }
    Entry.DecayRateMultiplier = DecayRateMultiplier >= 0.0f ? DecayRateMultiplier : 1.0f;
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

float UBreakerMomentumComponent::GetDecayRateMultiplier() const
{
    PruneLoopOverrides();
    TArray<float> Active;
    Active.Reserve(LoopOverrides.Num());
    for (const TPair<FName, FLoopOverrideEntry>& Pair : LoopOverrides)
    {
        Active.Add(Pair.Value.DecayRateMultiplier);
    }
    // Composes multiplicatively like the generation stack — with the one
    // divergence that zero must SURVIVE the fold, because "decay stops" is a
    // real request (the ComposeGenerationMultipliers helper skips non-positive
    // entries and would quietly turn a suspension into full decay).
    float Composed = 1.0f;
    for (const float Multiplier : Active)
    {
        Composed *= FMath::Max(0.0f, Multiplier);
    }
    return Composed;
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

void UBreakerMomentumComponent::ObserveExternalSpend()
{
    const float Current = GetMomentum();
    // A drop between our own writes is an external spend: every write this
    // component makes re-baselines through here, and the one external writer
    // that DECREASES the class resource is the ability cost effect. External
    // increases just re-baseline.
    if (LastKnownResource >= 0.0f && Current < LastKnownResource - KINDA_SMALL_NUMBER)
    {
        LastObservedSpend = LastKnownResource - Current;
    }
    LastKnownResource = Current;
}

int32 UBreakerMomentumComponent::GetFrenzyNodeRank(FName NodeId) const
{
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    if (!Progression && GetOwner())
    {
        Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    return Progression ? Progression->GetNodeRank(NodeId, EBreakerPointCurrency::DoctrinePoints) : 0;
}

bool UBreakerMomentumComponent::WeakPointPostureSatisfied(bool bRequiresAirborneOrSlide, bool bAirborneOrSliding, int32 TriggerDisciplineRank)
{
    // Class-Kits §1.3 F1, transcribed: "Momentum generation from weak-point
    // hits no longer requires being airborne or sliding."
    return !bRequiresAirborneOrSlide || bAirborneOrSliding || TriggerDisciplineRank > 0;
}

float UBreakerMomentumComponent::WeakPointIntervalForRank(float BaseInterval, int32 TriggerDisciplineRank)
{
    // §1.3 F1 R2, transcribed: internal cooldown 0.25s -> 0.15s. The base
    // interval is the component's own authored knob, so ranks 0-1 keep
    // whatever it says even if it is retuned away from 0.25.
    return TriggerDisciplineRank >= 2 ? 0.15f : BaseInterval;
}

int32 UBreakerMomentumComponent::RhythmStride(int32 RhythmRank)
{
    if (RhythmRank <= 0) return 0;
    return RhythmRank >= 2 ? 4 : 5;   // §1.3 F4 R1/R2
}

float UBreakerMomentumComponent::FeedRefundFraction(int32 FeedRank)
{
    if (FeedRank <= 0) return 0.0f;
    return FeedRank >= 2 ? 0.20f : 0.10f;   // §1.3 F6 R1/R2
}

FName UBreakerMomentumComponent::MomentumShieldModifierKey()
{
    return TEXT("MomentumShield");
}

float UBreakerMomentumComponent::MomentumShieldIncomingMultiplier(bool bNodeOwned, EBreakerMomentumState State, bool bGrounded, float ReductionFraction)
{
    // Class-Kits §1.4 K9, transcribed: "While at Redline, incoming damage is
    // reduced ... even when grounded." Without the node, off Redline, or
    // airborne (the affix's own posture, not this node's), the chain sees
    // exactly nothing — the bit-identity half of the rule.
    if (!bNodeOwned || State != EBreakerMomentumState::Redline || !bGrounded)
    {
        return 1.0f;
    }
    return 1.0f - FMath::Clamp(ReductionFraction, 0.0f, 1.0f);
}

void UBreakerMomentumComponent::UpdateMomentumShield(bool bGrounded)
{
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    if (!Progression && GetOwner())
    {
        Progression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const bool bNodeOwned = bIsSwift && Progression && Progression->HasNodeTag(BreakerNodeTags::Node_MomentumShield.GetTag());
    const float Multiplier = MomentumShieldIncomingMultiplier(bNodeOwned, CachedState, bGrounded, MomentumShieldReductionFraction);
    const bool bWantPush = Multiplier < 1.0f;
    if (bWantPush == bMomentumShieldPushed)
    {
        return;
    }
    UBreakerCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!Combat)
    {
        return;
    }
    if (bWantPush)
    {
        Combat->PushIncomingDamageModifier(MomentumShieldModifierKey(), Multiplier);
    }
    else
    {
        Combat->RemoveIncomingDamageModifier(MomentumShieldModifierKey());
    }
    bMomentumShieldPushed = bWantPush;
}

void UBreakerMomentumComponent::ApplyMomentumDelta(float Delta)
{
    if (!Attributes || FMath::IsNearlyZero(Delta)) return;
    // Observe BEFORE writing: our own write must not mask a spend that
    // happened since the last one.
    ObserveExternalSpend();
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
    // Re-baseline so this write never reads back as a spend.
    LastKnownResource = Attributes->GetClassResource();
}

void UBreakerMomentumComponent::GrantMomentum(float Amount)
{
    // Same guard shape as UBreakerManaComponent::GrantMana: non-positive
    // amounts are a no-op, server authority only, and inert for any owner
    // this loop is not currently running for (a non-Swift owner must not gain
    // Momentum). Unlike Mana this has no generation-suspension concept to
    // check — Momentum's decay/generation suspension lives in LoopOverrides
    // and this grant, like Mana's bIgnoreGlobalCap path, is deliberately
    // outside that loop's metering.
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner()) return;
    ApplyMomentumDelta(Amount);
    RefreshState();
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
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;

    // Rhythm (Class-Kits §1.3 F4, LIVE, transcribed): "Every 5th consecutive
    // hit on any target generates +8 Momentum, ignoring the global per-second
    // cap. R2: every 4th. Missing resets the counter." Any-target, so this is
    // the component's own counter rather than the ability state's per-target
    // streak; "ignoring the cap" is GrantMomentum's direct-credit path rather
    // than the metered PendingGrants queue. Hitscan only — a rocket's shot
    // record carries no pellets and is neither a hit nor a miss here.
    if (Shot.GetPelletCount() > 0)
    {
        if (Shot.bHit)
        {
            ++ConsecutiveHits;
            const int32 Stride = RhythmStride(GetFrenzyNodeRank(TEXT("Swift.Frenzy.Rhythm")));
            if (Stride > 0 && ConsecutiveHits % Stride == 0)
            {
                GrantMomentum(8.0f);   // §1.3 F4: +8, outside the cap
            }
        }
        else
        {
            ConsecutiveHits = 0;
        }
    }

    if (!Shot.bWeakPoint) return;
    // Trigger Discipline (Class-Kits §1.3 F1, LIVE, transcribed): weak-point
    // generation no longer requires being airborne or sliding, and R2 runs
    // the internal cooldown at 0.15s instead of the authored 0.25s.
    const int32 TriggerRank = GetFrenzyNodeRank(TEXT("Swift.Frenzy.TriggerDiscipline"));
    const UBreakerCharacterMovementComponent* Movement = GetBreakerMovement();
    const bool bAirborneOrSliding = Movement && (Movement->IsFalling() || Movement->IsSliding());
    if (!WeakPointPostureSatisfied(bWeakPointRequiresAirborneOrSlide, bAirborneOrSliding, TriggerRank)) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastWeakPointGrantTime < WeakPointIntervalForRank(WeakPointInterval, TriggerRank)) return;
    LastWeakPointGrantTime = Now;
    PendingGrants += WeakPointGrant;
}

void UBreakerMomentumComponent::HandleMagazineEmptied(bool bStartedFull)
{
    // Dry Fire (Class-Kits §1.3 F5, LIVE, transcribed): "Firing the last
    // round in a magazine generates +12 Momentum." The event already fires on
    // the last round LEAVING the magazine and never on the reload, and a
    // reload always fills the whole magazine, so emptying one costs a full
    // magazine of ammunition — the node cannot be farmed faster than the
    // player can shoot. bStartedFull is Scrap's dump clause, not this one:
    // F5 rewards emptying rather than tapping, wherever the magazine started.
    // R2's second half — "also refunds 1s of ability cooldown" — has no seam
    // here (cooldowns live on the ability system); it stays WAITING, recorded
    // on the node in BreakerProgressionLibrary.cpp.
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    if (GetFrenzyNodeRank(TEXT("Swift.Frenzy.DryFire")) <= 0) return;
    GrantMomentum(12.0f);   // §1.3 F5: +12
}

void UBreakerMomentumComponent::HandleKillDealt(const FBreakerHitContext& Hit)
{
    // Feed (Class-Kits §1.3 F6, LIVE, transcribed): "Kills refund Momentum
    // equal to 10% of the ability cost most recently paid (R2: 20%). Ties the
    // loop to the kill without a flat Resource on Kill duplicate." The cost
    // most recently paid is whatever the spend observer last saw leave the
    // bar; a Swift who has cast nothing yet gets nothing, which is the node's
    // own design (the refund is OF a cost).
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    // A spend this frame may not have been observed yet — look before paying.
    ObserveExternalSpend();
    const float Fraction = FeedRefundFraction(GetFrenzyNodeRank(TEXT("Swift.Frenzy.Feed")));
    if (Fraction <= 0.0f || LastObservedSpend <= 0.0f) return;
    GrantMomentum(LastObservedSpend * Fraction);
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
    // One deterministic spend observation per tick, so Feed's "cost most
    // recently paid" is current even on frames where this loop writes nothing.
    ObserveExternalSpend();
    if (!IsActiveForOwner())
    {
        PendingGrants = 0.0f;
        bHasLastLocation = false;
        // A non-Swift owner holds no shield entry: tear down anything a class
        // swap left behind (bGrounded=false composes to "remove").
        UpdateMomentumShield(false);
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

    // K9 Momentum Shield rides the loop tick because this is the one place
    // that knows both the band and the posture. CachedState is last tick's
    // refresh at worst — a one-frame edge on a defensive window, the same
    // tolerance every band-conditioned read in this file accepts.
    UpdateMomentumShield(!bAirborne);

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
        // The loop valve's decay lane: the tree's composed ClassResourceDecay
        // multiplier (and any ability-pushed override) scales the rate here,
        // at the one place decay is actually paid.
        ApplyMomentumDelta(-DecayRateForSpeed(Speed, SettledSpeed, GroundThresholdSpeed, SettledDecayRate, SlowDecayRate)
            * GetDecayRateMultiplier() * DeltaTime);
    }
    RefreshState();
}
