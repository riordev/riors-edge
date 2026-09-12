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
    // The fire pool: FireVoice0..2, same construction as every other voice so
    // the settings routing (which walks every component) picks them up.
    for (int32 Slot = 0; Slot < FireVoiceCount; ++Slot)
    {
        FireVoices.Add(MakeVoice(*FString::Printf(TEXT("FireVoice%d"), Slot)));
    }
    HitVoice = MakeVoice(TEXT("HitVoice"));
    KillVoice = MakeVoice(TEXT("KillVoice"));
    AbilityVoice = MakeVoice(TEXT("AbilityVoice"));
    PlayerDeathVoice = MakeVoice(TEXT("PlayerDeathVoice"));
    LevelUpVoice = MakeVoice(TEXT("LevelUpVoice"));
    ChestVoice = MakeVoice(TEXT("ChestVoice"));
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

int32 ABreakerSoundDirector::LoadOrSynth(const TCHAR* FileName, void (*Synth)(TArray<int16>&), TArray<int16>& OutPcm, bool* bOutLoaded)
{
    if (bOutLoaded) *bOutLoaded = false;
    const FString Path = FPaths::ProjectContentDir() / TEXT("Breaker/Audio") / FileName;
    TArray<uint8> Bytes;
    const bool bOverrideExists = FPaths::FileExists(Path);
    if (bOverrideExists && FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        BreakerWave::FParsedWave Parsed = BreakerWave::ParseWav(Bytes);
        if (Parsed.IsValid())
        {
            OutPcm = MoveTemp(Parsed.Samples);
            UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s: sample loaded (%d Hz, %.2f s)."),
                FileName, Parsed.SampleRate, static_cast<float>(OutPcm.Num()) / Parsed.SampleRate);
            if (bOutLoaded) *bOutLoaded = true;
            return Parsed.SampleRate;
        }
        UE_LOG(LogTemp, Warning, TEXT("[BreakerSound] %s exists but is not 16-bit PCM WAV — synth fallback."), FileName);
    }
    else if (bOverrideExists)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BreakerSound] %s exists but could not be read — synth fallback."), FileName);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s optional override absent — authored synth fallback."), FileName);
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
    FireRate = LoadOrSynth(TEXT("weapon_fire.wav"), &BreakerSound::RenderWeaponFire, FirePcm, &bFireSampleLoaded);
    const int32 HitRate = LoadOrSynth(TEXT("hit_confirm.wav"), &BreakerSound::RenderHitConfirm, HitPcm);
    const int32 KillRate = LoadOrSynth(TEXT("kill_confirm.wav"), &BreakerSound::RenderKill, KillPcm);

    // The ability DEFAULT loads here with the other four. Per-ability overrides
    // do NOT: there are THIRTY-FIVE abilities — seven per class across five —
    // the owner has authored none of them yet, and opening thirty-five files
    // that are expected to be missing at every level load is a cost paid for
    // nothing. They resolve on first cast instead. Nothing here is sized to
    // that count: resolution is per-id and lazy, so the number is prose.
    const int32 AbilityRate = LoadOrSynth(TEXT("ability_cast.wav"), &BreakerSound::RenderAbilityCast, AbilityDefaultPcm);
    // The sixth verb (O193): one low cue at the death beat's cut to black.
    const int32 PlayerDeathRate = LoadOrSynth(TEXT("player_death.wav"), &BreakerSound::RenderPlayerDeath, PlayerDeathPcm);
    const int32 LevelUpRate = LoadOrSynth(TEXT("level_up.wav"), &BreakerSound::RenderLevelUp, LevelUpPcm);
    // O275: the supply chest's latch. Ships as a Kenney cut; the synth's
    // two-tap render is its floor.
    const int32 ChestRate = LoadOrSynth(TEXT("chest_open.wav"), &BreakerSound::RenderChestOpen, ChestPcm);
    const int32 EntropyRate = LoadOrSynth(TEXT("entropy_activate.wav"), &BreakerSound::RenderEntropyActivation, EntropyPcm);

    // One wave per pool slot, seeded at the shared clip's rate; PlayWeaponFire
    // re-rates a slot per play to match whichever archetype's clip it carries.
    FireWaves.Reset();
    for (int32 Slot = 0; Slot < FireVoices.Num(); ++Slot)
    {
        USoundWaveProcedural* SlotWave = MakeWave(FireRate);
        FireWaves.Add(SlotWave);
        if (FireVoices[Slot]) FireVoices[Slot]->SetSound(SlotWave);
    }
    HitWave = MakeWave(HitRate);
    KillWave = MakeWave(KillRate);
    HitVoice->SetSound(HitWave);
    KillVoice->SetSound(KillWave);
    AbilityDefaultWave = MakeWave(AbilityRate);
    AbilityVoice->SetSound(AbilityDefaultWave);
    PlayerDeathWave = MakeWave(PlayerDeathRate);
    PlayerDeathVoice->SetSound(PlayerDeathWave);
    LevelUpWave = MakeWave(LevelUpRate);
    LevelUpVoice->SetSound(LevelUpWave);
    ChestWave = MakeWave(ChestRate);
    ChestVoice->SetSound(ChestWave);
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
    // EVERY voice this actor owns, not a hand-kept list of twelve. The list
    // was the drift: adding the level-up cue left it silent-but-full-volume
    // — routed nowhere, ignoring the settings the player set — and only the
    // volume assertion in the routing test caught it. The next verb is
    // correct by construction, and the test still pins the roster by name
    // and by count so a MISSING voice is still a red.
    TArray<UAudioComponent*> Voices;
    GetComponents(Voices);
    for (UAudioComponent* Voice : Voices)
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
    // THIS PLAY'S PITCH, set on the stopped voice so Play() below makes a
    // source with it. One site, every verb: the clip bytes never change, the
    // rate they are read at does, inside ±PitchSpread, and no two consecutive
    // plays anywhere in the director are identical.
    Voice->SetPitchMultiplier(BreakerSound::PitchForPlay(++PlayCounter));
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
    // Resolve the clip once per archetype and keep it: the PCM and the rate it
    // was authored at. Every archetype resolves to a sound of its own — an
    // authored .wav if one is there, the shipped recording for the Rifle,
    // otherwise its synthesized voice. A key present in ArchetypeFireRates
    // means "probed"; there is no null sentinel because "none authored" no
    // longer means "play the shared one".
    const TArray<int16>* Pcm = nullptr;
    int32 Rate = 0;

    if (const int32* FoundRate = ArchetypeFireRates.Find(Archetype))
    {
        Rate = *FoundRate;
        Pcm = ArchetypeFirePcm.Find(Archetype);
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
            Rate = Parsed.SampleRate;
            Pcm = &ArchetypeFirePcm.Add(Archetype, MoveTemp(Parsed.Samples));
            ArchetypeFireRates.Add(Archetype, Rate);
            UE_LOG(LogTemp, Log, TEXT("[BreakerSound] %s: per-archetype fire cue loaded (%d Hz)."),
                *FileName, Rate);
        }
        else if (Archetype == EBreakerWeaponArchetype::Rifle && bFireSampleLoaded)
        {
            // THE RECORDING PLAYS FOR THE GUN HE HOLDS. The header's ruling is
            // recorded samples first, synth as fallback, and weapon_fire.wav
            // ships — but the per-archetype pass routed EVERY archetype with
            // no weapon_fire_<Archetype>.wav to its synth, which is all eight
            // of them, so the recording was loaded at BeginPlay and never
            // played. The Rifle is the Standard Issue gun and the recording is
            // its report; the other seven keep their synth voices, because
            // sharing one recording across eight guns is the sameness the
            // per-archetype pass ended. A copy, not a move, so FirePcm stays
            // what BeginPlay loaded.
            Rate = FireRate;
            Pcm = &ArchetypeFirePcm.Add(Archetype, FirePcm);
            ArchetypeFireRates.Add(Archetype, Rate);
            UE_LOG(LogTemp, Log,
                TEXT("[BreakerSound] %s not authored; Rifle uses the shipped weapon_fire.wav recording (%d Hz, %.2fs)."),
                *FileName, Rate, static_cast<float>(Pcm->Num()) / FMath::Max(Rate, 1));
        }
        else
        {
            // NO AUTHORED FILE IS NOT NO SOUND. This branch used to store a
            // null sentinel and let every archetype fall back to the one
            // shared render, which is exactly what the owner heard: "my gun
            // sounds like a nerf gun" and "i dont know what weapon is in my
            // hand" are the same finding, and the second one is the worse of
            // the two. Eight guns played one sound.
            //
            // So the fallback is SYNTHESIZED PER ARCHETYPE from
            // BreakerSound::FireVoiceFor, and an authored .wav still overrides
            // it the moment one is dropped in. The naming convention stays
            // discoverable the same way: the log names the file it looked for.
            // The Rifle lands here only when weapon_fire.wav failed to load.
            TArray<int16> Rendered;
            BreakerSound::RenderArchetypeFire(Rendered, Archetype);
            Rate = BreakerSound::SampleRate;
            Pcm = &ArchetypeFirePcm.Add(Archetype, MoveTemp(Rendered));
            ArchetypeFireRates.Add(Archetype, Rate);
            UE_LOG(LogTemp, Log,
                TEXT("[BreakerSound] %s not authored; %s uses its synthesized voice (%.2fs)."),
                *FileName, *BreakerWeaponArchetypeNames::Display(Archetype),
                BreakerSound::FireVoiceFor(Archetype).DurationSeconds);
        }
    }

    if (!Pcm || Pcm->IsEmpty() || FireVoices.IsEmpty() || FireWaves.Num() != FireVoices.Num()) return;

    // ROTATE THE POOL. This shot takes the next slot; the slot it cuts is the
    // one FireVoiceCount shots back, whose clip has had that many shot
    // intervals to ring out.
    const int32 Slot = NextFireVoice;
    NextFireVoice = (NextFireVoice + 1) % FireVoices.Num();
    UAudioComponent* Voice = FireVoices[Slot];
    USoundWaveProcedural* Wave = FireWaves[Slot];
    if (!Voice || !Wave) return;
    // The slot's wave carries whatever rate the last clip through it was
    // authored at, and the recording and the synth voices differ. Trigger
    // stops the voice before Play() makes a fresh source, and a procedural
    // wave reports SampleRate verbatim, so the rate is set here per play and
    // is read at that source's init — never under a running one.
    Wave->SetSampleRate(Rate);
    if (Voice->Sound != Wave) Voice->SetSound(Wave);
    Trigger(Voice, Wave, *Pcm);
}
void ABreakerSoundDirector::PlayHitConfirm() { Trigger(HitVoice, HitWave, HitPcm); }
void ABreakerSoundDirector::PlayKill()       { Trigger(KillVoice, KillWave, KillPcm); }
void ABreakerSoundDirector::PlayPlayerDeath() { Trigger(PlayerDeathVoice, PlayerDeathWave, PlayerDeathPcm); }
void ABreakerSoundDirector::PlayLevelUp() { Trigger(LevelUpVoice, LevelUpWave, LevelUpPcm); }
void ABreakerSoundDirector::PlayChestOpen() { Trigger(ChestVoice, ChestWave, ChestPcm); }
