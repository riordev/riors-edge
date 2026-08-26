#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakerSoundDirector.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

// ---------------------------------------------------------------------------
// The game's combat sounds, and deliberately only four of them.
//
// FIVE VERBS (ruled): weapon fire, hit confirm, kill, taking a hit — which
// matters more than the other three — and, since ORDERS ruling 2, the ability
// cast. There is still no generic PlaySound(AnyWave) surface: the roster grows
// by a ruling adding a verb, never by a caller passing a wave.
//
// THE FIFTH VERB TAKES AN ABILITY ID, and that is the whole of the override
// mechanism. The owner will author per-ability sounds "eventually", so the cue
// resolves ability_<id>.wav first and falls back to one shared default — which
// itself falls back to the synth. Nothing is silent before the assets exist and
// nothing is re-plumbed when they arrive: the owner drops a file named after an
// ability and that ability stops sharing the default. NO ASSET FIELD ON THE
// ABILITY DEFINITION — that asset is another lane's, and a filename convention
// buys the same result with no cross-lane surface and no save migration.
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
    // An ability was cast. AbilityId selects a per-ability override if one has
    // been authored; NAME_None, or an id with no file, plays the shared
    // default. Resolved on first use per id and cached, so the miss costs one
    // failed file open per ability per session rather than one per cast.
    void PlayAbilityCast(FName AbilityId);

protected:
    virtual void BeginPlay() override;

private:
    // One persistent voice per verb; PCM cached at BeginPlay and queued
    // verbatim per trigger.
    UPROPERTY() TObjectPtr<UAudioComponent> FireVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> HitVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> KillVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> TakeHitVoice;
    // One voice for every ability: a cast cuts the previous cast, exactly as
    // the other four verbs cut themselves.
    UPROPERTY() TObjectPtr<UAudioComponent> AbilityVoice;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> FireWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> HitWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> KillWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> TakeHitWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> AbilityDefaultWave;
    // Per-ability overrides, resolved lazily. A key present with a NULL value
    // means "probed, no override authored" — the sentinel is what stops a
    // missing file being re-opened on every cast.
    UPROPERTY() TMap<FName, TObjectPtr<USoundWaveProcedural>> AbilityWaves;

    TArray<int16> FirePcm;
    TArray<int16> HitPcm;
    TArray<int16> KillPcm;
    TArray<int16> TakeHitPcm;
    TArray<int16> AbilityDefaultPcm;
    TMap<FName, TArray<int16>> AbilityPcm;

    USoundWaveProcedural* MakeWave(int32 SampleRate);
    // Loads Content/Breaker/Audio/<FileName> into OutPcm and returns its
    // sample rate, or renders the synth fallback and returns the synth
    // rate. Logs which path each verb took, once, so a silent fallback is
    // visible in any run's log.
    int32 LoadOrSynth(const TCHAR* FileName, void (*Synth)(TArray<int16>&), TArray<int16>& OutPcm);
    void Trigger(UAudioComponent* Voice, USoundWaveProcedural* Wave, const TArray<int16>& Pcm);
};
