#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakerSoundDirector.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

// ---------------------------------------------------------------------------
// The game's combat sounds, and deliberately only four of them.
//
// FOUR VERBS (ruled): weapon fire, hit confirm, kill — and taking a hit,
// which matters more than the other three. No generic PlaySound(AnyWave)
// surface exists for the roster to grow through without a ruling.
//
// RECORDED SAMPLES FIRST, SYNTH AS FALLBACK (ruled): a gunshot is a crack,
// a body, a mechanical action and a tail — recorded layers, not
// arithmetic. Each verb loads its CC0 sample from
// Content/Breaker/Audio/<verb>.wav at BeginPlay (raw PCM WAV, parsed by
// the pure reader in BreakerWaveFile.h); a missing or unreadable file
// falls back to the synthesized wave in BreakerSoundMath.h, so a clone
// with no audio assets still makes noise. Provenance lives in
// Content/Breaker/Audio/SOURCES.txt. The synth's original job — proving
// the audio path end to end — is done; it remains as the floor.
//
// The rest is unchanged from the first pass: pooled persistent voices
// (retrigger cuts, the four voices overlap each other), client-side
// cosmetic actor spawned lazily by the HUD, replicates nothing, never
// ticks.
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
    // A hit the player dealt landed — any source, never a DoT tick (the
    // caller owns that exclusion). Scheduled by the caller on the round's
    // arrival clock.
    void PlayHitConfirm();
    // Something the player hit died of it.
    void PlayKill();
    // The player took real damage. Immediate — being hit has no flight.
    void PlayTakeHit();

protected:
    virtual void BeginPlay() override;

private:
    // One persistent voice per verb; PCM cached at BeginPlay and queued
    // verbatim per trigger.
    UPROPERTY() TObjectPtr<UAudioComponent> FireVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> HitVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> KillVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> TakeHitVoice;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> FireWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> HitWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> KillWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> TakeHitWave;

    TArray<int16> FirePcm;
    TArray<int16> HitPcm;
    TArray<int16> KillPcm;
    TArray<int16> TakeHitPcm;

    USoundWaveProcedural* MakeWave(int32 SampleRate);
    // Loads Content/Breaker/Audio/<FileName> into OutPcm and returns its
    // sample rate, or renders the synth fallback and returns the synth
    // rate. Logs which path each verb took, once, so a silent fallback is
    // visible in any run's log.
    int32 LoadOrSynth(const TCHAR* FileName, void (*Synth)(TArray<int16>&), TArray<int16>& OutPcm);
    void Trigger(UAudioComponent* Voice, USoundWaveProcedural* Wave, const TArray<int16>& Pcm);
};
