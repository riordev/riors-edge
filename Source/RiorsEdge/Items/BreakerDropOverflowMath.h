#pragma once

#include "CoreMinimal.h"

// O249: "Drop Chance above its cap converts to rarity weight. A stat that
// saturates is a stat the player stops reading."
//
// Items/BreakerDropTable.h runs three steps. Step 1 is a PROBABILITY, so it has
// a hard ceiling at 1.0 that no tuning can lift — and the shipped BossDropChance
// is authored at 1.0, so a boss is at that ceiling before the player has rolled
// a single point of the affix. Every point of Drop Chance on a boss kill used to
// buy quantity that could not move, and the affix's only remaining effect was
// the quality shift, which itself clamped the bonus at +100%. Past that the
// stat's own tooltip was the whole of what it did.
//
// This header is the conversion, and it is deliberately world-free in the
// precedent of Combat/BreakerHealthRegenMath.h: plain floats in, plain floats
// out, no rank enum, no params struct, no UObject. The rank lookup stays in the
// library that owns the table; the ARITHMETIC lives here where a test can reach
// it without a world.
//
// THE SHAPE, in one sentence:
//
//   The bonus splits at the point where step 1 reaches 1.0; everything below
//   that point is spent on quantity exactly as it always was, and everything
//   above it drains a share of the remaining Standard weight up the rarity
//   ladder in the same proportions the below-cap shift already uses.
//
// TWO PROPERTIES THIS SHAPE IS CHOSEN FOR, and neither is a balance claim:
//
//   Below the cap the overflow is EXACTLY zero, so the conversion is inert and
//   the pre-ruling arithmetic is untouched bit for bit. A ruling about the top
//   of a stat's range must not silently retune the bottom of it.
//
//   The drain is hyperbolic in the overflow rather than linear, so it is
//   strictly monotonic for an unbounded input and can never take Standard
//   negative or to zero. A linear drain would need its own clamp, and that
//   clamp would be a second saturation point — the exact thing this ruling
//   exists to remove.

namespace BreakerDropOverflow
{
    // How much of Standard's weight the BELOW-cap shift drains at a full +100%.
    // Extracted from the shift's own literal so the two lanes cannot drift into
    // disagreeing about what a point of Drop Chance is worth.
    inline constexpr float BaseShiftFraction = 0.5f;   // O2 PLACEHOLDER

    // How much of the REMAINING Standard weight 100 points of overflow drain.
    // Deliberately the same figure as BaseShiftFraction: a point above the cap
    // converts at the rate a point below the cap was already worth, which is
    // the claim "the stat keeps reading the same way" stated as arithmetic.
    inline constexpr float ConversionRatePerHundredPercent = 0.5f;   // O2 PLACEHOLDER

    // Where drained weight lands. The rarity ladder's shares of one unit of
    // drained Standard weight; they sum to 1 and both lanes use them, so the
    // conversion moves weight along the SAME ladder the below-cap shift does
    // rather than inventing a second, steeper one that only rich players see.
    inline constexpr float UncommonShare = 0.55f;      // O2 PLACEHOLDER
    inline constexpr float ExceptionalShare = 0.30f;   // O2 PLACEHOLDER
    inline constexpr float AberrantShare = 0.12f;      // O2 PLACEHOLDER
    inline constexpr float UnwrittenShare = 0.03f;     // O2 PLACEHOLDER

    // The Drop Chance bonus percent at which step 1 first reaches 1.0 — solving
    // BaseChance * (1 + Percent/100 * QuantityScale) = 1 for Percent.
    //
    // TNumericLimits<float>::Max() means "never saturates", and it is returned
    // for the two cases where no amount of bonus can move step 1:
    //
    //   BaseChance <= 0 — a rank authored never to drop. Nothing overflows
    //   because there is no drop to be rare; converting here would hand rarity
    //   weight to an item that is never rolled.
    //
    //   QuantityScale <= 0 — the owner switching the quantity lane off, which
    //   FBreakerDropTableParams documents as "the affix goes back to being a
    //   pure quality stat". It already is one through the below-cap shift, so
    //   routing the same points through the conversion as well would pay the
    //   player twice for switching a lane off.
    inline float SaturationPercent(float BaseChance, float QuantityScale)
    {
        const float Scale = FMath::Max(QuantityScale, 0.0f);
        if (BaseChance <= 0.0f || Scale <= 0.0f) return TNumericLimits<float>::Max();
        if (BaseChance >= 1.0f) return 0.0f;
        return (1.0f / BaseChance - 1.0f) * 100.0f / Scale;
    }

    // The part of the bonus step 1 cannot spend. Exactly 0.0f at or below the
    // cap, which is what makes the conversion inert there.
    inline float OverflowPercent(float BonusPercent, float BaseChance, float QuantityScale)
    {
        const float Saturation = SaturationPercent(BaseChance, QuantityScale);
        const float Bonus = FMath::Max(BonusPercent, 0.0f);
        if (Bonus <= Saturation) return 0.0f;
        return Bonus - Saturation;
    }

    // The share of the remaining Standard weight this much overflow converts.
    // 0 at zero overflow, strictly increasing, and asymptotic to 1 — so the
    // stat never stops paying and never overpays into a negative weight.
    inline float DrainFraction(float OverflowPercentValue, float Rate = ConversionRatePerHundredPercent)
    {
        const float Overflow = FMath::Max(OverflowPercentValue, 0.0f) / 100.0f;
        const float ConversionRate = FMath::Max(Rate, 0.0f);
        if (Overflow <= 0.0f || ConversionRate <= 0.0f) return 0.0f;
        return 1.0f - 1.0f / (1.0f + ConversionRate * Overflow);
    }
}
