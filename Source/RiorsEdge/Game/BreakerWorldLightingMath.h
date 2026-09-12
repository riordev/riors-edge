#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// THE SHIPPED LIGHTING RIG, world-free (O277).
//
// Every map without an authored directional light is lit by one runtime rig
// (BreakerWorldBasics::EnsureWorldLighting, O42). The owner's read of the
// world was "flat, stale, soulless": a high white sun, a bare atmosphere,
// haze that started beyond the yard, and no anti-aliasing. O277 rules the
// world is lit by one LOW WARM sun and a COOL sky, and that the air has depth
// inside play range. The numbers live here so a pin can assert the rig the
// owner photographs is the rig the code ships; the actor is a thin caller.
//
// EVERY VALUE IS O2 PLACEHOLDER, judged only by photograph.
// ---------------------------------------------------------------------------
namespace BreakerWorldLighting
{
    // THE SUN. Pitch -28 is a late-afternoon sun: long shadows across the
    // yard that give every block a lit face and a shaded face (the -50 of
    // the old rig lit both faces nearly alike). Yaw 40 keeps the plaza's
    // forward axis lit from the side. 5200 K is warm without the orange
    // wash of a sunset; intensity drops with the angle so the lit face is
    // not blown against the exposure below.
    constexpr float SunPitchDegrees = -28.0f;      // O2 PLACEHOLDER
    constexpr float SunYawDegrees = 40.0f;         // O2 PLACEHOLDER
    constexpr float SunIntensityLux = 5.5f;        // O2 PLACEHOLDER
    constexpr float SunTemperatureKelvin = 5200.0f; // O2 PLACEHOLDER

    // THE SKY. The sky light carries the atmosphere's blue into shadow;
    // intensity 1.0 is the engine default and reads as a cool fill against
    // the warm sun, which is the whole warm/cool contrast the ruling asks
    // for. The lower hemisphere is a dark earth so bounce does not read
    // blue on the underside of overhangs.
    constexpr float SkyIntensity = 1.0f;                               // O2 PLACEHOLDER
    inline FLinearColor SkyLowerHemisphereColor() { return FLinearColor(0.08f, 0.07f, 0.05f); } // O2 PLACEHOLDER

    // THE AIR. Haze begins at 600 cm — inside the lane, so the far end of
    // the yard (106 m) recedes and the buildings beyond it sit in a band
    // of cool air rather than at full local contrast. Density doubled from
    // the old 0.010; falloff lower so the haze is a ground layer, not a
    // wall. Inscatter cooler and lighter than the old steel-blue so the
    // far field lifts toward the sky instead of darkening.
    constexpr float FogDensity = 0.022f;                 // O2 PLACEHOLDER
    constexpr float FogHeightFalloff = 0.20f;            // O2 PLACEHOLDER
    constexpr float FogStartDistanceCm = 600.0f;         // O2 PLACEHOLDER
    constexpr float FogMaxOpacity = 0.85f;               // O2 PLACEHOLDER
    inline FLinearColor FogInscatteringColor() { return FLinearColor(0.42f, 0.52f, 0.66f); } // O2 PLACEHOLDER

    // THE EXPOSURE. Auto-exposure is off project-wide, so this bias is the
    // whole exposure. -0.4 against the lower sun keeps the lit concrete off
    // white while the shaded faces stay readable.
    constexpr float ExposureBiasEV = -0.4f;              // O2 PLACEHOLDER

    // THE GRADE. A touch of contrast and a slight pull on saturation so the
    // flat placeholder colours stop reading as a palette swatch; the shadow
    // gain lifts the cool fill so shadow is blue, not black.
    constexpr float GradeContrast = 1.06f;               // O2 PLACEHOLDER
    constexpr float GradeSaturation = 0.92f;             // O2 PLACEHOLDER
    inline FVector4 GradeShadowsGain() { return FVector4(0.98f, 1.0f, 1.06f, 1.0f); } // O2 PLACEHOLDER

    // The one relation the ruling states in words: haze starts INSIDE play
    // range. The yard slab is 106 m long; the lane between chest pairs is
    // 19.8 m. Start must sit past arm's reach (a body at 2 m is not hazed)
    // and inside the lane.
    constexpr float PlayRangeCm = 1980.0f;               // O2 PLACEHOLDER, the lane
    constexpr bool HazeBeginsInsidePlayRange(float StartCm) { return StartCm > 200.0f && StartCm < PlayRangeCm; }

    // The sun is LOW and WARM by the ruling's words: below 35 degrees, above
    // the horizon enough to clear the perimeter (a 16 m tower at 56 m casts
    // 30 m at -28), and below daylight-white (5500 K).
    constexpr bool SunIsLowAndWarm(float PitchDegrees, float Kelvin) { return PitchDegrees > -35.0f && PitchDegrees < -15.0f && Kelvin < 5500.0f && Kelvin > 3500.0f; }
}
