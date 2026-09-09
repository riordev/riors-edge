#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// THE RUINED PALETTE: WHAT TELLS A PLAYER WHICH RULES THEY ARE UNDER.
//
// A rift interior is the yard's own geometry — same map, PendingRift set, which
// is what an instanced rift means. That was always the cheap way to build one,
// but it left the player standing in ground they could not tell apart from the
// world outside, and under O268 the two now run DIFFERENT RULES: a rift is
// finite and has a completion condition, the world has patrols that come back.
// Clearing a pocket means "the objective is progressing" in one and "these will
// reform" in the other. The visual is what carries that distinction, which is
// why this is legibility and not decoration.
//
// The ruin is DERIVED from the living colour rather than authored as a second
// table. A parallel palette drifts the moment somebody adds a prefix to the
// living one, and the two would quietly stop being the same place — which is
// the one thing a dilapidated version of an area must never stop being.
// ---------------------------------------------------------------------------
namespace BreakerZonePalette
{
    // What is left of a surface nobody maintains is its value, not its hue.
    // O2 PLACEHOLDER.
    constexpr float DrainedFraction = 0.55f;
    // A ruin is not merely grey, it is oxidised. O2 PLACEHOLDER.
    constexpr float StainedFraction = 0.22f;
    // The lights went too. O2 PLACEHOLDER.
    constexpr float DimFraction = 0.78f;

    // Rust-earth. Deliberately warm: a ruin oxidises, and pulling toward
    // anything cool would collide with the one reserved colour in the project.
    // Teal is a noun — rift objects, suppression hardware, top-rarity frames,
    // beams and name text — and a rift's GROUND is not a rift object. Painting
    // the floor of a rift teal would spend the only colour that means
    // something on the surface the player looks at least.
    inline FLinearColor RuinAnchor() { return FLinearColor(0.26f, 0.17f, 0.11f); }

    inline FLinearColor Dilapidate(const FLinearColor& Living)
    {
        // Rec. 601 luma, the same weighting the rest of the project reads
        // brightness by.
        const float Luma = 0.30f * Living.R + 0.59f * Living.G + 0.11f * Living.B;
        const FLinearColor Grey(Luma, Luma, Luma, Living.A);
        FLinearColor Ruined = FMath::Lerp(Living, Grey, DrainedFraction);
        Ruined = FMath::Lerp(Ruined, RuinAnchor(), StainedFraction);
        Ruined.R *= DimFraction;
        Ruined.G *= DimFraction;
        Ruined.B *= DimFraction;
        Ruined.A = Living.A;
        return Ruined;
    }

    // How far apart two painted surfaces read, as a plain RGB distance. Held
    // here so a test can assert that the ruin is VISIBLY a ruin rather than a
    // slightly different grey — the owner's acceptance is "can you tell within
    // a few seconds of arriving", and this is as close as arithmetic gets to it.
    inline float Separation(const FLinearColor& A, const FLinearColor& B)
    {
        return FMath::Sqrt(FMath::Square(A.R - B.R) + FMath::Square(A.G - B.G) + FMath::Square(A.B - B.B));
    }

    // The floor under that: below this, two surfaces are the same surface.
    // O2 PLACEHOLDER — the owner's eye is the real acceptance, through frames.
    constexpr float ReadableSeparation = 0.045f;
}
