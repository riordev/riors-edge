#include "Classes/BreakerGritComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

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
    //
    // The HEALED delegate is a different matter (2026-08-16): FBreakerHealResult
    // reports effective heal, overheal and granted shield as separate
    // quantities, which is exactly the split Leech L9 Nothing Wasted needs, so
    // that one binding is now taken. The handler is a no-op without the node.
    if (AActor* Owner = GetOwner())
    {
        if (UBreakerCombatComponent* Combat = Owner->FindComponentByClass<UBreakerCombatComponent>())
        {
            CachedCombat = Combat;
            Combat->OnHealed.AddDynamic(this, &UBreakerGritComponent::HandleOwnerHealed);
        }
    }
    HandleProgressionChanged();
    CachedBand = BandForFraction(GetGritFraction());
    PreviousShield = Attributes ? Attributes->GetShield() : 0.0f;
}

UBreakerCombatComponent* UBreakerGritComponent::ResolveCombat()
{
    if (!CachedCombat.IsValid() && GetOwner())
    {
        CachedCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    }
    return CachedCombat.Get();
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

    // Node-rank cache: read once per progression change, never per frame.
    // Ranked nodes read GetNodeRank (their R2 clause is a magnitude); pure
    // rule-rewrite nodes read their tag.
    RankSlowBleed = 0; RankFeedTheWound = 0; RankTransfusion = 0;
    bSecondHeart = false; bNothingWasted = false; bReciprocity = false;
    RankFooting = 0; RankHeldGround = 0; bInterposition = false; bConversion = false;
    if (bIsTank && Progression)
    {
        RankSlowBleed = Progression->GetNodeRank(TEXT("Tank.Leech.SlowBleed"), EBreakerPointCurrency::ClassPoints);
        RankFeedTheWound = Progression->GetNodeRank(TEXT("Tank.Leech.FeedTheWound"), EBreakerPointCurrency::ClassPoints);
        RankTransfusion = Progression->GetNodeRank(TEXT("Tank.Leech.Transfusion"), EBreakerPointCurrency::ClassPoints);
        bSecondHeart = Progression->HasNodeTag(BreakerNodeTags::Node_L_SecondHeart.GetTag());
        bNothingWasted = Progression->HasNodeTag(BreakerNodeTags::Node_L_NothingWasted.GetTag());
        bReciprocity = Progression->HasNodeTag(BreakerNodeTags::Node_L_Reciprocity.GetTag());
        RankFooting = Progression->GetNodeRank(TEXT("Tank.Bastion.Footing"), EBreakerPointCurrency::ClassPoints);
        RankHeldGround = Progression->GetNodeRank(TEXT("Tank.Bastion.HeldGround"), EBreakerPointCurrency::ClassPoints);
        bInterposition = Progression->HasNodeTag(BreakerNodeTags::Node_B_Interposition.GetTag());
        bConversion = Progression->HasNodeTag(BreakerNodeTags::Node_B_Conversion.GetTag());
    }
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

    // L10 Reciprocity's ledger: what the shield absorbed since it last stood.
    if (ShieldDamage > 0.0f) ShieldAbsorbedSinceGain += ShieldDamage;

    // L4 Feed the Wound rewrites the shield-absorption rate: half -> two-thirds
    // (R2: full rate). The shared damage cap still binds below, exactly as the
    // node text promises.
    const float EffectiveShieldRate = RankFeedTheWound >= 2 ? 1.0f
        : (RankFeedTheWound == 1 ? (2.0f / 3.0f) : ShieldRateFraction);
    const float Raw = DamageTakenGeneration(HealthDamage, ShieldDamage, Attributes->GetMaxHealth(),
        GritPerHealthFraction, HealthFractionPerGrit, EffectiveShieldRate, ProcCoefficient);
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
    // L6 Transfusion: WHILE SHIELDED, the proc pays more on a slightly faster
    // internal cooldown (+9, R2 +12, ICD 0.4 -> 0.3). G4's per-source cap below
    // is exactly why the shorter ICD is not a back door — the node comment says
    // so and the budget draw enforces it.
    const bool bTransfusion = RankTransfusion > 0 && Attributes && Attributes->GetShield() > 0.0f;
    const float EffectiveInterval = bTransfusion ? 0.3f : BlockProcInterval;   // O2 PLACEHOLDER
    const float EffectiveGrant = bTransfusion ? (RankTransfusion >= 2 ? 12.0f : 9.0f) : BlockProcGrant;   // node text
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastBlockGrantTime < EffectiveInterval) return;
    LastBlockGrantTime = Now;
    // The internal cooldown ALREADY implies the per-source cap at the default
    // values. The cap is enforced anyway, and separately, so that a node
    // shortening the ICD cannot silently uncap the source — the guard is
    // against a future node, not against today's numbers.
    PendingGrants += DrawFromBudget(EffectiveGrant, BlockBudget);
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
    if (bInCombat && !bWasInCombat)
    {
        GrantGrit(CombatEntryGrant);
        // B4 Held Ground R2's once-per-combat re-grant re-arms on the same edge
        // the entry grant fires on.
        bAnchorRegrantUsed = false;
    }
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

void UBreakerGritComponent::NotifyAnchorPlaced()
{
    // B4 Held Ground, second rank only: placing an Anchor Point re-triggers the
    // combat-entry grant, ONCE per combat state — same edge-triggered shape as
    // the entry grant itself, so it can never be farmed by re-placing.
    if (RankHeldGround < 2 || !bInCombat || bAnchorRegrantUsed) return;
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    bAnchorRegrantUsed = true;
    GrantGrit(CombatEntryGrant);
}

void UBreakerGritComponent::PushProximityRateBoost(FName Key, float Multiplier, float Duration)
{
    if (Key.IsNone()) return;
    FProximityBoostEntry Entry;
    Entry.Multiplier = FMath::Max(0.0f, Multiplier);
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    ProximityBoosts.Add(Key, Entry);
}

void UBreakerGritComponent::PopProximityRateBoost(FName Key)
{
    ProximityBoosts.Remove(Key);
}

float UBreakerGritComponent::GetProximityRateMultiplier() const
{
    const UWorld* World = GetWorld();
    const double Now = World ? World->GetTimeSeconds() : 0.0;
    float Composed = 1.0f;
    for (auto It = ProximityBoosts.CreateIterator(); It; ++It)
    {
        if (IsLoopOverrideExpired(It.Value().ExpiryTime, Now)) { It.RemoveCurrent(); continue; }
        Composed *= It.Value().Multiplier;
    }
    return Composed;
}

int32 UBreakerGritComponent::RegisterExplosiveBlast(AActor* Target)
{
    if (!Target) return 0;
    const UWorld* World = GetWorld();
    const double Now = World ? World->GetTimeSeconds() : 0.0;
    // Prune targets whose window lapsed (or who died), so the map never grows.
    for (auto It = ChainReactionStamps.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || Now - It.Value().LastBlastTime > ChainReactionWindowSeconds)
        {
            It.RemoveCurrent();
        }
    }
    FChainReactionEntry& Entry = ChainReactionStamps.FindOrAdd(Target);
    // First blast in a window pays no bonus; each later blast inside the window
    // pays one more stack, hard-capped — the node's exact sentence. The -1000
    // sentinel keeps a fresh entry out of the window even at world time zero.
    if (Now - Entry.LastBlastTime <= ChainReactionWindowSeconds)
    {
        Entry.Stacks = FMath::Min(Entry.Stacks + 1, ChainReactionMaxStacks);
    }
    else
    {
        Entry.Stacks = 0;
    }
    Entry.LastBlastTime = Now;
    return Entry.Stacks;
}

void UBreakerGritComponent::HandleOwnerHealed(const FBreakerHealResult& Result)
{
    // L9 Nothing Wasted: EVERY heal's overheal routes to Leech shield, not just
    // Rend's. Only the UNROUTED remainder converts — a request that already
    // asked for overheal-to-shield reports both, and converting the reported
    // overheal again would pay the same units twice.
    if (!bNothingWasted || !bIsTank || bRoutingOverheal || !Attributes) return;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const float Unrouted = Result.Overheal - Result.ShieldGranted;
    if (Unrouted <= 0.0f) return;
    const float MaxShield = Attributes->GetMaxShield();
    if (MaxShield <= 0.0f) return;
    bRoutingOverheal = true;
    Attributes->ApplyShield(FMath::Min(MaxShield, Attributes->GetShield() + Unrouted));
    bRoutingOverheal = false;
}

float UBreakerGritComponent::GetOwnAnchorDistanceCm() const
{
    const AActor* Owner = GetOwner();
    if (!Owner) return TNumericLimits<float>::Max();
    float Best = TNumericLimits<float>::Max();
    for (const TWeakObjectPtr<ABreakerDeployable>& Weak : ABreakerDeployable::GetLiveDeployables())
    {
        const ABreakerDeployable* Deployable = Weak.Get();
        if (!Deployable || Deployable->GetDeployableType() != EBreakerDeployableType::AnchorPoint) continue;
        if (Deployable->GetOwningCharacter() != Owner) continue;
        Best = FMath::Min(Best, static_cast<float>(FVector::Dist(Owner->GetActorLocation(), Deployable->GetActorLocation())));
    }
    return Best;
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

    // ---- Anchor-keyed node rules (B2/B4/B8), one distance query for all ----
    const bool bAnyAnchorNode = RankFooting > 0 || RankHeldGround > 0 || bInterposition;
    const float AnchorDistance = bAnyAnchorNode ? GetOwnAnchorDistanceCm() : TNumericLimits<float>::Max();
    const bool bNearOwnAnchor = AnchorDistance <= AnchorNearRadiusCm;

    // B2 Footing: near your own Anchor Point the proximity source reaches 7 m
    // (R2: 9 m) instead of 5. The 5 m scan lives with the character; this is
    // the EXTENSION only, so a build without the node is bit-identical.
    bool bExtendedNear = false;
    if (RankFooting > 0 && bNearOwnAnchor && !bEnemyNear && bInCombat)
    {
        const float ExtendedRadius = RankFooting >= 2 ? 900.0f : 700.0f;   // node text
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
            {
                const ABreakerEnemy* Enemy = *It;
                if (!Enemy) continue;
                const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
                if (!EnemyCombat || EnemyCombat->IsDead()) continue;
                if (FVector::DistSquared(Owner->GetActorLocation(), Enemy->GetActorLocation()) <= ExtendedRadius * ExtendedRadius)
                {
                    bExtendedNear = true;
                    break;
                }
            }
        }
        // The extended contact holds the lapse window open on the same footing
        // as the 5 m contact — half the window's definition, wider.
        if (bExtendedNear) SecondsSinceContact = 0.0f;
    }

    // ---- The Leech shield clock (L2/L8/L10) --------------------------------
    // A Tank OWNS a shield ceiling: §T1 authors the overheal-to-shield cap at
    // 25% of maximum health, and until this write existed MaxShield was 0 for
    // every player, so the entire conversion path granted nothing. Raise-only,
    // so a larger ceiling from any future source survives. O2 PLACEHOLDER.
    const float LeechShieldCeiling = Attributes->GetMaxHealth() * 0.25f;   // §T1
    if (LeechShieldCeiling > Attributes->GetMaxShield())
    {
        Attributes->ApplyMaxShield(LeechShieldCeiling);
    }
    // Shield GAIN resets the hold clock; after the hold, the shield bleeds.
    // Built for the Leech nodes; base numbers are §T1's 3s / 4%/s.
    const float ShieldNow = Attributes->GetShield();
    if (ShieldNow > PreviousShield + KINDA_SMALL_NUMBER)
    {
        SecondsSinceShieldGain = 0.0f;
    }
    else
    {
        SecondsSinceShieldGain += DeltaTime;
    }
    // Break detection BEFORE decay, so only damage-driven breaks pay
    // Reciprocity — decay reaching zero pays nothing (post-break heal is the
    // node; a decayed shield absorbed what it absorbed and simply lapsed).
    if (bReciprocity && PreviousShield > 0.0f && ShieldNow <= 0.0f && ShieldAbsorbedSinceGain > 0.0f)
    {
        ReciprocityHealRemaining = ShieldAbsorbedSinceGain * ReciprocityReturnFraction;
        ReciprocityHealPerSecond = ReciprocityHealRemaining / FMath::Max(0.05f, ReciprocityReturnSeconds);
    }
    if (ShieldNow <= 0.0f) ShieldAbsorbedSinceGain = 0.0f;

    float ShieldAfterDecay = ShieldNow;
    if (ShieldNow > 0.0f)
    {
        // L2 Slow Bleed rewrites the HOLD, never the rate: 3s -> 5s (R2: 8s).
        const float EffectiveDelay = RankSlowBleed >= 2 ? 8.0f : (RankSlowBleed == 1 ? 5.0f : LeechShieldDecayDelaySeconds);   // node text
        // L8 Second Heart: no decay at all while IRONCLAD.
        const bool bDecayHeld = bSecondHeart && CachedBand == EBreakerGritBand::Ironclad;
        if (!bDecayHeld && SecondsSinceShieldGain >= EffectiveDelay && LeechShieldDecayFractionPerSecond > 0.0f)
        {
            ShieldAfterDecay = FMath::Max(0.0f, ShieldNow - ShieldNow * LeechShieldDecayFractionPerSecond * DeltaTime);
            Attributes->ApplyShield(ShieldAfterDecay);
        }
    }

    // B8 Interposition's solo half: alone, the sharing field pays its owner —
    // a shield trickle while standing inside it. Recorded substitution for the
    // ally share (O2 PLACEHOLDER magnitude); the field is a radius, not the
    // panel-backed wedge, until the panel owns real geometry.
    if (bInterposition && bInCombat && AnchorDistance <= InterpositionRadiusCm)
    {
        const float MaxShield = Attributes->GetMaxShield();
        if (MaxShield > 0.0f && ShieldAfterDecay < MaxShield)
        {
            const float Trickle = Attributes->GetMaxHealth() * InterpositionShieldFractionPerSecond * DeltaTime;
            ShieldAfterDecay = FMath::Min(MaxShield, ShieldAfterDecay + Trickle);
            Attributes->ApplyShield(ShieldAfterDecay);
        }
    }
    PreviousShield = ShieldAfterDecay;

    // L10 Reciprocity's payout: 20% of what broke, over 2s, AFTER the break.
    if (ReciprocityHealRemaining > 0.0f)
    {
        const float Pay = FMath::Min(ReciprocityHealPerSecond * DeltaTime, ReciprocityHealRemaining);
        ReciprocityHealRemaining -= Pay;
        if (UBreakerCombatComponent* Combat = ResolveCombat())
        {
            Combat->ApplyHealingAmount(Pay, Owner, FGameplayTag());
        }
    }

    // B9 Conversion: hits carry flat damage scaled to CURRENT shield. The keyed
    // outgoing modifier is refreshed every frame so spending the shield drops
    // the bonus with it — "hold it or use it", literally.
    if (bConversion)
    {
        if (UBreakerCombatComponent* Combat = ResolveCombat())
        {
            if (ShieldAfterDecay > 0.0f)
            {
                Combat->PushOutgoingModifier(TEXT("Bastion.Conversion"), ShieldAfterDecay * ConversionFlatPerShieldPoint, 1.0f, 0.5f);
            }
            else
            {
                Combat->RemoveOutgoingModifier(TEXT("Bastion.Conversion"));
            }
        }
    }

    const float GenerationMultiplier = GetGenerationMultiplier();
    const float EffectiveCap = FMath::Max(0.0f, GlobalGenerationCap * GenerationMultiplier);

    // Proximity is the loop's only RATE source and the one that guarantees solo
    // viability without requiring damage intake — a Tank holding ground against
    // one enemy is generating. It is metered through the same global budget as
    // the events rather than paid separately, so "20/s from all sources
    // combined" means all sources. The proximity boost lane (B5) scales this
    // source alone and composes under the same caps.
    float Rate = 0.0f;
    if (bInCombat) Rate += ProximityGeneration(bEnemyNear || bExtendedNear, ProximityRate * GetProximityRateMultiplier()) * GenerationMultiplier;

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
    // B4 Held Ground: standing within 3 m of your own Anchor Point suspends
    // Grit decay outright — the banking rewrite, keyed to the panel's presence
    // and bounded by its lifetime and cooldown exactly as the node argues.
    const bool bHeldGround = RankHeldGround > 0 && bNearOwnAnchor;
    const float Decay = DecayRate(IsLapseWindowOpen(), IsDecaySuspended() || bHeldGround, DecayPerSecond) * GetDecayRateMultiplier();
    if (Decay > 0.0f) ApplyGritDelta(-Decay * DeltaTime);
    RefreshBand();
}
