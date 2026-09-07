#include "Combat/BreakerVoid.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Data/BreakerDataFile.h"
#include "UI/BreakerVoidFeedback.h"

namespace
{
    struct FBreakerVoidTuning
    {
        float Threshold = 0, Damage = 0, Delay = 0, Timeout = 0;
    };

    const FBreakerVoidTuning& BreakerVoidTuning()
    {
        static const FBreakerVoidTuning Tuning = []
        {
            FBreakerVoidTuning Value;
            BreakerDataFile::FBreakerDataErrors Errors;
            const auto Data = BreakerDataFile::Load(TEXT("Data/elements.json"), Errors);
            auto Read = [&](const TCHAR* Key, float& Field, double Maximum)
            {
                double Number = 0;
                if (!Data || !Data->TryGetNumberField(Key, Number) || !FMath::IsFinite(Number)
                    || Number <= 0 || Number > Maximum || !FMath::IsFinite(static_cast<float>(Number)))
                    Errors.Add(FString::Printf(TEXT("Invalid Void tuning: %s"), Key));
                else Field = static_cast<float>(Number);
            };
            Read(TEXT("voidThresholdHealthFraction"), Value.Threshold, 1);
            Read(TEXT("voidDamageFraction"), Value.Damage, 1);
            Read(TEXT("voidDelaySeconds"), Value.Delay, 3600);
            Read(TEXT("voidBuildupTimeoutSeconds"), Value.Timeout, 3600);
            if (!ensureAlwaysMsgf(Errors.IsClean(), TEXT("%s"), *Errors.Join())) return FBreakerVoidTuning();
            return Value;
        }();
        return Tuning;
    }
}

float BreakerVoid::ThresholdHealthFraction() { return BreakerVoidTuning().Threshold; }
float BreakerVoid::DamageFraction() { return BreakerVoidTuning().Damage; }
float BreakerVoid::DelaySeconds() { return BreakerVoidTuning().Delay; }
float BreakerVoid::BuildupTimeoutSeconds() { return BreakerVoidTuning().Timeout; }

float UBreakerStatusComponent::GetVoidBuildup() const
{
    float Total = VoidBuildup;
    for (const auto& Contribution : VoidProtectedContributions) Total += Contribution.Decay.Amount;
    return Total;
}

float UBreakerStatusComponent::GetVoidThreshold() const
{
    const auto* Sink = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    return Sink ? Sink->GetMaxHealth() * BreakerVoid::ThresholdHealthFraction() : 0.0f;
}

float UBreakerStatusComponent::GetVoidResistancePercent() const
{
    float Value = FMath::IsFinite(VoidResistancePercent) ? VoidResistancePercent : 0;
    if (const auto* Gear = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr)
        Value += Gear->GetStats().ElementalResistancePercent;
    return FMath::Clamp(Value, 0.0f, 100.0f);
}

void UBreakerStatusComponent::ResetVoidBuildup()
{
    VoidBuildup = 0;
    VoidBuildupRemaining = 0;
    VoidProtectedContributions.Reset();
}

void UBreakerStatusComponent::AdvanceVoidBuildup(float DeltaSeconds)
{
    VoidBuildupRemaining = FMath::Max(0.0f, VoidBuildupRemaining - DeltaSeconds);
    if (VoidBuildupRemaining <= 0) VoidBuildup = 0;
    for (auto& Contribution : VoidProtectedContributions)
        Contribution.Decay = BreakerBuildup::Advance(Contribution.Decay, DeltaSeconds);
    VoidProtectedContributions.RemoveAll([](const FEntropyProtectedContribution& Contribution)
    { return Contribution.Decay.Amount <= 0; });
}

void UBreakerStatusComponent::ApplyVoidHit(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
{
    const auto* Sink = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const auto Erased = FGameplayTag::RequestGameplayTag(TEXT("Status.Erased"));
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Sink || Sink->IsDead() || Result.bKilled
        || Request.Element != EBreakerElement::Void || !Request.bCanApplyElementBuildup
        || Request.bIsDamageOverTime || Result.bDodged || Result.bParried || IsStatusImmune()
        || HasStatus(Erased) || DeliveringTickTag == Erased
        || !FMath::IsFinite(Result.RawDamage) || !FMath::IsFinite(Request.ProcCoefficient)
        || !FMath::IsFinite(Request.ElementalFraction) || Result.RawDamage <= 0) return;
    const float Snapshot = Result.RawDamage * FMath::Clamp(Request.ElementalFraction, 0.0f, 1.0f);
    const float Budget = Snapshot * BreakerVoid::DamageFraction();
    const float Threshold = GetVoidThreshold();
    if (!FMath::IsFinite(Budget) || Budget <= 0 || !FMath::IsFinite(Threshold) || Threshold <= 0) return;
    const float Bonus = FMath::IsFinite(Request.ElementBuildupFlat) ? FMath::Max(0.0f, Request.ElementBuildupFlat) : 0;
    const float Ordinary = Result.ShieldDamage + Result.HealthDamage > 0 ? Snapshot : 0;
    const float Buildup = (Ordinary + Bonus) * FMath::Clamp(Request.ProcCoefficient, 0.0f, 1.0f)
        * (1.0f - GetVoidResistancePercent() / 100.0f);
    if (!FMath::IsFinite(Buildup) || Buildup <= 0) return;
    if (FMath::IsFinite(Request.ElementBuildupFadeSeconds) && Request.ElementBuildupFadeSeconds > 0)
    {
        auto* Contribution = VoidProtectedContributions.FindByPredicate([&](const FEntropyProtectedContribution& Value)
        { return Value.Applier == Request.Instigator; });
        if (!Contribution)
        {
            Contribution = &VoidProtectedContributions.AddDefaulted_GetRef();
            Contribution->Applier = Request.Instigator;
        }
        Contribution->Decay.Amount += Buildup;
        Contribution->Decay.GraceRemaining = BreakerVoid::BuildupTimeoutSeconds();
        Contribution->Decay.FadeRemaining = Request.ElementBuildupFadeSeconds;
    }
    else
    {
        VoidBuildup += Buildup;
        VoidBuildupRemaining = BreakerVoid::BuildupTimeoutSeconds();
    }
    if (!Sink->OnDeath.IsAlreadyBound(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath))
        const_cast<UBreakerCombatComponent*>(Sink)->OnDeath.AddDynamic(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
    if (GetVoidBuildup() < Threshold) return;
    // The applying hit earns the damage. Accumulated buildup, bonuses and
    // resistance decide when it activates, never how much damage it pays.
    ResetVoidBuildup();
    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = Erased;
    Spec.Duration = BreakerVoid::DelaySeconds();
    Spec.TickInterval = Spec.Duration;
    Spec.BaseDamagePerTick = 0;
    Spec.ProcCoefficient = FMath::Clamp(Request.ProcCoefficient, 0.0f, 1.0f);
    Spec.Snapshot.SourcePower = 1;
    Spec.Snapshot.CriticalChance = 0;
    Spec.Snapshot.bRolledCritical = false;
    Spec.Snapshot.SourceTags = Request.SourceTags;
    ApplyStatusInternal(Spec, EBreakerDamageFamily::Elemental, Request.Instigator.Get(), true, Budget);
}

void UBreakerStatusComponent::DeliverVoidBurst(uint64 ApplicationSerial)
{
    auto FindApplication = [this, ApplicationSerial]()
    { return ActiveStatuses.IndexOfByPredicate([ApplicationSerial](const FBreakerActiveStatus& Value)
        { return Value.ApplicationSerial == ApplicationSerial; }); };
    const int32 Index = FindApplication();
    if (Index == INDEX_NONE || !Combat || Combat->IsDead()) return;
    const FBreakerActiveStatus Status = ActiveStatuses[Index];
    if (!FMath::IsFinite(Status.UnpaidDamageBudget) || Status.UnpaidDamageBudget <= 0) return;
    // Claim payment while retaining the inflicted status during lethal-hit
    // callbacks. Attrition can credit it; a consumer can only retrieve zero.
    ActiveStatuses[Index].UnpaidDamageBudget = 0;
    ++ActiveStatuses[Index].TicksDelivered;
    FBreakerStatusApplicationSpec BurstSpec = Status.Spec;
    BurstSpec.BaseDamagePerTick = Status.UnpaidDamageBudget;
    BurstSpec.InitialStacks = 1;
    FBreakerDamageRequest Burst = UBreakerDamageLibrary::MakeSnapshotDotTick(BurstSpec,
        EBreakerDamageFamily::Elemental, 1, Status.Instigator.Get(),
        Status.SourceLocationSnapshot, Status.bHasSourceLocationSnapshot);
    Burst.Element = EBreakerElement::Void;
    Burst.ElementalFraction = 1;
    Burst.bCanApplyElementBuildup = false;
    Burst.bCanCritical = false;
    Burst.bBypassShield = false;
    {
        TGuardValue<FGameplayTag> Delivering(DeliveringTickTag, Status.Spec.StatusTag);
        BreakerVoidFeedback::PlayBurst(GetOwner(), Status.Instigator.Get());
        Combat->ReceiveDamage(Burst);
    }
    const int32 RemainingIndex = FindApplication();
    if (RemainingIndex != INDEX_NONE)
    {
        const FBreakerActiveStatus Expired = ActiveStatuses[RemainingIndex];
        ActiveStatuses.RemoveAt(RemainingIndex);
        OnStatusExpired.Broadcast(Expired);
    }
}
