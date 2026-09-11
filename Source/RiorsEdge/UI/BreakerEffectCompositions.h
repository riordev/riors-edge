#pragma once

#include "CoreMinimal.h"
#include "UI/BreakerEffectMath.h"
#include "UI/BreakerEffectRenderer.h"

// ---------------------------------------------------------------------------
// THE COMPOSITIONS: what a glow and a stroke add up to.
//
// Owner, 2026-09-11: "adding some minor visual effects for each of them is
// super important, not just those two, but all of Swift and Caster's
// abilities". Every ability already drew SOMETHING, and every something was
// the same something — one glow and one to four strokes, alive for a fifth of
// a second — so eleven abilities looked like one ability at eleven sizes.
// BreakerEffectRenderer.h names these as the unbuilt "Phase C".
//
// Five shapes, each a handful of pooled primitives, each with a direction in
// TIME (staggered births) because a shape that pops is a fence and a shape
// that opens is an event. Published from GLASS for KIT to compose with; the
// renderer's public surface is unchanged.
//
// THE BUDGET IS REAL: 48 strokes, 16 glows, 4 lights, and a live Rot rim owns
// 16 strokes. Every shape here states its cost.
// ---------------------------------------------------------------------------
namespace BreakerFXCompose
{
    // A ring on the ground that OPENS — the strokes arrive round the circle
    // over SweepSeconds. Twelve strokes. The blast ring the owner liked is this
    // shape; nothing in the kit was using it.
    inline void GroundRing(ABreakerEffectRenderer* Effects, const FVector& Center, float RadiusCm,
        const FLinearColor& Color, float ThicknessCm, float Intensity,
        const BreakerFX::FEffectTiming& Timing, float SweepSeconds, float DelaySeconds = 0.0f)
    {
        if (!Effects) return;
        constexpr int32 Count = 12;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FVector A, B;
            BreakerFX::RingStroke(Center, RadiusCm, Index, Count, A, B);
            Effects->AddStroke(A, B, ThicknessCm, Color, Intensity, Timing,
                DelaySeconds + SweepSeconds * Index / Count);
        }
    }

    // Two rings, the second a beat after the first: outward when the second
    // is larger (a shockwave), inward when it is smaller (a stop, a gather).
    // Twenty-four strokes.
    inline void Ripple(ABreakerEffectRenderer* Effects, const FVector& Center, float FirstRadiusCm,
        float SecondRadiusCm, const FLinearColor& Color, float ThicknessCm, float Intensity,
        const BreakerFX::FEffectTiming& Timing, float BeatSeconds)
    {
        GroundRing(Effects, Center, FirstRadiusCm, Color, ThicknessCm, Intensity, Timing, 0.0f, 0.0f);
        BreakerFX::FEffectTiming Second = Timing;
        Second.DurationSeconds = FMath::Max(0.05f, Timing.DurationSeconds - BeatSeconds);
        GroundRing(Effects, Center, SecondRadiusCm, Color, ThicknessCm * 0.7f, Intensity * 0.8f, Second, 0.0f, BeatSeconds);
    }

    // A point that goes off: glow, light, six spokes. Eight primitives.
    inline void Burst(ABreakerEffectRenderer* Effects, const FVector& Center, float RadiusCm,
        const FLinearColor& Color, float Intensity, const BreakerFX::FEffectTiming& Timing,
        float LightRadiusCm, float LightIntensity, float DelaySeconds = 0.0f)
    {
        if (!Effects) return;
        Effects->AddGlow(Center, RadiusCm, Color, Intensity, Timing, DelaySeconds);
        Effects->AddBlinkLight(Center, LightRadiusCm, Color, LightIntensity, Timing, DelaySeconds);
        for (int32 Index = 0; Index < 6; ++Index)
        {
            // Tilted off the horizontal plane so the spokes read from any
            // height, not only from above.
            const FVector Out = FRotator(Index % 2 == 0 ? 22.0f : -18.0f, 60.0f * Index, 0.0f).Vector();
            Effects->AddStroke(Center + Out * RadiusCm * 0.6f, Center + Out * RadiusCm * 2.2f,
                FMath::Max(2.0f, RadiusCm * 0.12f), Color, Intensity * 0.8f, Timing,
                DelaySeconds + 0.02f * Index);
        }
    }

    // A line that TRAVELS: segments born in order from A to B over
    // TravelSeconds, each living a little past the next one's birth so the
    // whole reads as one thing moving. Segments strokes.
    inline void Trail(ABreakerEffectRenderer* Effects, const FVector& From, const FVector& To,
        int32 Segments, float ThicknessCm, const FLinearColor& Color, float Intensity,
        float TravelSeconds, float SegmentLifeSeconds, float DelaySeconds = 0.0f)
    {
        if (!Effects) return;
        Segments = FMath::Max(1, Segments);
        BreakerFX::FEffectTiming Life;
        Life.DurationSeconds = SegmentLifeSeconds;
        Life.FadeInSeconds = 0.0f;
        Life.FadeOutSeconds = SegmentLifeSeconds * 0.7f;
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float T0 = static_cast<float>(Index) / Segments;
            const float T1 = static_cast<float>(Index + 1) / Segments;
            Effects->AddStroke(FMath::Lerp(From, To, T0), FMath::Lerp(From, To, T1), ThicknessCm,
                Color, Intensity, Life, DelaySeconds + TravelSeconds * T0);
        }
    }

    // Pips along a line: glows at even intervals, born outward. Count glows.
    inline void Pips(ABreakerEffectRenderer* Effects, const FVector& From, const FVector& To,
        int32 Count, float RadiusCm, const FLinearColor& Color, float Intensity,
        const BreakerFX::FEffectTiming& Timing, float TravelSeconds)
    {
        if (!Effects) return;
        Count = FMath::Max(1, Count);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const float T = static_cast<float>(Index + 1) / Count;
            Effects->AddGlow(FMath::Lerp(From, To, T), RadiusCm, Color, Intensity, Timing, TravelSeconds * T);
        }
    }
}
