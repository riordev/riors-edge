#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/BreakerUIStyle.h"

// ---------------------------------------------------------------------------
// PUBLISHED PATH — owner GLASS, consumers KIT (Abilities/), FIELD (Combat/) — NOT GROUND.
//
// A published path has ONE owner and NAMED consumers. GLASS changes the
// implementation freely; a change to the PUBLIC SURFACE is a DECLARED
// CROSSING, told to the consumers before it lands. Same rule as a header
// across a lane boundary, except the consumers are named in advance instead
// of being discovered by whatever broke.
//
// The list above is MEASURED, not remembered, and re-measuring is one command:
//
//     grep -rl BreakerEffectMath Source/RiorsEdge
//
// minus UI/ (GLASS's own). Tests/ IS NEVER A CONSUMER — every lane writes its
// own tests, so a test touching this path belongs to whoever wrote it and adds
// no obligation. Re-measure before relying on this list: a stale one licenses a
// change that silently breaks a caller nobody listed. The row for the header
// beside this one was wrong for exactly that reason — measured for one file and
// transcribed onto the other.
// ---------------------------------------------------------------------------
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

    // --- Ground ring geometry ----------------------------------------------
    // A zone's rim as a closed polygon of strokes. Sixteen sides at Rot's
    // 400 cm radius means each chord bows at most ~8 cm off the true circle,
    // which reads as a circle; the count is O2 PLACEHOLDER and the stroke
    // pool fits two full rings by the pool test. The RADIUS IS NEVER
    // APPROXIMATED INWARD: every vertex sits exactly on the circle, so the
    // drawn rim is the true footprint and a target standing on the line is
    // standing on the real membership boundary (chords bow inward between
    // vertices, never outward past it).
    constexpr int32 GroundRingStrokes = 16;   // O2 PLACEHOLDER

    // PhaseRadians rotates the whole polygon about its centre (FIELD's rim
    // animation turns a ring by advancing the phase per frame); every vertex
    // stays exactly on the circle whatever the phase.
    inline FVector RingVertex(const FVector& Center, float RadiusCm, int32 Index, int32 Count,
        float PhaseRadians = 0.0f)
    {
        const int32 Wrapped = Count > 0 ? ((Index % Count) + Count) % Count : 0;
        const float Angle = (Count > 0 ? 2.0f * PI * static_cast<float>(Wrapped) / static_cast<float>(Count) : 0.0f)
            + PhaseRadians;
        return Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * RadiusCm;
    }

    // Endpoints of the Index'th side. Side i runs vertex i -> vertex i+1, so
    // consecutive sides share their endpoints exactly and the loop closes.
    // DashFraction (0..1) shortens each side about its own chord midpoint, so
    // a ring at 0.5 is a dashed ring with gaps as long as its dashes; at 1 the
    // loop is closed and the vertices are shared. A dash never leaves its
    // chord, so the drawn rim still never bows outward past the true circle.
    inline void RingStroke(const FVector& Center, float RadiusCm, int32 Index, int32 Count,
        FVector& OutA, FVector& OutB, float PhaseRadians = 0.0f, float DashFraction = 1.0f)
    {
        OutA = RingVertex(Center, RadiusCm, Index, Count, PhaseRadians);
        OutB = RingVertex(Center, RadiusCm, Index + 1, Count, PhaseRadians);
        const float Dash = FMath::Clamp(DashFraction, 0.0f, 1.0f);
        if (Dash < 1.0f)
        {
            const FVector Mid = (OutA + OutB) * 0.5;
            OutA = Mid + (OutA - Mid) * Dash;
            OutB = Mid + (OutB - Mid) * Dash;
        }
    }

    // --- Swept arc geometry -------------------------------------------------
    // Cleave's edge: the outer rim of a melee arc as consecutive chords.
    // ArcDegrees is the FULL included angle (the melee sweep's own
    // convention: 90 means +/-45 off Forward), Forward must be horizontal,
    // and vertex 0 sits on the arc's LEFT edge so a caller staggering
    // stroke delays by index gets a left-to-right sweep.
    constexpr int32 SweptArcStrokes = 8;   // O2 PLACEHOLDER

    inline FVector ArcVertex(const FVector& Origin, const FVector& Forward,
        float ArcDegrees, float RangeCm, int32 Index, int32 Count)
    {
        const float Half = ArcDegrees * 0.5f;
        const float Fraction = Count > 0 ? static_cast<float>(FMath::Clamp(Index, 0, Count)) / static_cast<float>(Count) : 0.0f;
        const float AngleDeg = -Half + ArcDegrees * Fraction;
        return Origin + Forward.RotateAngleAxis(AngleDeg, FVector::UpVector) * RangeCm;
    }

    inline void ArcStroke(const FVector& Origin, const FVector& Forward,
        float ArcDegrees, float RangeCm, int32 Index, int32 Count,
        FVector& OutA, FVector& OutB)
    {
        OutA = ArcVertex(Origin, Forward, ArcDegrees, RangeCm, Index, Count);
        OutB = ArcVertex(Origin, Forward, ArcDegrees, RangeCm, Index + 1, Count);
    }

    // --- The swept BLADE ----------------------------------------------------
    // Owner, on Cleave: "it's just a blue line that goes across your screen".
    // It was: eight chords on the OUTER RIM only, at one radius, drawn at
    // ActorLocation + 60 cm — within centimetres of the first-person camera. A
    // 120-degree arc at six and a half metres seen from its own centre, at eye
    // height, IS a line across the screen; there was nothing else to see
    // because the sweep had no interior and no thickness in depth.
    //
    // The blade is three arcs at different radii on one origin, so the swing
    // has a near edge and a far edge and reads as a fan passing THROUGH the
    // world rather than a fence popping up in it. Fractions rather than
    // authored distances: an Edge-widened cleave grows all three together, and
    // a shorter weapon shrinks them, with nothing to keep in step by hand.
    //
    // WHY THREE. Four is the stroke budget talking: the pool holds 48 and a
    // live zone rim is 16 of them, so a sweep is allowed 24 and three rings of
    // eight is exactly that with none left over for a second cleave mid-air.
    constexpr int32 SweptArcRings = 3;                              // O2 PLACEHOLDER
    inline float SweptArcRingFraction(int32 Ring)
    {
        // Not evenly spaced: the outer edge is the one that says how far the
        // swing reaches, so it keeps its own ring and the inner two crowd
        // toward it rather than filling the middle evenly.
        constexpr float Fractions[SweptArcRings] = { 0.42f, 0.74f, 1.0f };   // O2 PLACEHOLDER
        return Fractions[FMath::Clamp(Ring, 0, SweptArcRings - 1)];
    }
    // The inner rings are thinner and shorter-lived: an afterimage behind the
    // edge, not three edges.
    inline float SweptArcRingThickness(int32 Ring, float EdgeThicknessCm)
    {
        constexpr float Scale[SweptArcRings] = { 0.45f, 0.7f, 1.0f };        // O2 PLACEHOLDER
        return EdgeThicknessCm * Scale[FMath::Clamp(Ring, 0, SweptArcRings - 1)];
    }
    // And they arrive a little before the edge does, so the swing has a
    // direction in TIME as well as in angle.
    inline float SweptArcRingDelay(int32 Ring, float SweepSeconds)
    {
        constexpr float Lead[SweptArcRings] = { 0.0f, 0.35f, 0.7f };         // O2 PLACEHOLDER
        return SweepSeconds * Lead[FMath::Clamp(Ring, 0, SweptArcRings - 1)] * 0.25f;
    }

    // --- What colour a status is --------------------------------------------
    // The tint a carried status lends the thing carrying it (Fracture's
    // round). Total: an unmapped tag keeps the projectile's shipped violet
    // rather than inventing a colour. All O2 PLACEHOLDER; the poison green
    // is the zone actor's shipped ZoneColor so the round that will seed a
    // puddle matches the puddle. Deliberately not teal (O19) and the mapped
    // colours reuse tokens that already carry these meanings elsewhere
    // (Harm red is what a Bleed already draws in the damage feed).
    inline FLinearColor ColorForStatusTag(const FGameplayTag& StatusTag, const FLinearColor& Fallback)
    {
        static const FGameplayTag BleedTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
        static const FGameplayTag PoisonTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false);
        if (StatusTag.IsValid() && StatusTag == BleedTag) return FLinearColor(1.0f, 0.25f, 0.25f);
        if (StatusTag.IsValid() && StatusTag == PoisonTag) return FLinearColor(0.35f, 0.85f, 0.25f);
        if (StatusTag.IsValid() && StatusTag == FGameplayTag::RequestGameplayTag(TEXT("Status.Rot"), false)) return BreakerUI::Orange;
        return Fallback;
    }

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
