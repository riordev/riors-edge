#include "Classes/BreakerGritComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"

UBreakerGritComponent::UBreakerGritComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerGritComponent::BeginPlay()
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
            Progression->OnProgressionChanged.AddDynamic(this, &UBreakerGritComponent::HandleProgressionChanged);
        }
    }
    // DELIBERATELY NO COMBAT BINDING, and this is the one place this component
    // diverges from the Momentum loop's wiring rather than copying it. Momentum
    // binds UBreakerCombatComponent::OnDamageReceived and reads bDodged off the
    // result. Grit needs POST-MITIGATION health damage and shield damage as
    // SEPARATE quantities, and it needs to know whether the instigator was the
    // Tank themselves — the self-damage source is 25% rate, and `Instigator` on
    // the damage request does not exist yet (Class-Kits-Tank §6.0 ranks it as a
    // missing hook that three separate Tank systems block on). Binding
    // OnDamageReceived today would credit a rocket-jump at the full rate and
    // make rocket-jumping the cheapest Grit engine in the game — the exact
    // outcome §1.3 rule 2 exists to forbid. NotifyDamageTaken takes the split
    // explicitly instead, so the caller cannot get it wrong by omission.
    HandleProgressionChanged();
    CachedBand = BandForFraction(GetGritFraction());
}

void UBreakerGritComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    HandleProgressionChanged();
    CachedBand = BandForFraction(GetGritFraction());
}

EBreakerGritBand UBreakerGritComponent::BandForFraction(float Fraction)
{
    // Winded 0-33, Braced 34-66, IRONCLAD 67-100 — the same thirds Momentum's
    // Settled/Running/Redline uses, and read as FRACTIONS so a Maximum Resource
    // roll raises the IRONCLAD threshold proportionally instead of making
    // IRONCLAD easier to hold.
    if (Fraction >= 2.0f / 3.0f) return EBreakerGritBand::Ironclad;
    if (Fraction >= 1.0f / 3.0f) return EBreakerGritBand::Braced;
    return EBreakerGritBand::Winded;
}

float UBreakerGritComponent::ComposeGenerationMultipliers(const TArray<float>& Multipliers)
{
    float Composed = 1.0f;
    for (const float Multiplier : Multipliers)
    {
        if (Multiplier > 0.0f) Composed *= Multiplier;
    }
    return Composed;
}

bool UBreakerGritComponent::IsLoopOverrideExpired(double ExpiryTime, double Now)
{
    return ExpiryTime >= 0.0 && ExpiryTime <= Now;
}

float UBreakerGritComponent::ClampGeneration(float RequestedRate, float GlobalCap)
{
    return FMath::Clamp(RequestedRate, 0.0f, FMath::Max(0.0f, GlobalCap));
}

float UBreakerGritComponent::ClampToBank(float Value, float MaxGrit)
{
    return FMath::Clamp(Value, 0.0f, FMath::Max(0.0f, MaxGrit));
}

float UBreakerGritComponent::DamageTakenGeneration(float HealthDamage, float ShieldDamage, float MaxHealth,
    float GritPerHealthFraction, float HealthFractionPerGrit, float ShieldRateFraction, float ProcCoefficient)
{
    if (MaxHealth <= 0.0f || HealthFractionPerGrit <= 0.0f) return 0.0f;
    const float SafeHealth = FMath::Max(0.0f, HealthDamage);
    const float SafeShield = FMath::Max(0.0f, ShieldDamage);
    // The health fraction is the unit of account, not the raw damage: a Tank
    // with twice the health generates half as much Grit from the same hit,
    // which is what keeps the loop honest across the whole level band without
    // a level term anywhere in it.
    const float HealthUnits = (SafeHealth / MaxHealth) / HealthFractionPerGrit;
    const float ShieldUnits = (SafeShield / MaxHealth) / HealthFractionPerGrit * FMath::Clamp(ShieldRateFraction, 0.0f, 1.0f);
    return (HealthUnits + ShieldUnits) * FMath::Max(0.0f, GritPerHealthFraction) * FMath::Max(0.0f, ProcCoefficient);
}

float UBreakerGritComponent::SelfDamageScalar(float SelfDamageRate, bool bSelfInflicted)
{
    return bSelfInflicted ? FMath::Clamp(SelfDamageRate, 0.0f, 1.0f) : 1.0f;
}

float UBreakerGritComponent::ProximityGeneration(bool bEnemyNear, float ProximityRate)
{
    // There is deliberately no count parameter. The anti-pack-farm rule is not
    // enforced here, it is UNEXPRESSIBLE here, which is a stronger guarantee
    // than a clamp: no future caller can pass nine and be paid nine times.
    return bEnemyNear ? FMath::Max(0.0f, ProximityRate) : 0.0f;
}

bool UBreakerGritComponent::IsLapseOpen(float SecondsSinceContact, float LapseSeconds)
{
    return SecondsSinceContact < FMath::Max(0.0f, LapseSeconds);
}

float UBreakerGritComponent::DecayRate(bool bLapseOpen, bool bDecaySuspended, float DecayPerSecond)
{
    if (bLapseOpen || bDecaySuspended) return 0.0f;
    return FMath::Max(0.0f, DecayPerSecond);
}

bool UBreakerGritComponent::CanSpendFrom(float Grit, float Cost)
{
    if (Cost <= 0.0f) return true;
    return Grit >= Cost;
}

float UBreakerGritComponent::DrawFromBudget(float Requested, float& InOutBudgetRemaining)
{
    if (Requested <= 0.0f || InOutBudgetRemaining <= 0.0f) return 0.0f;
    const float Drawn = FMath::Min(Requested, InOutBudgetRemaining);
    InOutBudgetRemaining -= Drawn;
    return Drawn;
}

void UBreakerGritComponent::PushLoopOverride(FName Key, bool bSuspendDecay, float GenerationMultiplier, float Duration, float DecayRateMultiplier)
{
    if (Key.IsNone()) return;
    FLoopOverrideEntry Entry;
    Entry.bSuspendDecay = bSuspendDecay;
    if (GenerationMultiplier < 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("PushLoopOverride('%s'): negative generation multiplier %.3f is meaningless; falling back to 1.0."),
            *Key.ToString(), GenerationMultiplier);
    }
    // Zero is legal and must survive: see the same guard on the Momentum loop,
    // where promoting 0 to 1.0 shipped a keystone as its own opposite.
    Entry.GenerationMultiplier = GenerationMultiplier >= 0.0f ? GenerationMultiplier : 1.0f;
    // Decay lane, same rule: zero is a legal suspension, negative is loud.
    if (DecayRateMultiplier < 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("PushLoopOverride('%s'): negative decay-rate multiplier %.3f is meaningless; falling back to 1.0."),
            *Key.ToString(), DecayRateMultiplier);
    }
    Entry.DecayRateMultiplier = DecayRateMultiplier >= 0.0f ? DecayRateMultiplier : 1.0f;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    LoopOverrides.Add(Key, Entry);
}

void UBreakerGritComponent::PopLoopOverride(FName Key)
{
    LoopOverrides.Remove(Key);
}

void UBreakerGritComponent::PruneLoopOverrides() const
{
    const UWorld* World = GetWorld();
    if (!World || LoopOverrides.Num() == 0) return;
    const double Now = World->GetTimeSeconds();
    for (auto It = LoopOverrides.CreateIterator(); It; ++It)
    {
        if (IsLoopOverrideExpired(It.Value().ExpiryTime, Now)) It.RemoveCurrent();
    }
}

bool UBreakerGritComponent::IsDecaySuspended() const
{
    PruneLoopOverrides();
    for (const TPair<FName, FLoopOverrideEntry>& Pair : LoopOverrides)
    {
        if (Pair.Value.bSuspendDecay) return true;
    }
    return false;
}

float UBreakerGritComponent::GetGenerationMultiplier() const
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

float UBreakerGritComponent::GetDecayRateMultiplier() const
{
    PruneLoopOverrides();
    // Multiplicative like the generation stack, except zero must SURVIVE the
    // fold — "decay stops" is a real request, and ComposeGenerationMultipliers
    // skips non-positive entries by design.
    float Composed = 1.0f;
    for (const TPair<FName, FLoopOverrideEntry>& Pair : LoopOverrides)
    {
        Composed *= FMath::Max(0.0f, Pair.Value.DecayRateMultiplier);
    }
    return Composed;
}

int32 UBreakerGritComponent::GetActiveLoopOverrideCount() const
{
    PruneLoopOverrides();
    return LoopOverrides.Num();
}

void UBreakerGritComponent::HandleProgressionChanged()
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    bIsTank = Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::Tank;
    if (!bIsTank) PendingGrants = 0.0f;
}

bool UBreakerGritComponent::IsActiveForOwner() const
{
    return bIsTank;
}

float UBreakerGritComponent::GetGrit() const
{
    return Attributes ? Attributes->GetClassResource() : 0.0f;
}

float UBreakerGritComponent::GetGritFraction() const
{
    if (!Attributes) return 0.0f;
    const float Max = Attributes->GetMaxClassResource();
    return Max > 0.0f ? FMath::Clamp(Attributes->GetClassResource() / Max, 0.0f, 1.0f) : 0.0f;
}

bool UBreakerGritComponent::IsLapseWindowOpen() const
{
    return IsLapseOpen(SecondsSinceContact, LapseSeconds);
}

bool UBreakerGritComponent::IsInSafeZone() const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !GetWorld()) return false;
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    return GameMode && GameMode->IsInSafeZone(Owner->GetActorLocation());
}

void UBreakerGritComponent::ApplyGritDelta(float Delta)
{
    if (!Attributes || FMath::IsNearlyZero(Delta)) return;
    Attributes->ApplyClassResource(ClampToBank(Attributes->GetClassResource() + Delta, Attributes->GetMaxClassResource()));
}

void UBreakerGritComponent::RefreshBand()
{
    const EBreakerGritBand NewBand = BandForFraction(GetGritFraction());
    if (NewBand != CachedBand)
    {
        CachedBand = NewBand;
        OnGritBandChanged.Broadcast(NewBand);
    }
}

void UBreakerGritComponent::RefreshBudgets(float DeltaTime)
{
    // TOKEN BUCKETS, NOT PER-FRAME ALLOWANCES, and the distinction matters. The
    // caps in §1.2 are per SECOND, but the events they meter arrive between
    // ticks and in bursts — a boss slam and a block proc in the same frame is
    // the normal case, not the pathological one. A per-frame allowance would
    // make the effective cap depend on frame rate, which would be a balance bug
    // masquerading as a performance one. Each bucket therefore accrues at its
    // cap and holds at most one second's worth.
    DamageBudget = FMath::Min(DamageBudget + DamageRateCap * DeltaTime, DamageRateCap);
    SelfDamageBudget = FMath::Min(SelfDamageBudget + SelfDamageRateCap * DeltaTime, SelfDamageRateCap);
    BlockBudget = FMath::Min(BlockBudget + BlockRateCap * DeltaTime, BlockRateCap);
}

void UBreakerGritComponent::NotifyDamageTaken(float HealthDamage, float ShieldDamage, bool bSelfInflicted, float ProcCoefficient)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || !bInCombat || IsInSafeZone()) return;
    if (!Attributes) return;

    // Contact refreshes the lapse window whether or not any Grit is generated —
    // a hit that lands while the per-source budget is spent still means the Tank
    // is doing its job, and the bar must not start bleeding because the cap was
    // full.
    SecondsSinceContact = 0.0f;

    const float Raw = DamageTakenGeneration(HealthDamage, ShieldDamage, Attributes->GetMaxHealth(),
        GritPerHealthFraction, HealthFractionPerGrit, ShieldRateFraction, ProcCoefficient);
    const float Scaled = Raw * SelfDamageScalar(SelfDamageRate, bSelfInflicted);
    // Self-damage draws from its OWN, much smaller bucket as well as being
    // rate-scaled. Two independent guards on one source, because §1.3 rule 2
    // makes this the one source a player can produce at will.
    const float Drawn = bSelfInflicted
        ? DrawFromBudget(FMath::Min(Scaled, SelfDamageBudget), DamageBudget)
        : DrawFromBudget(Scaled, DamageBudget);
    if (bSelfInflicted) SelfDamageBudget = FMath::Max(0.0f, SelfDamageBudget - Drawn);
    PendingGrants += Drawn;
}

void UBreakerGritComponent::NotifyBlockProc()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || !bInCombat || IsInSafeZone()) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastBlockGrantTime < BlockProcInterval) return;
    LastBlockGrantTime = Now;
    // The internal cooldown ALREADY implies the per-source cap at the default
    // values. The cap is enforced anyway, and separately, so that a node
    // shortening the ICD cannot silently uncap the source — the guard is
    // against a future node, not against today's numbers.
    PendingGrants += DrawFromBudget(BlockProcGrant, BlockBudget);
}

void UBreakerGritComponent::NotifyMeleeKill()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || !bInCombat || IsInSafeZone()) return;
    // No ICD and no per-source bucket: kills are self-limiting, and this is the
    // aggression source that stops the loop being purely masochistic. It is
    // still bound by the global cap in AdvanceLoop.
    PendingGrants += FMath::Max(0.0f, MeleeKillGrant);
}

void UBreakerGritComponent::SetEnemyInProximity(bool bNowNear)
{
    bEnemyNear = bNowNear;
    // Proximity refreshes the lapse window on the same footing as damage: "an
    // enemy within 5 m in the last 6s" is half of the window's definition.
    if (bEnemyNear) SecondsSinceContact = 0.0f;
}

void UBreakerGritComponent::SetInCombat(bool bNowInCombat)
{
    if (bNowInCombat == bInCombat) return;
    const bool bWasInCombat = bInCombat;
    bInCombat = bNowInCombat;
    // ONCE PER COMBAT STATE, on the edge INTO combat only — the same
    // edge-triggered shape as UBreakerManaComponent's FillToMaximum, and for
    // the same reason: a grant on every refresh hands a player a free bar for
    // doing nothing. Deliberately outside the metered budget, because the entry
    // grant's whole job is to be there in the first frame.
    if (bInCombat && !bWasInCombat) GrantGrit(CombatEntryGrant);
    if (!bInCombat)
    {
        // Leaving combat arms the drain rather than snapping the bar to zero, so
        // a re-engage inside a few seconds is not punished.
        PendingGrants = 0.0f;
        SecondsSinceContact = LapseSeconds;
    }
}

void UBreakerGritComponent::NotifyDeath()
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner()) return;
    PendingGrants = 0.0f;
    SecondsSinceContact = LapseSeconds;
    Attributes->ApplyClassResource(0.0f);
    RefreshBand();
}

void UBreakerGritComponent::GrantGrit(float Amount)
{
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner()) return;
    ApplyGritDelta(Amount);
    RefreshBand();
}

bool UBreakerGritComponent::CanAffordSpend(float Cost) const
{
    return IsActiveForOwner() && CanSpendFrom(GetGrit(), Cost);
}

bool UBreakerGritComponent::TrySpendGrit(float Cost)
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (!CanAffordSpend(Cost)) return false;
    ApplyGritDelta(-Cost);
    RefreshBand();
    return true;
}

void UBreakerGritComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceLoop(DeltaTime);
}

void UBreakerGritComponent::AdvanceLoop(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !Attributes || DeltaTime <= 0.0f) return;
    if (!IsActiveForOwner())
    {
        PendingGrants = 0.0f;
        return;
    }

    RefreshBudgets(DeltaTime);
    SecondsSinceContact += DeltaTime;

    if (IsInSafeZone())
    {
        // Neither generation nor decay in an Anchor or at a Forge (§0.3). The
        // lapse clock is held open rather than allowed to run out, so stepping
        // out of a safe zone does not immediately start a drain.
        PendingGrants = 0.0f;
        SecondsSinceContact = 0.0f;
        RefreshBand();
        return;
    }

    const float GenerationMultiplier = GetGenerationMultiplier();
    const float EffectiveCap = FMath::Max(0.0f, GlobalGenerationCap * GenerationMultiplier);

    // Proximity is the loop's only RATE source and the one that guarantees solo
    // viability without requiring damage intake — a Tank holding ground against
    // one enemy is generating. It is metered through the same global budget as
    // the events rather than paid separately, so "20/s from all sources
    // combined" means all sources.
    float Rate = 0.0f;
    if (bInCombat) Rate += ProximityGeneration(bEnemyNear, ProximityRate) * GenerationMultiplier;

    float Budget = ClampGeneration(EffectiveCap, EffectiveCap) * DeltaTime;
    float Generated = FMath::Min(ClampGeneration(Rate, EffectiveCap) * DeltaTime, Budget);
    Budget -= Generated;
    if (PendingGrants > 0.0f && Budget > 0.0f)
    {
        const float Drawn = FMath::Min(PendingGrants * GenerationMultiplier, Budget);
        PendingGrants -= (GenerationMultiplier > 0.0f) ? Drawn / GenerationMultiplier : PendingGrants;
        Generated += Drawn;
    }

    if (Generated > 0.0f)
    {
        ApplyGritDelta(Generated);
        RefreshBand();
        return;
    }

    // Decay only outside the lapse window. Inside it Grit BANKS — the Tank may
    // build through an approach and spend at the point of contact, which is the
    // whole reason the shape is a lapse timer rather than Momentum's state test.
    // The loop valve's decay lane scales the rate at the one place it is paid.
    const float Decay = DecayRate(IsLapseWindowOpen(), IsDecaySuspended(), DecayPerSecond) * GetDecayRateMultiplier();
    if (Decay > 0.0f) ApplyGritDelta(-Decay * DeltaTime);
    RefreshBand();
}
