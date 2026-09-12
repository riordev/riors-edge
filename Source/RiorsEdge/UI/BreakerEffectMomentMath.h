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
    // authored yet. O282: EVERY combat moment has a visible fallback that
    // reads at play distance, built from primitives until the ASSETS-5
    // systems land (O190). bDrawn false is reserved for a moment with no
    // primitive stand-in; none of the four is that today. All magnitudes
    // O2 PLACEHOLDER.
    struct FMomentFallback
    {
        bool bDrawn = true;
        // 0: no glow disc at all (the muzzle, the impact and the cast — see below).
        float RadiusCm = 30.0f;
        // Emissive intensity shared by every primitive of the fallback.
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
        // A FLARE beside the tongue: a cone with its base at the location and
        // its apex FlareLengthCm along Direction, so the flash has a body at
        // the barrel that tapers into the tongue. The base radius is the part
        // that sits at the muzzle offset, so it is the part the reticle law
        // measures: it stays under MuzzleFallbackRadiusCeilingCm at the aimed
        // offset, scaled by the largest shipped MuzzleFlashScale. 0: none.
        float FlareBaseRadiusCm = 0.0f;
        float FlareLengthCm = 0.0f;
        // SHARDS: ShardCount short bright cubes thrown from the location
        // inside a cone of ShardConeDegrees (full included angle) about
        // Direction, each on its own ballistic arc for ShardSeconds
        // (ShardPose below). The impact's shape (sparks leaving the surface)
        // and the cast's (a burst leaving the hand along the aim, O284).
        // 0: none. Shards live on their own clock, not on Timing.
        int32 ShardCount = 0;
        float ShardSeconds = 0.0f;
        float ShardSpeedCms = 0.0f;
        float ShardGravityCms2 = 0.0f;
        float ShardConeDegrees = 0.0f;
        float ShardLengthCm = 0.0f;
        float ShardThicknessCm = 0.0f;
        FEffectTiming Timing;
    };

    // How big a muzzle flash is, from how hard the gun kicks. The viewmodel
    // kick is the one per-archetype number that already says "this gun is
    // violent": a pistol at 2 kick units draws a flash at 0.91 of authored
    // size, a rifle at 3.2 draws it at 1.0, a sniper at 10 at 1.55. Linear,
    // floored at zero kick so a profile that kicks nothing still flashes.
    // Slope and base O2 PLACEHOLDER.
    inline float MuzzleFlashScale(float KickUnits)
    {
        return 0.75f + 0.08f * FMath::Max(KickUnits, 0.0f);   // O2 PLACEHOLDER
    }

    // One shard's pose at Age seconds after the impact. OutPos is the OFFSET
    // from the impact point (p = v t + g t^2 / 2 with gravity straight down),
    // OutDir the shard's travel direction at that instant, so a cube aligned
    // to it draws as a streak along the arc. The launch direction comes from
    // a deterministic per-index hash inside the cone about Normal — the same
    // shard always flies the same way, so a scheduled impact looks the same
    // as an immediate one — and is NEVER into the surface: the polar angle
    // is bounded by half the cone, which is bounded below ninety degrees.
    inline void ShardPose(const FVector& Normal, int32 Index, int32 Count, float Age,
        float Speed, float Gravity, float ConeDeg, FVector& OutPos, FVector& OutDir)
    {
        const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
        // A basis about the normal. The helper axis is whichever world axis
        // the normal is furthest from, so the cross product never degenerates.
        const FVector Helper = FMath::Abs(N.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
        const FVector T = FVector::CrossProduct(N, Helper).GetSafeNormal();
        const FVector B = FVector::CrossProduct(N, T);

        // Two unit hashes from the index (a Wang-style integer mix): one for
        // how far off the normal, one to jitter the azimuth off the even fan.
        uint32 Hash = static_cast<uint32>(Index) * 2654435761u;
        Hash ^= Hash >> 13;
        Hash *= 0x5bd1e995u;
        Hash ^= Hash >> 15;
        const float Polar01 = static_cast<float>(Hash & 0xffffu) / 65535.0f;
        const float Jitter01 = static_cast<float>((Hash >> 16) & 0xffffu) / 65535.0f;

        const int32 SafeCount = FMath::Max(Count, 1);
        const float HalfCone = FMath::Clamp(ConeDeg * 0.5f, 0.0f, 89.0f) * (PI / 180.0f);
        // sqrt spreads the shards evenly over the cone's cap rather than
        // bunching them on the axis.
        const float Polar = HalfCone * FMath::Sqrt(Polar01);
        const float Azimuth = 2.0f * PI * (static_cast<float>(Index) + Jitter01) / static_cast<float>(SafeCount);
        const FVector Launch = N * FMath::Cos(Polar)
            + (T * FMath::Cos(Azimuth) + B * FMath::Sin(Azimuth)) * FMath::Sin(Polar);

        const float t = FMath::Max(Age, 0.0f);
        const FVector Velocity0 = Launch * Speed;
        const FVector G(0.0f, 0.0f, -Gravity);
        OutPos = Velocity0 * t + G * (0.5f * t * t);
        const FVector Velocity = Velocity0 + G * t;
        OutDir = Velocity.IsNearlyZero() ? Launch : Velocity.GetSafeNormal();
    }

    // A shard's brightness: full at birth, gone at ShardSeconds, straight
    // between. Zero for a shard with no life at all.
    inline float ShardAlpha(float Age, float ShardSeconds)
    {
        if (ShardSeconds <= KINDA_SMALL_NUMBER) return 0.0f;
        return FMath::Clamp(1.0f - Age / ShardSeconds, 0.0f, 1.0f);
    }

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
            //
            // THE FLARE (O282). A tongue alone is a line; the flash needs a
            // body at the barrel to read as a flash at play distance. A cone
            // with its base at the muzzle, tapering forward into the tongue,
            // is that body. Its base radius is the tongue's half-thickness,
            // already proven under the aimed-offset ceiling; the renderer
            // scales it by MuzzleFlashScale, and the largest shipped kick
            // (10 units, 1.55x) keeps it under that ceiling too.
            F.RadiusCm = 0.0f;
            F.TongueCm = 34.0f;               // O2 PLACEHOLDER
            F.TongueThicknessCm = 3.2f;       // O2 PLACEHOLDER
            F.FlareBaseRadiusCm = 1.6f;       // O2 PLACEHOLDER
            F.FlareLengthCm = 22.0f;          // O2 PLACEHOLDER
            F.Intensity = 4.0f;               // O2 PLACEHOLDER
            F.LightRadiusCm = 360.0f;         // O2 PLACEHOLDER
            F.LightIntensity = 2600.0f;       // O2 PLACEHOLDER
            F.Timing.DurationSeconds = 0.06f; // O2 PLACEHOLDER
            F.Timing.FadeOutSeconds = 0.04f;  // O2 PLACEHOLDER
            break;
        case EBreakerEffectMoment::Impact:
            // SPARKS LEAVE THE SURFACE (O282). The tracer renderer's spark
            // marks the point and stays; it is a dot, and a dot at twenty
            // metres is not a hit. Five short bright shards thrown out of
            // the surface along the normal, falling under gravity for a third
            // of a second, are. No disc (a glow on the point would
            // double-draw the spark), no light: the damage number carries
            // the read. Timing mirrors the shard life so the moment's clock
            // and the shards' agree about when the impact is over.
            F.RadiusCm = 0.0f;
            F.Intensity = 3.4f;               // O2 PLACEHOLDER
            F.ShardCount = 5;                 // O2 PLACEHOLDER
            F.ShardSeconds = 0.35f;           // O2 PLACEHOLDER
            F.ShardSpeedCms = 420.0f;         // O2 PLACEHOLDER
            F.ShardGravityCms2 = 980.0f;      // O2 PLACEHOLDER
            F.ShardConeDegrees = 55.0f;       // O2 PLACEHOLDER
            F.ShardLengthCm = 9.0f;           // O2 PLACEHOLDER
            F.ShardThicknessCm = 1.6f;        // O2 PLACEHOLDER
            F.Timing.DurationSeconds = 0.35f; // O2 PLACEHOLDER — equals ShardSeconds
            break;
        case EBreakerEffectMoment::Cast:
            // A BURST OF THE VERB'S COLOUR LEAVES THE HAND (O284). The cast
            // moment is played at the caster's hand, thirty-odd centimetres
            // from the lens; a forty-centimetre disc there is not a flash, it
            // is a full-frame wash of the verb's colour. NO DISC. The blink
            // light stays — it IS the hand light, the wall and the gun catch
            // the verb's colour for a fifth of a second — and the impact's
            // shard block is borrowed for the body: five short bright shards
            // thrown from the hand along the aim, in a tighter fan than the
            // impact's (an impact sprays off a surface; a cast leaves in the
            // direction it was cast), falling under the same gravity for the
            // same third of a second.
            F.RadiusCm = 0.0f;
            F.Intensity = 3.2f;            // O2 PLACEHOLDER
            F.LightRadiusCm = 380.0f;      // O2 PLACEHOLDER
            F.LightIntensity = 2200.0f;    // O2 PLACEHOLDER
            F.ShardCount = 5;                 // O2 PLACEHOLDER
            F.ShardSeconds = 0.35f;           // O2 PLACEHOLDER
            F.ShardSpeedCms = 420.0f;         // O2 PLACEHOLDER
            F.ShardGravityCms2 = 980.0f;      // O2 PLACEHOLDER
            F.ShardConeDegrees = 30.0f;       // O2 PLACEHOLDER — tighter than Impact's 55: along the aim
            F.ShardLengthCm = 9.0f;           // O2 PLACEHOLDER
            F.ShardThicknessCm = 1.6f;        // O2 PLACEHOLDER
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
