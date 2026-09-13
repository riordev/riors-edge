#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// THE HITCH A BODY GIVES WHEN IT IS SHOT. World-free.
//
// Owner: "enemies should stagger or flinch when shot". The STAGGER half of
// that already ships and is a gameplay state — UBreakerCombatComponent's
// ApplyStagger, with resistance, immunity and replication — and it is applied
// by exactly two things: a Tank ability and the Warden's slam. That is
// correct and it must stay that way. A rifle round that staggered would be a
// stunlock, and the answer to "an enemy does not react" is not "an enemy stops
// being able to act".
//
// So this is the OTHER half: a purely cosmetic recoil on the body, lasting a
// tenth of a second, that changes nothing an enemy can do. It is what makes a
// hit land visually; the stagger stays what makes a hit land tactically.
//
// And it fires only on a HEAVY hit. Owner: "they should only stagger when
// taking large amounts of damage" — a hitch on every rifle round read as a
// stagger, and a body that hitches four times a second is a body that never
// stands still. IsHeavy is the whole rule: a weak point, a single blow worth a
// slice of the bar, or a burst that adds up to one inside half a second.
//
// Everything here is arithmetic so the curve can be proved on bare floats:
// what a test can hold is that the hitch is instant, that it always returns to
// exactly zero, and that it can never accumulate into a body that drifts away
// from its own capsule.
// ---------------------------------------------------------------------------
namespace BreakerFlinch
{
    // WHAT COUNTS AS HEAVY. Fractions of the victim's max health, so the same
    // rule reads the same on a rat and on an elite: a round that is nothing to
    // the elite is a heavy hit to the rat. All O2 PLACEHOLDER.
    inline constexpr float HeavyHitFraction = 0.08f;     // O2 PLACEHOLDER
    inline constexpr float HeavyWindowFraction = 0.15f;  // O2 PLACEHOLDER
    inline constexpr float HeavyWindowSeconds = 0.5f;    // O2 PLACEHOLDER

    // A weak point is always heavy: the hit was earned by aim, not by weight.
    // Otherwise the blow, or the last half-second of blows, has to reach the
    // fraction. A body with no health to speak of cannot be hit heavily —
    // the guard is what keeps a zeroed target from hitching on every graze.
    inline bool IsHeavy(float Damage, float RecentDamage, float MaxHealth, bool bWeakPoint)
    {
        if (bWeakPoint) return true;
        if (!FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f) return false;
        return Damage >= HeavyHitFraction * MaxHealth
            || RecentDamage >= HeavyWindowFraction * MaxHealth;
    }

    // All O2 PLACEHOLDER. Short on purpose: sustained fire lands four or five
    // rounds a second, so a hitch that outlives its own interval would stop
    // being a reaction and become a wobble.
    // Up from 0.12 s and 9 cm: at a rifle's range a nine-centimetre rock on a
    // body two metres tall was under a pixel of motion. O2 PLACEHOLDER.
    inline constexpr float DurationSeconds = 0.18f;
    inline constexpr float ReachCm = 14.0f;
    inline constexpr float PitchDegrees = 5.0f;
    // A weak-point hit is worth more of everything. The same multiplier on
    // both, so the shape does not change with the hit — only its size.
    inline constexpr float WeakPointScale = 1.8f;

    // INSTANT ATTACK, QUICK RELEASE, and the shape matters more than the
    // numbers: a hitch that eases IN is not a hit, it is a lean. Squared so
    // the body snaps and then settles rather than sliding back linearly.
    //
    // Elapsed counts UP from the moment of the hit. Zero before the hit and
    // zero at or past the duration — that second one is what guarantees the
    // body always returns to exactly where it was.
    inline float Amount(float Elapsed, float Duration)
    {
        if (Duration <= 0.0f || Elapsed < 0.0f || Elapsed >= Duration) return 0.0f;
        const float Remaining = 1.0f - Elapsed / Duration;
        return Remaining * Remaining;
    }

    // Where the body sits during the hitch: back along the shot and slightly
    // down, because a thing that is struck drops into the blow rather than
    // floating away from it.
    inline FVector Offset(const FVector& AwayDirection, float Amount, float Reach)
    {
        const FVector Away = AwayDirection.GetSafeNormal2D();
        return (Away - FVector(0.0f, 0.0f, 0.28f)) * (Amount * FMath::Max(Reach, 0.0f));
    }

    // How far it rocks. Pitch alone: a roll would read as a body falling over
    // and this has to be legible four or five times a second.
    inline float Pitch(float Amount, float Degrees)
    {
        return Amount * Degrees;
    }
}
