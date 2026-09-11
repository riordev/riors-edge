#pragma once

#include "CoreMinimal.h"
#include "UI/BreakerEffectMath.h"
#include "UI/BreakerUIStyle.h"

// ---------------------------------------------------------------------------
// THE FOUR MOMENTS (ORDERS Part Five, GLASS-1). World-free: which Niagara
// asset a moment resolves to, which O179 colour it wears, and what the pooled
// primitive fallback draws while the asset is unauthored.
//
// A Niagara system is content, and content is authored in the editor by a
// person. Everything here exists so that the systems can land LATER without a
// line of plumbing changing: the renderer resolves `/Game/Breaker/FX/NS_<Moment>`
// lazily, caches the answer (including "none authored"), and draws the
// fallback below until the file exists — the same shape ruling 2 blessed for
// the ability cue (ability_<id>.wav -> ability_cast.wav -> synth).
//
// The colour is handed to the system as the `Color` user parameter, so one
// authored NS_Impact serves a body shot in Orange and a weak point in Gold.
// ---------------------------------------------------------------------------
enum class EBreakerEffectMoment : uint8
{
    // The gun's own flash at the visual muzzle. Weapon/heat: Orange.
    Muzzle,
    // Where a round or pellet landed. Orange; Gold on a weak point (the
    // weak-point promise, O179).
    Impact,
    // An ability's cast MOMENT (O179: moments get world flashes, windows stay
    // HUD bars). The caller is the verb and brings its own colour.
    Cast,
    // A body leaving the fight. Not named in O179; wears the kill confirm's
    // colours (Harm, Gold on a weak-point kill) so the crosshair and the world
    // agree about one event. Asked in Docs/reports/GLASS.md.
    Death,
};

namespace BreakerFX
{
    constexpr int32 EffectMomentCount = 4;

    // The asset each moment looks for. Directory and prefix are fixed so the
    // owner names a system after its moment and nothing here has to learn it.
    inline const TCHAR* MomentAssetName(EBreakerEffectMoment Moment)
    {
        switch (Moment)
        {
        case EBreakerEffectMoment::Muzzle: return TEXT("NS_Muzzle");
        case EBreakerEffectMoment::Impact: return TEXT("NS_Impact");
        case EBreakerEffectMoment::Cast:   return TEXT("NS_Cast");
        case EBreakerEffectMoment::Death:  return TEXT("NS_Death");
        }
        return TEXT("NS_Cast");
    }

    inline FString MomentAssetPath(EBreakerEffectMoment Moment)
    {
        const TCHAR* Name = MomentAssetName(Moment);
        return FString::Printf(TEXT("/Game/Breaker/FX/%s.%s"), Name, Name);
    }

    // O179, applied. Cast returns Cyan only as the documented default: a cast
    // site knows its verb and passes the role itself.
    inline FLinearColor MomentColor(EBreakerEffectMoment Moment, bool bWeakPoint)
    {
        switch (Moment)
        {
        case EBreakerEffectMoment::Muzzle: return BreakerUI::Orange;
        case EBreakerEffectMoment::Impact: return bWeakPoint ? BreakerUI::Gold : BreakerUI::Orange;
        case EBreakerEffectMoment::Cast:   return BreakerUI::Cyan;
        case EBreakerEffectMoment::Death:  return bWeakPoint ? BreakerUI::Gold : BreakerUI::Harm;
        }
        return BreakerUI::Cyan;
    }

    // THE MUZZLE IS SIZED ON SCREEN, NOT IN THE WORLD. Three of the four
    // moments are drawn metres from the player, where a world centimetre is
    // the right unit. The muzzle is drawn at a FIXED offset from the player's
    // own camera (UBreakerWeaponComponent::GetVisualMuzzleLocation is the
    // viewpoint plus a camera-space offset), so its screen size is a function
    // of that offset and nothing else, and the thing it must not do is reach
    // the reticle: O179's camera law, met at the one primitive that sits in
    // front of the camera every shot. This is the largest glow radius whose
    // disc, drawn at that offset, stays clear of the view axis by the
    // clearance. Angle against angle, so it holds at any field of view,
    // including under the aim narrow. Zero when the muzzle itself sits inside
    // the clearance (a gun drawn dead down the axis has no flash the reticle
    // can survive).
    inline float MuzzleFallbackRadiusCeilingCm(const FVector& MuzzleViewOffsetCm, float ReticleClearanceRadians)
    {
        const float Forward = static_cast<float>(MuzzleViewOffsetCm.X);
        if (Forward <= 0.0f) return 0.0f;
        const float Lateral = static_cast<float>(FMath::Sqrt(
            MuzzleViewOffsetCm.Y * MuzzleViewOffsetCm.Y + MuzzleViewOffsetCm.Z * MuzzleViewOffsetCm.Z));
        const float OffAxis = FMath::Atan2(Lateral, Forward);
        const float Spare = OffAxis - FMath::Max(ReticleClearanceRadians, 0.0f);
        if (Spare <= 0.0f) return 0.0f;
        return static_cast<float>(MuzzleViewOffsetCm.Size()) * FMath::Tan(Spare);
    }

    // How far the flash's edge stays from the view axis. The kicked crosshair
    // is 12 px of arm at 1080p, about 0.7 degrees at the 90-degree default
    // field of view, and every shot is a kicked frame; a degree and a half
    // gives it a gap its own width. O2 PLACEHOLDER.
    constexpr float MuzzleReticleClearanceRadians = 1.5f * (PI / 180.0f);

    // What the pooled renderer draws for a moment whose system is not
    // authored yet. bDrawn false means the moment has no primitive stand-in:
    // the impact's fallback is the tracer renderer's spark, and a second glow
    // on the same point would double-draw; the muzzle draws only from an
    // authored NS_Muzzle. All magnitudes O2 PLACEHOLDER.
    struct FMomentFallback
    {
        bool bDrawn = true;
        // 0: no glow disc at all (the muzzle — see below).
        float RadiusCm = 30.0f;
        float Intensity = 3.0f;
        float LightRadiusCm = 0.0f;   // 0: no blink light
        float LightIntensity = 0.0f;
        // A TONGUE along Direction rather than a disc: a stroke starting at
        // the location and running TongueCm forward. 0: none. This is the
        // muzzle's shape, because a stroke parallel to the view axis from an
        // off-axis muzzle converges TOWARD the reticle and never crosses it,
        // where a disc of any useful size at the aimed muzzle offset does.
        float TongueCm = 0.0f;
        float TongueThicknessCm = 0.0f;
        FEffectTiming Timing;
    };

    inline FMomentFallback MomentFallback(EBreakerEffectMoment Moment)
    {
        FMomentFallback F;
        switch (Moment)
        {
        case EBreakerEffectMoment::Muzzle:
            // THE GUN FLASHES. Owner, 2026-09-11: "visual effects, whether
            // that's your gun and stuff like that". The muzzle drew NOTHING
            // until NS_Muzzle was authored, and NS_Muzzle is an ASSETS item
            // (O190) that nobody has made, so every shot in the game has been
            // a silent tracer leaving a gun that did not move a photon.
            //
            // NO DISC. MuzzleFallbackRadiusCeilingCm says the largest glow that
            // clears the reticle at the AIMED muzzle offset (95, 2, -6) is
            // under four centimetres, which is nothing; that is why this was
            // off. A tongue — a short bright stroke down the barrel — starts
            // at the muzzle's own off-axis angle and converges toward the
            // axis without reaching it, so it reads at any size, and a blink
            // light at the muzzle lights the gun and the nearest wall, which
            // is most of what a muzzle flash IS. Both are one frame's worth
            // of life: a flash that lingers is a lamp.
            F.RadiusCm = 0.0f;
            F.TongueCm = 34.0f;               // O2 PLACEHOLDER
            F.TongueThicknessCm = 3.2f;       // O2 PLACEHOLDER
            F.Intensity = 4.0f;               // O2 PLACEHOLDER
            F.LightRadiusCm = 360.0f;         // O2 PLACEHOLDER
            F.LightIntensity = 2600.0f;       // O2 PLACEHOLDER
            F.Timing.DurationSeconds = 0.06f; // O2 PLACEHOLDER
            F.Timing.FadeOutSeconds = 0.04f;  // O2 PLACEHOLDER
            break;
        case EBreakerEffectMoment::Impact:
            F.bDrawn = false;
            break;
        case EBreakerEffectMoment::Cast:
            F.RadiusCm = 40.0f;            // O2 PLACEHOLDER
            F.Intensity = 3.2f;            // O2 PLACEHOLDER
            F.LightRadiusCm = 380.0f;      // O2 PLACEHOLDER
            F.LightIntensity = 2200.0f;    // O2 PLACEHOLDER
            F.Timing.DurationSeconds = 0.2f;    // O2 PLACEHOLDER
            F.Timing.FadeOutSeconds = 0.12f;    // O2 PLACEHOLDER
            break;
        case EBreakerEffectMoment::Death:
            // The longest of the four: a body leaving should read after the
            // damage number has started to rise.
            F.RadiusCm = 55.0f;            // O2 PLACEHOLDER
            F.Intensity = 3.6f;            // O2 PLACEHOLDER
            F.LightRadiusCm = 520.0f;      // O2 PLACEHOLDER
            F.LightIntensity = 2600.0f;    // O2 PLACEHOLDER
            F.Timing.DurationSeconds = 0.45f;   // O2 PLACEHOLDER
            F.Timing.FadeInSeconds = 0.04f;     // O2 PLACEHOLDER
            F.Timing.FadeOutSeconds = 0.3f;     // O2 PLACEHOLDER
            break;
        }
        return F;
    }
}
