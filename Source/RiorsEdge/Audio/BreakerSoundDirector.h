#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/BreakerWeaponArchetype.h"
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
// The per-archetype fire cue's filename, pure and world-free so the one rule
// it carries can be asserted: EVERY ARCHETYPE MUST PRODUCE A DISTINCT NAME.
// Two archetypes collapsing onto one file would not fail — they would quietly
// share a gun sound, which is the exact defect this override exists to end.
//
// The AUTHORED display name with spaces removed, not the enumerator: a Burst
// Rifle is weapon_fire_BurstRifle.wav, called what the player sees it called.
// The enumerator is free to be renamed — the project's enum rule is that
// renaming is safe and only MOVING a value is not, and the archetype header
// guards the ordering rather than the spelling — so keying a filename on it
// would orphan the owner's file in silence the day somebody renames one.
namespace BreakerSoundFiles
{
    inline FString ArchetypeFire(EBreakerWeaponArchetype Archetype)
    {
        FString Key = BreakerWeaponArchetypeNames::Display(Archetype);
        Key.ReplaceInline(TEXT(" "), TEXT(""));
        return FString::Printf(TEXT("weapon_fire_%s.wav"), *Key);
    }
}

UCLASS(NotBlueprintable, NotPlaceable)
class RIORSEDGE_API ABreakerSoundDirector : public AActor
{
    GENERATED_BODY()

public:
    ABreakerSoundDirector();

    // The trigger was pulled and a round left. Per cosmetic shot, not per
    // pellet: a shotgun blast is one report.
    //
    // PER-ARCHETYPE, and that is the structural half of "all of the sound is
    // bad ... does not sound like real guns". This verb took no argument, so a
    // sidearm, a rifle and a shotgun fired the IDENTICAL clip — sameness no
    // amount of asset quality fixes. Same override shape as the ability cue one
    // level over: weapon_fire_<Archetype>.wav, then weapon_fire.wav, then the
    // synth. Still one verb; no generic PlaySound, no asset field.
    void PlayWeaponFire(EBreakerWeaponArchetype Archetype);
    // A hit the player dealt landed — any source, never a DoT tick (the
    // caller owns that exclusion). Scheduled by the caller on the round's
    // arrival clock.
    void PlayHitConfirm();
    // Something the player hit died of it.
    //
    // NO CALLER, BY RULING, AND THAT IS NOT THE DEAD-API DEFECT. "No death
    // sound for now" (owner, 2026-08-26) retired both death stings — the
    // player's and this one — and the kill now falls through to
    // PlayHitConfirm at the arrival-clock site, so a killing shot still
    // confirms it connected. Kept rather than deleted because the ruling is
    // explicitly "for now" and the open question is what a kill should SOUND
    // like, not whether the verb should exist. The distinction that matters:
    // GetPromptLabel spent a milestone uncalled because nobody knew it was
    // there. This one is uncalled on purpose and says so.
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
    // Per-archetype fire overrides, same lazy resolve and same NULL-value
    // sentinel meaning "probed, none authored". Eight archetypes, not
    // thirty-five, but the cost of eager loading is still eight failed opens
    // for files that do not exist yet.
    UPROPERTY() TMap<EBreakerWeaponArchetype, TObjectPtr<USoundWaveProcedural>> ArchetypeFireWaves;

    TArray<int16> FirePcm;
    TArray<int16> HitPcm;
    TArray<int16> KillPcm;
    TArray<int16> TakeHitPcm;
    TArray<int16> AbilityDefaultPcm;
    TMap<FName, TArray<int16>> AbilityPcm;
    TMap<EBreakerWeaponArchetype, TArray<int16>> ArchetypeFirePcm;

    USoundWaveProcedural* MakeWave(int32 SampleRate);
    // Loads Content/Breaker/Audio/<FileName> into OutPcm and returns its
    // sample rate, or renders the synth fallback and returns the synth
    // rate. Logs which path each verb took, once, so a silent fallback is
    // visible in any run's log.
    int32 LoadOrSynth(const TCHAR* FileName, void (*Synth)(TArray<int16>&), TArray<int16>& OutPcm);
    void Trigger(UAudioComponent* Voice, USoundWaveProcedural* Wave, const TArray<int16>& Pcm);
};
