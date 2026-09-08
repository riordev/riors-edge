#include "Combat/BreakerRift.h"
#include "Combat/BreakerElementSourceMath.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerRiftDisplacement.h"
#include "Combat/BreakerEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Data/BreakerDataFile.h"
#include "UI/BreakerRiftFeedback.h"

namespace
{
    struct FBreakerRiftTuning
    {
        float Threshold = 0, Damage = 0, Marker = 0, Timeout = 0, Displacement = 0;
        float ImpactFraction = 0, HardLandingFraction = 0;
    };

    const FBreakerRiftTuning& BreakerRiftTuning()
    {
        static const FBreakerRiftTuning Tuning = []
        {
            FBreakerRiftTuning Value;
            BreakerDataFile::FBreakerDataErrors Errors;
            const auto Data = BreakerDataFile::Load(TEXT("Data/elements.json"), Errors);
            auto Read = [&](const TCHAR* Key, float& Field, double Maximum)
            {
                double Number = 0;
                if (!Data || !Data->TryGetNumberField(Key, Number) || !FMath::IsFinite(Number)
                    || Number <= 0 || Number > Maximum || !FMath::IsFinite(static_cast<float>(Number)))
                    Errors.Add(FString::Printf(TEXT("Invalid Rift tuning: %s"), Key));
                else Field = static_cast<float>(Number);
            };
            Read(TEXT("riftThresholdHealthFraction"), Value.Threshold, 1);
            Read(TEXT("riftDamageFraction"), Value.Damage, 1);
            Read(TEXT("riftMarkerSeconds"), Value.Marker, 3600);
            Read(TEXT("riftBuildupTimeoutSeconds"), Value.Timeout, 3600);
            Read(TEXT("riftDisplacementCm"), Value.Displacement, 10000);
            Read(TEXT("riftImpactDamageFraction"), Value.ImpactFraction, 1);
            Read(TEXT("riftHardLandingDamageFraction"), Value.HardLandingFraction, 1);
            if (!ensureAlwaysMsgf(Errors.IsClean(), TEXT("%s"), *Errors.Join())) return FBreakerRiftTuning();
            return Value;
        }();
        return Tuning;
    }
}

namespace
{
    TWeakObjectPtr<AActor> BreakerFindRiftImpact(AActor* Target, AActor* Applier, const BreakerRiftDisplacement::FResult& Move)
    {
        if (!Target || !Target->GetWorld() || Move.DistanceCm <= 0) return nullptr;
        const auto* Capsule = Cast<UCapsuleComponent>(Target->GetRootComponent());
        if (!Capsule) return nullptr;
        TWeakObjectPtr<AActor> Nearest;
        float NearestDistance = TNumericLimits<float>::Max();
        auto Consider = [&](AActor* Candidate, float Distance)
        {
            auto* Enemy = Cast<ABreakerEnemy>(Candidate);
            auto* Sink = IsValid(Enemy) ? Enemy->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
            if (!Sink || Sink->IsDead() || Enemy->IsActorBeingDestroyed() || Enemy == Target || Enemy == Applier) return false;
            if (!Nearest.IsValid() || Distance < NearestDistance - UE_KINDA_SMALL_NUMBER
                || (FMath::IsNearlyEqual(Distance, NearestDistance) && Enemy->GetUniqueID() < Nearest->GetUniqueID()))
            { Nearest = Enemy; NearestDistance = Distance; }
            return true;
        };
        TArray<FHitResult> Hits;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerRiftImpact), false, Target);
        if (Applier) Query.AddIgnoredActor(Applier);
        // Channel sweeps report only their first blocking actor. Repeat while
        // excluding encountered enemies so equal-distance blockers share the
        // same stable selection policy; geometry remains blocking throughout.
        bool bFoundEnemy = false;
        do
        {
            Hits.Reset(); bFoundEnemy = false;
            Target->GetWorld()->SweepMultiByChannel(Hits, Move.Start, Move.End, Capsule->GetComponentQuat(),
                ECC_GameTraceChannel2, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query);
            for (const auto& Hit : Hits)
            {
                if (Consider(Hit.GetActor(), FMath::Max(0.0f, Hit.Time) * Move.DistanceCm))
                { Query.AddIgnoredActor(Hit.GetActor()); bFoundEnemy = true; }
            }
        } while (bFoundEnemy);
        // The movement stops two centimetres before a blocking capsule. Its
        // terminal enemy still receives the collision, even if its hitbox is smaller.
        Consider(Move.TerminalActor.Get(), Move.DistanceCm);
        return Nearest;
    }
}
float BreakerRift::ThresholdHealthFraction() { return BreakerRiftTuning().Threshold; }
float BreakerRift::DamageFraction() { return BreakerRiftTuning().Damage; }
float BreakerRift::MarkerSeconds() { return BreakerRiftTuning().Marker; }
float BreakerRift::BuildupTimeoutSeconds() { return BreakerRiftTuning().Timeout; }
float BreakerRift::DisplacementCm() { return BreakerRiftTuning().Displacement; }

float UBreakerStatusComponent::GetRiftBuildup() const
{
    float Total = RiftBuildup + DeferredElementBuildup(EBreakerElement::Rift);
    for (const auto& Contribution : RiftProtectedContributions) Total += Contribution.Decay.Amount;
    return Total;
}

float UBreakerStatusComponent::GetRiftThreshold() const
{
    const auto* Sink = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    return Sink ? Sink->GetMaxHealth() * BreakerRift::ThresholdHealthFraction() : 0.0f;
}

float UBreakerStatusComponent::GetRiftResistancePercent() const
{
    float Value = FMath::IsFinite(RiftResistancePercent) ? RiftResistancePercent : 0;
    if (const auto* Gear = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr)
        Value += Gear->GetStats().ElementalResistancePercent;
    if (const auto* Progression = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerProgressionComponent>() : nullptr)
        Value += Progression->GetNodeStats().ElementalResistancePercent;
    return FMath::Clamp(Value, 0.0f, 100.0f);
}

void UBreakerStatusComponent::ResetRiftBuildup()
{
    RiftBuildup = 0;
    RiftBuildupRemaining = 0;
    RiftProtectedContributions.Reset();
}

void UBreakerStatusComponent::AdvanceRiftBuildup(float DeltaSeconds)
{
    RiftBuildupRemaining = FMath::Max(0.0f, RiftBuildupRemaining - DeltaSeconds);
    if (RiftBuildupRemaining <= 0) RiftBuildup = 0;
    for (auto& Contribution : RiftProtectedContributions)
        Contribution.Decay = BreakerBuildup::Advance(Contribution.Decay, DeltaSeconds);
    RiftProtectedContributions.RemoveAll([](const FEntropyProtectedContribution& Contribution)
    { return Contribution.Decay.Amount <= 0; });
}

uint64 UBreakerStatusComponent::ApplyRiftHit(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
{
    const auto* Sink = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const auto Unstable = FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
    if (IsElementTransactionActive() || !GetOwner() || !GetOwner()->HasAuthority() || !Sink || Sink->IsDead() || Result.bKilled
        || Request.Element != EBreakerElement::Rift || !Request.bCanApplyElementBuildup
        || Request.bIsDamageOverTime || Result.bDodged || Result.bParried || IsStatusImmune()
        || DeliveringTickTag == Unstable
        || !FMath::IsFinite(Result.RawDamage) || !FMath::IsFinite(Request.ProcCoefficient)
        || !FMath::IsFinite(Request.ElementalFraction) || Result.RawDamage <= 0) return 0;
    const float Snapshot = BreakerElementSource::RawPart(Request, Result);
    const float Budget = BreakerElementSource::StatusBudget(Request, Snapshot * BreakerRift::DamageFraction());
    const float Threshold = BreakerElementSource::Threshold(Request, GetRiftThreshold());
    if (!FMath::IsFinite(Budget) || Budget <= 0 || !FMath::IsFinite(Threshold) || Threshold <= 0) return 0;
    const float Bonus = FMath::IsFinite(Request.ElementBuildupFlat) ? FMath::Max(0.0f, Request.ElementBuildupFlat) : 0;
    const float Ordinary = Result.ShieldDamage + Result.HealthDamage > 0 ? Snapshot : 0;
    const float ComputedBuildup = (Ordinary + Bonus) * FMath::Clamp(Request.ProcCoefficient, 0.0f, 1.0f)
        * BreakerElementSource::BuildupMultiplier(Request)
        * BreakerElementSource::ResistanceFactor(Request, GetRiftResistancePercent());
    if (DeferSympatheticElement(Request, Result, ComputedBuildup, BreakerRift::BuildupTimeoutSeconds())) return 0;
    if (HasStatus(Unstable)) return 0; // Existing finite applications never refresh.
    const float Buildup = ClaimedElementBuildup(Request, ComputedBuildup);
    if (!FMath::IsFinite(Buildup) || Buildup <= 0) return 0;
    if (FMath::IsFinite(Request.ElementBuildupFadeSeconds) && Request.ElementBuildupFadeSeconds > 0)
    {
        auto* Contribution = RiftProtectedContributions.FindByPredicate([&](const FEntropyProtectedContribution& Value)
        { return Value.Applier == Request.Instigator; });
        if (!Contribution)
        {
            Contribution = &RiftProtectedContributions.AddDefaulted_GetRef();
            Contribution->Applier = Request.Instigator;
        }
        Contribution->Decay.Amount += Buildup;
        Contribution->Decay.GraceRemaining = BreakerRift::BuildupTimeoutSeconds();
        Contribution->Decay.FadeRemaining = Request.ElementBuildupFadeSeconds;
    }
    else
    {
        RiftBuildup += Buildup;
        RiftBuildupRemaining = BreakerRift::BuildupTimeoutSeconds();
    }
    if (!Sink->OnDeath.IsAlreadyBound(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath))
        const_cast<UBreakerCombatComponent*>(Sink)->OnDeath.AddDynamic(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
    if (GetRiftBuildup() - DeferredElementBuildup(EBreakerElement::Rift) < Threshold) return 0;
    ResetRiftBuildup();
    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = Unstable;
    Spec.bVectorFieldSnapshot = Request.ElementSource.bVectorField;
    Spec.bRiftImpactSnapshot = Request.ElementSource.bRiftImpact;
    Spec.bHardLandingSnapshot = Request.ElementSource.bHardLanding;
    Spec.Duration = BreakerRift::MarkerSeconds();
    Spec.TickInterval = Spec.Duration;
    Spec.BaseDamagePerTick = 0;
    Spec.ProcCoefficient = FMath::Clamp(Request.ProcCoefficient, 0.0f, 1.0f);
    Spec.Snapshot.SourcePower = 1;
    Spec.Snapshot.CriticalChance = 0;
    Spec.Snapshot.bRolledCritical = false;
    Spec.Snapshot.SourceTags = Request.SourceTags;
    BreakerElementSource::SnapshotReactionCredit(Request, Spec);
    const FVector* SourceLocation = Request.bHasSourceLocation && !Request.SourceLocation.ContainsNaN()
        ? &Request.SourceLocation : nullptr;
    return ApplyStatusInternal(Spec, EBreakerDamageFamily::Elemental, Request.Instigator.Get(), true, Budget, SourceLocation, &Request);
}

void UBreakerStatusComponent::FlushRiftActivation(uint64 ApplicationSerial)
{
    AActor* Target = GetOwner();
    if (!Target || !Target->HasAuthority() || Target->IsActorBeingDestroyed() || ApplicationSerial == 0) return;
    if (!Combat) Combat = Target->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat || Combat->IsDead()) return;
    const auto Unstable = FGameplayTag::RequestGameplayTag(TEXT("Status.Unstable"));
    if (DeliveringTickTag == Unstable) return;
    const int32 Index = ActiveStatuses.IndexOfByPredicate([&](const FBreakerActiveStatus& Entry)
    { return Entry.ApplicationSerial == ApplicationSerial && Entry.Spec.StatusTag == Unstable; });
    if (Index == INDEX_NONE) return;
    const FBreakerActiveStatus Status = ActiveStatuses[Index];
    if (Status.RemainingDuration <= 0 || !FMath::IsFinite(Status.UnpaidDamageBudget) || Status.UnpaidDamageBudget <= 0) return;
    // Both the movement and the damage are claimed before any overlap/hit
    // callback. The marker remains after payment, with no recyclable budget.
    ActiveStatuses[Index].UnpaidDamageBudget = 0;
    ++ActiveStatuses[Index].TicksDelivered;
    TGuardValue<FGameplayTag> Delivering(DeliveringTickTag, Unstable);
    // O2: existing stagger immunity refuses displacement only, never damage.
    BreakerRiftDisplacement::FResult Displacement;
    if (!Combat->IsStaggerImmune() && Status.bHasSourceLocationSnapshot)
        Displacement = BreakerRiftDisplacement::ApplyDetailed(Target, Status.SourceLocationSnapshot, BreakerRift::DisplacementCm(), Status.Spec.bVectorFieldSnapshot);
    if (!IsValid(Target) || Target->IsActorBeingDestroyed() || Combat->IsDead()) return;
    FBreakerStatusApplicationSpec BurstSpec = Status.Spec;
    BurstSpec.BaseDamagePerTick = Status.UnpaidDamageBudget;
    BurstSpec.InitialStacks = 1;
    FBreakerDamageRequest Burst = UBreakerDamageLibrary::MakeSnapshotDotTick(BurstSpec,
        EBreakerDamageFamily::Elemental, 1, Status.Instigator.Get(),
        Status.SourceLocationSnapshot, Status.bHasSourceLocationSnapshot);
    Burst.Element = EBreakerElement::Rift;
    Status.CopyThreatTo(Burst);
    Burst.ElementalFraction = 1;
    Burst.bCanApplyElementBuildup = false;
    Burst.bCanCritical = false;
    Burst.bBypassShield = false;
    // These local entitlements belong to the already-claimed activation. No
    // callback can discover an unpaid secondary budget on the live marker.
    const TWeakObjectPtr<AActor> ImpactTarget = Status.Spec.bRiftImpactSnapshot
        ? BreakerFindRiftImpact(Target, Status.Instigator.Get(), Displacement) : TWeakObjectPtr<AActor>();
    const float WallDamage = Status.Spec.bHardLandingSnapshot && Displacement.bReachedWall
        ? Status.UnpaidDamageBudget * BreakerRiftTuning().HardLandingFraction : 0;
    BreakerRiftFeedback::PlayActivation(Target, Status.Instigator.Get());
    Combat->ReceiveDamage(Burst);
    FBreakerDamageRequest Extra = Burst;
    Extra.ProcCoefficient = 0;
    if (WallDamage > 0 && IsValid(Target) && !Target->IsActorBeingDestroyed() && IsValid(Combat) && !Combat->IsDead())
    {
        Extra.BaseDamage = WallDamage;
        Combat->ReceiveDamage(Extra);
    }
    AActor* ImpactActor = ImpactTarget.Get();
    auto* ImpactCombat = IsValid(ImpactActor) && !ImpactActor->IsActorBeingDestroyed()
        ? ImpactActor->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (ImpactCombat && !ImpactCombat->IsDead())
    {
        Extra.BaseDamage = Status.UnpaidDamageBudget * BreakerRiftTuning().ImpactFraction;
        ImpactCombat->ReceiveDamage(Extra);
    }
}
