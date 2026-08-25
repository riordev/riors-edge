#include "Audio/BreakerSoundDirector.h"

#include "Audio/BreakerSoundMath.h"
#include "Audio/BreakerWaveFile.h"
#include "Components/AudioComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"

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

    const int32 FireRate = LoadOrSynth(TEXT("weapon_fire.wav"), &BreakerSound::RenderWeaponFire, FirePcm);
    const int32 HitRate = LoadOrSynth(TEXT("hit_confirm.wav"), &BreakerSound::RenderHitConfirm, HitPcm);
    const int32 KillRate = LoadOrSynth(TEXT("kill_confirm.wav"), &BreakerSound::RenderKill, KillPcm);
    const int32 TakeHitRate = LoadOrSynth(TEXT("take_hit.wav"), &BreakerSound::RenderTakeHit, TakeHitPcm);

    FireWave = MakeWave(FireRate);
    HitWave = MakeWave(HitRate);
    KillWave = MakeWave(KillRate);
    TakeHitWave = MakeWave(TakeHitRate);
    FireVoice->SetSound(FireWave);
    HitVoice->SetSound(HitWave);
    KillVoice->SetSound(KillWave);
    TakeHitVoice->SetSound(TakeHitWave);
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

void ABreakerSoundDirector::PlayWeaponFire() { Trigger(FireVoice, FireWave, FirePcm); }
void ABreakerSoundDirector::PlayHitConfirm() { Trigger(HitVoice, HitWave, HitPcm); }
void ABreakerSoundDirector::PlayKill()       { Trigger(KillVoice, KillWave, KillPcm); }
void ABreakerSoundDirector::PlayTakeHit()    { Trigger(TakeHitVoice, TakeHitWave, TakeHitPcm); }
