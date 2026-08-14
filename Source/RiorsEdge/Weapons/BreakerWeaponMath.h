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

    // ---- Item level -> base damage ----------------------------------------
    // Power-Curve.md §3, the multiplicand half of player offence:
    //
    //     WeaponBase(ilvl) = ArchetypeBase * (1 + w)^(ilvl - 1)
    //
    // The archetype constant is therefore the ITEM LEVEL 1 number, and every
    // authored damage figure in the archetype table keeps meaning exactly what
    // it means today at the bottom of the curve. `w` is chosen to track the
    // monster health growth `g` so a BASELINE build holds a roughly constant
    // TTK across the game and all felt progression comes from the multiplier
    // band, which this function deliberately knows nothing about.
    //
    // Growth is a FRACTION per level (0.09 is +9%/level), not a percentage.

    // Item levels below 1 clamp to 1. The ceiling is now the DESIGN ceiling
    // rather than a garbage guard: O29 rules that item level runs to 120, which
    // is what makes "all endgame character power comes from gear" function
    // rather than merely be stated. It must equal
    // UBreakerAffixLibrary::MaxItemLevel, and RiorsEdge.Items.TierLadder pins
    // that the two agree -- a weapon clamping lower than the item system rolls
    // would silently cap base damage while the affixes on the same item kept
    // climbing, which is the same class of split the 74x endgame gap was.
    //
    // It is a constant here rather than an EditAnywhere property because this
    // is a static maths class with no instance. O2 PLACEHOLDER.
    //
    // Not duplicated as an include of Items/ on purpose: BreakerWeaponMath is
    // deliberately dependency-free pure maths, in the precedent of
    // BreakerRangedBehavior.h and BreakerMonsterChassis.h, so it stays
    // unit-testable with no world and no item system. The test is the seam.
    static constexpr int32 MaxSupportedItemLevel = 120;

    /**
     * (1 + Growth)^(ItemLevel - 1). Exactly 1.0 at item level 1 for any growth,
     * so an unscaled call site and a level-1 call site agree to the bit.
     */
    static float ItemLevelDamageScalar(int32 ItemLevel, float GrowthPerLevel);

    /** ArchetypeBase * ItemLevelDamageScalar(...). Negative bases clamp to 0. */
    static float WeaponBaseDamage(float ArchetypeBase, int32 ItemLevel, float GrowthPerLevel);
};
