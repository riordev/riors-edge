#include "Classes/BreakerChargeComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Game/BreakerGameMode.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

UBreakerChargeComponent::UBreakerChargeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UBreakerChargeComponent::BeginPlay()
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
            Progression->OnProgressionChanged.AddDynamic(this, &UBreakerChargeComponent::HandleProgressionChanged);
        }
    }
    // DELIBERATELY NO HEALING BINDING. `ApplyHealing` exists, but it does not
    // report OVERHEAL as a separate quantity, and "overheal generates zero" is
    // this loop's single most load-bearing anti-farm rule — it is what closes
    // "spam a heal on a full-health target forever". Binding a delegate that
    // cannot distinguish the two would credit the exploit by construction. The
    // Notify* entry points take the split explicitly until the healing result
    // carries it (Class-Kits-Support §5.1 ranks that as the missing hook that
    // blocks the Medic branch entirely).
    //
    // 2026-08-16: the healing result NOW carries the split (FBreakerHealResult
    // reports HealthHealed / Overheal / ShieldGranted separately), so the
    // OnHealed binding below is honest at last. It exists for MD1 Field
    // Dressing — the heals that arrive from OUTSIDE the Support kit — and
    // stands down whenever a Support ability's explicit crediting scope is
    // open, so no heal is ever counted twice.
    if (AActor* Owner = GetOwner())
    {
        if (UBreakerCombatComponent* Combat = Owner->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->OnHealed.AddDynamic(this, &UBreakerChargeComponent::HandleOwnerHealed);
        }
    }
    HandleProgressionChanged();
    CachedBand = BandForFraction(GetChargeFraction(), AttunedFraction, ResonantFraction);
}

void UBreakerChargeComponent::BindAttributes(UBreakerAttributeSet* InAttributes)
{
    Attributes = InAttributes;
    HandleProgressionChanged();
    CachedBand = BandForFraction(GetChargeFraction(), AttunedFraction, ResonantFraction);
}

EBreakerChargeBand UBreakerChargeComponent::BandForFraction(float Fraction, float AttunedAt, float ResonantAt)
{
    if (Fraction >= ResonantAt) return EBreakerChargeBand::Resonant;
    if (Fraction >= AttunedAt) return EBreakerChargeBand::Attuned;
    return EBreakerChargeBand::Cold;
}

EBreakerChargeBand UBreakerChargeComponent::BandForFraction(float Fraction)
{
    // Cold 0-33, Attuned 34-74, RESONANT 75-100 — O2 PLACEHOLDER seeds.
    //
    // NOTE THE ASYMMETRY against the even thirds Momentum and Grit use, and do
    // not "correct" it: Resonant starts at three quarters, not two thirds,
    // because the ultimate costs a full bar and the band that matters most is
    // the one immediately below it. Support is built to REACH its top band,
    // spend it, and climb back; Swift is built to HOLD Redline. Putting Resonant
    // just under the ultimate is what creates the "spend it or hold it for the
    // node bonuses" tension that Swift's loop does not have.
    return BandForFraction(Fraction, 1.0f / 3.0f, 0.75f);
}

float UBreakerChargeComponent::ComposeGenerationMultipliers(const TArray<float>& Multipliers)
{
    float Composed = 1.0f;
    for (const float Multiplier : Multipliers)
    {
        if (Multiplier > 0.0f) Composed *= Multiplier;
    }
    return Composed;
}

bool UBreakerChargeComponent::IsLoopOverrideExpired(double ExpiryTime, double Now)
{
    return ExpiryTime >= 0.0 && ExpiryTime <= Now;
}

float UBreakerChargeComponent::ClampGeneration(float RequestedRate, float GlobalCap)
{
    return FMath::Clamp(RequestedRate, 0.0f, FMath::Max(0.0f, GlobalCap));
}

float UBreakerChargeComponent::ClampToBank(float Value, float MaxCharge)
{
    return FMath::Clamp(Value, 0.0f, FMath::Max(0.0f, MaxCharge));
}

float UBreakerChargeComponent::HealingGeneration(float EffectiveHeal, float TargetMaxHealth, float HealthFractionPerCharge, float ProcCoefficient)
{
    if (TargetMaxHealth <= 0.0f || HealthFractionPerCharge <= 0.0f) return 0.0f;
    // EffectiveHeal is the health the bar actually gained. Overheal never
    // reaches this function, which is why there is no overheal parameter to
    // forget to subtract.
    const float Units = (FMath::Max(0.0f, EffectiveHeal) / TargetMaxHealth) / HealthFractionPerCharge;
    // A heal-over-time tick credits at the healing source's proc coefficient,
    // not at 1.0, so a HoT ticking ten times does not out-generate an instant
    // heal of the same total by an order of magnitude.
    return Units * FMath::Max(0.0f, ProcCoefficient);
}

float UBreakerChargeComponent::ShieldingGeneration(float EffectiveShield, float TargetMaxHealth, float HealthFractionPerCharge)
{
    if (TargetMaxHealth <= 0.0f || HealthFractionPerCharge <= 0.0f) return 0.0f;
    return (FMath::Max(0.0f, EffectiveShield) / TargetMaxHealth) / HealthFractionPerCharge;
}

float UBreakerChargeComponent::MarkedDamageGeneration(float DamageDealt, float TargetMaxHealth, float HealthFractionPerCharge)
{
    if (TargetMaxHealth <= 0.0f || HealthFractionPerCharge <= 0.0f) return 0.0f;
    return (FMath::Max(0.0f, DamageDealt) / TargetMaxHealth) / HealthFractionPerCharge;
}

float UBreakerChargeComponent::CleanseGeneration(int32 StatusesRemoved, float PerStatusGrant)
{
    // Must have removed a REAL status: cleansing a clean target pays nothing.
    return FMath::Max(0, StatusesRemoved) * FMath::Max(0.0f, PerStatusGrant);
}

float UBreakerChargeComponent::BuffUptimeGeneration(bool bAnyBuffActive, bool bInCombat, float UptimeRate)
{
    // Two guards in one expression. The BOOL is the count-independence rule made
    // unexpressible-otherwise. The combat gate is the pre-combat-generation
    // rule: standing in an Anchor with a buff running generates nothing, which
    // together with "no passive regeneration" means there is nothing at all to
    // farm before an encounter starts.
    return (bAnyBuffActive && bInCombat) ? FMath::Max(0.0f, UptimeRate) : 0.0f;
}

float UBreakerChargeComponent::OutOfCombatDecay(float Current, float Ceiling, float DecayPerSecond, float DeltaTime)
{
    const float SafeCeiling = FMath::Max(0.0f, Ceiling);
    if (Current <= SafeCeiling || DecayPerSecond <= 0.0f || DeltaTime <= 0.0f) return Current;
    // Falls TO the ceiling and stops there — never past it, never to zero. The
    // clamp is the only decay in the loop and its whole job is to deny a free
    // ultimate at the start of a fight, not to punish being out of combat.
    return FMath::Max(SafeCeiling, Current - DecayPerSecond * DeltaTime);
}

bool UBreakerChargeComponent::CanSpendFrom(float Charge, float Cost)
{
    if (Cost <= 0.0f) return true;
    return Charge >= Cost;
}

float UBreakerChargeComponent::DrawFromBudget(float Requested, float& InOutBudgetRemaining)
{
    if (Requested <= 0.0f || InOutBudgetRemaining <= 0.0f) return 0.0f;
    const float Drawn = FMath::Min(Requested, InOutBudgetRemaining);
    InOutBudgetRemaining -= Drawn;
    return Drawn;
}

void UBreakerChargeComponent::PushLoopOverride(FName Key, bool bSuspendClamp, float GenerationMultiplier, float Duration)
{
    if (Key.IsNone()) return;
    FLoopOverrideEntry Entry;
    Entry.bSuspendClamp = bSuspendClamp;
    if (GenerationMultiplier < 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("PushLoopOverride('%s'): negative generation multiplier %.3f is meaningless; falling back to 1.0."),
            *Key.ToString(), GenerationMultiplier);
    }
    Entry.GenerationMultiplier = GenerationMultiplier >= 0.0f ? GenerationMultiplier : 1.0f;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    LoopOverrides.Add(Key, Entry);
}

void UBreakerChargeComponent::PopLoopOverride(FName Key)
{
    LoopOverrides.Remove(Key);
}

void UBreakerChargeComponent::PruneLoopOverrides() const
{
    const UWorld* World = GetWorld();
    if (!World || LoopOverrides.Num() == 0) return;
    const double Now = World->GetTimeSeconds();
    for (auto It = LoopOverrides.CreateIterator(); It; ++It)
    {
        if (IsLoopOverrideExpired(It.Value().ExpiryTime, Now)) It.RemoveCurrent();
    }
}

bool UBreakerChargeComponent::IsClampSuspended() const
{
    PruneLoopOverrides();
    for (const TPair<FName, FLoopOverrideEntry>& Pair : LoopOverrides)
    {
        if (Pair.Value.bSuspendClamp) return true;
    }
    return false;
}

float UBreakerChargeComponent::GetGenerationMultiplier() const
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

int32 UBreakerChargeComponent::GetActiveLoopOverrideCount() const
{
    PruneLoopOverrides();
    return LoopOverrides.Num();
}

void UBreakerChargeComponent::HandleProgressionChanged()
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    const UBreakerProgressionComponent* Progression = CachedProgression.Get();
    bIsSupport = Progression && Progression->GetProgressionState().PermanentClass == EBreakerClassId::Support;
    if (!bIsSupport) PendingGrants = 0.0f;

    RankFieldDressing = 0; RankSteadyHands = 0; bBloodDebt = false; RankSustain = 0;
    bShieldConversionNodes = false;
    if (bIsSupport && Progression)
    {
        RankFieldDressing = Progression->GetNodeRank(TEXT("Support.Medic.FieldDressing"), EBreakerPointCurrency::CorePoints);
        RankSteadyHands = Progression->GetNodeRank(TEXT("Support.Medic.SteadyHands"), EBreakerPointCurrency::CorePoints);
        bBloodDebt = Progression->HasNodeTag(BreakerNodeTags::Node_MD_BloodDebt.GetTag());
        RankSustain = Progression->GetNodeRank(TEXT("Support.Conductor.Sustain"), EBreakerPointCurrency::CorePoints);
        bShieldConversionNodes = Progression->HasNodeTag(BreakerNodeTags::Node_MD_Overflow.GetTag())
            || Progression->GetNodeRank(TEXT("Support.Medic.SecondOpinion"), EBreakerPointCurrency::CorePoints) > 0;
    }
    if (!bBloodDebt) BloodDebtPool = 0.0f;
}

bool UBreakerChargeComponent::IsActiveForOwner() const
{
    return bIsSupport;
}

float UBreakerChargeComponent::GetCharge() const
{
    return Attributes ? Attributes->GetClassResource() : 0.0f;
}

float UBreakerChargeComponent::GetChargeFraction() const
{
    if (!Attributes) return 0.0f;
    const float Max = Attributes->GetMaxClassResource();
    return Max > 0.0f ? FMath::Clamp(Attributes->GetClassResource() / Max, 0.0f, 1.0f) : 0.0f;
}

bool UBreakerChargeComponent::IsInSafeZone() const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !GetWorld()) return false;
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    return GameMode && GameMode->IsInSafeZone(Owner->GetActorLocation());
}

void UBreakerChargeComponent::ApplyChargeDelta(float Delta)
{
    if (!Attributes || FMath::IsNearlyZero(Delta)) return;
    Attributes->ApplyClassResource(ClampToBank(Attributes->GetClassResource() + Delta, Attributes->GetMaxClassResource()));
}

void UBreakerChargeComponent::RefreshBand()
{
    const EBreakerChargeBand NewBand = BandForFraction(GetChargeFraction(), AttunedFraction, ResonantFraction);
    if (NewBand != CachedBand)
    {
        CachedBand = NewBand;
        OnChargeBandChanged.Broadcast(NewBand);
    }
}

void UBreakerChargeComponent::QueueGrant(float Amount)
{
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner() || IsInSafeZone()) return;
    PendingGrants += Amount;
}

void UBreakerChargeComponent::NotifyHealingDone(float EffectiveHeal, float Overheal, float TargetMaxHealth, bool bSelfTargeted, float ProcCoefficient)
{
    // Overheal is accepted and DISCARDED here rather than being refused at the
    // signature, so a caller that has it always has somewhere honest to put it.
    // It contributes nothing; the parameter exists to make the rule visible at
    // every call site instead of buried in this file.
    (void)Overheal;
    // MD10 Blood Debt: healing — self included, at FULL rate, before any
    // metering — banks into the pool. The pool is health units, not Charge, and
    // it is capped; the spend lives with the Mark ability.
    if (bBloodDebt && EffectiveHeal > 0.0f && IsActiveForOwner())
    {
        BloodDebtPool = FMath::Min(BloodDebtPoolCap, BloodDebtPool + EffectiveHeal);
    }
    // MD4 Steady Hands: at Attuned or better, self-heal Charge credits shave
    // Support cooldowns (R2: ally-heal credits too), at most once a second.
    if (RankSteadyHands > 0 && EffectiveHeal > 0.0f && IsActiveForOwner()
        && (bSelfTargeted || RankSteadyHands >= 2)
        && CachedBand != EBreakerChargeBand::Cold)
    {
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (Now - LastSteadyHandsTime >= SteadyHandsIntervalSeconds)
        {
            LastSteadyHandsTime = Now;
            ShaveAllAbilityCooldowns(SteadyHandsShaveSeconds);
        }
    }
    float Amount = HealingGeneration(EffectiveHeal, TargetMaxHealth, HealthFractionPerCharge, ProcCoefficient);
    if (bSelfTargeted)
    {
        // THE RATE IS IDENTICAL — this branch does not scale the amount, it only
        // meters it. Self-healing pays exactly what ally-healing pays, which is
        // the non-negotiable anti-solo-penalty clause; what the sub-cap bounds
        // is how much of it can arrive PER SECOND, so a Life-on-Hit build
        // supplements the loop instead of becoming it.
        Amount = DrawFromBudget(Amount, SelfHealBudget);
    }
    QueueGrant(Amount);
}

void UBreakerChargeComponent::NotifyShieldingDone(float EffectiveShield, float OverShield, float TargetMaxHealth, bool bSelfTargeted)
{
    (void)OverShield;   // The shield-side twin of the overheal rule: pays nothing.
    float Amount = ShieldingGeneration(EffectiveShield, TargetMaxHealth, HealthFractionPerCharge);
    // Self-shielding shares the self-heal bucket rather than getting its own:
    // they are the same lane (self-directed sustain) and two independent buckets
    // would let a build alternate between them to double the sub-cap.
    if (bSelfTargeted) Amount = DrawFromBudget(Amount, SelfHealBudget);
    QueueGrant(Amount);
}

void UBreakerChargeComponent::NotifyMarkedTargetDamage(float DamageDealt, float TargetMaxHealth)
{
    // The caller owns the "the mark must be YOURS" rule and the "re-marking
    // within 3s does not refresh generation eligibility" rule — both are
    // properties of a mark component that does not exist yet, and neither can
    // be checked from here without inventing one.
    QueueGrant(MarkedDamageGeneration(DamageDealt, TargetMaxHealth, MarkedHealthFractionPerCharge));
}

void UBreakerChargeComponent::NotifyStatusCleansed(int32 StatusesRemoved)
{
    if (StatusesRemoved <= 0) return;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastCleanseTime < CleanseInterval) return;
    LastCleanseTime = Now;
    QueueGrant(CleanseGeneration(StatusesRemoved, CleansePerStatusGrant));
}

void UBreakerChargeComponent::NotifyAssist()
{
    // THE ONLY PARTY-EXCLUSIVE SOURCE IN THE LOOP, and deliberately the
    // smallest: one source out of eight, which is what keeps the party premium
    // an efficiency advantage rather than a requirement. It is also the only
    // source with a solo yield of zero, and no branch routes through it.
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (Now - LastAssistTime < AssistInterval) return;
    LastAssistTime = Now;
    QueueGrant(AssistGrant);
}

void UBreakerChargeComponent::SetAnyBuffActive(bool bActive)
{
    // CO3 Sustain's edge: the grace clock starts when the LAST buff expires.
    if (bAnyBuffActive && !bActive)
    {
        SecondsSinceBuffExpire = 0.0f;
    }
    bAnyBuffActive = bActive;
}

void UBreakerChargeComponent::HandleOwnerHealed(const FBreakerHealResult& Result)
{
    // MD1 Field Dressing: healing yourself pays Charge at the ally rate even
    // from non-Support sources — leech, regen, pickups. Anything a Support
    // ability healed arrives inside a crediting scope and is skipped here, so
    // this listener covers exactly the sources the node names. The RATE is the
    // ordinary one and the self-heal sub-cap still meters it — metering is not
    // a rate change, per this component's own top comment. R2's ally-received
    // clause rides the same event (an ally's heal lands here too) and is
    // vacuous solo. KNOWN LIMIT, recorded: OnHealed carries no source tag, so
    // Support-vs-non-Support is told apart by the scope, not the source.
    if (!IsActiveForOwner() || SupportHealScopeDepth > 0 || RankFieldDressing <= 0) return;
    if (!Attributes) return;
    const float OwnMaxHealth = Attributes->GetMaxHealth();
    if (Result.HealthHealed > 0.0f || Result.Overheal > 0.0f)
    {
        NotifyHealingDone(Result.HealthHealed, Result.Overheal, OwnMaxHealth, /*bSelfTargeted=*/true, 1.0f);
    }
    if (Result.ShieldGranted > 0.0f)
    {
        NotifyShieldingDone(Result.ShieldGranted, 0.0f, OwnMaxHealth, /*bSelfTargeted=*/true);
    }
}

float UBreakerChargeComponent::ConsumeBloodDebt()
{
    const float Pool = BloodDebtPool;
    BloodDebtPool = 0.0f;
    return Pool;
}

void UBreakerChargeComponent::ShaveAllAbilityCooldowns(float Seconds)
{
    if (Seconds <= 0.0f) return;
    const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner());
    UAbilitySystemComponent* ASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;
    // Every live cooldown effect moves its start time BACK, which shortens its
    // remaining duration by the same amount — the one honest way to shave a
    // GAS cooldown without owning the effect.
    FGameplayEffectQuery Query;
    Query.EffectDefinition = UBreakerAbilityCooldownEffect::StaticClass();
    for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(Query))
    {
        ASC->ModifyActiveEffectStartTime(Handle, -Seconds);
    }
}

void UBreakerChargeComponent::SetInCombat(bool bNowInCombat)
{
    bInCombat = bNowInCombat;
}

void UBreakerChargeComponent::GrantCharge(float Amount)
{
    if (Amount <= 0.0f || !GetOwner() || !GetOwner()->HasAuthority() || !IsActiveForOwner()) return;
    ApplyChargeDelta(Amount);
    RefreshBand();
}

bool UBreakerChargeComponent::CanAffordSpend(float Cost) const
{
    return IsActiveForOwner() && CanSpendFrom(GetCharge(), Cost);
}

bool UBreakerChargeComponent::TrySpendCharge(float Cost)
{
    if (!Attributes || !GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (!CanAffordSpend(Cost)) return false;
    ApplyChargeDelta(-Cost);
    RefreshBand();
    return true;
}

void UBreakerChargeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceLoop(DeltaTime);
}

void UBreakerChargeComponent::AdvanceLoop(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !Attributes || DeltaTime <= 0.0f) return;
    if (!IsActiveForOwner())
    {
        PendingGrants = 0.0f;
        return;
    }

    // Self-directed sustain refills its own token bucket independently of the
    // global budget, for the same frame-rate-independence reason the Grit loop's
    // per-source buckets do.
    SelfHealBudget = FMath::Min(SelfHealBudget + SelfHealRateCap * DeltaTime, SelfHealRateCap);

    // MD5/MD9's shield conversions need a ceiling to exist: MaxShield
    // initialises to 0 for every player, and a conversion into a zero-cap bar
    // grants nothing. Node-gated and raise-only. O2 PLACEHOLDER fraction,
    // borrowed from Tank §T1's 25%-of-max-health cap.
    if (bShieldConversionNodes)
    {
        const float ShieldCeiling = Attributes->GetMaxHealth() * 0.25f;
        if (ShieldCeiling > Attributes->GetMaxShield())
        {
            Attributes->ApplyMaxShield(ShieldCeiling);
        }
    }

    if (IsInSafeZone())
    {
        PendingGrants = 0.0f;
        RefreshBand();
        return;
    }

    if (!bInCombat)
    {
        // OUT OF COMBAT: no generation at all, and the clamp runs. Queued income
        // is dropped rather than banked — it was earned in a combat that has
        // ended, and paying it out now is exactly the pre-combat bar the clamp
        // exists to deny.
        PendingGrants = 0.0f;
        if (!IsClampSuspended())
        {
            const float Ceiling = FMath::Min(OutOfCombatCeiling, Attributes->GetMaxClassResource());
            const float Rate = OutOfCombatClampSeconds > 0.0f
                ? FMath::Max(0.0f, GetCharge() - Ceiling) / OutOfCombatClampSeconds
                : 0.0f;
            // The rate is recomputed from the CURRENT surplus each frame rather
            // than latched on leaving combat, so the clamp takes its authored
            // window from wherever the bar happens to be and cannot overshoot
            // the ceiling on a long frame.
            const float Clamped = OutOfCombatDecay(GetCharge(), Ceiling, Rate, DeltaTime);
            ApplyChargeDelta(Clamped - GetCharge());
        }
        RefreshBand();
        return;
    }

    const float GenerationMultiplier = GetGenerationMultiplier();
    const float EffectiveCap = FMath::Max(0.0f, GlobalGenerationCap * GenerationMultiplier);

    // Buff uptime is the loop's only rate source, and it is the one that makes
    // the class work solo: it pays for keeping a buff up on ONE target, and the
    // one target is allowed to be yourself. CO3 Sustain smooths the sawtooth:
    // for a short grace after the last buff expires the source keeps paying —
    // still combat-gated, still count-independent.
    bool bEffectiveUptime = bAnyBuffActive;
    if (!bEffectiveUptime && RankSustain > 0)
    {
        const float Grace = RankSustain >= 2 ? SustainGraceSecondsRank2 : SustainGraceSeconds;
        bEffectiveUptime = SecondsSinceBuffExpire <= Grace;
    }
    if (!bAnyBuffActive) SecondsSinceBuffExpire += DeltaTime;
    const float Rate = BuffUptimeGeneration(bEffectiveUptime, bInCombat, BuffUptimeRate) * GenerationMultiplier;

    float Budget = ClampGeneration(EffectiveCap, EffectiveCap) * DeltaTime;
    float Generated = FMath::Min(ClampGeneration(Rate, EffectiveCap) * DeltaTime, Budget);
    Budget -= Generated;
    if (PendingGrants > 0.0f && Budget > 0.0f)
    {
        const float Drawn = FMath::Min(PendingGrants * GenerationMultiplier, Budget);
        PendingGrants -= (GenerationMultiplier > 0.0f) ? Drawn / GenerationMultiplier : PendingGrants;
        Generated += Drawn;
    }

    if (Generated > 0.0f) ApplyChargeDelta(Generated);
    RefreshBand();
}
