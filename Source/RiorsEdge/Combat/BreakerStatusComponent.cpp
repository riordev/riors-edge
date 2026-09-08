#include "Combat/BreakerStatusComponent.h"

#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerZoneMath.h"
#include "Combat/BreakerStatusRules.h"
#include "Classes/BreakerManaComponent.h"
#include "Characters/BreakerCharacter.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Engine/World.h"
#include "UI/BreakerEntropyFeedback.h"
#include "UI/BreakerVoidFeedback.h"
#include "Combat/BreakerElementReactions.h"
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
    if (const auto* Progression = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerProgressionComponent>() : nullptr)
        Chance += Progression->GetNodeStats().AilmentAvoidancePercent / 100.0f;
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

void UBreakerStatusComponent::ApplyPierceSpread(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator)
{
    const FBreakerStatusRule* Rule = BreakerStatusRules::FindRule(Spec.StatusTag);
    if (!Rule || !Rule->bSpreadsOnPierce || Spec.ProcCoefficient != 0.0f) return;
    ApplyStatusInternal(Spec, DamageFamily, Instigator, true);
}

void UBreakerStatusComponent::ApplyStatus(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator)
{
    // Entropy is earned through accepted-hit buildup, never a carried status payload.
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))
        || Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))
        || Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"))) return;
    ApplyStatusInternal(Spec, DamageFamily, Instigator, false);
}

void UBreakerStatusComponent::ApplyStatusFromHit(const FBreakerStatusApplicationSpec& Spec,
    EBreakerDamageFamily DamageFamily, const FBreakerDamageRequest& ApplyingHit)
{
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"))
        || Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))
        || Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"))) return;
    ApplyStatusInternal(Spec, DamageFamily, ApplyingHit.Instigator.Get(), false, 0, nullptr, &ApplyingHit);
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

void UBreakerStatusComponent::SnapshotAilmentRules(FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator)
{
    const auto* Rule = BreakerStatusRules::FindRule(Spec.StatusTag);
    const bool bPhysicalAilment = DamageFamily == EBreakerDamageFamily::Physical
        && Rule && Rule->bDealsPeriodicDamage && Spec.BaseDamagePerTick > 0;
    if (!Spec.bHasAilmentRuleSnapshot)
    {
        Spec.bHasAilmentRuleSnapshot = true;
        Spec.bHemorrhageSnapshot = false;
        Spec.AdditionalStackCapSnapshot = 0;
        if (bPhysicalAilment && Instigator)
        {
            if (const auto* SourceProgression = Instigator->FindComponentByClass<UBreakerProgressionComponent>())
            {
                const auto& Stats = SourceProgression->GetNodeStats();
                Spec.bHemorrhageSnapshot = Stats.bHemorrhage;
                Spec.AdditionalStackCapSnapshot = Stats.bDeepen ? 1 : 0;
                if (Spec.bHemorrhageSnapshot)
                {
                    Spec.Duration *= .5f;
                    Spec.TickInterval *= .5f;
                }
            }
        }
    }
}

uint64 UBreakerStatusComponent::ApplyStatusInternal(const FBreakerStatusApplicationSpec& InputSpec, EBreakerDamageFamily DamageFamily, AActor* Instigator, bool bDurationAlreadyScaled, float UnpaidDamageBudget, const FVector* SourceLocationOverride, const FBreakerDamageRequest* ApplyingHit)
{
    // Caller may have passed an entry in a live status array; callbacks can
    // consume or reallocate it. Accepted application owns its payload.
    FBreakerStatusApplicationSpec Spec = InputSpec;
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Void"), false)) return 0;
    const bool bErased = Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    const bool bUnstable = Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
    const bool bRot = Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    if (IsElementTransactionActive() && (bErased || bUnstable || bRot)) return 0;
    // Only the accepted-hit kernel supplies a deferred budget. Chain, carried
    // payloads and refreshes cannot mint another copy of already-earned damage.
    if ((bErased || bUnstable) && (!FMath::IsFinite(UnpaidDamageBudget) || UnpaidDamageBudget <= 0
        || HasStatus(Spec.StatusTag) || DeliveringTickTag == Spec.StatusTag)) return 0;
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")) && HasStatus(Spec.StatusTag)) return 0;
    const FBreakerStatusRule* Rule = BreakerStatusRules::FindRule(Spec.StatusTag);
    const bool bEffectOnly = Rule && Rule->IsNonDamagingDebuff();
    if (bEffectOnly) { Spec.BaseDamagePerTick = 0; Spec.InitialStacks = 1; }
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Spec.StatusTag.IsValid()
        || !FMath::IsFinite(Spec.Duration) || !FMath::IsFinite(Spec.TickInterval)
        || !FMath::IsFinite(Spec.ProcCoefficient) || Spec.Duration <= 0.0f || Spec.TickInterval <= 0.0f) return 0;
    const UBreakerCombatComponent* TargetCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (TargetCombat && TargetCombat->IsDead()) return 0;

    // These offensive rules belong to this application, not the recipient's global cap.
    // Copies retain the rewritten clock and snapshot, so Pierce/Chain cannot halve it again.
    const bool bPhysicalAilment = DamageFamily == EBreakerDamageFamily::Physical
        && Rule && Rule->bDealsPeriodicDamage && Spec.BaseDamagePerTick > 0;
    SnapshotAilmentRules(Spec, DamageFamily, Instigator);
    for (const auto& Active : ActiveStatuses)
        if (Active.Spec.StatusTag == Spec.StatusTag
            && (Active.Spec.bHemorrhageSnapshot || Spec.bHemorrhageSnapshot)) return 0;
    const int32 ApplicationStackCap = GetEffectiveStackCap()
        + (bPhysicalAilment ? FMath::Clamp(Spec.AdditionalStackCapSnapshot, 0, 1) : 0);
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
    if (!FMath::IsFinite(ScaledDuration) || ScaledDuration <= 0.0f) return 0;
    if (bRot)
    {
        // The accepted-hit kernel supplies the finite original budget. The
        // existing Chain copy path retains its authored snapshot/tick count,
        // but future duration changes cannot create more damage for either.
        if (UnpaidDamageBudget <= 0)
            UnpaidDamageBudget = Spec.BaseDamagePerTick * FMath::Max(1, Spec.InitialStacks)
                * FMath::FloorToFloat(ScaledDuration / Spec.TickInterval);
        if (!FMath::IsFinite(UnpaidDamageBudget) || UnpaidDamageBudget <= 0
            || !FMath::IsFinite(Spec.BaseDamagePerTick) || Spec.BaseDamagePerTick <= 0) return 0;
    }

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
            return 0;
        }
    }

    // The immunity window refuses NEW applications outright — refreshes and
    // stack adds included, because a refresh IS an application.
    if (IsStatusImmune()) return 0;
    // Null consumes its one refusal before any avoidance listener can re-enter.
    if (auto* Player = Cast<ABreakerCharacter>(GetOwner()); Player && (bPhysicalAilment || bRot || bErased || bUnstable))
    {
        Player->NotifyCombatActivityBoundary(); // A direct ailment can be the first hostile event, before damage.
        if (NullCombatEpoch != Player->GetCombatEpoch()) { NullCombatEpoch = Player->GetCombatEpoch(); bNullSpent = false; }
        const auto* Progression = Player->GetProgression();
        if (!bNullSpent && Progression && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Null"))))
        {
            bNullSpent = true;
            FBreakerActiveStatus Avoided; Avoided.Spec = Spec; Avoided.DamageFamily = DamageFamily; Avoided.Instigator = Instigator;
            OnStatusAvoided.Broadcast(Avoided);
            return 0;
        }
    }

    if (UBreakerCombatComponent* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>())
        if (!OwnerCombat->OnDeath.IsAlreadyBound(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath))
            OwnerCombat->OnDeath.AddDynamic(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
    UBreakerManaComponent* SourceMana = Instigator ? Instigator->FindComponentByClass<UBreakerManaComponent>() : nullptr;

    for (FBreakerActiveStatus& Active : ActiveStatuses)
    {
        if (Active.Spec.StatusTag == Spec.StatusTag)
        {
            Active.Stacks = bEffectOnly ? 1 : FMath::Max(Active.Stacks, FMath::Min(Active.Stacks + FMath::Max(1, Spec.InitialStacks), ApplicationStackCap));
            Active.RemainingDuration = FMath::Max(Active.RemainingDuration, ScaledDuration);
            // Refresh credit to whoever most recently reapplied it — and the
            // facing snapshot with it, because credit and angle belong to the
            // same application.
            if (Instigator)
            {
                Active.Instigator = Instigator;
                Active.ThreatSource = ApplyingHit ? ApplyingHit->ThreatSource : TWeakObjectPtr<AActor>();
                Active.bHasThreatSource = ApplyingHit && (ApplyingHit->bHasThreatSource || !ApplyingHit->ThreatSource.IsExplicitlyNull());
                Active.SourceLocationSnapshot = Instigator->GetActorLocation();
                Active.bHasSourceLocationSnapshot = true;
                Active.ResourceProcCoefficient = Spec.ProcCoefficient;
            }
            const FBreakerActiveStatus Applied = Active;
            if (SourceMana) SourceMana->NotifyStatusApplication(Spec, true, GetOwner());
            SpreadNewestStatus(Spec, DamageFamily, Instigator, ScaledDuration, ApplyingHit);
            OnStatusApplied.Broadcast(Applied);
            return Applied.ApplicationSerial;
        }
    }

    UBreakerStatusComponent* LeaseSource = nullptr;
    if (bRot && Spec.bLongDarkSnapshot)
    {
        LeaseSource = Instigator ? Instigator->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
        if (LeaseSource && LeaseSource->bChangingLongDarkLease) return 0;
        // An emitted hit can outlive a respec/death; it keeps finite damage,
        // but cannot acquire a new permanent ownership lease afterwards.
        if (!LeaseSource || !LeaseSource->CanMaintainLongDark()) { Spec.bLongDarkSnapshot = false; LeaseSource = nullptr; }
    }
    FBreakerActiveStatus Status;
    Status.Spec = Spec;
    Status.UnpaidDamageBudget = (bErased || bUnstable || bRot) ? UnpaidDamageBudget : 0.0f;
    Status.InitialDamageBudget = Status.UnpaidDamageBudget;
    Status.bHasReactionCreditSnapshot = Spec.bHasReactionCreditSnapshot;
    Status.InitialReactionBudget = Spec.bHasReactionCreditSnapshot && FMath::IsFinite(Spec.ReactionCreditMultiplier)
        ? Status.InitialDamageBudget * FMath::Max(0.0f, Spec.ReactionCreditMultiplier) : Status.InitialDamageBudget;
    Status.ApplicationSerial = NextApplicationSerial++;
    Status.DamageFamily = DamageFamily;
    Status.Stacks = FMath::Clamp(Spec.InitialStacks, 1, ApplicationStackCap);
    Status.RemainingDuration = ScaledDuration;
    Status.TimeUntilNextTick = Spec.TickInterval;
    Status.Instigator = Instigator;
    Status.ThreatSource = ApplyingHit ? ApplyingHit->ThreatSource : TWeakObjectPtr<AActor>();
    Status.bHasThreatSource = ApplyingHit && (ApplyingHit->bHasThreatSource || !ApplyingHit->ThreatSource.IsExplicitlyNull());
    Status.ResourceProcCoefficient = Spec.ProcCoefficient;
    if (bRot && !Spec.bLongDarkSnapshot && Spec.ProcCoefficient > 0 && Instigator)
        if (const auto* SourceProgression = Instigator->FindComponentByClass<UBreakerProgressionComponent>())
        {
            Status.bSympathyOnExpiry = SourceProgression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Sympathy")));
            Status.SympathyDurationSnapshot = ScaledDuration;
            if (Status.bSympathyOnExpiry)
                if (auto* SourceStatus = Instigator->FindComponentByClass<UBreakerStatusComponent>())
                    if (auto* SourceCombat = Instigator->FindComponentByClass<UBreakerCombatComponent>())
                        SourceCombat->OnDeath.AddUniqueDynamic(SourceStatus, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
        }
    // Application-time facing snapshot. Taken from the applier's position NOW,
    // not per tick: the DoT contract snapshots at application, and a tick that
    // re-read the applier's live position would let a shooter flank AFTER the
    // wound to retroactively strip armour off every remaining tick.
    if (SourceLocationOverride && !SourceLocationOverride->ContainsNaN())
    {
        Status.SourceLocationSnapshot = *SourceLocationOverride;
        Status.bHasSourceLocationSnapshot = true;
    }
    else if (Instigator)
    {
        Status.SourceLocationSnapshot = Instigator->GetActorLocation();
        Status.bHasSourceLocationSnapshot = true;
    }
    if (LeaseSource)
    {
        Status.bPersistentRot = true;
        Status.LongDarkSource = LeaseSource;
        if (!LeaseSource->ReserveLongDarkLease(this, Status.ApplicationSerial)) return 0;
        GetOwner()->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleLongDarkOwnerDestroyed);
    }
    ActiveStatuses.Add(Status);
    if (Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")))
        BreakerEntropyFeedback::PlayActivation(GetOwner(), Instigator);
    if (bErased) BreakerVoidFeedback::PlayActivation(GetOwner(), Instigator);
    if (SourceMana) SourceMana->NotifyStatusApplication(Spec, false, GetOwner());
    SpreadNewestStatus(Spec, DamageFamily, Instigator, ScaledDuration, ApplyingHit);
    OnStatusApplied.Broadcast(Status);
    return Status.ApplicationSerial;
}

void UBreakerStatusComponent::SpreadNewestStatus(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator, float ScaledDuration, const FBreakerDamageRequest* ApplyingHit)
{
    if (Spec.bLongDarkSnapshot || Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"))
        || Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"))) return;
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
    Nearest->ApplyStatusInternal(Copy, DamageFamily, Instigator, true, 0, nullptr, ApplyingHit);
}

void UBreakerStatusComponent::SpreadExpiredRot(const FBreakerActiveStatus& Expired)
{
    if (!Expired.bSympathyOnExpiry || Expired.bPersistentRot || !GetOwner() || !GetOwner()->HasAuthority()
        || GetOwner()->IsActorBeingDestroyed() || !GetWorld() || Expired.InitialDamageBudget <= 0) return;
    AActor* Source = Expired.Instigator.Get();
    const auto* SourceCombat = IsValid(Source) ? Source->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const auto* OwnerCombat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (!SourceCombat || SourceCombat->IsDead() || Source->IsActorBeingDestroyed() || !OwnerCombat || OwnerCombat->IsDead()) return;
    UBreakerStatusComponent* Nearest = nullptr;
    double NearestSquared = FMath::Square(400.0); // O2 PLACEHOLDER, authored 4m Sympathy radius.
    const FVector Origin = GetOwner()->GetActorLocation();
    for (TObjectIterator<UBreakerStatusComponent> It; It; ++It)
    {
        auto* Candidate = *It; auto* Enemy = Cast<ABreakerEnemy>(Candidate->GetOwner());
        if (!IsValid(Enemy) || Enemy == GetOwner() || Enemy->IsActorBeingDestroyed() || Enemy->GetWorld() != GetWorld()
            || Candidate->HasStatus(Expired.Spec.StatusTag) || Candidate->IsStatusImmune()) continue;
        const auto* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        if (!EnemyCombat || EnemyCombat->IsDead()) continue;
        const double Distance = FVector::DistSquared(Origin,Enemy->GetActorLocation());
        if (Distance > NearestSquared || (Distance == NearestSquared && Nearest
            && Enemy->GetUniqueID() >= Nearest->GetOwner()->GetUniqueID())) continue;
        Nearest = Candidate; NearestSquared = Distance;
    }
    if (!Nearest) return;
    FBreakerStatusApplicationSpec Copy = Expired.Spec;
    Copy.Duration = Expired.SympathyDurationSnapshot;
    Copy.ProcCoefficient = 0; Copy.bLongDarkSnapshot = false;
    FBreakerDamageRequest Attribution; Attribution.SetInstigator(Source); Expired.CopyThreatTo(Attribution);
    Nearest->ApplyStatusInternal(Copy,Expired.DamageFamily,Source,true,Expired.InitialDamageBudget,
        Expired.bHasSourceLocationSnapshot ? &Expired.SourceLocationSnapshot : nullptr,&Attribution);
}

void UBreakerStatusComponent::HandleAfflictedOwnerDeath()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    // Death permanently withdraws pending expiry copies, even if either actor
    // revives before its ordinary Rot clock finishes.
    for (TObjectIterator<UBreakerStatusComponent> It; It; ++It)
        if (It->GetWorld() == GetWorld())
            for (auto& Active : It->ActiveStatuses)
                if (*It == this || Active.Instigator.Get() == GetOwner()) Active.bSympathyOnExpiry = false;
    // Keep the transaction guard until its owning outer hit flushes, even
    // if another callback revives this actor before that flush.
    PendingReactionStatus.UnpaidDamageBudget = 0;
    EntropyBuildup = 0;
    EntropyBuildupRemaining = 0;
    EntropyProtectedContributions.Reset();
    ResetVoidBuildup();
    ResetRiftBuildup();
    TSet<AActor*> Credited;
    // Copy before refunds can notify resource listeners and change state.
    const TArray<FBreakerActiveStatus> AtDeath = ActiveStatuses;
    for (const FBreakerActiveStatus& Status : AtDeath)
    {
        AActor* Applier = Status.Instigator.Get();
        if (!Applier || Credited.Contains(Applier)
            || (Status.RemainingDuration <= 0 && !Status.bPersistentRot && Status.Spec.StatusTag != DeliveringTickTag)
            || (Status.Spec.BaseDamagePerTick <= 0 && Status.UnpaidDamageBudget <= 0
                && Status.Spec.StatusTag != DeliveringTickTag)
            || Status.ResourceProcCoefficient <= 0) continue;
        Credited.Add(Applier);
        if (UBreakerManaComponent* Mana = Applier->FindComponentByClass<UBreakerManaComponent>()) Mana->NotifyAfflictedVictimDeath();
    }
    bool bConsumedRot = false;
    ConsumeStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")), bConsumedRot);
    bool bConsumedErased = false;
    ConsumeStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Erased")), bConsumedErased);
    bool bConsumedUnstable = false;
    ConsumeStatus(FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable")), bConsumedUnstable);
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
    if (bAdvancingStatuses || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f) return;
    TGuardValue<bool> AdvancingStatuses(bAdvancingStatuses, true);
    const auto* Player = Cast<ABreakerCharacter>(GetOwner());
    const auto* Progression = Player ? Player->GetProgression() : nullptr;
    const float BuildupSeconds = DeltaTime * (Progression && Progression->HasNodeTag(FGameplayTag::RequestGameplayTag(TEXT("Progression.Node.Core.Insulation"))) ? 2.0f : 1.0f);
    EntropyBuildupRemaining = FMath::Max(0.0f, EntropyBuildupRemaining - BuildupSeconds);
    if (EntropyBuildupRemaining <= 0) EntropyBuildup = 0;
    for (auto& Contribution : EntropyProtectedContributions)
        Contribution.Decay = BreakerBuildup::Advance(Contribution.Decay, BuildupSeconds);
    EntropyProtectedContributions.RemoveAll([](const FEntropyProtectedContribution& Contribution) { return Contribution.Decay.Amount <= 0; });
    AdvanceVoidBuildup(BuildupSeconds);
    AdvanceRiftBuildup(BuildupSeconds);
    if (StatusImmunityRemaining > 0.0f) StatusImmunityRemaining = FMath::Max(0.0f, StatusImmunityRemaining - DeltaTime);
    if (!GetOwner() || !GetOwner()->HasAuthority() || ActiveStatuses.IsEmpty()) return;
    // Lazy re-bind: BeginPlay's bind misses a combat component added after it
    // (and never runs at all on a worldless test rig). A status list with no
    // combat sink still cannot tick.
    if (!Combat) Combat = GetOwner()->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat) return;

    TArray<uint64> AdvancingSerials;
    for (const FBreakerActiveStatus& Status : ActiveStatuses) AdvancingSerials.Add(Status.ApplicationSerial);
    for (const uint64 ApplicationSerial : AdvancingSerials)
    {
        auto FindActive = [this, ApplicationSerial]()
        {
            return ActiveStatuses.IndexOfByPredicate([ApplicationSerial](const FBreakerActiveStatus& Entry) { return Entry.ApplicationSerial == ApplicationSerial; });
        };
        int32 Index = FindActive();
        if (Index == INDEX_NONE) continue;
        FBreakerActiveStatus& Initial = ActiveStatuses[Index];
        const FGameplayTag Tag = Initial.Spec.StatusTag;
        // A damage callback may advance the component recursively. The
        // currently dispatched application has already claimed its payment.
        if (Tag == DeliveringTickTag) continue;
        if (Tag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")))
        {
            AdvanceRotStatus(ApplicationSerial, DeltaTime);
            continue;
        }
        const float ActiveSeconds = FMath::Min(DeltaTime, FMath::Max(0.0f, Initial.RemainingDuration));
        Initial.RemainingDuration -= ActiveSeconds;
        // Status damage is owed for its whole remaining lifetime; do not discard
        // overdue damage under the zone presentation's burst guard.
        const FBreakerStatusRule* Rule = BreakerStatusRules::FindRule(Initial.Spec.StatusTag);
        if (Rule && Rule->bDealsDamageOnExpiry)
        {
            if (Initial.RemainingDuration <= 0.0f) DeliverVoidBurst(ApplicationSerial);
            continue;
        }
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
            Tick.bBypassShield = Status.DamageFamily == EBreakerDamageFamily::Physical;
            Status.CopyThreatTo(Tick);
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
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    for (FBreakerActiveStatus& Status : ActiveStatuses)
        if (Status.Spec.StatusTag == Rot)
            Status.UnpaidDamageBudget = BreakerElementReactions::RemainingRotBudget(Status);
    Consumed = MoveTemp(ActiveStatuses);
    ActiveStatuses.Reset();
    for (const FBreakerActiveStatus& Status : Consumed) ReleaseLongDarkLink(Status);
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
        if (StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")))
            ActiveStatuses[Index].UnpaidDamageBudget = BreakerElementReactions::RemainingRotBudget(ActiveStatuses[Index]);
        Consumed = ActiveStatuses[Index];
        ActiveStatuses.RemoveAt(Index);
        bOutFound = true;
        ReleaseLongDarkLink(Consumed);
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
    if (!GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(Scalar)) return;
    const float Clamped = FMath::Max(0.0f, Scalar);
    TArray<uint64> Serials;
    for (const FBreakerActiveStatus& Status : ActiveStatuses) Serials.Add(Status.ApplicationSerial);
    for (uint64 Serial : Serials)
    {
        const int32 Index = ActiveStatuses.IndexOfByPredicate([Serial](const FBreakerActiveStatus& Status)
        { return Status.ApplicationSerial == Serial; });
        if (Index == INDEX_NONE) continue;
        ActiveStatuses[Index].RemainingDuration *= Clamped;
        if (ActiveStatuses[Index].Spec.StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")))
            ActiveStatuses[Index].UnpaidDamageBudget = BreakerElementReactions::RemainingRotBudget(ActiveStatuses[Index]);
        // A scalar of zero means gone, and gone is gone by exactly one route:
        // it broadcasts as a consumption, because a caller that shortened a
        // duration to nothing did consume it.
        if (ActiveStatuses[Index].RemainingDuration <= 0.0f && (!ActiveStatuses[Index].bPersistentRot || Clamped == 0.0f))
        {
            const FBreakerActiveStatus Removed = ActiveStatuses[Index];
            ActiveStatuses.RemoveAt(Index);
            ReleaseLongDarkLink(Removed);
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
