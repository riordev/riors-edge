#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// Tracer flight maths.
//
// A hitscan shot resolves instantly — that is the rule, and nothing here may
// change it. But a round that APPEARS instantly, at full length, from the
// muzzle to the wall, reads as a laser rather than as a bullet. So the shot is
// recorded once and this header replays it: a short bright segment that
// travels from the visual muzzle to the impact over a handful of frames, and
// an impact burst that starts when the round ARRIVES rather than when the
// trigger was pulled.
//
// Everything here is pure — no world, no canvas, no actors — which is what
// makes it testable at all, since the drawing itself is not. Same discipline
// as BreakerHUDResourceRow.h.
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
        float SpeedCms = 26000.0f;          // O2 PLACEHOLDER
        // Length of the visible streak. Short enough to read as an object in
        // flight, long enough to survive a 60 Hz sample.
        float LengthCm = 900.0f;            // O2 PLACEHOLDER
        // Flight time is clamped into this band whatever the distance, by
        // trimming the effective speed. The floor guarantees a point-blank
        // shotgun round still occupies ~3 frames instead of vanishing; the
        // ceiling stops a 100 m sniper round from floating lazily downrange.
        float MinFlightSeconds = 0.05f;     // O2 PLACEHOLDER
        float MaxFlightSeconds = 0.22f;     // O2 PLACEHOLDER
        // Below this the muzzle and the impact are effectively the same point
        // and a streak would be a dot with a random direction: draw nothing.
        float MinimumTravelCm = 60.0f;      // O2 PLACEHOLDER
    };

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
    // burst is scheduled off this, so the spark appears where and WHEN the
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
        // hand over to the impact burst.
        bool bArrived = false;
        FVector Tail = FVector::ZeroVector;
        FVector Head = FVector::ZeroVector;
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

        Sample.bVisible = true;
        Sample.Head = Origin + Direction * HeadDistance;
        Sample.Tail = Origin + Direction * TailDistance;
        Sample.HeadFraction = HeadDistance / Distance;
        return Sample;
    }

    // Two unit vectors spanning the plane the round punched through. The
    // impact burst is drawn in this plane, so it sits ON the surface being
    // shot instead of facing the camera like a sticker.
    //
    // FBreakerShotResult carries no impact normal (adding one touches the
    // weapon component's shot contract, which a recoil layer just landed in),
    // so the plane is taken perpendicular to the round's travel. For anything
    // that is not a glancing shot the two are within a few degrees.
    inline void ImpactBasis(const FVector& TravelDirection, FVector& OutU, FVector& OutV)
    {
        const FVector Forward = TravelDirection.GetSafeNormal();
        if (Forward.IsNearlyZero())
        {
            OutU = FVector::RightVector;
            OutV = FVector::UpVector;
            return;
        }
        // Pick whichever world axis is least parallel to the travel so the
        // cross product never degenerates.
        const FVector Seed = FMath::Abs(Forward.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
        OutU = FVector::CrossProduct(Forward, Seed).GetSafeNormal();
        OutV = FVector::CrossProduct(Forward, OutU).GetSafeNormal();
    }

    // Screen half-thickness in pixels for a world radius at a given depth,
    // given the canvas height and the vertical half-FOV. This is what makes a
    // far round thin and a near round fat; the old tracer was a constant
    // 1.25px whether the wall was 2 m or 80 m away.
    inline float WorldRadiusToPixels(float WorldRadiusCm, float DepthCm, float ViewportHeight,
        float VerticalHalfFOVRadians)
    {
        const float Depth = FMath::Max(DepthCm, 1.0f);
        const float Tangent = FMath::Max(FMath::Tan(VerticalHalfFOVRadians), KINDA_SMALL_NUMBER);
        return WorldRadiusCm * ViewportHeight * 0.5f / (Depth * Tangent);
    }
}
