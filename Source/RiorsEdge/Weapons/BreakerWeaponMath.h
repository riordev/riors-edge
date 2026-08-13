#pragma once

#include "CoreMinimal.h"

class UBreakerWeaponDefinition;

class RIORSEDGE_API FBreakerWeaponMath
{
public:
    static float FireInterval(float RoundsPerMinute);
    static float DamageMultiplierAtDistance(const UBreakerWeaponDefinition* Definition, float Distance);
    static FVector ApplyConeSpread(const FVector& Direction, float SpreadDegrees, int32 RandomSeed);

    /**
     * Closest distance from a point to the FORWARD half of a ray. Points
     * behind the muzzle clamp to the origin, so a weak point the shooter has
     * already walked past never counts.
     */
    static float DistanceFromRayToPoint(const FVector& RayOrigin, const FVector& RayDirection, const FVector& Point);

    /**
     * Weak-point acceptance with a forgiveness halo.
     *
     * A line trace against a 20 cm head sphere is a binary that a player
     * cannot feel the edges of: one pixel is a 1.75x hit and the next is a
     * body shot, with nothing in between and no way to tell which you got by
     * aiming better. This widens the acceptance to Radius + ToleranceCm in
     * WORLD space, so the halo is the same physical size around the head at
     * every range and the reward for near-misses is legible rather than
     * random. Tolerance 0 restores the exact geometric test.
     */
    static bool IsWithinWeakPointTolerance(const FVector& RayOrigin, const FVector& RayDirection, const FVector& WeakPointCenter, float WeakPointRadius, float ToleranceCm);
};
