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

    // ---- Swift projectile channels (owner ruling 2026-08-16) --------------
    // Pure halves of the multishot / pierce / chain / ricochet mechanics, kept
    // dependency-free here so the suite (which is world-free by construction)
    // can exercise the decision math while UBreakerWeaponComponent keeps only
    // the trace loop itself.

    /**
     * Multishot consumption, once per trigger pull. Returns the number of
     * WHOLE extra pellets this shot fires; the fractional part of
     * AdditionalProjectiles banks in Accumulator across pulls, so a +0.5 line
     * is a visible every-other-shot second pellet rather than a rounding loss.
     * Zero (or negative) input drains nothing and leaves the accumulator
     * untouched, so a non-Swift weapon is bit-identical to before the channel
     * existed.
     */
    static int32 ConsumeMultishot(float AdditionalProjectiles, float& Accumulator);

    /**
     * The pierce damage ladder: the multiplier the NEXT pierced target pays,
     * given what the shot has already paid. Each penetration multiplies by
     * FalloffPerTarget, except that Swift.Marksman.Overpenetration
     * (Class-Kits §1.5 M10) skips the step after a KILLING hit — the shot
     * carries on at its full remaining damage.
     */
    static float NextPierceMultiplier(float CurrentMultiplier, float FalloffPerTarget, bool bPreviousHitKilled, bool bOverpenetration);

    /**
     * Nearest-target selection for chain arcs and ricochet seeks. Candidates
     * are world positions the caller has already filtered for legality (alive,
     * not already struck, line of sight); returns the index of the nearest one
     * within MaxRadiusCm of Origin, or INDEX_NONE. Deterministic: distance
     * ties break toward the lower index, so the same world state always picks
     * the same target.
     */
    static int32 SelectNearestTarget(const FVector& Origin, const TArray<FVector>& Candidates, float MaxRadiusCm);

    // ---- Marksman / Frenzy rule halves (Class-Kits §1.3 / §1.5) -----------
    // The pure halves of the weapon-layer node rules, kept dependency-free
    // here so the world-free suite can pin buy-the-node-observable changes
    // while UBreakerWeaponComponent keeps only the wiring.

    /**
     * Steady (Class-Kits §1.5 M2): "ADS while moving above the slide
     * threshold does not increase spread. R2: ADS while airborne likewise."
     * Takes the COMPOSED movement-spread penalty the feel layer already
     * produced and relieves it by aim progress: at full ADS the penalty is
     * exactly zero, partway into the sights it is partway gone — the same
     * ramp every other ADS benefit rides. Rank 1 covers grounded movement
     * only; airborne movement keeps its full penalty until rank 2. With no
     * ranks (or from the hip) the input passes through untouched, so every
     * non-owner is bit-identical.
     */
    static float SteadyMovementSpreadDegrees(float MovementSpreadDegrees, float AimAlpha, int32 SteadyRank, bool bAirborne);

    /**
     * Called Shot (Class-Kits §1.5 M11, the node text's own numbers): "At
     * Redline, Lead's range gate drops from 25 m to 10 m." Both clauses are
     * required — the node owned AND the bar at Redline — otherwise the
     * authored gate passes through unchanged.
     */
    static float LeadRangeGateCm(float BaseGateCm, bool bCalledShotOwned, bool bRedline);

    /**
     * Ledger (Class-Kits §1.5 M3): "Momentum spent on Marksman abilities is
     * refunded at 25% (R2: 50%) if the ability's effect lands a hit within
     * its window." Returns the refunded FRACTION of the ability's cost;
     * rank 0 refunds nothing.
     */
    static float LedgerRefundFraction(int32 LedgerRank);

    /**
     * Mark Economy (Class-Kits §1.5 M5): "Lead's mark persists through the
     * target's death and jumps to the nearest enemy within 15 m (R2: 25 m)."
     * Returns the seek radius in centimetres; rank 0 returns 0 (no jump).
     */
    static float MarkJumpRadiusCm(int32 MarkEconomyRank);

    /**
     * Loaded (Class-Kits §1.3 F2): "Reloading while at Redline refunds
     * ammunition to the magazine equal to the shots fired in the previous 2s
     * (R1: half, R2: all)." Returns the FREE rounds this reload adds before
     * reserve is drawn; the half rounds down, so one shot at rank 1 refunds
     * nothing rather than half a round. Rank 0 refunds nothing.
     */
    static int32 LoadedRefundRounds(int32 ShotsInWindow, int32 LoadedRank);

    // ---- Gunsmith / Tank weapon-half node rules (Class-Kits-Gunsmith §4.1,
    // Class-Kits-Tank Bastion B7) ------------------------------------------
    // The pure halves of the weapon-layer consumers, world-free by the same
    // standing as the Marksman/Frenzy block above: the suite pins the rule,
    // the component keeps only the wiring.

    /**
     * Chambered (Class-Kits-Gunsmith §4.1 AR3): "The first shot after a
     * completed reload consumes no ammunition." The reload-to-fire boundary:
     * FinishReload arms the chambered round when the node is owned, and the
     * next round fired debits this many rounds from the magazine. Unarmed
     * (every build without the node, and every shot after the first) debits
     * exactly 1 — the pre-node behaviour to the bit.
     */
    static int32 MagazineDebitRounds(bool bChamberedRoundArmed);

    /**
     * Last Round (Class-Kits-Gunsmith §4.1 AR5, weapon half): "The
     * magazine-dump payout fires on your last round rather than on empty."
     * Returns the magazine count AT OR BELOW which the dump event fires,
     * post-debit: 0 without the node (the last round leaving, the authored
     * boundary), 1 with it (the payout arrives while the last round is still
     * chambered — and the rig's window rule, which ignores the event under
     * this node, already lives on UBreakerAbility_SidearmRig).
     */
    static int32 MagazineDumpThresholdRounds(bool bLastRoundOwned);

    /**
     * No Reserve (Class-Kits-Gunsmith §4.1 AR11, weapon half): "Your maximum
     * reserve is halved." The reserve ceiling AddReserveAmmoFraction fills to,
     * in rounds: ceil(Starting x CapMultiplier), halved under the node before
     * the ceil so the halving is exact and still grants at least one round of
     * headroom on the smallest reserves. Without the node the number is the
     * pre-node cap to the bit. (The doubled Scrap payout half lives on
     * UBreakerScrapComponent, already shipped.)
     */
    static int32 ReserveCapRounds(int32 StartingReserve, float CapMultiplier, bool bNoReserveOwned);

    /**
     * Emplacement (Class-Kits-Tank, Bastion B7, weapon half): "behind your
     * own Anchor Point your spread reads as stationary." True when the node
     * is owned and the owner stands within the anchor-proximity radius the
     * Grit layer already measures (B2/B4/B8's own geometry, one definition of
     * "at your anchor" for the whole class). The weapon zeroes its speed
     * fraction — movement spread reads exactly as standing still — and
     * touches nothing else about the cone.
     */
    static bool SpreadReadsStationary(bool bEmplacementOwned, float AnchorDistanceCm, float AnchorNearRadiusCm);

    /**
     * Capacity-delta clamp for PushMagazineCapacityOverride's shrink form
     * (Overpressure, §4.1 AR10). A positive delta passes through untouched; a
     * negative delta is floored so the effective magazine never drops below
     * one round — a weapon that cannot chamber anything is a hang, not a
     * downside. EffectiveSizeWithoutEntry is the capacity as it stands before
     * this entry lands (base plus every OTHER live override).
     */
    static int32 ClampMagazineCapacityDelta(int32 EffectiveSizeWithoutEntry, int32 DeltaRounds);

    /**
     * Seed for a draw that must not perturb the primary shot sequence.
     * Multishot's extra pellets and the secondary hits' crit rolls draw from
     * these salted sub-streams, so a build with the channels at zero produces
     * bit-identical recoil and spread sequences to a build from before the
     * channels existed — and the same owner, shot and index always reproduce
     * the same draw on the server.
     */
    static int32 SecondaryShotSeed(uint32 OwnerHash, int32 ShotSequence, uint32 Salt, int32 Index);
};
