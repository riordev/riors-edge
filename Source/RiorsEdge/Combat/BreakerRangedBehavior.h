#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerRangedBehavior.generated.h"

// Where a ranged enemy thinks it is relative to its preferred engagement band.
// The whole archetype hangs off this: a ranged enemy that advances forever is
// a slow melee enemy, and one that never moves is a turret.
UENUM(BlueprintType)
enum class EBreakerRangedBand : uint8
{
    // Too far to land a shot the player can be expected to feel. Close.
    Advance,
    // Inside the band. Strafe and fire; never close further.
    Hold,
    // The player got inside the minimum. Back off before this turns melee.
    Retreat
};

// Pure maths behind the ranged archetype: band classification with hysteresis,
// the projectile lead solve, and the telegraph ramp. Deliberately world-free
// and side-effect-free so every one of them is unit-testable — the behaviour
// itself cannot be tested without a running game, but the decisions can.
UCLASS()
class RIORSEDGE_API UBreakerRangedBehaviorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Classifies a distance into the band, widening the boundary the enemy is
    // ALREADY in by Hysteresis. Without this an enemy sitting exactly on a
    // boundary flips advance/retreat every frame and reads as a twitching bug.
    // Hysteresis is clamped so the widened band can never invert.
    UFUNCTION(BlueprintPure, Category="Enemy|Ranged")
    static EBreakerRangedBand ClassifyBand(float Distance, float MinDistance, float MaxDistance,
        float Hysteresis, EBreakerRangedBand PreviousBand);

    // +1 = close the gap, -1 = back off, 0 = hold station and strafe.
    UFUNCTION(BlueprintPure, Category="Enemy|Ranged")
    static float GetBandRadialSign(EBreakerRangedBand Band);

    // Movement speed multiplier for a band. Retreat is the fastest gear on
    // purpose: being crowded is the failure state this archetype must escape.
    UFUNCTION(BlueprintPure, Category="Enemy|Ranged")
    static float GetBandSpeedScale(EBreakerRangedBand Band, float AdvanceScale, float RetreatScale, float StrafeScale);

    // One step of the band controller expressed purely in radial distance, so
    // the advance/hold/retreat loop can be simulated in a test with no world.
    // The live enemy composes GetBandRadialSign/GetBandSpeedScale with a
    // direction vector; this is the same decision in one dimension.
    UFUNCTION(BlueprintCallable, Category="Enemy|Ranged")
    static float StepEngagementDistance(float Distance, float MinDistance, float MaxDistance, float Hysteresis,
        float MoveSpeed, float AdvanceScale, float RetreatScale, float DeltaSeconds,
        UPARAM(ref) EBreakerRangedBand& InOutBand);

    // Solves for the time at which a projectile of ProjectileSpeed launched now
    // from ShooterLocation meets a target moving at constant TargetVelocity.
    // Returns false when no positive solution exists (target outrunning the
    // shot), in which case callers must fall back to the current position.
    UFUNCTION(BlueprintCallable, Category="Enemy|Ranged")
    static bool SolveInterceptTime(const FVector& ShooterLocation, const FVector& TargetLocation,
        const FVector& TargetVelocity, float ProjectileSpeed, float& OutTime);

    // The point to aim at. LeadFraction 0 = fire at where the player is right
    // now (never hits a moving player); 1 = full intercept (only a direction
    // change beats it). The project ships a PARTIAL lead — see the comment on
    // ABreakerRangedEnemy::LeadFraction for why.
    UFUNCTION(BlueprintPure, Category="Enemy|Ranged")
    static FVector ComputeAimPoint(const FVector& ShooterLocation, const FVector& TargetLocation,
        const FVector& TargetVelocity, float ProjectileSpeed, float LeadFraction);

    // Telegraph ramp, 0 at the start of the wind-up and 1 at the shot. Drives
    // the emitter scale, colour, and light so "a shot is coming" is legible
    // before the projectile exists.
    UFUNCTION(BlueprintPure, Category="Enemy|Ranged")
    static float GetTelegraphAlpha(float ElapsedSeconds, float WindupSeconds);
};
