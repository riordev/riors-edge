#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// Tracer flight maths.
//
// A hitscan shot resolves instantly — that is the rule, and nothing here may
// change it. But a round that APPEARS instantly, at full length, from the
// muzzle to the wall, reads as a laser rather than as a bullet. So the shot is
// recorded once and this header replays it: a short bright dash that travels
// from the visual muzzle to the impact over a handful of frames, and an impact
// spark that starts when the round ARRIVES rather than when the trigger was
// pulled.
//
// SECOND PASS, and the change is the APPROACH, not the numbers. The first pass
// drew all of this on the HUD canvas and wrote down, in DrawTracers, the trade
// it was taking: a canvas line does not depth-sort, so it composites over the
// world. That is exactly the "weird" the owner reported — a bright stroke
// sliding over the screen rather than an object in the room. The drawing now
// lives in ABreakerTracerRenderer, which puts pooled unlit-additive primitives
// in the world where the depth buffer can occlude them.
//
// Everything in THIS header stays pure — no world, no canvas, no actors —
// which is what makes it testable at all, since the drawing itself is not.
// Same discipline as BreakerHUDResourceRow.h.
// ---------------------------------------------------------------------------
namespace BreakerHUD
{
    // Tuning for the whole tracer layer. Every value is O2 PLACEHOLDER: no
    // playtest has set any of them.
    struct FTracerFlight
    {
        // Nominal round speed in cm/s. Real rifle rounds are ~90,000 cm/s,
        // which crosses a 30 m room in a third of a frame and is therefore
        // invisible. This is a READABILITY speed, not a ballistic one.
        float SpeedCms = 30000.0f;          // O2 PLACEHOLDER
        // Total length of the visible streak, head plus trail.
        //
        // Was 900 cm. Nine metres is not a round, it is a rod: at any range
        // where you can see both ends it reads as a thrown javelin. A real
        // tracer reads as a bright point with a short smear behind it, so the
        // streak is now roughly one body-length of a car and most of its
        // brightness is in the front fifth.
        float LengthCm = 240.0f;            // O2 PLACEHOLDER
        // The bright front section. The remainder is the dim trail.
        float HeadLengthCm = 55.0f;         // O2 PLACEHOLDER
        // Flight time is clamped into this band whatever the distance, by
        // trimming the effective speed. The floor guarantees a close round
        // still occupies ~3 frames instead of vanishing; the ceiling stops a
        // 100 m sniper round from floating lazily downrange.
        float MinFlightSeconds = 0.045f;    // O2 PLACEHOLDER
        float MaxFlightSeconds = 0.20f;     // O2 PLACEHOLDER
        // Below this the muzzle and the impact are effectively the same point
        // and a streak would be a dot with a random direction: draw nothing.
        // Raised with the shorter streak — a 1 m shot cannot show a 2.4 m dash
        // without the dash sticking out the back of whatever was shot.
        float MinimumTravelCm = 120.0f;     // O2 PLACEHOLDER
    };

    // How a round is painted. Separate from the flight because these are the
    // knobs you reach for when it looks wrong rather than when it feels wrong.
    struct FTracerLook
    {
        // World thickness of the bright head, in centimetres. A round is about
        // this wide; the screen-width floor below is what keeps it visible at
        // range without making it fat up close.
        float ThicknessCm = 2.6f;               // O2 PLACEHOLDER
        // The trail is thinner than the head, which is what makes the round
        // read as pointed rather than as a bar.
        float TrailThicknessScale = 0.6f;       // O2 PLACEHOLDER
        // Minimum on-screen width as a fraction of viewport HEIGHT. 0.0013 is
        // ~1.4 px at 1080p: below that a round at 80 m aliases out of
        // existence between frames.
        float MinScreenFraction = 0.0013f;      // O2 PLACEHOLDER
        // Additive colour multipliers. The head is the bright one; the trail
        // is a fraction of it. Both are applied to the orange token, never to
        // a hand-authored colour.
        float HeadIntensity = 3.2f;             // O2 PLACEHOLDER
        float TrailIntensity = 0.55f;           // O2 PLACEHOLDER

        // --- Impact -----------------------------------------------------
        // Was a six-spoke star drawn in the plane perpendicular to travel. A
        // star is a symbol, not an event: six even rays with no impact normal
        // to sit on read as a HUD sticker pasted on the wall. This is a point
        // flash instead — a small bright sphere that pops and dies. It has no
        // orientation, so there is nothing about it that can be oriented
        // wrongly, which is the entire argument for it.
        float ImpactSeconds = 0.10f;            // O2 PLACEHOLDER
        float ImpactRadiusCm = 13.0f;           // O2 PLACEHOLDER
        float ImpactIntensity = 4.0f;           // O2 PLACEHOLDER

        // --- Impact light -----------------------------------------------
        // The emissive spark is a shape; the point light is what makes the
        // wall AROUND the impact answer it. Slightly longer than the spark so
        // the glow is the thing that lingers, the way an afterimage does.
        // Weak points blink brighter, in their gold.
        float ImpactLightSeconds = 0.14f;       // O2 PLACEHOLDER
        float ImpactLightIntensity = 5000.0f;   // O2 PLACEHOLDER
        float ImpactLightRadiusCm = 420.0f;     // O2 PLACEHOLDER
        float WeakPointLightScale = 2.6f;       // O2 PLACEHOLDER
    };

    // --- The live tuning surface --------------------------------------------
    // ONE mutable copy of each knob struct, shared by the renderer (which
    // draws with it) and the HUD (which schedules impact arrival with it),
    // so a tuned round speed moves the streak and the spark's timing
    // together. Every field is exposed as a Breaker.Tracer.* console
    // variable in BreakerTracerRenderer.cpp — the owner tunes with a
    // controller in hand; nothing in code picks a value. The defaults are
    // exactly the structs' authored O2 PLACEHOLDER figures above, which the
    // automation tests still read from fresh struct instances.
    inline FTracerFlight& LiveTracerFlight() { static FTracerFlight GLiveFlight; return GLiveFlight; }
    inline FTracerLook& LiveTracerLook() { static FTracerLook GLiveLook; return GLiveLook; }

    // Effective speed for one shot: the nominal speed, trimmed so the flight
    // lands inside the band. Distance is the muzzle-to-impact length.
    inline float TracerEffectiveSpeed(const FTracerFlight& Flight, float DistanceCm)
    {
        const float Distance = FMath::Max(DistanceCm, 1.0f);
        const float MinSpeed = Distance / FMath::Max(Flight.MaxFlightSeconds, KINDA_SMALL_NUMBER);
        const float MaxSpeed = Distance / FMath::Max(Flight.MinFlightSeconds, KINDA_SMALL_NUMBER);
        return FMath::Clamp(Flight.SpeedCms, MinSpeed, MaxSpeed);
    }

    // Seconds between the trigger pull and the round landing. The impact
    // spark is scheduled off this, so the flash appears where and WHEN the
    // round gets there.
    inline float TracerFlightSeconds(const FTracerFlight& Flight, float DistanceCm)
    {
        const float Distance = FMath::Max(DistanceCm, 1.0f);
        return Distance / TracerEffectiveSpeed(Flight, Distance);
    }

    // One frame's worth of streak.
    struct FTracerSample
    {
        // False means draw nothing this frame: the round has not left, has
        // already landed, or never had room to be a streak at all.
        bool bVisible = false;
        // True once the head reaches the destination. The caller uses this to
        // hand over to the impact spark.
        bool bArrived = false;
        FVector Tail = FVector::ZeroVector;
        FVector Head = FVector::ZeroVector;
        // Where the bright head section starts. Always between Tail and Head;
        // equal to Tail on the first frames, when the whole round is head.
        FVector HeadStart = FVector::ZeroVector;
        // 0 at the muzzle, 1 at the impact. Drives the brightness ramp so a
        // round dims very slightly as it goes away from the shooter.
        float HeadFraction = 0.0f;
    };

    inline FTracerSample SampleTracer(const FTracerFlight& Flight, const FVector& Origin,
        const FVector& Destination, float AgeSeconds)
    {
        FTracerSample Sample;
        const FVector Delta = Destination - Origin;
        const float Distance = static_cast<float>(Delta.Size());
        if (Distance < Flight.MinimumTravelCm) return Sample;
        if (AgeSeconds < 0.0f) return Sample;

        const FVector Direction = Delta / Distance;
        const float Speed = TracerEffectiveSpeed(Flight, Distance);
        const float HeadDistance = AgeSeconds * Speed;
        if (HeadDistance >= Distance)
        {
            Sample.bArrived = true;
            return Sample;
        }

        const float TailDistance = FMath::Max(0.0f, HeadDistance - Flight.LengthCm);
        // A streak shorter than a couple of centimetres is a dot; the first
        // frame after firing is allowed to be one, but not a degenerate one.
        if (HeadDistance - TailDistance < 1.0f) return Sample;

        // The head section never runs off the back of the streak: on the first
        // frames the whole round IS the head and there is no trail at all.
        const float HeadStartDistance =
            FMath::Max(TailDistance, HeadDistance - FMath::Max(Flight.HeadLengthCm, 1.0f));

        Sample.bVisible = true;
        Sample.Head = Origin + Direction * HeadDistance;
        Sample.Tail = Origin + Direction * TailDistance;
        Sample.HeadStart = Origin + Direction * HeadStartDistance;
        Sample.HeadFraction = HeadDistance / Distance;
        return Sample;
    }

    // True when the sample has a trail worth drawing behind its head. Below a
    // centimetre the trail primitive is a degenerate sliver and is skipped.
    inline bool TracerHasTrail(const FTracerSample& Sample)
    {
        return Sample.bVisible && (Sample.HeadStart - Sample.Tail).SizeSquared() > 1.0;
    }

    // World thickness that never falls below a floor in SCREEN terms. A round
    // 80 m away is genuinely about 1 cm wide and would be sub-pixel, so it
    // would strobe in and out between frames; this widens it in world space
    // just enough to hold a stable pixel or so, and leaves near rounds alone.
    inline float TracerThicknessCm(float BaseThicknessCm, float CameraDistanceCm,
        float MinScreenFraction, float VerticalHalfFOVRadians)
    {
        const float Distance = FMath::Max(CameraDistanceCm, 1.0f);
        const float Tangent = FMath::Max(FMath::Tan(VerticalHalfFOVRadians), KINDA_SMALL_NUMBER);
        // Full world height covered by the viewport at that distance.
        const float WorldHeight = 2.0f * Distance * Tangent;
        return FMath::Max(BaseThicknessCm, WorldHeight * FMath::Max(MinScreenFraction, 0.0f));
    }

    // --- Tracer cadence -----------------------------------------------------
    // Real belts carry one tracer every few rounds. Drawing a streak for every
    // single round is most of why held automatic fire read as a laser show:
    // at 600 RPM with a 0.15 s flight there are two and a half streaks alive
    // at all times, overlapping into one continuous rod out of the barrel.
    //
    // Slow weapons are exempt. A bolt-action that traced one shot in three
    // would look broken rather than restrained, because the player sees each
    // shot as a separate event and two of them would show nothing.
    inline int32 TracerRoundsPerTracer(float RoundsPerMinute)
    {
        // EVERY ROUND LEAVES A STREAK. This returned 3 above 300 RPM on the
        // reasoning that individual rounds stop being individually readable at
        // five a second -- true, but it made the wrong trade: two rounds in
        // three drew nothing at all, so the rifle read as a weapon that
        // produces impacts without ever firing anything. An unreadable streak
        // still says a round went out; an absent one says nothing did.
        //
        // The cost this bought is real and is now unpaid: held automatic fire
        // reads as one continuous beam. That is a LOOK problem -- thickness,
        // lifetime, head/trail falloff -- and thinning the cadence was fixing
        // it in the wrong place, by deleting rounds rather than by making a
        // dense stream read as a stream. O2 PLACEHOLDER.
        return 1;
    }

    // Which rounds of a burst get a streak. RoundIndex counts every round the
    // player has fired; the modulo makes the first round of any fresh
    // engagement trace, which is the one that teaches where the gun points.
    inline bool ShouldTraceRound(int32 RoundIndex, int32 RoundsPerTracer)
    {
        if (RoundsPerTracer <= 1) return true;
        return (FMath::Max(RoundIndex, 0) % RoundsPerTracer) == 0;
    }
}
