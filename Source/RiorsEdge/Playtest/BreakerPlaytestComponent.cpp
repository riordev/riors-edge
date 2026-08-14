#include "Playtest/BreakerPlaytestComponent.h"

#include "Characters/BreakerCharacter.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerPlaytestComponent::UBreakerPlaytestComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBreakerPlaytestComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const ABreakerCharacter* Character = Cast<ABreakerCharacter>(GetOwner()))
    {
        if (UBreakerWeaponComponent* Weapon = Character->GetWeapon())
        {
            Weapon->OnShot.AddDynamic(this, &ThisClass::HandleShot);
            Weapon->OnReloadChanged.AddDynamic(this, &ThisClass::HandleReload);
        }
    }
}

void UBreakerPlaytestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Stats.SessionSeconds += DeltaTime;
}

void UBreakerPlaytestComponent::HandleShot(const FBreakerShotResult& Shot)
{
    if (!Shot.bFired) return;
    ++Stats.ShotsFired;
    if (Shot.bHit) ++Stats.Hits;
    if (Shot.bWeakPoint) ++Stats.WeakPointHits;
    Stats.DamageDealt += Shot.DamageResult.ShieldDamage + Shot.DamageResult.HealthDamage;
}

void UBreakerPlaytestComponent::HandleReload(bool bReloading)
{
    if (bReloading) ++Stats.Reloads;
}

void UBreakerPlaytestComponent::ResetStats()
{
    Stats = FBreakerPlaytestStats();
}

const TArray<float>& FBreakerPlaytestStats::SamplesForBucket(EBreakerKillBucket Bucket) const
{
    return const_cast<FBreakerPlaytestStats*>(this)->SamplesForBucket(Bucket);
}

TArray<float>& FBreakerPlaytestStats::SamplesForBucket(EBreakerKillBucket Bucket)
{
    switch (Bucket)
    {
    case EBreakerKillBucket::RangedTrash:     return RangedTimeToKillSamples;
    case EBreakerKillBucket::Elite:           return EliteTimeToKillSamples;
    case EBreakerKillBucket::ModifierBearing: return ModifierTimeToKillSamples;
    case EBreakerKillBucket::Boss:            return BossTimeToKillSamples;
    default:                                  return TimeToKillSamples;
    }
}

void UBreakerPlaytestComponent::NotePendingKillBucket(EBreakerKillBucket Bucket)
{
    PendingKillBucket = Bucket;
    PendingKillBucketTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UBreakerPlaytestComponent::AddTimeToKillSample(float Seconds, bool bElite, bool bRanged)
{
    if (Seconds <= 0.0f) return;

    // The hint, if one was posted for THIS frame. It carries the rank, which
    // the two booleans structurally cannot: a Champion's rank is
    // ModifierBearing and the boss's is Boss, so `bElite` is false for both and
    // the legacy path would file a 25x-health kill under melee trash.
    const bool bHintIsFresh = PendingKillBucket != EBreakerKillBucket::Count
        && GetWorld() && FMath::IsNearlyEqual(GetWorld()->GetTimeSeconds(), PendingKillBucketTime);
    const EBreakerKillBucket Bucket = bHintIsFresh
        ? PendingKillBucket
        : UBreakerKillBucketLibrary::ClassifyLegacyKill(bElite, bRanged);
    // Consumed either way: a hint that survived its frame is stale by
    // definition and must not be spent on somebody else's kill.
    PendingKillBucket = EBreakerKillBucket::Count;

    Stats.SamplesForBucket(Bucket).Add(Seconds);
}

void UBreakerPlaytestComponent::AddClassifiedTimeToKillSample(float Seconds, EBreakerKillBucket Bucket)
{
    if (Seconds <= 0.0f || Bucket == EBreakerKillBucket::Count) return;
    Stats.SamplesForBucket(Bucket).Add(Seconds);
}

FString UBreakerPlaytestComponent::BuildReport() const
{
    return FString::Printf(
        TEXT("Rior's Edge Playtest Report\nDuration: %.1f minutes\nShots: %d\nHits: %d\nAccuracy: %.1f%%\nWeak-point hits: %d\nWeak-point rate: %.1f%%\nDamage dealt: %.0f\nReloads: %d\nMelee trash kills: %d (avg TTK %.2fs)\nRanged trash kills: %d (avg TTK %.2fs)\nElite kills: %d (avg TTK %.2fs)\nModifier-bearing kills: %d (avg TTK %.2fs)\nBoss kills: %d (avg TTK %.2fs)\nTargets [O18]: trash <1s | elite ~3s | boss 20-45s | TTD 4-5s no sustain\nFlags [O23]: Veteran XP multiplier 3.0x vs 2.0x-health chassis\n\nMovement notes:\n- Walk/stopping:\n- Sprint:\n- Dash:\n- Slide:\n- Wall ride/jump:\n\nWeapon notes:\n- Hip fire / aim:\n- Cadence / reload:\n- Weak points / falloff:\n\nDefects or discomfort:\n- "),
        Stats.SessionSeconds / 60.0f, Stats.ShotsFired, Stats.Hits, Stats.Accuracy(), Stats.WeakPointHits,
        Stats.WeakPointRate(), Stats.DamageDealt, Stats.Reloads,
        Stats.TimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.TimeToKillSamples),
        Stats.RangedTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.RangedTimeToKillSamples),
        Stats.EliteTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.EliteTimeToKillSamples),
        Stats.ModifierTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.ModifierTimeToKillSamples),
        Stats.BossTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.BossTimeToKillSamples));
}

void UBreakerPlaytestComponent::CopyReportToClipboard() const
{
    FPlatformApplicationMisc::ClipboardCopy(*BuildReport());
    LastReportCopyTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

float UBreakerPlaytestComponent::GetSecondsSinceReportCopy() const
{
    return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastReportCopyTime) : BIG_NUMBER;
}
