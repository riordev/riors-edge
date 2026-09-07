#include "Audio/BreakerSoundDirector.h"

#include "Audio/BreakerSoundMath.h"
#include "Audio/BreakerWaveFile.h"
#include "Components/AudioComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "Settings/BreakerGameSettings.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ABreakerSoundDirector::ABreakerSoundDirector()
{
    // Never ticks: every voice is event-driven and the audio thread owns the
    // playback clock.
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetCanBeDamaged(false);

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    const auto MakeVoice = [this](const TCHAR* Name)
    {
        UAudioComponent* Voice = CreateDefaultSubobject<UAudioComponent>(Name);
        Voice->SetupAttachment(RootComponent);
        Voice->bAutoActivate = false;
        // The player's own events, flat 2D: fire, confirm, kill and taking a
        // hit all happen TO the listener, and spatializing them at the
        // pawn's feet would only add a doppler artifact when they sprint.
        Voice->bAllowSpatialization = false;
        Voice->bIsUISound = true;
        return Voice;
    };
    FireVoice = MakeVoice(TEXT("FireVoice"));
    HitVoice = MakeVoice(TEXT("HitVoice"));
    KillVoice = MakeVoice(TEXT("KillVoice"));
    TakeHitVoice = MakeVoice(TEXT("TakeHitVoice"));
    AbilityVoice = MakeVoice(TEXT("AbilityVoice"));
    PlayerDeathVoice = MakeVoice(TEXT("PlayerDeathVoice"));
    EntropyVoice = MakeVoice(TEXT("EntropyVoice"));
    VoidMarkVoice = MakeVoice(TEXT("VoidMarkVoice"));
    RiftVoice = MakeVoice(TEXT("RiftVoice"));
    ReactionVoice = MakeVoice(TEXT("ReactionVoice"));
    VoidBurstVoice = MakeVoice(TEXT("VoidBurstVoice"));
}

USoundWaveProcedural* ABreakerSoundDirector::MakeWave(int32 SampleRate)
{
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
    Wave->SetSampleRate(SampleRate);
    Wave->NumChannels = 1;
    Wave->SampleByteSize = sizeof(int16);
    Wave->SoundGroup = SOUNDGROUP_Default;
    Wave->bLooping = false;
    // A procedural wave has no length until fed; INDEFINITELY_LOOPING keeps
    // the engine from culling the source between triggers.
    Wave->Duration = INDEFINITELY_LOOPING_DURATION;
    return Wave;
}

int32 ABreakerSoundDirector::LoadOrSynth(const TCHAR* FileName, void (*Synth)(TArray<int16>&), TArray<int16>& OutPcm)
{
    const FString Path = FPaths::ProjectContentDir() / TEXT("Breaker/Audio") / FileName;
    TArray<uint8> Bytes;
    if (FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        BreakerWave::FParsedWave Parsed = BreakerWave::ParseWav(Bytes);
        if (Parsed.IsValid())
        {
            OutPcm = MoveTemp(Parsed.Samples);
            UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s: sample loaded (%d Hz, %.2f s)."),
                FileName, Parsed.SampleRate, static_cast<float>(OutPcm.Num()) / Parsed.SampleRate);
            return Parsed.SampleRate;
        }
        UE_LOG(LogTemp, Warning, TEXT("[BreakerSound] %s exists but is not 16-bit PCM WAV — synth fallback."), FileName);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s missing — synth fallback."), FileName);
    }
    Synth(OutPcm);
    return BreakerSound::SampleRate;
}

void ABreakerSoundDirector::BeginPlay()
{
    Super::BeginPlay();
    UBreakerGameSettings* Settings = NewObject<UBreakerGameSettings>(this);
    Settings->LoadOrDefaults();
    ApplyVolumeSettings(Settings->MasterVolume, Settings->EffectsVolume);

    const int32 FireRate = LoadOrSynth(TEXT("weapon_fire.wav"), &BreakerSound::RenderWeaponFire, FirePcm);
    const int32 HitRate = LoadOrSynth(TEXT("hit_confirm.wav"), &BreakerSound::RenderHitConfirm, HitPcm);
    const int32 KillRate = LoadOrSynth(TEXT("kill_confirm.wav"), &BreakerSound::RenderKill, KillPcm);
    const int32 TakeHitRate = LoadOrSynth(TEXT("take_hit.wav"), &BreakerSound::RenderTakeHit, TakeHitPcm);

    // The ability DEFAULT loads here with the other four. Per-ability overrides
    // do NOT: there are THIRTY-FIVE abilities — seven per class across five —
    // the owner has authored none of them yet, and opening thirty-five files
    // that are expected to be missing at every level load is a cost paid for
    // nothing. They resolve on first cast instead. Nothing here is sized to
    // that count: resolution is per-id and lazy, so the number is prose.
    const int32 AbilityRate = LoadOrSynth(TEXT("ability_cast.wav"), &BreakerSound::RenderAbilityCast, AbilityDefaultPcm);
    // The sixth verb (O193): one low cue at the death beat's cut to black.
    const int32 PlayerDeathRate = LoadOrSynth(TEXT("player_death.wav"), &BreakerSound::RenderPlayerDeath, PlayerDeathPcm);
    const int32 EntropyRate = LoadOrSynth(TEXT("entropy_activate.wav"), &BreakerSound::RenderEntropyActivation, EntropyPcm);

    FireWave = MakeWave(FireRate);
    HitWave = MakeWave(HitRate);
    KillWave = MakeWave(KillRate);
    TakeHitWave = MakeWave(TakeHitRate);
    FireVoice->SetSound(FireWave);
    HitVoice->SetSound(HitWave);
    KillVoice->SetSound(KillWave);
    TakeHitVoice->SetSound(TakeHitWave);
    AbilityDefaultWave = MakeWave(AbilityRate);
    AbilityVoice->SetSound(AbilityDefaultWave);
    PlayerDeathWave = MakeWave(PlayerDeathRate);
    PlayerDeathVoice->SetSound(PlayerDeathWave);
    EntropyWave = MakeWave(EntropyRate);
    EntropyVoice->SetSound(EntropyWave);
    VoidMarkWave = MakeWave(LoadOrSynth(TEXT("void_activate.wav"), &BreakerSound::RenderVoidActivation, VoidMarkPcm));
    VoidBurstWave = MakeWave(LoadOrSynth(TEXT("void_burst.wav"), &BreakerSound::RenderVoidBurst, VoidBurstPcm));
    VoidMarkVoice->SetSound(VoidMarkWave);
    RiftWave = MakeWave(LoadOrSynth(TEXT("rift_activate.wav"), &BreakerSound::RenderRiftActivation, RiftPcm));
    RiftVoice->SetSound(RiftWave);
    CollapseWave = MakeWave(LoadOrSynth(TEXT("reaction_collapse.wav"), &BreakerSound::RenderReactionCollapse, CollapsePcm));
    WitherWave = MakeWave(LoadOrSynth(TEXT("reaction_wither.wav"), &BreakerSound::RenderReactionWither, WitherPcm));
    TearWave = MakeWave(LoadOrSynth(TEXT("reaction_tear.wav"), &BreakerSound::RenderReactionTear, TearPcm));
    VoidBurstVoice->SetSound(VoidBurstWave);
}

void ABreakerSoundDirector::ApplyVolumeSettings(float Master, float Effects)
{
    const float Gain = BreakerSound::EffectsGain(Master, Effects);
    for (UAudioComponent* Voice : { FireVoice.Get(), HitVoice.Get(), KillVoice.Get(),
        TakeHitVoice.Get(), AbilityVoice.Get(), PlayerDeathVoice.Get(), EntropyVoice.Get(), VoidMarkVoice.Get(), VoidBurstVoice.Get(), RiftVoice.Get(), ReactionVoice.Get() })
    {
        if (Voice) Voice->SetVolumeMultiplier(Gain);
    }
}

void ABreakerSoundDirector::PlaySettingsTest(UWorld* World)
{
    if (!World) return;
    ABreakerSoundDirector* Director = nullptr;
    for (TActorIterator<ABreakerSoundDirector> It(World); It; ++It)
    {
        Director = *It;
        break;
    }
    if (!Director)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Director = World->SpawnActor<ABreakerSoundDirector>(ABreakerSoundDirector::StaticClass(), FTransform::Identity, Params);
        if (Director) Director->SetLifeSpan(2.0f); // O2 PLACEHOLDER: preview cleanup after its short cue.
    }
    if (Director) Director->PlayHitConfirm();
}

bool ABreakerSoundDirector::PlayEntropyActivation()
{
    if (!GetWorld() || !EntropyWave || EntropyPcm.IsEmpty()) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastEntropyCueTime < BreakerSound::EntropyActivationMinGapSeconds) return false;
    LastEntropyCueTime = Now;
    ++EntropyCueCount;
    Trigger(EntropyVoice, EntropyWave, EntropyPcm);
    return true;
}

bool ABreakerSoundDirector::PlayReaction(FGameplayTag ReactionTag)
{
    USoundWaveProcedural* Wave = nullptr;
    const TArray<int16>* Pcm = nullptr;
    if (ReactionTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Collapse"))) { Wave = CollapseWave; Pcm = &CollapsePcm; }
    else if (ReactionTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Wither"))) { Wave = WitherWave; Pcm = &WitherPcm; }
    else if (ReactionTag == FGameplayTag::RequestGameplayTag(TEXT("Reaction.Tear"))) { Wave = TearWave; Pcm = &TearPcm; }
    if (!GetWorld() || !ReactionVoice || !Wave || !Pcm || Pcm->IsEmpty()) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastReactionCueTime < .15) return false; // O2 shared crowd throttle.
    LastReactionCueTime = Now;
    ReactionVoice->Stop();
    ReactionVoice->SetSound(Wave);
    Trigger(ReactionVoice, Wave, *Pcm);
    return true;
}

bool ABreakerSoundDirector::PlayRiftActivation()
{
    if (!GetWorld() || !RiftWave || RiftPcm.IsEmpty()) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastRiftCueTime < .15) return false; // O2 crowd throttle.
    LastRiftCueTime = Now;
    Trigger(RiftVoice, RiftWave, RiftPcm);
    return true;
}

bool ABreakerSoundDirector::PlayVoidCue(bool bBurst)
{
    auto* Wave = bBurst ? VoidBurstWave.Get() : VoidMarkWave.Get();
    const auto& Pcm = bBurst ? VoidBurstPcm : VoidMarkPcm;
    if (!GetWorld() || !Wave || Pcm.IsEmpty()) return false;
    double& Last = bBurst ? LastVoidBurstTime : LastVoidMarkTime;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - Last < .15) return false; // O2 crowd throttle, independent mark and payout cues.
    Last = Now;
    Trigger(bBurst ? VoidBurstVoice.Get() : VoidMarkVoice.Get(), Wave, Pcm);
    return true;
}

void ABreakerSoundDirector::Trigger(UAudioComponent* Voice, USoundWaveProcedural* Wave, const TArray<int16>& Pcm)
{
    if (!Voice || !Wave || Pcm.IsEmpty()) return;
    // Retrigger cuts: flush whatever of the previous play is still queued and
    // start the clip over. Stop() first so a voice whose source ended when
    // its queue ran dry comes back.
    Voice->Stop();
    Wave->ResetAudio();
    Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
    Voice->Play();
}

void ABreakerSoundDirector::PlayAbilityCast(FName AbilityId)
{
    // Resolve the override once per id. A key present with a null wave is the
    // "probed, none authored" sentinel, so an ability with no file costs one
    // failed open for the whole session rather than one per cast.
    USoundWaveProcedural* Wave = AbilityDefaultWave;
    const TArray<int16>* Pcm = &AbilityDefaultPcm;

    if (!AbilityId.IsNone())
    {
        if (const TObjectPtr<USoundWaveProcedural>* Found = AbilityWaves.Find(AbilityId))
        {
            if (*Found)
            {
                Wave = *Found;
                Pcm = AbilityPcm.Find(AbilityId);
            }
        }
        else
        {
            // Not probed yet. The id is the filename: Swift.Skim becomes
            // ability_Swift.Skim.wav, so the owner names an asset after the
            // ability and nothing here has to learn about it.
            const FString FileName = FString::Printf(TEXT("ability_%s.wav"), *AbilityId.ToString());
            const FString Path = FPaths::ProjectContentDir() / TEXT("Breaker/Audio") / FileName;
            TArray<uint8> Bytes;
            BreakerWave::FParsedWave Parsed;
            if (FFileHelper::LoadFileToArray(Bytes, *Path)) Parsed = BreakerWave::ParseWav(Bytes);

            if (Parsed.IsValid())
            {
                TArray<int16>& Stored = AbilityPcm.Add(AbilityId, MoveTemp(Parsed.Samples));
                USoundWaveProcedural* Override = MakeWave(Parsed.SampleRate);
                AbilityWaves.Add(AbilityId, Override);
                UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s: per-ability cue loaded (%d Hz)."),
                    *FileName, Parsed.SampleRate);
                Wave = Override;
                Pcm = &Stored;
            }
            else
            {
                // Sentinel: probed, nothing authored, use the default forever.
                AbilityWaves.Add(AbilityId, nullptr);
            }
        }
    }

    // The voice is shared, so the wave has to be re-pointed whenever this cast
    // resolved to a different clip than the last one did — the four fixed verbs
    // never need this because each owns its wave for the whole session.
    if (Wave && AbilityVoice && AbilityVoice->Sound != Wave) AbilityVoice->SetSound(Wave);
    if (Pcm) Trigger(AbilityVoice, Wave, *Pcm);
}

void ABreakerSoundDirector::PlayWeaponFire(EBreakerWeaponArchetype Archetype)
{
    // Resolve the override once per archetype; a key present with a NULL wave
    // is the "probed, none authored" sentinel, exactly as the ability cue does.
    USoundWaveProcedural* Wave = FireWave;
    const TArray<int16>* Pcm = &FirePcm;

    if (const TObjectPtr<USoundWaveProcedural>* Found = ArchetypeFireWaves.Find(Archetype))
    {
        if (*Found)
        {
            Wave = *Found;
            Pcm = ArchetypeFirePcm.Find(Archetype);
        }
    }
    else
    {
        // Derived by the pure helper in the header, which the suite asserts is
        // injective across every archetype.
        const FString FileName = BreakerSoundFiles::ArchetypeFire(Archetype);
        const FString Path = FPaths::ProjectContentDir() / TEXT("Breaker/Audio") / FileName;
        TArray<uint8> Bytes;
        BreakerWave::FParsedWave Parsed;
        if (FFileHelper::LoadFileToArray(Bytes, *Path)) Parsed = BreakerWave::ParseWav(Bytes);

        if (Parsed.IsValid())
        {
            TArray<int16>& Stored = ArchetypeFirePcm.Add(Archetype, MoveTemp(Parsed.Samples));
            USoundWaveProcedural* Override = MakeWave(Parsed.SampleRate);
            ArchetypeFireWaves.Add(Archetype, Override);
            UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s: per-archetype fire cue loaded (%d Hz)."),
                *FileName, Parsed.SampleRate);
            Wave = Override;
            Pcm = &Stored;
        }
        else
        {
            // LOGGED, unlike the ability miss, and once per archetype per
            // session. The four fixed verbs already log which path they took
            // because "a silent fallback is visible in any run's log" is this
            // file's rule; the same rule makes the naming convention
            // DISCOVERABLE — the owner authoring gun audio reads the exact
            // filename the game is looking for instead of guessing it.
            UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s absent — %s fires the shared default."),
                *FileName, *BreakerWeaponArchetypeNames::Display(Archetype));
            ArchetypeFireWaves.Add(Archetype, nullptr);
        }
    }

    if (Wave && FireVoice && FireVoice->Sound != Wave) FireVoice->SetSound(Wave);
    if (Pcm) Trigger(FireVoice, Wave, *Pcm);
}
void ABreakerSoundDirector::PlayHitConfirm() { Trigger(HitVoice, HitWave, HitPcm); }
void ABreakerSoundDirector::PlayKill()       { Trigger(KillVoice, KillWave, KillPcm); }
void ABreakerSoundDirector::PlayTakeHit()    { Trigger(TakeHitVoice, TakeHitWave, TakeHitPcm); }
void ABreakerSoundDirector::PlayPlayerDeath() { Trigger(PlayerDeathVoice, PlayerDeathWave, PlayerDeathPcm); }
