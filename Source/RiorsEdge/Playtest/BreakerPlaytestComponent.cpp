#include "Playtest/BreakerPlaytestComponent.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
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
        // TTD instrumentation: all three EXISTING delegates the player's own
        // combat component already exposes (Combat/BreakerCombatComponent.h,
        // read-only to this lane) — incoming damage, death, and vitals
        // restored (the reset path). Nothing was missing here.
        if (UBreakerCombatComponent* Combat = Character->GetCombat())
        {
            Combat->OnDamageReceived.AddDynamic(this, &ThisClass::HandleIncomingDamageForTTD);
            Combat->OnDeath.AddDynamic(this, &ThisClass::HandleDeathForTTD);
            Combat->OnVitalsRestored.AddDynamic(this, &ThisClass::HandleVitalsRestoredForTTD);
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
    // Deaths and TTD samples deliberately SURVIVE a stats reset. ResetStats
    // runs on every F1 press AND on every player death
    // (ABreakerCharacter::HandlePlayerDeath -> ResetPlaytest), so a
    // death-triggered reset must not erase the very death it is measuring —
    // a counter that resets itself on the event that increments it could only
    // ever read 0 or 1. TTD is a whole-SESSION instrument in the same sense
    // O18's TTK targets are; it survives the per-encounter resets the rest of
    // this struct is scoped to.
    const int32 PreservedDeaths = Stats.Deaths;
    TArray<float> PreservedTimeToDeathSamples = MoveTemp(Stats.TimeToDeathSamples);
    Stats = FBreakerPlaytestStats();
    Stats.Deaths = PreservedDeaths;
    Stats.TimeToDeathSamples = MoveTemp(PreservedTimeToDeathSamples);
}

float UBreakerPlaytestComponent::AccumulateEngagedSeconds(double Now, double LastEventTime, float AccumulatedSeconds, float GapCapSeconds)
{
    // No previous event this life: nothing to measure yet, so the
    // accumulator does not move. Matches Combat/BreakerEnemy.cpp's own
    // `if (LastDamageEventTime >= 0.0)` guard exactly.
    if (LastEventTime < 0.0) return AccumulatedSeconds;
    const double Cap = FMath::Max(static_cast<double>(GapCapSeconds), 0.0);
    return AccumulatedSeconds + static_cast<float>(FMath::Min(Now - LastEventTime, Cap));
}

void UBreakerPlaytestComponent::HandleIncomingDamageForTTD(const FBreakerDamageResult& Result)
{
    // Mirrors ABreakerEnemy::HandleDamageReceived: only real health/shield
    // damage counts as engagement, so a dodged or fully-blocked hit does not
    // start (or extend) the clock.
    if (!GetWorld() || (Result.HealthDamage <= 0.0f && Result.ShieldDamage <= 0.0f)) return;
    const double Now = GetWorld()->GetTimeSeconds();
    EngagedSecondsThisLife = AccumulateEngagedSeconds(Now, LastIncomingDamageTime, EngagedSecondsThisLife, EngagementGapCapSeconds);
    if (FirstEngagementTime < 0.0) FirstEngagementTime = Now;
    LastIncomingDamageTime = Now;
}

void UBreakerPlaytestComponent::HandleDeathForTTD()
{
    // Mirrors ABreakerEnemy::HandleDeath's TTK sample: consume whatever
    // engagement accumulated this life into one TTD sample, then clear the
    // accumulator so the next life starts clean. A death with no prior
    // engagement this life (should not happen through the real damage path,
    // but a scripted or future death route might) records nothing rather
    // than a fabricated 0s TTD.
    if (FirstEngagementTime >= 0.0)
    {
        ++Stats.Deaths;
        Stats.TimeToDeathSamples.Add(FMath::Max(EngagedSecondsThisLife, 0.05f));
    }
    FirstEngagementTime = -1.0;
    LastIncomingDamageTime = -1.0;
    EngagedSecondsThisLife = 0.0f;
}

void UBreakerPlaytestComponent::HandleVitalsRestoredForTTD()
{
    // A NON-death reset (F1, fall-out-of-map, respawn): whatever was
    // accumulating must not leak into the next life's sample, but it was not
    // a death either, so nothing is recorded. RestoreVitals also fires on the
    // death path itself (via ResetPlaytest, downstream of HandleDeathForTTD
    // above on the same OnDeath broadcast), which lands here a second time
    // and finds the accumulator already clear — idempotent.
    FirstEngagementTime = -1.0;
    LastIncomingDamageTime = -1.0;
    EngagedSecondsThisLife = 0.0f;
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
        TEXT("Rior's Edge Playtest Report\nDuration: %.1f minutes\nShots: %d\nHits: %d\nAccuracy: %.1f%%\nWeak-point hits: %d\nWeak-point rate: %.1f%%\nDamage dealt: %.0f\nReloads: %d\nMelee trash kills: %d (avg TTK %.2fs)\nRanged trash kills: %d (avg TTK %.2fs)\nElite kills: %d (avg TTK %.2fs)\nModifier-bearing kills: %d (avg TTK %.2fs)\nBoss kills: %d (avg TTK %.2fs)\nDeaths: %d (avg engaged TTD %.2fs)\nTargets [O18]: trash <1s | elite ~3s | boss 20-45s | TTD target 4-5s no sustain\nFlags [O23]: Veteran XP multiplier 3.0x vs 2.0x-health chassis\n\nMovement notes:\n- Walk/stopping:\n- Sprint:\n- Dash:\n- Slide:\n- Wall ride/jump:\n\nWeapon notes:\n- Hip fire / aim:\n- Cadence / reload:\n- Weak points / falloff:\n\nDefects or discomfort:\n- "),
        Stats.SessionSeconds / 60.0f, Stats.ShotsFired, Stats.Hits, Stats.Accuracy(), Stats.WeakPointHits,
        Stats.WeakPointRate(), Stats.DamageDealt, Stats.Reloads,
        Stats.TimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.TimeToKillSamples),
        Stats.RangedTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.RangedTimeToKillSamples),
        Stats.EliteTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.EliteTimeToKillSamples),
        Stats.ModifierTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.ModifierTimeToKillSamples),
        Stats.BossTimeToKillSamples.Num(), FBreakerPlaytestStats::Average(Stats.BossTimeToKillSamples),
        Stats.Deaths, FBreakerPlaytestStats::Average(Stats.TimeToDeathSamples));
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
