#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// HOW THE WORLD LOOKS WHEN YOU ARE HURT.
//
// Owner, 2026-09-11: "the red screen flash, how you have just a giant red
// border around your entire screen is just not good looking. We need to find
// an alternative way to signify low HP or taking damage, maybe reduce
// visibility in some way — think about Path of Exile's light radius."
//
// Two red frames used to do this: four full-bleed edge bars for 0.28 s after
// any hit — including a parried or fully-mitigated one, since the clock they
// read arms on every ReceiveDamage — and a border pulsing 8 to 16 px, every
// frame, for as long as health sat under a fifth. Both were chrome painted
// over the world. This is the world itself answering instead: the edges of
// vision close in and the colour drains, a little on a hit and steadily as
// health falls, which is what a light radius does when the light is you.
//
// PURE. Three inputs — how long since real damage, how far under the low line,
// and where in its breath the low-health state is — plus the death beat's own
// saturation, composed into ONE post-process look, because the camera has one
// blend weight and it must have one writer.
// ---------------------------------------------------------------------------
namespace BreakerHarmPresentation
{
    // --- The hit ------------------------------------------------------------
    // A short closing of the edges, gone before the next shot lands at any
    // shipped cadence. Ease-out, so the first frame is the loudest and it
    // never reads as a swell. O2 PLACEHOLDER.
    inline constexpr float HitPulseSeconds = 0.30f;
    inline constexpr float HitVignette = 0.55f;
    inline constexpr float HitDesaturation = 0.28f;

    inline float HitPulse(float SecondsSinceHit)
    {
        if (SecondsSinceHit < 0.0f || SecondsSinceHit >= HitPulseSeconds) return 0.0f;
        const float T = SecondsSinceHit / HitPulseSeconds;
        return (1.0f - T) * (1.0f - T);
    }

    // --- The light radius ---------------------------------------------------
    // Zero above the low line, one at no health left: the closer to dead, the
    // less of the world you have. LowLine is the HUD's own threshold, passed
    // in rather than restated so the bar going harm and the world going dark
    // are the same event.
    inline float LowHealthDepth(float HealthFraction, float LowLine)
    {
        if (LowLine <= 0.0f || HealthFraction >= LowLine) return 0.0f;
        return FMath::Clamp(1.0f - FMath::Max(HealthFraction, 0.0f) / LowLine, 0.0f, 1.0f);
    }

    // The edges at the low line, and how much further they close on the way
    // to zero. Never total: a player at one hit point still has to see the
    // thing that is about to hit them. O2 PLACEHOLDER.
    inline constexpr float LowVignetteAtLine = 0.42f;
    inline constexpr float LowVignetteAtZero = 0.85f;
    inline constexpr float LowDesaturationAtZero = 0.55f;
    // A slow breath on the low state so it reads as a condition and not as a
    // filter that was switched on. O2 PLACEHOLDER.
    inline constexpr float BreathSeconds = 2.2f;
    inline constexpr float BreathVignette = 0.08f;

    inline float Breath(double Now)
    {
        const float Phase = BreathSeconds > 0.0f
            ? FMath::Fmod(static_cast<float>(FMath::Max(Now, 0.0)), BreathSeconds) / BreathSeconds : 0.0f;
        return 0.5f - 0.5f * FMath::Cos(Phase * 2.0f * PI);
    }

    // --- The look -----------------------------------------------------------
    struct FLook
    {
        float Saturation = 1.0f;
        float Vignette = 0.0f;
        // Zero when nothing is happening, so a healthy frame costs the
        // renderer nothing at all.
        float BlendWeight = 0.0f;
    };

    // DeathSaturation is the death beat's own drain (1 when no beat is
    // running); it is composed here rather than written by the beat because
    // the camera has one blend weight.
    inline FLook Compose(float SecondsSinceHit, float HealthFraction, float LowLine, double Now,
        float DeathSaturation)
    {
        FLook Look;
        const float Pulse = HitPulse(SecondsSinceHit);
        const float Depth = LowHealthDepth(HealthFraction, LowLine);

        float Vignette = Pulse * HitVignette;
        float Saturation = 1.0f - Pulse * HitDesaturation;
        if (Depth > 0.0f)
        {
            Vignette = FMath::Max(Vignette,
                FMath::Lerp(LowVignetteAtLine, LowVignetteAtZero, Depth) + Breath(Now) * BreathVignette);
            Saturation = FMath::Min(Saturation, 1.0f - LowDesaturationAtZero * Depth);
        }
        Saturation = FMath::Min(Saturation, FMath::Clamp(DeathSaturation, 0.0f, 1.0f));

        Look.Vignette = FMath::Clamp(Vignette, 0.0f, 1.0f);
        Look.Saturation = FMath::Clamp(Saturation, 0.0f, 1.0f);
        Look.BlendWeight = (Look.Vignette > 0.0f || Look.Saturation < 1.0f) ? 1.0f : 0.0f;
        return Look;
    }
}
