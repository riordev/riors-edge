#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BreakerGameMode.generated.h"

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ABreakerGameMode();
    virtual void Tick(float DeltaSeconds) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    UFUNCTION(BlueprintCallable, Category="Playtest") void ResetPlaytestTargets();

    // Enemies never target players inside the zone and will not enter it.
    UFUNCTION(BlueprintPure, Category="Playtest") bool IsInSafeZone(const FVector& Location) const;
    UFUNCTION(BlueprintPure, Category="Playtest") FVector GetSafeZoneCenter() const { return SafeZoneCenter; }
    UFUNCTION(BlueprintPure, Category="Playtest") float GetSafeZoneRadius() const { return SafeZoneRadius; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest", meta=(ClampMin="0")) float SafeZoneRadius = 900.0f;

    // Wave mode: the TTK measurement instrument. Escalating non-respawning
    // waves in the elite arena; every third wave carries an elite.
    UFUNCTION(BlueprintCallable, Category="Playtest|Waves") void StartNextWave();
    UFUNCTION(BlueprintPure, Category="Playtest|Waves") int32 GetCurrentWave() const { return CurrentWave; }
    UFUNCTION(BlueprintPure, Category="Playtest|Waves") int32 GetWaveEnemiesAlive() const;
    UFUNCTION(BlueprintPure, Category="Playtest|Waves") bool IsWaveActive() const { return CurrentWave > 0 && GetWaveEnemiesAlive() > 0; }

    // --- Ammo economy (O2 placeholders) ------------------------------------
    // Third and last resupply channel alongside kill drops and wave-clear:
    // an amber supply crate in the Anchor camp. Standing next to it for
    // SupplyCrateDwellSeconds fully restocks, then goes on cooldown.
    // Chosen over a safe-zone-wide refill because the crate is a visible,
    // walk-to affordance and needs no NPC/interaction plumbing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Ammo", meta=(ClampMin="0")) float SupplyCrateRadius = 350.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Ammo", meta=(ClampMin="0")) float SupplyCrateDwellSeconds = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playtest|Ammo", meta=(ClampMin="0")) float SupplyCrateCooldownSeconds = 8.0f;

private:
    bool bPlaytestTargetsSpawned = false;
    int32 CurrentWave = 0;
    UPROPERTY() TArray<TObjectPtr<class ABreakerEnemy>> WaveEnemies;
    FVector SafeZoneCenter = FVector::ZeroVector;
    bool bSafeZoneSet = false;
    FVector SupplyCrateLocation = FVector::ZeroVector;
    bool bSupplyCrateSet = false;
    float SupplyCrateDwell = 0.0f;
    double LastSupplyCrateUseTime = -1000.0;
    void TickSupplyCrate(float DeltaSeconds);
    // Refills the first player's weapon to full (both slots).
    void RefillPlayerAmmo();
    // Map expansion: a much larger playable field grown outward from the
    // existing gym. Nothing already placed moves.
    void SpawnExpandedField(const APawn* Pawn);
    void SpawnPlaytestTargets(const APawn* Pawn);
    void SpawnMovementCourse(const APawn* Pawn);
    void SpawnCombatEncounter(const APawn* Pawn);
    void SpawnSafeZone(const APawn* Pawn);
    void SpawnAnchorCamp(const APawn* Pawn);
    // Overgrown-Earth dressing (O24): vegetation, ruins, and scattered tech.
    // Cosmetic only — no gameplay values, spawn points, or radii change.
    void SpawnWorldDressing(const APawn* Pawn);
    void SpawnOvergrowth(const FVector& Origin, const FVector& Forward, const FVector& Right, struct FRandomStream& Stream);
    void SpawnRuins(const FVector& Origin, const FVector& Forward, const FVector& Right, struct FRandomStream& Stream);
    void SpawnScatteredTech(const FVector& Origin, const FVector& Forward, const FVector& Right, struct FRandomStream& Stream);
};
