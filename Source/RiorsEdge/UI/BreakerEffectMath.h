#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// Ability-effect lifetime maths.
//
// The effects renderer's whole schedule is here: when a primitive becomes
// visible, how its brightness ramps in, when it starts dying and the moment
// it is finished. Pure, header-inline, no world, no actors — same discipline
// as BreakerTracerMath.h and for the same reason: whether a primitive
// disappears ON SCHEDULE is arithmetic, and arithmetic is the part of a
// visual a headless suite can actually prove. The placing of the primitive
// is ABreakerEffectRenderer's job and is proven by the capture harness, not
// by a test.
//
// One deliberate simplification: every clip has a FIXED duration. Siphon's
// beam — alive exactly as long as the channel and not a frame longer — is
// not a second lifetime mode; when the channel breaks, the owning slot's
// duration is rewritten to (current age + fade-out), which turns "held" into
// "fixed, ending now" and keeps this header one function. That rewrite is
// the Phase C caller's move, recorded here so nobody adds a bHeld flag.
// ---------------------------------------------------------------------------
namespace BreakerFX
{
    // One clip's timing. Every authored value is O2 PLACEHOLDER until the
    // owner has seen it.
    struct FEffectTiming
    {
        // Total life, first visible frame to gone. Zero or negative renders
        // nothing at all.
        float DurationSeconds = 1.0f;
        // Brightness ramp at each end. Zero means the edge is a hard pop —
        // correct for an impact-like event, a click for anything that lingers.
        // When the two fades overlap mid-clip the DIMMER of the two wins, so
        // a short clip peaks below full brightness rather than snapping.
        float FadeInSeconds = 0.0f;
        float FadeOutSeconds = 0.0f;
    };

    // One frame of one clip.
    struct FEffectSample
    {
        // False with bFinished false: not born yet (scheduled, or age
        // negative). The slot is hidden, not recycled — same contract as the
        // tracer's future-scheduled secondary legs.
        bool bVisible = false;
        // True the instant age reaches duration. The slot is dead and may be
        // recycled; the primitive must be hidden THIS frame, not next.
        bool bFinished = false;
        // 0..1 brightness envelope. The caller multiplies its authored
        // intensity by this; additive materials fade by dimming toward black.
        float Alpha = 0.0f;
    };

    inline FEffectSample SampleEffect(const FEffectTiming& Timing, float AgeSeconds)
    {
        FEffectSample Sample;
        if (AgeSeconds < 0.0f) return Sample;   // scheduled for the future
        const float Duration = Timing.DurationSeconds;
        if (AgeSeconds >= Duration || Duration <= 0.0f)
        {
            Sample.bFinished = true;
            return Sample;
        }
        const float In = Timing.FadeInSeconds > KINDA_SMALL_NUMBER
            ? FMath::Min(AgeSeconds / Timing.FadeInSeconds, 1.0f) : 1.0f;
        const float Out = Timing.FadeOutSeconds > KINDA_SMALL_NUMBER
            ? FMath::Min((Duration - AgeSeconds) / Timing.FadeOutSeconds, 1.0f) : 1.0f;
        Sample.bVisible = true;
        Sample.Alpha = FMath::Clamp(FMath::Min(In, Out), 0.0f, 1.0f);
        return Sample;
    }
}
