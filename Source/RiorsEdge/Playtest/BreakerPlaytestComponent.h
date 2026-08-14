#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "Playtest/BreakerKillBuckets.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerPlaytestComponent.generated.h"

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerPlaytestStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 ShotsFired = 0;
    UPROPERTY(BlueprintReadOnly) int32 Hits = 0;
    UPROPERTY(BlueprintReadOnly) int32 WeakPointHits = 0;
    UPROPERTY(BlueprintReadOnly) int32 Reloads = 0;
    UPROPERTY(BlueprintReadOnly) float DamageDealt = 0.0f;
    UPROPERTY(BlueprintReadOnly) float SessionSeconds = 0.0f;
    // Time-to-kill instrumentation: the measurement the whole value-
    // authoring pass waits on (Decisions.md O2).
    UPROPERTY(BlueprintReadOnly) TArray<float> TimeToKillSamples;
    UPROPERTY(BlueprintReadOnly) TArray<float> EliteTimeToKillSamples;
    // Ranged trash is sampled separately. It subclasses ABreakerEnemy, so it
    // would otherwise land in the trash bucket and drag that average up — it
    // is fought across a 9-19 m band rather than in contact, so its kill time
    // measures a different thing. Mixing them would corrupt the one number the
    // O18 re-anchor is waiting on.
    UPROPERTY(BlueprintReadOnly) TArray<float> RangedTimeToKillSamples;
    // The two buckets that were missing, and the reason is the same bug class
    // twice over. Before this, a ModifierBearing enemy (x2.5 health plus the
    // per-modifier step) and the boss (x25) both fell into MELEE TRASH — not,
    // as the handover assumed, into elite: `IsElite()` is `rank == Elite`, and
    // neither of those ranks is Elite. One boss kill therefore moved the one
    // number O18's re-anchor reads by more than a hundred trash kills could.
    UPROPERTY(BlueprintReadOnly) TArray<float> ModifierTimeToKillSamples;
    UPROPERTY(BlueprintReadOnly) TArray<float> BossTimeToKillSamples;

    // TTD instrumentation — the unmeasured half of O18 ("TTD: 4-5 seconds
    // with no resources/sustain"). The F2 report used to print that target
    // and measure nothing. Same engagement-gapped discipline as the TTK
    // arrays above, applied to the PLAYER's own incoming damage instead of an
    // enemy's (Combat/BreakerEnemy.cpp is the pattern this reproduces;
    // READ-ONLY to this lane). Deliberately NOT wiped by ResetStats(): see
    // UBreakerPlaytestComponent::ResetStats for why.
    UPROPERTY(BlueprintReadOnly) int32 Deaths = 0;
    UPROPERTY(BlueprintReadOnly) TArray<float> TimeToDeathSamples;

    // Read-only routing so nothing outside this struct has to know which array
    // is which. Non-const twin below for the writer.
    const TArray<float>& SamplesForBucket(EBreakerKillBucket Bucket) const;
    TArray<float>& SamplesForBucket(EBreakerKillBucket Bucket);

    float Accuracy() const { return ShotsFired > 0 ? 100.0f * Hits / ShotsFired : 0.0f; }
    float WeakPointRate() const { return Hits > 0 ? 100.0f * WeakPointHits / Hits : 0.0f; }
    static float Average(const TArray<float>& Samples)
    {
        if (Samples.IsEmpty()) return 0.0f;
        float Total = 0.0f;
        for (float Sample : Samples) Total += Sample;
        return Total / Samples.Num();
    }
};

UCLASS(ClassGroup=Playtest, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerPlaytestComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerPlaytestComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintPure, Category="Playtest") const FBreakerPlaytestStats& GetStats() const { return Stats; }
    UFUNCTION(BlueprintPure, Category="Playtest") FString BuildReport() const;
    UFUNCTION(BlueprintCallable, Category="Playtest") void CopyReportToClipboard() const;
    UFUNCTION(BlueprintCallable, Category="Playtest") void ResetStats();
    // The call ABreakerEnemy::HandleDeath still makes. It can only describe
    // three of the five buckets, so it consults a HINT first (see
    // NotePendingKillBucket) and falls back to the legacy classification.
    UFUNCTION(BlueprintCallable, Category="Playtest") void AddTimeToKillSample(float Seconds, bool bElite, bool bRanged = false);
    // The unambiguous path: the caller already knows what it killed.
    UFUNCTION(BlueprintCallable, Category="Playtest") void AddClassifiedTimeToKillSample(float Seconds, EBreakerKillBucket Bucket);

    // WHY A HINT AND NOT AN ARGUMENT. The sample is fed from inside
    // ABreakerEnemy::HandleDeath, in Combat/, which this lane does not own and
    // which two other agents are editing in parallel — so the three-argument
    // call cannot be widened to carry the rank. Instead
    // UBreakerKillTelemetryComponent, which rides on every enemy the gym
    // spawns, classifies its OWN owner and posts the answer here.
    //
    // The ordering is a CODE PATH, not a delegate registration order (which
    // this codebase is explicit is an accident and not a contract):
    // UBreakerCombatComponent::ApplyDamage broadcasts OnDamageReceived — where
    // the hint is posted — and only then broadcasts OnDeath, which is what
    // eventually reaches HandleDeath and the sample. The hint is stamped with
    // the world time it was posted and is only honoured within the same frame,
    // so a Wakeful revive (which takes the damage broadcast and then SUPPRESSES
    // the death) cannot leave a hint lying around for the next kill.
    UFUNCTION(BlueprintCallable, Category="Playtest") void NotePendingKillBucket(EBreakerKillBucket Bucket);
    UFUNCTION(BlueprintCallable, Category="Playtest") void ToggleDiagnostics() { bDiagnosticsVisible = !bDiagnosticsVisible; }
    UFUNCTION(BlueprintPure, Category="Playtest") bool AreDiagnosticsVisible() const { return bDiagnosticsVisible; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetSecondsSinceReportCopy() const;

    // --- TTD instrumentation (O18's unmeasured half) ------------------------
    // Pure world-free maths: one step of the SAME engagement-gapped
    // accumulation discipline Combat/BreakerEnemy.cpp uses for its TTK sample
    // (that file is READ-ONLY to this lane; this reproduces the rule rather
    // than editing it there) — gaps between damage events longer than
    // GapCapSeconds are disengagement, not fighting, and are capped rather
    // than counted in full. Precedent for pure, static, world-free,
    // unit-testable rule maths living on the owning component: this mirrors
    // UBreakerMomentumComponent/UBreakerManaComponent's own static rule
    // functions (GroundSpeedRate, HitGeneration, ...) and, one domain over,
    // Combat/BreakerRangedBehavior.h.
    UFUNCTION(BlueprintPure, Category="Playtest|Engagement")
    static float AccumulateEngagedSeconds(double Now, double LastEventTime, float AccumulatedSeconds, float GapCapSeconds);

private:
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    UFUNCTION() void HandleReload(bool bReloading);
    // Bound to the owner's UBreakerCombatComponent. Mirrors
    // ABreakerEnemy::HandleDamageReceived / HandleDeath exactly: damage
    // accumulates engaged seconds, death consumes them into one sample and
    // clears the accumulator, and OnVitalsRestored (F1, fall-out-of-map, any
    // non-death reset) clears the accumulator WITHOUT recording a sample, so
    // a manual reset mid-fight cannot leak a stale partial engagement into
    // the next life's number.
    UFUNCTION() void HandleIncomingDamageForTTD(const FBreakerDamageResult& Result);
    UFUNCTION() void HandleDeathForTTD();
    UFUNCTION() void HandleVitalsRestoredForTTD();

    UPROPERTY() FBreakerPlaytestStats Stats;
    bool bDiagnosticsVisible = true;
    mutable double LastReportCopyTime = -1000.0;
    EBreakerKillBucket PendingKillBucket = EBreakerKillBucket::Count;
    double PendingKillBucketTime = -1000.0;

    // Same cap the enemy TTK sampler uses (Combat/BreakerEnemy.cpp, 1.5s).
    // Named rather than a bare literal so a retune of the MEASUREMENT
    // discipline is one field, not a grep; not O2-flagged because it is a
    // sampling-methodology constant, not a balance value.
    UPROPERTY(EditAnywhere, Category="Playtest|Engagement", meta=(ClampMin="0")) float EngagementGapCapSeconds = 1.5f;
    double FirstEngagementTime = -1.0;
    double LastIncomingDamageTime = -1.0;
    float EngagedSecondsThisLife = 0.0f;
};
