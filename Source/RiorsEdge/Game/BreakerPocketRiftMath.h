#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// THE SHAPE AND THE CLOCK OF A POCKET RIFT, world-free.
//
// A pocket rift is the thing a returning patrol comes OUT of. O268 gave the
// world a repopulation clock and a clearance gate, which made a return FAIR;
// nothing made it READ. The courtyard is the one pocket with an authored
// doorway (BreakerFernhallCourtyardBuilder), and authoring four more mouths
// across the yard is a composer job. A small rift needs no geometry at all and
// fits the fiction better than a door would: this is a place where the world
// is coming apart, and the things that hunt you are coming through it.
//
// EVERYTHING HERE IS ARITHMETIC. The actor is a thin caller, so the silhouette
// and the pulse are provable on bare floats and the only thing the runtime adds
// is meshes.
// ---------------------------------------------------------------------------
namespace BreakerPocketRift
{
    // THE SILHOUETTE, and it is deliberately not a circle. A ring on the
    // ground already means "explosion" (O179 spends orange on that verb and
    // the Volatile blast draws exactly that shape), and a rectangle means
    // "door". A vertical LENS — pointed top and bottom, full through the
    // middle — is neither, and it is the one shape in this project that is
    // not a polygon or a disc.
    //
    // T runs 0 at the bottom point to 1 at the top point. The sine is zero at
    // both ends, which is what makes them points rather than caps; the
    // exponent above one sharpens them without narrowing the middle, so the
    // shape stays wide enough to walk a body out of.
    inline float TearHalfWidth(float T, float WidthCm)
    {
        const float Clamped = FMath::Clamp(T, 0.0f, 1.0f);
        const float Span = FMath::Max(WidthCm, 0.0f);
        // THE SINE IS FLOORED AT ZERO AND THAT IS NOT DEFENSIVE PADDING. In
        // single precision Sin(PI) is about -8.7e-8, not 0, and a fractional
        // Pow of a negative base is NaN — so the TOP POINT of the tear came out
        // NaN, every segment touching it took a NaN transform, and the engine
        // dumped a stack from FRotator::Quaternion. Found by the capture, on
        // the first frame it drew.
        const float Curve = FMath::Max(0.0f, FMath::Sin(PI * Clamped));
        return 0.5f * Span * FMath::Pow(Curve, 1.4f);
    }

    // WHERE ON THE EDGE. Y is across the tear, Z up it, X is zero: the tear is
    // flat and the actor's own rotation is what aims it. bRightEdge picks which
    // side, so one function draws both and they cannot drift apart.
    inline FVector TearEdgePoint(float T, float WidthCm, float HeightCm, bool bRightEdge)
    {
        const float Clamped = FMath::Clamp(T, 0.0f, 1.0f);
        const float Half = TearHalfWidth(Clamped, WidthCm);
        return FVector(0.0f, bRightEdge ? Half : -Half, (Clamped - 0.5f) * HeightCm);
    }

    // ---- RAGGED, AND DETERMINISTICALLY SO --------------------------------
    // A clean lens reads as an EMBLEM — the first capture looked like a logo
    // painted on the air. A tear has to look torn, which means the edge has to
    // wander. One hash, no RNG state and no seed to carry: the same tear draws
    // the same way in every run and in every process, which is what makes it
    // photographable at all.
    inline float EdgeNoise(int32 Key)
    {
        uint32 Mixed = static_cast<uint32>(Key) * 2654435761u;
        Mixed ^= Mixed >> 15;
        Mixed *= 2246822519u;
        Mixed ^= Mixed >> 13;
        return static_cast<float>(Mixed & 0xFFFFu) / 65535.0f;
    }

    // The edge point with its wander applied. OUTWARD IS POSITIVE ON EACH SIDE,
    // so the two edges wander independently rather than mirroring each other,
    // which is the difference between a tear and a leaf.
    //
    // The wander is a FRACTION OF THE LOCAL HALF-WIDTH, which is what keeps the
    // two tips sharp: the profile is zero there, so there is nothing to wander
    // by, and the shape still closes to a point at both ends.
    inline FVector RaggedEdgePoint(int32 Step, int32 Steps, float WidthCm, float HeightCm,
        bool bRightEdge, float JitterFraction)
    {
        const float T = Steps > 0 ? static_cast<float>(Step) / static_cast<float>(Steps) : 0.0f;
        FVector Point = TearEdgePoint(T, WidthCm, HeightCm, bRightEdge);
        const float Room = TearHalfWidth(T, WidthCm) * FMath::Max(JitterFraction, 0.0f);
        const float Offset = (EdgeNoise(Step * 7 + (bRightEdge ? 3671 : 911)) * 2.0f - 1.0f) * Room;
        Point.Y += bRightEdge ? Offset : -Offset;
        return Point;
    }

    // THE RESTING BREATH. A rift that is perfectly still is a prop; one that
    // moves is a thing the level is doing. Small on purpose — this is five
    // standing lights in a yard the player crosses constantly, and anything
    // large enough to catch the eye at rest would compete with the flare that
    // actually means something.
    inline float IdleScale(float Seconds, float Hz, float Amplitude)
    {
        return 1.0f + Amplitude * FMath::Sin(Seconds * Hz * 2.0f * PI);
    }

    // THE ARRIVAL. Remaining counts DOWN from Total, so the boost is 1 at the
    // instant the body comes through and falls away after it.
    //
    // SQUARED, WHICH IS AN INSTANT ATTACK AND A SLOW RELEASE. The player is
    // usually looking somewhere else when a patrol returns — that is what the
    // clearance gate guarantees — so the spike has to be at frame one and the
    // tail has to last long enough to be found by an eye that turns toward it.
    inline float FlareBoost(float Remaining, float Total)
    {
        if (Total <= 0.0f || Remaining <= 0.0f) return 0.0f;
        const float Fraction = FMath::Clamp(Remaining / Total, 0.0f, 1.0f);
        return Fraction * Fraction;
    }
}
