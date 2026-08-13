#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
    UFUNCTION(BlueprintCallable, Category="Playtest") void AddTimeToKillSample(float Seconds, bool bElite);
    UFUNCTION(BlueprintCallable, Category="Playtest") void ToggleDiagnostics() { bDiagnosticsVisible = !bDiagnosticsVisible; }
    UFUNCTION(BlueprintPure, Category="Playtest") bool AreDiagnosticsVisible() const { return bDiagnosticsVisible; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetSecondsSinceReportCopy() const;

private:
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    UFUNCTION() void HandleReload(bool bReloading);

    UPROPERTY() FBreakerPlaytestStats Stats;
    bool bDiagnosticsVisible = true;
    mutable double LastReportCopyTime = -1000.0;
};
