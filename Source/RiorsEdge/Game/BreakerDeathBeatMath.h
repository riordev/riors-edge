#pragma once

#include "CoreMinimal.h"
#include "BreakerDeathBeatMath.generated.h"

// THE DEATH BEAT (O193), as world-free maths.
//
// A campaign death is a beat, not a screen: the weapon lowers, the camera
// drops and tilts while the colour drains, the frame goes black, the pawn is
// put back at the tileset start under the black, and the world fades back in
// with input live on the very first frame the player can see. The character
// asks this header what every channel is doing at time t and copies the
// answer onto the camera, the rig, the fade and the HUD flag — it decides
// nothing itself, so the whole rhythm is provable without a world
// (RiorsEdge.Game.DeathBeatTimeline).
//
// No sound is authored here: the beat's audio needs a ruling first.
//
// O2: every figure in FBreakerDeathBeatTimeline is a PLACEHOLDER. The
// automation proves the shape of the curve and cannot say whether the beat
// feels like a consequence.

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDeathBeatTimeline
{
    GENERATED_BODY()

    // The weapon finishes lowering here, well before the camera has finished
    // dropping: the gun is the first thing to give up.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death Beat", meta=(ClampMin="0.05")) float WeaponLowerSeconds = 0.3f;   // O2 PLACEHOLDER
    // The visible half of dying: the camera drops and tilts while the colour
    // drains, easing over this span. At its end the frame cuts hard to black.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death Beat", meta=(ClampMin="0.05")) float LowerAndDropSeconds = 0.8f;   // O2 PLACEHOLDER
    // Full black. The teleport lands at the end of it, under cover.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death Beat", meta=(ClampMin="0.0")) float BlackSeconds = 1.2f;   // O2 PLACEHOLDER
    // Black to world at the tileset start. Input is live from its first sample.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death Beat", meta=(ClampMin="0.05")) float FadeInSeconds = 0.4f;   // O2 PLACEHOLDER
    // How far the eye sinks toward the floor at full drop.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death Beat", meta=(ClampMin="0")) float CameraDropCm = 60.0f;   // O2 PLACEHOLDER
    // The head lolls: a pitch toward the ground. THE ROLL IS ZERO BY RULING
    // (O241). No spec line ever named it — spec.md authors the 12 degree pitch
    // alone — and an unauthored figure that ships becomes canon by accident.
    // The channel stays, so restoring the loll is one number the day the beat
    // reads flat; it is not deleted, it is unauthored.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death Beat", meta=(ClampMin="-45", ClampMax="45")) float CameraRollDegrees = 0.0f;   // O241
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death Beat", meta=(ClampMin="-45", ClampMax="45")) float CameraPitchDegrees = -12.0f;   // O2 PLACEHOLDER
};

// Which stretch of the beat a sample sits in. Plain enum on purpose: this is
// derived from a clock every frame and is never written to a save, a config
// or a Blueprint pin, so the append-only rule does not bind it.
enum class EBreakerDeathBeatPhase : uint8
{
    Falling,
    Black,
    FadeIn,
    Done
};

// Every channel the character copies onto the world for one frame.
struct FBreakerDeathBeatSample
{
    EBreakerDeathBeatPhase Phase = EBreakerDeathBeatPhase::Done;
    // 0 = the ready pose, 1 = the holster pose. The Anchor's holster is 1.
    float WeaponLowerFraction = 0.0f;
    float CameraDropCm = 0.0f;
    float CameraRollDegrees = 0.0f;
    float CameraPitchDegrees = 0.0f;
    // 1 = full colour, 0 = grey.
    float Saturation = 1.0f;
    // 0 = the world, 1 = black.
    float FadeAlpha = 0.0f;
    bool bInputEnabled = true;
    bool bHudVisible = true;
};

namespace BreakerDeathBeat
{
    // When the pawn is put back at the tileset start: the end of the black,
    // which is also the first FadeIn instant.
    inline float TeleportAtSeconds(const FBreakerDeathBeatTimeline& Timeline)
    {
        return Timeline.LowerAndDropSeconds + Timeline.BlackSeconds;
    }

    inline float TotalSeconds(const FBreakerDeathBeatTimeline& Timeline)
    {
        return TeleportAtSeconds(Timeline) + Timeline.FadeInSeconds;
    }

    inline FBreakerDeathBeatSample Sample(const FBreakerDeathBeatTimeline& Timeline, float Elapsed)
    {
        FBreakerDeathBeatSample Out;   // Done, at rest, input on
        if (Elapsed < 0.0f)
        {
            return Out;
        }
        const float Lower = FMath::Max(Timeline.LowerAndDropSeconds, UE_KINDA_SMALL_NUMBER);
        const float BlackEnd = TeleportAtSeconds(Timeline);
        const float Total = TotalSeconds(Timeline);

        if (Elapsed < Lower)
        {
            // FALLING: the weapon lowers on its own short clock and is
            // holstered at WeaponLowerSeconds while the camera drop, the
            // tilt and the colour drain keep easing to LowerAndDropSeconds.
            // The fade stays at 0 for the whole fall: the cut to black at
            // Lower is hard, not a tail.
            Out.Phase = EBreakerDeathBeatPhase::Falling;
            const float WeaponLower = FMath::Max(Timeline.WeaponLowerSeconds, UE_KINDA_SMALL_NUMBER);
            Out.WeaponLowerFraction = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Elapsed / WeaponLower, 0.0f, 1.0f));
            const float T = FMath::Clamp(Elapsed / Lower, 0.0f, 1.0f);
            const float Eased = FMath::SmoothStep(0.0f, 1.0f, T);
            Out.CameraDropCm = Timeline.CameraDropCm * Eased;
            Out.CameraRollDegrees = Timeline.CameraRollDegrees * Eased;
            Out.CameraPitchDegrees = Timeline.CameraPitchDegrees * Eased;
            Out.Saturation = 1.0f - Eased;
            Out.FadeAlpha = 0.0f;
            Out.bInputEnabled = false;
            Out.bHudVisible = true;
            return Out;
        }
        if (Elapsed < BlackEnd)
        {
            // BLACK: the fall's end pose held under a full fade. Nothing is
            // visible, so nothing moves.
            Out.Phase = EBreakerDeathBeatPhase::Black;
            Out.WeaponLowerFraction = 1.0f;
            Out.CameraDropCm = Timeline.CameraDropCm;
            Out.CameraRollDegrees = Timeline.CameraRollDegrees;
            Out.CameraPitchDegrees = Timeline.CameraPitchDegrees;
            Out.Saturation = 0.0f;
            Out.FadeAlpha = 1.0f;
            Out.bInputEnabled = false;
            Out.bHudVisible = false;
            return Out;
        }
        if (Elapsed < Total)
        {
            // FADE IN: the pawn is already at the tileset start. Camera,
            // weapon and colour are at rest; only the black lifts. Input is
            // live from this first sample — the player owns the frame that
            // resolves, not the one after it.
            Out.Phase = EBreakerDeathBeatPhase::FadeIn;
            const float FadeIn = FMath::Max(Timeline.FadeInSeconds, UE_KINDA_SMALL_NUMBER);
            Out.FadeAlpha = 1.0f - FMath::Clamp((Elapsed - BlackEnd) / FadeIn, 0.0f, 1.0f);
            Out.bInputEnabled = true;
            Out.bHudVisible = true;
            return Out;
        }
        return Out;
    }
}
