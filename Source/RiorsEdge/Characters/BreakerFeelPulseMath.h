#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// The movement-feel envelopes (O283): the camera and the hands answer every
// change of state. World-free on purpose — the character is a thin caller
// and RiorsEdge.Characters.FeelPulse proves the shapes without a pawn.
//
// Two shapes cover every cue in the layer:
//   PulseAlpha  — a one-shot hit that arrives fast and settles. The dash
//                 punch's own envelope (linear attack, squared ease-out)
//                 generalised so a second cue cannot drift from the first.
//   EaseToward  — an exponential chase, for a value that follows a target it
//                 must never snap to: the crouch camera easing down to the
//                 lowered capsule instead of arriving there in one frame.
// ---------------------------------------------------------------------------
namespace BreakerFeel
{
    /**
     * 0 before the pulse begins, linear to 1 at AttackSeconds, squared
     * ease-out back to 0 at AttackSeconds + RecoverySeconds, 0 after. A
     * non-positive attack or recovery is read as the smallest positive one,
     * so a zero-attack pulse peaks on its first sample rather than dividing.
     */
    inline float PulseAlpha(float Elapsed, float AttackSeconds, float RecoverySeconds)
    {
        if (Elapsed < 0.0f) return 0.0f;
        const float Attack = FMath::Max(AttackSeconds, UE_KINDA_SMALL_NUMBER);
        const float Recovery = FMath::Max(RecoverySeconds, UE_KINDA_SMALL_NUMBER);
        if (Elapsed <= Attack)
        {
            return FMath::Clamp(Elapsed / Attack, 0.0f, 1.0f);
        }
        // Ease-out on the way back: a linear recovery reads as a mechanical
        // slide, a decaying one reads as the camera settling.
        const float Alpha = FMath::Clamp((Elapsed - Attack) / Recovery, 0.0f, 1.0f);
        const float Remaining = 1.0f - Alpha;
        return Remaining * Remaining;
    }

    /**
     * Exponential ease of Current toward Target over Dt with time constant
     * Tau: after one Tau the gap has closed to 1/e, after three to ~5 %.
     * Frame-rate independent. A non-positive Tau or Dt returns Target — an
     * ease of zero length is a snap, never a stall.
     */
    inline float EaseToward(float Current, float Target, float Tau, float Dt)
    {
        if (Tau <= 0.0f || Dt <= 0.0f)
        {
            return Target;
        }
        const float Alpha = 1.0f - FMath::Exp(-Dt / Tau);
        return Current + (Target - Current) * Alpha;
    }
}
