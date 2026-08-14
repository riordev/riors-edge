#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerZoneMath.generated.h"

// The world-free half of the zone primitive (Ability-Implementation-Spec §5.3).
// Membership, tick cadence and expiry are arithmetic; they are separated from
// ABreakerZoneActor for the same reason BreakerRangedBehavior.h and
// BreakerMonsterChassis.h are separated from their actors — a rule that can be
// proven with no world, no actor and no overlap query is a rule that stays
// proven when the actor around it is rewritten.
//
// The failure mode this file exists to prevent is a zone whose damage cadence
// is coupled to the frame rate. A zone that ticks "when the accumulator crosses
// the interval" and then RESETS the accumulator to zero silently loses the
// remainder every frame, so a 1.0s zone running at 60fps ticks slightly slower
// than one running at 30fps. ConsumeTicks subtracts the interval instead, and
// returns a COUNT so a long frame delivers every tick it owes.
UCLASS()
class RIORSEDGE_API UBreakerZoneMath : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Membership. A zone with a positive half-height is a vertical cylinder;
    // with zero it is a sphere. Cylinder is the default for ground effects
    // because a 4 m Rot puddle must catch a target standing on a crate inside
    // its footprint, and must not catch one standing on the roof above it.
    UFUNCTION(BlueprintPure, Category="Combat|Zone")
    static bool IsInsideZone(const FVector& ZoneCenter, float RadiusCm, float HalfHeightCm, const FVector& Point);

    // Ticks owed for this frame, in-place on the caller's countdown. Returns
    // how many ticks to deliver now (0 or more) and leaves TimeUntilNextTick
    // holding the remainder — never a reset to the full interval, which is the
    // frame-rate coupling above.
    //
    // MaximumTicksPerAdvance is a hitch guard: a 3-second hitch on a 0.1s zone
    // owes 30 ticks, and delivering 30 damage events in one frame is a worse
    // outcome than dropping some. The excess is discarded, not banked.
    static int32 ConsumeTicks(float& TimeUntilNextTick, float DeltaSeconds, float TickInterval, int32 MaximumTicksPerAdvance = 8);

    // Remaining lifetime after a frame. A paused zone does not age — that is
    // the Long Dark keystone's "zones placed during the window do not expire
    // until it closes" (Class-Kits VW12), expressed as one branch rather than
    // as a second timer somebody has to remember to stop.
    UFUNCTION(BlueprintPure, Category="Combat|Zone")
    static float RemainingAfter(float Remaining, float DeltaSeconds, bool bPaused);

    // VW4's anti-stack rule, as pure geometry: a new zone should REFRESH an
    // existing one rather than spawn when they are the same kind of zone from
    // the same caster and their centres are close enough that a player would
    // read them as one puddle. Overlap is not the test — two 4 m zones 7 m
    // apart overlap slightly and are obviously two puddles.
    UFUNCTION(BlueprintPure, Category="Combat|Zone")
    static bool ShouldRefreshExisting(const FVector& ExistingCenter, const FVector& NewCenter, float RadiusCm, float RefreshFractionOfRadius);
};
