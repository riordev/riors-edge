#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// Camera shake, pure. A TRAUMA model: discrete events (a shot fired, a hit
// taken) add trauma, trauma decays linearly, and the applied amplitude is
// trauma SQUARED — so a single small event barely registers while a burst
// of damage lands hard, and everything dies fast. The offset itself is
// Perlin noise over time, deterministic and continuous, never a random
// jitter (jitter reads as a broken camera; noise reads as force).
//
// RULED CONSTRAINTS, recorded here: SUBTLE — this is a game played for
// hours and shake is the most over-applied tool in the box; and TUNABLE
// FROM THE FIRST COMMIT — every figure below is the default behind a
// Breaker.Shake.* console variable (BreakerCharacter.cpp), so the owner
// tunes it with a controller in hand rather than through a compile. Fire
// and take-damage only; nothing on kills, nothing on ability casts, until
// asked for.
//
// The offset is applied as a NET-ZERO control-rotation delta (new minus
// last, every frame): the camera follows the control rotation directly, so
// this is the one channel that moves the view, and because the noise is
// zero-centred and the trauma decays to nothing, the aim ends where it
// began. All figures O2 PLACEHOLDER.
// ---------------------------------------------------------------------------
namespace BreakerShake
{
    inline float DecayTrauma(float Trauma, float DecayPerSecond, float DeltaSeconds)
    {
        return FMath::Max(0.0f, Trauma - FMath::Max(DecayPerSecond, 0.0f) * FMath::Max(DeltaSeconds, 0.0f));
    }

    inline float AddTrauma(float Trauma, float Amount)
    {
        return FMath::Clamp(Trauma + FMath::Max(Amount, 0.0f), 0.0f, 1.0f);
    }

    // Trauma squared: the perceptual ramp that keeps single events small.
    inline float ShakeAmplitude(float Trauma)
    {
        const float Clamped = FMath::Clamp(Trauma, 0.0f, 1.0f);
        return Clamped * Clamped;
    }

    // The offset for one frame. Two independent noise channels (offset far
    // apart along the axis) so pitch and yaw never sync into a circle.
    // |Pitch| <= MaxPitchDegrees * amplitude, |Yaw| likewise; zero trauma is
    // exactly zero offset.
    inline FRotator ShakeOffset(float Trauma, double TimeSeconds, float FrequencyHz,
        float MaxPitchDegrees, float MaxYawDegrees)
    {
        const float Amplitude = ShakeAmplitude(Trauma);
        if (Amplitude <= 0.0f) return FRotator::ZeroRotator;
        const float T = static_cast<float>(TimeSeconds) * FMath::Max(FrequencyHz, 0.0f);
        return FRotator(
            MaxPitchDegrees * Amplitude * FMath::PerlinNoise1D(T),
            MaxYawDegrees * Amplitude * FMath::PerlinNoise1D(T + 71.3f),
            0.0f);
    }
}
