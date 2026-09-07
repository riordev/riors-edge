#include "Combat/BreakerStatusComponent.h"

#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerZoneMath.h"
#include "Combat/BreakerStatusRules.h"
#include "Classes/BreakerManaComponent.h"
#include "Characters/BreakerCharacter.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

UBreakerStatusComponent::UBreakerStatusComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerStatusComponent::BeginPlay()
{
    Super::BeginPlay();
    Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
}

void UBreakerStatusComponent::GrantStatusImmunity(float DurationSeconds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || DurationSeconds <= 0.0f) return;
    // Refreshes to the longer window, never stacks.
    StatusImmunityRemaining = FMath::Max(StatusImmunityRemaining, DurationSeconds);
}

float UBreakerStatusComponent::GetEffectiveAilmentAvoidanceChance() const
{
    float Chance = AilmentAvoidanceChance;
    // Gear's leg, read from GetStats() exactly as the combat component reads
    // Physical DR — one consumer, one clamp, no attribute lane to audit.
    if (const AActor* Owner = GetOwner())
    {
        if (const UBreakerEquipmentComponent* Equipment = Owner->FindComponentByClass<UBreakerEquipmentComponent>())
        {
            Chance += Equipment->GetStats().AilmentAvoidanceChancePercent / 100.0f;
        }
    }
    return FMath::Clamp(Chance, 0.0f, MaxAilmentAvoidanceChance);
}

void UBreakerStatusComponent::ApplyStatus(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator)
{
    // Entropy is earned through accepted-hit buildup, never a carried status payload.
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))) return;
    ApplyStatusInternal(Spec, DamageFamily, Instigator, false);
}

float UBreakerStatusComponent::GetArmorMultiplier() const
{
    const UBreakerCombatComponent* OwnerCombat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!OwnerCombat || OwnerCombat->IsDead()) return 1.0f;
    float Reduction = 0;
    for (const auto& Active : ActiveStatuses)
        if (Active.RemainingDuration > 0)
            if (const auto* Rule = BreakerStatusRules::FindRule(Active.Spec.StatusTag))
                Reduction = FMath::Max(Reduction, Rule->ArmorReductionPercent);
    return 1.0f - Reduction / 100.0f;
}

float UBreakerStatusComponent::GetHealingReceivedMultiplier() const
{
    const UBreakerCombatComponent* OwnerCombat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!OwnerCombat || OwnerCombat->IsDead()) return 1.0f;
    float Reduction = 0;
    for (const auto& Active : ActiveStatuses)
        if (Active.RemainingDuration > 0)
            if (const auto* Rule = BreakerStatusRules::FindRule(Active.Spec.StatusTag))
                Reduction = FMath::Max(Reduction, Rule->HealingReductionPercent);
    return 1.0f - Reduction / 100.0f;
}

void UBreakerStatusComponent::ApplyStatusInternal(const FBreakerStatusApplicationSpec& InputSpec, EBreakerDamageFamily DamageFamily, AActor* Instigator, bool bDurationAlreadyScaled)
{
    // Caller may have passed an entry in a live status array; callbacks can
    // consume or reallocate it. Accepted application owns its payload.
    FBreakerStatusApplicationSpec Spec = InputSpec;
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Void"), false)) return;
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")) && HasStatus(Spec.StatusTag)) return;
    const FBreakerStatusRule* Rule = BreakerStatusRules::FindRule(Spec.StatusTag);
    const bool bEffectOnly = Rule && Rule->IsNonDamagingDebuff();
    if (bEffectOnly) { Spec.BaseDamagePerTick = 0; Spec.InitialStacks = 1; }
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Spec.StatusTag.IsValid()
        || !FMath::IsFinite(Spec.Duration) || !FMath::IsFinite(Spec.TickInterval)
        || !FMath::IsFinite(Spec.ProcCoefficient) || Spec.Duration <= 0.0f || Spec.TickInterval <= 0.0f) return;
    const UBreakerCombatComponent* TargetCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (TargetCombat && TargetCombat->IsDead()) return;

    // --- StatusDuration, the APPLIER's lane, folded at the door -----------
    // This is the one funnel every application path passes through — weapon
    // bleed, Cleave, zones, projectile-carried specs — so the tree's
    // StatusDuration lane pays here once rather than at five build sites.
    // Application-time only, like the rest of the snapshot (O10): a respec
    // never rewrites a status already running. The tick interval is
    // deliberately untouched — duration buys more ticks, never slower ones,
    // so stacking keeps its visibly discrete steps. An instigator with no
    // progression component (every enemy) scales by exactly 1. At a composed
    // 0 the application does not exist, mirroring the Duration guard above.
    float DurationScale = 1.0f;
    if (Instigator && !bDurationAlreadyScaled)
    {
        if (const UBreakerProgressionComponent* InstigatorProgression = Instigator->FindComponentByClass<UBreakerProgressionComponent>())
        {
            DurationScale = InstigatorProgression->GetNodeStats().StatusDurationMultiplier;
        }
    }
    const float ScaledDuration = Spec.Duration * DurationScale;
    if (!FMath::IsFinite(ScaledDuration) || ScaledDuration <= 0.0f) return;

    // --- Ailment avoidance: one roll per application, at the door ---------
    // BEFORE the immunity check by ruling: avoidance is the ORDINARY defence
    // and immunity the absolute one, so an application refused by the roll
    // never consumes anyone's attention during an immunity window, and the
    // determinism tests can pin the roll without granting immunity first.
    // Seeded like dodge (FRandomStream over a derived seed plus a salt,
    // never a shared RNG): the spec carries no seed field — it lives in
    // Progression/, another lane's file — so the application seed is the
    // status tag's hash mixed with this component's application ordinal,
    // which preserves the property that matters: same component, same
    // sequence of applications, same verdicts, in a live game and in
    // automation alike. Refreshes and stack adds roll too, because a
    // reapplication IS an application; ticks of a status that already landed
    // are built in AdvanceStatuses and never come back through this door,
    // so they structurally cannot re-roll.
    const uint32 ApplicationSeed = HashCombine(GetTypeHash(Spec.StatusTag), ApplicationsAttempted++);
    const float AvoidanceChance = GetEffectiveAilmentAvoidanceChance();
    if (AvoidanceChance > 0.0f)
    {
        FRandomStream AvoidanceRandom(HashCombine(ApplicationSeed, 0xA110Bu));
        if (AvoidanceRandom.FRand() < AvoidanceChance)
        {
            // Refused entirely: no DoT, no stacks, no refresh — and not
            // silently. The transient status hands the HUD what was avoided.
            FBreakerActiveStatus Avoided;
            Avoided.Spec = Spec;
            Avoided.DamageFamily = DamageFamily;
            Avoided.Instigator = Instigator;
            OnStatusAvoided.Broadcast(Avoided);
            return;
        }
    }

    // The immunity window refuses NEW applications outright — refreshes and
    // stack adds included, because a refresh IS an application.
    if (IsStatusImmune()) return;

    if (UBreakerCombatComponent* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>())
        if (!OwnerCombat->OnDeath.IsAlreadyBound(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath))
            OwnerCombat->OnDeath.AddDynamic(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
    UBreakerManaComponent* SourceMana = Instigator ? Instigator->FindComponentByClass<UBreakerManaComponent>() : nullptr;

    for (FBreakerActiveStatus& Active : ActiveStatuses)
    {
        if (Active.Spec.StatusTag == Spec.StatusTag)
        {
            Active.Stacks = bEffectOnly ? 1 : FMath::Min(Active.Stacks + FMath::Max(1, Spec.InitialStacks), GetEffectiveStackCap());
            Active.RemainingDuration = FMath::Max(Active.RemainingDuration, ScaledDuration);
            // Refresh credit to whoever most recently reapplied it — and the
            // facing snapshot with it, because credit and angle belong to the
            // same application.
            if (Instigator)
            {
                Active.Instigator = Instigator;
                Active.SourceLocationSnapshot = Instigator->GetActorLocation();
                Active.bHasSourceLocationSnapshot = true;
                Active.ResourceProcCoefficient = Spec.ProcCoefficient;
            }
            const FBreakerActiveStatus Applied = Active;
            if (SourceMana) SourceMana->NotifyStatusApplication(Spec, true, GetOwner());
            SpreadNewestStatus(Spec, DamageFamily, Instigator, ScaledDuration);
            OnStatusApplied.Broadcast(Applied);
            return;
        }
    }

    FBreakerActiveStatus Status;
    Status.Spec = Spec;
    Status.DamageFamily = DamageFamily;
    Status.Stacks = FMath::Clamp(Spec.InitialStacks, 1, GetEffectiveStackCap());
    Status.RemainingDuration = ScaledDuration;
    Status.TimeUntilNextTick = Spec.TickInterval;
    Status.Instigator = Instigator;
    Status.ResourceProcCoefficient = Spec.ProcCoefficient;
    // Application-time facing snapshot. Taken from the applier's position NOW,
    // not per tick: the DoT contract snapshots at application, and a tick that
    // re-read the applier's live position would let a shooter flank AFTER the
    // wound to retroactively strip armour off every remaining tick.
    if (Instigator)
    {
        Status.SourceLocationSnapshot = Instigator->GetActorLocation();
        Status.bHasSourceLocationSnapshot = true;
    }
    ActiveStatuses.Add(Status);
    if (SourceMana) SourceMana->NotifyStatusApplication(Spec, false, GetOwner());
    SpreadNewestStatus(Spec, DamageFamily, Instigator, ScaledDuration);
    OnStatusApplied.Broadcast(Status);
}

void UBreakerStatusComponent::SpreadNewestStatus(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator, float ScaledDuration)
{
    if (!Instigator || Spec.ProcCoefficient <= 0 || GetDistinctStatusTypeCount() < 2 || !GetWorld()) return;
    const UBreakerCombatComponent* SourceCombat = Instigator->FindComponentByClass<UBreakerCombatComponent>();
    const UBreakerCombatComponent* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if ((SourceCombat && SourceCombat->IsDead()) || (OwnerCombat && OwnerCombat->IsDead())) return;
    const UBreakerProgressionComponent* Progression = Instigator->FindComponentByClass<UBreakerProgressionComponent>();
    const int32 Rank = Progression ? Progression->GetNodeRank(TEXT("Caster.Multispell.Chain"), EBreakerPointCurrency::DoctrinePoints) : 0;
    if (Rank <= 0) return;
    const float Radius = Rank >= 2 ? ChainRankTwoRangeCm : ChainRankOneRangeCm;
    UBreakerStatusComponent* Nearest = nullptr;
    double NearestSquared = FMath::Square(static_cast<double>(FMath::Max(0.0f, Radius)));
    const FVector Origin = GetOwner()->GetActorLocation();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerStatusChain), true, GetOwner());
    Params.AddIgnoredActor(Instigator);
    for (TObjectIterator<UBreakerStatusComponent> It; It; ++It)
    {
        UBreakerStatusComponent* Candidate = *It;
        AActor* Actor = Candidate->GetOwner();
        if (!Actor || Actor == GetOwner() || Actor == Instigator || Actor->GetWorld() != GetWorld()
            || Actor->IsA<ABreakerCharacter>()) continue;
        const UBreakerCombatComponent* CandidateCombat = Actor->FindComponentByClass<UBreakerCombatComponent>();
        if (!CandidateCombat || CandidateCombat->IsDead()) continue;
        const double Distance = FVector::DistSquared(Origin, Actor->GetActorLocation());
        if (Distance > NearestSquared) continue;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Actor->GetActorLocation(), ECC_GameTraceChannel2, Params)
            && Hit.GetActor() != Actor) continue;
        Nearest = Candidate;
        NearestSquared = Distance;
    }
    if (!Nearest) return;
    FBreakerStatusApplicationSpec Copy = Spec;
    Copy.Duration = ScaledDuration;
    Copy.ProcCoefficient = 0;
    Nearest->ApplyStatusInternal(Copy, DamageFamily, Instigator, true);
}

void UBreakerStatusComponent::HandleAfflictedOwnerDeath()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    EntropyBuildup = 0;
    EntropyBuildupRemaining = 0;
    EntropyProtectedContributions.Reset();
    TSet<AActor*> Credited;
    // Copy before refunds can notify resource listeners and change state.
    const TArray<FBreakerActiveStatus> AtDeath = ActiveStatuses;
    for (const FBreakerActiveStatus& Status : AtDeath)
    {
        AActor* Applier = Status.Instigator.Get();
        if (!Applier || Credited.Contains(Applier)
            || (Status.RemainingDuration <= 0 && Status.Spec.StatusTag != DeliveringTickTag)
            || Status.Spec.BaseDamagePerTick <= 0 || Status.ResourceProcCoefficient <= 0) continue;
        Credited.Add(Applier);
        if (UBreakerManaComponent* Mana = Applier->FindComponentByClass<UBreakerManaComponent>()) Mana->NotifyAfflictedVictimDeath();
    }
    bool bConsumedRot = false;
    ConsumeStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")), bConsumedRot);
    for (const FBreakerActiveStatus& Status : AtDeath)
        if (const auto* Rule = BreakerStatusRules::FindRule(Status.Spec.StatusTag))
            if (!Rule->bDealsPeriodicDamage)
            {
                bool bFound = false;
                ConsumeStatus(Status.Spec.StatusTag, bFound);
            }
}

void UBreakerStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceStatuses(DeltaTime);
}

void UBreakerStatusComponent::AdvanceStatuses(float DeltaTime)
{
    // The immunity clock runs whether or not any status is live — it is a
    // window on the OWNER, not on the list.
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f) return;
    EntropyBuildupRemaining = FMath::Max(0.0f, EntropyBuildupRemaining - DeltaTime);
    if (EntropyBuildupRemaining <= 0) EntropyBuildup = 0;
    for (auto& Contribution : EntropyProtectedContributions)
        Contribution.Decay = BreakerBuildup::Advance(Contribution.Decay, DeltaTime);
    EntropyProtectedContributions.RemoveAll([](const FEntropyProtectedContribution& Contribution) { return Contribution.Decay.Amount <= 0; });
    if (StatusImmunityRemaining > 0.0f) StatusImmunityRemaining = FMath::Max(0.0f, StatusImmunityRemaining - DeltaTime);
    if (!GetOwner() || !GetOwner()->HasAuthority() || ActiveStatuses.IsEmpty()) return;
    // Lazy re-bind: BeginPlay's bind misses a combat component added after it
    // (and never runs at all on a worldless test rig). A status list with no
    // combat sink still cannot tick.
    if (!Combat) Combat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat) return;

    TArray<FGameplayTag> AdvancingTags;
    for (const FBreakerActiveStatus& Status : ActiveStatuses) AdvancingTags.Add(Status.Spec.StatusTag);
    for (const FGameplayTag Tag : AdvancingTags)
    {
        auto FindActive = [this, Tag]()
        {
            return ActiveStatuses.IndexOfByPredicate([Tag](const FBreakerActiveStatus& Entry) { return Entry.Spec.StatusTag == Tag; });
        };
        int32 Index = FindActive();
        if (Index == INDEX_NONE) continue;
        FBreakerActiveStatus& Initial = ActiveStatuses[Index];
        const float ActiveSeconds = FMath::Min(DeltaTime, FMath::Max(0.0f, Initial.RemainingDuration));
        Initial.RemainingDuration -= ActiveSeconds;
        // Status damage is owed for its whole remaining lifetime; do not discard
        // overdue damage under the zone presentation's burst guard.
        const FBreakerStatusRule* Rule = BreakerStatusRules::FindRule(Initial.Spec.StatusTag);
        const int32 Ticks = Rule && !Rule->bDealsPeriodicDamage ? 0
            : UBreakerZoneMath::ConsumeTicks(Initial.TimeUntilNextTick, ActiveSeconds, Initial.Spec.TickInterval, MAX_int32);
        int32 ExpectedDelivered = Initial.TicksDelivered;
        for (int32 TickIndex = 0; TickIndex < Ticks; ++TickIndex)
        {
            // Damage callbacks may consume/clear statuses. Re-find before each tick,
            // and never continue an old application against its replacement.
            Index = FindActive();
            if (Index == INDEX_NONE || ActiveStatuses[Index].TicksDelivered != ExpectedDelivered) break;
            FBreakerActiveStatus& Status = ActiveStatuses[Index];
            ++Status.TicksDelivered;
            ExpectedDelivered = Status.TicksDelivered;
            FBreakerStatusApplicationSpec TickSpec = Status.Spec;
            TickSpec.InitialStacks = Status.Stacks;
            FBreakerDamageRequest Tick = UBreakerDamageLibrary::MakeSnapshotDotTick(TickSpec, Status.DamageFamily, Status.TicksDelivered, Status.Instigator.Get(),
                Status.SourceLocationSnapshot, Status.bHasSourceLocationSnapshot);
            if (Tag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")))
            {
                Tick.Element = EBreakerElement::Entropy; Tick.ElementalFraction = 1.0f;
                Tick.bCanApplyElementBuildup = false;
            }
            Tick.bBypassShield = Status.DamageFamily == EBreakerDamageFamily::Physical;
            TGuardValue<FGameplayTag> DeliveringTick(DeliveringTickTag, Tag);
            Combat->ReceiveDamage(Tick);
        }
        Index = FindActive();
        if (Index != INDEX_NONE && ActiveStatuses[Index].RemainingDuration <= 0.0f)
        {
            const FBreakerActiveStatus Expired = ActiveStatuses[Index];
            ActiveStatuses.RemoveAt(Index);
            OnStatusExpired.Broadcast(Expired);
        }
    }
}

bool UBreakerStatusComponent::HasStatus(FGameplayTag StatusTag) const
{
    return ActiveStatuses.ContainsByPredicate([StatusTag](const FBreakerActiveStatus& Status) { return Status.Spec.StatusTag == StatusTag; });
}

int32 UBreakerStatusComponent::GetEffectiveStackCap() const
{
    // Never below one: a cap of zero would make ApplyStatus create a status
    // with zero stacks that ticks for nothing and never expires early.
    return FMath::Max(1, MaximumStacksPerStatus + StackCapDelta);
}

int32 UBreakerStatusComponent::GetDistinctStatusTypeCount() const
{
    // ApplyStatus already guarantees one entry per tag, so the list length IS
    // the distinct count. Counted through a set anyway: the invariant is worth
    // one allocation on a list that is never longer than a handful, and if the
    // invariant is ever broken this keeps Resonance honest instead of paying
    // out twice for the same status.
    TSet<FGameplayTag> Distinct;
    for (const FBreakerActiveStatus& Status : ActiveStatuses)
    {
        if (Status.Spec.StatusTag.IsValid()) Distinct.Add(Status.Spec.StatusTag);
    }
    return Distinct.Num();
}

TArray<FBreakerActiveStatus> UBreakerStatusComponent::ConsumeAllStatuses()
{
    TArray<FBreakerActiveStatus> Consumed;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return Consumed;

    // Move first, broadcast second. A listener that reapplies a status (Cascade
    // does exactly that) must land in an EMPTY list, not into the array being
    // iterated — otherwise the reapplication is consumed by its own detonation.
    Consumed = MoveTemp(ActiveStatuses);
    ActiveStatuses.Reset();
    for (const FBreakerActiveStatus& Status : Consumed) OnStatusConsumed.Broadcast(Status);
    return Consumed;
}

FBreakerActiveStatus UBreakerStatusComponent::ConsumeStatus(FGameplayTag StatusTag, bool& bOutFound)
{
    bOutFound = false;
    FBreakerActiveStatus Consumed;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return Consumed;

    for (int32 Index = 0; Index < ActiveStatuses.Num(); ++Index)
    {
        if (ActiveStatuses[Index].Spec.StatusTag != StatusTag) continue;
        Consumed = ActiveStatuses[Index];
        ActiveStatuses.RemoveAt(Index);
        bOutFound = true;
        OnStatusConsumed.Broadcast(Consumed);
        return Consumed;
    }
    // Consuming a status that is not present is a legal no-op. Callers branch
    // on bOutFound; returning a default-constructed status with a zero
    // magnitude means a caller that ignores the flag still detonates for zero
    // rather than for garbage.
    return Consumed;
}

void UBreakerStatusComponent::ScaleRemainingDurations(float Scalar)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const float Clamped = FMath::Max(0.0f, Scalar);
    for (int32 Index = ActiveStatuses.Num() - 1; Index >= 0; --Index)
    {
        ActiveStatuses[Index].RemainingDuration *= Clamped;
        // A scalar of zero means gone, and gone is gone by exactly one route:
        // it broadcasts as a consumption, because a caller that shortened a
        // duration to nothing did consume it.
        if (ActiveStatuses[Index].RemainingDuration <= 0.0f)
        {
            const FBreakerActiveStatus Removed = ActiveStatuses[Index];
            ActiveStatuses.RemoveAt(Index);
            OnStatusConsumed.Broadcast(Removed);
        }
    }
}

void UBreakerStatusComponent::SetStackCapDelta(int32 Delta)
{
    StackCapDelta = Delta;
    const int32 Cap = GetEffectiveStackCap();
    // Lowering the cap trims immediately. A live status carrying more stacks
    // than the cap allows would keep ticking at the old magnitude and read as
    // a broken cap.
    for (FBreakerActiveStatus& Status : ActiveStatuses) Status.Stacks = FMath::Min(Status.Stacks, Cap);
}
