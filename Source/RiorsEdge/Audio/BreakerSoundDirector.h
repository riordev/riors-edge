#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakerSoundDirector.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

// ---------------------------------------------------------------------------
// The game's first sounds, and deliberately only three of them.
//
// EXACTLY THREE: weapon fire, hit confirm, kill. The owner's brief is
// explicit that three deliberate sounds beat a half-mixed twenty, so this
// actor exposes three verbs and nothing generic — no PlaySound(AnyWave)
// surface for the next caller to grow the roster through without a ruling.
//
// This is the tracer renderer's sibling and holds the same four decisions,
// translated: POOLED VOICES rather than pooled primitives (one persistent
// audio component per sound, allocated once, retriggered forever — an SMG at
// 800 RPM is thirteen fire sounds a second and a component per shot would be
// thirteen component lifecycles a second); the waveform maths PURE in
// BreakerSoundMath.h where the suite can reach it; and like the tracer it is
// a CLIENT-SIDE COSMETIC actor — replicates nothing, resolves nothing, its
// absence changes no rule, spawned lazily by the HUD on the first shot.
// (The other two tracer decisions — world placement for depth occlusion and
// TG_PostUpdateWork — are answers to questions sound does not ask: these are
// the player's OWN events, so all three voices play flat 2D and this actor
// never ticks.)
//
// RETRIGGER CUTS. A new fire sound resets its voice's queue and starts over
// rather than overlapping the previous one. At high rates of fire that is the
// classic retrigger every game's placeholder pass ships; true polyphony is a
// mixing question for the owner, not a placeholder one. The three voices are
// independent, so fire + hit + kill do overlap each other.
// ---------------------------------------------------------------------------
UCLASS(NotBlueprintable, NotPlaceable)
class RIORSEDGE_API ABreakerSoundDirector : public AActor
{
    GENERATED_BODY()

public:
    ABreakerSoundDirector();

    // The trigger was pulled and a round left. Per cosmetic shot, not per
    // pellet: a shotgun blast is one report.
    void PlayWeaponFire();
    // A hit the player dealt landed — any source, weapon or ability, but
    // never a DoT tick (the caller owns that exclusion, for the same reason
    // the crosshair confirm does: a Bleed on three targets would strobe
    // forever over nothing the player just did).
    void PlayHitConfirm();
    // Something the player hit died of it.
    void PlayKill();

protected:
    virtual void BeginPlay() override;

private:
    // One persistent voice per sound. The procedural wave is queued from the
    // PCM cache on every trigger; the component is created once and reused.
    UPROPERTY() TObjectPtr<UAudioComponent> FireVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> HitVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> KillVoice;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> FireWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> HitWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> KillWave;

    // Rendered once in BeginPlay and queued verbatim per trigger — the synth
    // runs per sound, never per play.
    TArray<int16> FirePcm;
    TArray<int16> HitPcm;
    TArray<int16> KillPcm;

    USoundWaveProcedural* MakeWave();
    void Trigger(UAudioComponent* Voice, USoundWaveProcedural* Wave, const TArray<int16>& Pcm);
};
