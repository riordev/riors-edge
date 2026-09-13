#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Weapons/BreakerWeaponArchetype.h"
#include "BreakerSoundDirector.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

// ---------------------------------------------------------------------------
// The game's event-driven combat sounds.
//
// Weapon fire, hit confirm, kill, taking a hit — which
// matters more than the other three — the ability cast (ORDERS ruling 2) and,
// since O193, the player's own death; since O275, opening a supply chest.
// Earned Rot has a separate activation cue, never a per-tick cue. There is
// no generic PlaySound(AnyWave) surface.
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
// The rest is unchanged from the first pass: persistent voices (retrigger
// cuts on every verb but weapon fire, which rotates a small pool so a shot
// never cuts the previous shot's tail; separate voices overlap each other),
// client-side cosmetic actor spawned lazily by the HUD, replicates nothing,
// never ticks. Every play, every verb, carries its own pitch multiplier
// (BreakerSound::PitchForPlay): two consecutive plays are never identical.
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
    void ApplyVolumeSettings(float Master, float Effects);
    static void PlaySettingsTest(UWorld* World);

    // The trigger was pulled and a round left. Per cosmetic shot, not per
    // pellet: a shotgun blast is one report.
    //
    // PER-ARCHETYPE, and that is the structural half of "all of the sound is
    // bad ... does not sound like real guns". This verb took no argument, so a
    // sidearm, a rifle and a shotgun fired the IDENTICAL clip — sameness no
    // amount of asset quality fixes. Same override shape as the ability cue one
    // level over: weapon_fire_<Archetype>.wav, then — for the Rifle only —
    // the shipped weapon_fire.wav recording, then each archetype's own synth
    // voice. Still one verb; no generic PlaySound, no asset field.
    void PlayWeaponFire(EBreakerWeaponArchetype Archetype);
    // A hit the player dealt landed — any source, never a DoT tick (the
    // caller owns that exclusion). Scheduled by the caller on the round's
    // arrival clock.
    void PlayHitConfirm();
    // Something the player hit died of it.
    //
    // NO CALLER, BY RULING, AND THAT IS NOT THE DEAD-API DEFECT. "No death
    // sound for now" (owner, 2026-08-26) retired both death stings. O193
    // restores the PLAYER'S — one low cue at the cut, PlayPlayerDeath below —
    // and leaves this one retired: the kill still falls through to
    // PlayHitConfirm at the arrival-clock site, so a killing shot confirms it
    // connected and nothing more. Kept rather than deleted because the ruling
    // is explicitly "for now" and the open question is what a kill should
    // SOUND like, not whether the verb should exist. The distinction that
    // matters: GetPromptLabel spent a milestone uncalled because nobody knew
    // it was there. This one is uncalled on purpose and says so.
    void PlayKill();
    // The player took real damage. Immediate — being hit has no flight.
    // The player died (O193): one low sound at the death beat's hard cut to
    // black. The HUD schedules it at the character's DeathBeat
    // LowerAndDropSeconds, so the cue lands with the black rather than with
    // the fatal hit — the fatal hit is silent on purpose (take-hit skips it).
    void PlayPlayerDeath();
    // The player gained a level. One cue per level-up event, never per level
    // when several land at once: the banner states the count, and a stacked
    // arpeggio would read as a bug. Overridable with level_up.wav.
    void PlayLevelUp();
    // The local player opened a supply chest (O275: opening a chest is a
    // director verb, not a sound the chest actor owns). One two-tap latch per
    // open; retrigger cuts like every other one-shot. Overridable with
    // chest_open.wav. Reached through BreakerChestFeedback::PlayOpen, which
    // owns the local-pawn check — this verb plays for whoever calls it.
    void PlayChestOpen();
    // An ability was cast. AbilityId selects a per-ability override if one has
    // been authored; Cleave/Rot otherwise use a short synthesized transient.
    // Other ids with no file play the shared default. Resolved on first use per id and cached, so the miss costs one
    // failed file open per ability per session rather than one per cast.
    void PlayAbilityCast(FName AbilityId);
    // Earned Rot activation for the local applier or recipient, never per tick.
    bool PlayEntropyActivation();
    bool PlayVoidCue(bool bBurst);
    bool PlayRiftActivation();
    bool PlayReaction(FGameplayTag ReactionTag);
    int32 GetEntropyCueCount() const { return EntropyCueCount; }

protected:
    virtual void BeginPlay() override;

private:
    // One persistent voice per verb; PCM cached at BeginPlay and queued
    // verbatim per trigger. Weapon fire is the exception, below.
    //
    // WEAPON FIRE IS A ROUND-ROBIN POOL. One voice per verb means a
    // retrigger cuts, and at 600 RPM that is every shot killing the previous
    // shot's tail at 100 ms: the owner heard it as "ticky". Three voice+wave
    // pairs taken in turn, so a shot only ever cuts the one three shots back.
    // Each slot owns its wave because a procedural wave IS its queue — two
    // voices reading one wave would split the clip between them. The PCM
    // stays per archetype (ArchetypeFirePcm); the slots are what play it, and
    // a slot's wave takes the clip's sample rate per play, since the shipped
    // recording and the synth voices are not authored at the same rate.
    static constexpr int32 FireVoiceCount = 3;   // O2 PLACEHOLDER
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> FireVoices;
    UPROPERTY() TArray<TObjectPtr<USoundWaveProcedural>> FireWaves;
    int32 NextFireVoice = 0;
    // Every play of every verb gets its own pitch (BreakerSound::PitchForPlay).
    // One counter across all verbs, so a hit confirm landing between two shots
    // does not make the shots' pitches line up.
    uint32 PlayCounter = 0;
    UPROPERTY() TObjectPtr<UAudioComponent> HitVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> KillVoice;
    // One voice for every ability: a cast cuts the previous cast, exactly as
    // the other four verbs cut themselves.
    UPROPERTY() TObjectPtr<UAudioComponent> AbilityVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> PlayerDeathVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> LevelUpVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> ChestVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> EntropyVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> VoidMarkVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> RiftVoice;
    UPROPERTY() TObjectPtr<UAudioComponent> ReactionVoice;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> CollapseWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> WitherWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> TearWave;
    TArray<int16> CollapsePcm, WitherPcm, TearPcm;
    double LastReactionCueTime = -1000;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> RiftWave;
    TArray<int16> RiftPcm;
    double LastRiftCueTime = -1000;
    UPROPERTY() TObjectPtr<UAudioComponent> VoidBurstVoice;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> VoidMarkWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> VoidBurstWave;
    TArray<int16> VoidMarkPcm, VoidBurstPcm;
    double LastVoidMarkTime = -1000, LastVoidBurstTime = -1000;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> HitWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> KillWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> AbilityDefaultWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> PlayerDeathWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> LevelUpWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> ChestWave;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> EntropyWave;
    // Per-ability overrides, resolved lazily. A key present with a NULL value
    // means "probed, no override authored" — the sentinel is what stops a
    // missing file being re-opened on every cast.
    UPROPERTY() TMap<FName, TObjectPtr<USoundWaveProcedural>> AbilityWaves;
    // Per-archetype fire clips, resolved lazily: a key present means "probed",
    // and the value is the sample rate the clip in ArchetypeFirePcm was
    // authored at. No wave lives here any more — the pool slots own the
    // waves. Eight archetypes, not thirty-five, but the cost of eager
    // resolution is still eight failed opens for files that do not exist yet.
    TMap<EBreakerWeaponArchetype, int32> ArchetypeFireRates;

    // The shared weapon_fire.wav recording (or its synth floor), and whether
    // the recording actually loaded. The Rifle resolves to it when no
    // weapon_fire_Rifle.wav is authored; the other seven never do.
    TArray<int16> FirePcm;
    int32 FireRate = 0;
    bool bFireSampleLoaded = false;
    TArray<int16> HitPcm;
    TArray<int16> KillPcm;
    TArray<int16> AbilityDefaultPcm;
    TArray<int16> PlayerDeathPcm;
    TArray<int16> LevelUpPcm;
    TArray<int16> ChestPcm;
    TArray<int16> EntropyPcm;
    double LastEntropyCueTime = -1000;
    int32 EntropyCueCount = 0;
    TMap<FName, TArray<int16>> AbilityPcm;
    TMap<EBreakerWeaponArchetype, TArray<int16>> ArchetypeFirePcm;

    USoundWaveProcedural* MakeWave(int32 SampleRate);
    // Loads Content/Breaker/Audio/<FileName> into OutPcm and returns its
    // sample rate, or renders the synth fallback and returns the synth
    // rate. Logs which path each verb took, once, so a silent fallback is
    // visible in any run's log. bOutLoaded, when given, says which path.
    int32 LoadOrSynth(const TCHAR* FileName, void (*Synth)(TArray<int16>&), TArray<int16>& OutPcm, bool* bOutLoaded = nullptr);
    // The one site every verb plays through: cuts the voice, sets this play's
    // pitch, queues the clip, plays.
    void Trigger(UAudioComponent* Voice, USoundWaveProcedural* Wave, const TArray<int16>& Pcm);
};
