#include "Combat/BreakerEntropy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Data/BreakerDataFile.h"

namespace
{
    struct FBreakerEntropyTuning
    {
        float Threshold = 0, Damage = 0, Duration = 0, Tick = 0, Timeout = 0;
        float VestigeFraction = 0, VestigeResistance = 0;
        float AttunementTail = 0;
        float SympatheticFlat = 0, SympatheticFade = 0;
    };
    const FBreakerEntropyTuning& BreakerEntropyTuning()
    {
        static const FBreakerEntropyTuning Tuning = []
        {
            FBreakerEntropyTuning Value;
            BreakerDataFile::FBreakerDataErrors Errors;
            const auto Data = BreakerDataFile::Load(TEXT("Data/elements.json"), Errors);
            auto Read = [&](const TCHAR* Key, float& Field, bool bAllowZero = false)
            {
                double Number = 0;
                if (!Data || !Data->TryGetNumberField(Key, Number) || !FMath::IsFinite(Number) || Number < 0 || (!bAllowZero && Number == 0) || !FMath::IsFinite(static_cast<float>(Number)))
                    Errors.Add(FString::Printf(TEXT("Invalid Entropy tuning: %s"), Key));
                else Field = static_cast<float>(Number);
            };
            Read(TEXT("entropyThresholdHealthFraction"), Value.Threshold);
            Read(TEXT("entropyDamageFraction"), Value.Damage);
            Read(TEXT("entropyDurationSeconds"), Value.Duration);
            Read(TEXT("entropyTickSeconds"), Value.Tick);
            Read(TEXT("entropyBuildupTimeoutSeconds"), Value.Timeout);
            Read(TEXT("vestigeMeleeEntropyFraction"), Value.VestigeFraction, true);
            Read(TEXT("vestigeEntropyResistancePercent"), Value.VestigeResistance, true);
            Read(TEXT("attunementTailSeconds"), Value.AttunementTail, true);
            Read(TEXT("sympatheticFlatBuildup"), Value.SympatheticFlat, true);
            Read(TEXT("sympatheticFadeSeconds"), Value.SympatheticFade, true);
            if (Value.VestigeFraction > 1 || Value.VestigeResistance > 100)
                Errors.Add(TEXT("Invalid Vestige elemental tuning"));
            if (Value.Threshold > 1 || Value.Damage > 1 || Value.Tick > Value.Duration)
                Errors.Add(TEXT("Entropy fractions must be <= 1 and tick <= duration"));
            if (!ensureAlwaysMsgf(Errors.IsClean(), TEXT("%s"), *Errors.Join())) return FBreakerEntropyTuning();
            return Value;
        }();
        return Tuning;
    }
}

float BreakerEntropy::VestigeMeleeFraction() { return BreakerEntropyTuning().VestigeFraction; }
float BreakerEntropy::VestigeResistancePercent() { return BreakerEntropyTuning().VestigeResistance; }
float BreakerEntropy::AttunementTailSeconds() { return BreakerEntropyTuning().AttunementTail; }
float BreakerEntropy::SympatheticFlatBuildup() { return BreakerEntropyTuning().SympatheticFlat; }
float BreakerEntropy::SympatheticFadeSeconds() { return BreakerEntropyTuning().SympatheticFade; }

float UBreakerStatusComponent::GetEntropyBuildup() const
{
    float Total = EntropyBuildup;
    for (const auto& Contribution : EntropyProtectedContributions) Total += Contribution.Decay.Amount;
    return Total;
}

float UBreakerStatusComponent::GetEntropyThreshold() const
{
    const auto* Sink = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    return Sink ? Sink->GetMaxHealth() * BreakerEntropyTuning().Threshold : 0.0f;
}

float UBreakerStatusComponent::GetEntropyResistancePercent() const
{
    float Value = FMath::IsFinite(EntropyResistancePercent) ? EntropyResistancePercent : 0;
    if (const auto* Gear = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr)
        Value += Gear->GetStats().ElementalResistancePercent;
    return FMath::Clamp(Value, 0.0f, 100.0f);
}

void UBreakerStatusComponent::ApplyEntropyHit(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
{
    const auto* Sink = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Sink || Sink->IsDead() || Result.bKilled
        || Request.Element != EBreakerElement::Entropy || !Request.bCanApplyElementBuildup
        || Request.bIsDamageOverTime || Result.bDodged || Result.bParried || IsStatusImmune()
        || !FMath::IsFinite(Result.RawDamage) || !FMath::IsFinite(Request.ProcCoefficient)
        || !FMath::IsFinite(Request.ElementalFraction) || Result.RawDamage <= 0) return;
    const auto Rot = FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"));
    if (HasStatus(Rot)) return; // A live damage budget is never refreshed or multiplied.
    const auto& Tuning = BreakerEntropyTuning();
    const float Threshold = GetEntropyThreshold();
    const float Snapshot = Result.RawDamage * FMath::Clamp(Request.ElementalFraction, 0.0f, 1.0f);
    const float Bonus = FMath::IsFinite(Request.ElementBuildupFlat) ? FMath::Max(0.0f, Request.ElementBuildupFlat) : 0;
    // Damage-independent means a fully mitigated landed hit still contributes
    // the purchased flat amount. Avoidance, immunity and invalid hits do not.
    const float Ordinary = Result.ShieldDamage + Result.HealthDamage > 0 ? Snapshot : 0;
    if (Snapshot <= 0) return;
    const float Buildup = (Ordinary + Bonus) * FMath::Clamp(Request.ProcCoefficient, 0.0f, 1.0f)
        * (1.0f - GetEntropyResistancePercent() / 100.0f);
    if (Threshold <= 0 || !FMath::IsFinite(Buildup) || Buildup <= 0) return;
    if (FMath::IsFinite(Request.ElementBuildupFadeSeconds) && Request.ElementBuildupFadeSeconds > 0)
    {
        auto* Contribution = EntropyProtectedContributions.FindByPredicate([&](const FEntropyProtectedContribution& Value) { return Value.Applier == Request.Instigator; });
        if (!Contribution) { Contribution = &EntropyProtectedContributions.AddDefaulted_GetRef(); Contribution->Applier = Request.Instigator; }
        Contribution->Decay.Amount += Buildup;
        Contribution->Decay.GraceRemaining = Tuning.Timeout;
        Contribution->Decay.FadeRemaining = Request.ElementBuildupFadeSeconds;
    }
    else
    {
        EntropyBuildup += Buildup;
        EntropyBuildupRemaining = Tuning.Timeout;
    }
    if (!Sink->OnDeath.IsAlreadyBound(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath))
        const_cast<UBreakerCombatComponent*>(Sink)->OnDeath.AddDynamic(this, &UBreakerStatusComponent::HandleAfflictedOwnerDeath);
    if (GetEntropyBuildup() < Threshold) return;
    // Commit consumption before status/resource callbacks can re-enter combat.
    EntropyBuildup = 0;
    EntropyBuildupRemaining = 0;
    EntropyProtectedContributions.Reset();
    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = Rot;
    Spec.Duration = Tuning.Duration;
    Spec.TickInterval = Tuning.Tick;
    Spec.BaseDamagePerTick = Snapshot * Tuning.Damage / FMath::Max(1, FMath::FloorToInt(Tuning.Duration / Tuning.Tick));
    Spec.ProcCoefficient = FMath::Clamp(Request.ProcCoefficient, 0.0f, 1.0f);
    Spec.Snapshot.SourcePower = 1.0f;
    Spec.Snapshot.CriticalChance = 0;
    Spec.Snapshot.bRolledCritical = false;
    Spec.Snapshot.SourceTags = Request.SourceTags;
    ApplyStatusInternal(Spec, EBreakerDamageFamily::Elemental, Request.Instigator.Get(), true);
}
