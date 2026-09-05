#pragma once

#include "CoreMinimal.h"

// THE ARRIVAL, as world-free maths.
//
// Every travel arrives under a cover — the deployment briefing when there is
// one to read, a plain black otherwise — and the cover lifts only when the
// world behind it is worth seeing. Nothing in C++ can read Lumen's warmth or
// the eye adaptation's convergence, so "worth seeing" is approximated by two
// gates that must BOTH be met, counted from the moment the destination
// finished loading: a number of rendered frames (Lumen and exposure settle
// per frame, not per second) and a number of seconds (the briefing's stage
// line has to be readable as an arrival). Then the cover fades.
//
// The game instance asks this header how much of the world to reveal on a
// given frame and copies the answer onto the cover's opacity — it decides
// nothing itself, so the gate is provable without a world
// (RiorsEdge.Game.Arrival.SettleHold).
//
// O2: every figure in FBreakerArrivalHold is a PLACEHOLDER. The automation
// proves the shape of the gate and cannot say whether the first frame the
// player sees is warm.
struct FBreakerArrivalHold
{
    // The shortest the cover holds after the load, whatever the frame rate.
    // 0.9 is the arrive hold the briefing's "ON SITE" line is read under.
    float MinHoldSeconds = 0.9f;   // O2 PLACEHOLDER
    // The fewest rendered frames the destination gets before the cover
    // lifts. A frame-starved machine reaches this later than MinHoldSeconds
    // and holds black for longer — by design: a cold frame under a lifted
    // cover is the failure this exists to prevent.
    int32 MinSettleFrames = 24;   // O2 PLACEHOLDER
    // Cover to world, once both gates are met.
    float FadeInSeconds = 0.35f;   // O2 PLACEHOLDER
};

// How much of the world is revealed: 0 = the cover is opaque, 1 = the cover
// is gone. Pure: the caller supplies the clock and the frame count since the
// destination finished loading.
//
// The ramp is anchored to MinHoldSeconds, not to the moment the frame gate
// was met. On a frame-starved machine the frame gate is the later one, and
// the reveal then resumes from wherever the clock puts it — a shorter visible
// fade, never an earlier reveal. The gates are the contract; the fade is
// the courtesy.
inline float BreakerArrivalRevealAlpha(bool bWorldReady, double SecondsSinceReady, uint64 FramesSinceReady, const FBreakerArrivalHold& Hold)
{
    if (!bWorldReady) return 0.0f;
    if (FramesSinceReady < static_cast<uint64>(FMath::Max(Hold.MinSettleFrames, 0))) return 0.0f;
    const double SinceHold = SecondsSinceReady - static_cast<double>(FMath::Max(Hold.MinHoldSeconds, 0.0f));
    if (SinceHold < 0.0) return 0.0f;
    const double FadeIn = FMath::Max(static_cast<double>(Hold.FadeInSeconds), static_cast<double>(UE_KINDA_SMALL_NUMBER));
    return static_cast<float>(FMath::Clamp(SinceHold / FadeIn, 0.0, 1.0));
}
