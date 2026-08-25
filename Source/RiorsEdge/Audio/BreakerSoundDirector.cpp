#include "Audio/BreakerSoundDirector.h"

#include "Audio/BreakerSoundMath.h"
#include "Components/AudioComponent.h"
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
        // The player's own events, flat 2D: fire, confirm and kill all happen
        // TO the listener, and spatializing them at the pawn's feet would only
        // add a doppler artifact when they sprint.
        Voice->bAllowSpatialization = false;
        Voice->bIsUISound = true;
        return Voice;
    };
    FireVoice = MakeVoice(TEXT("FireVoice"));
    HitVoice = MakeVoice(TEXT("HitVoice"));
    KillVoice = MakeVoice(TEXT("KillVoice"));
}

USoundWaveProcedural* ABreakerSoundDirector::MakeWave()
{
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
    Wave->SetSampleRate(BreakerSound::SampleRate);
    Wave->NumChannels = 1;
    Wave->SampleByteSize = sizeof(int16);
    Wave->SoundGroup = SOUNDGROUP_Default;
    Wave->bLooping = false;
    // A procedural wave has no length until fed; INDEFINITELY_LOOPING keeps
    // the engine from culling the source between triggers.
    Wave->Duration = INDEFINITELY_LOOPING_DURATION;
    return Wave;
}

void ABreakerSoundDirector::BeginPlay()
{
    Super::BeginPlay();

    // Rendered here rather than per trigger: three fixed buffers, ~28 KB
    // total, and the synth never runs on the fire path.
    BreakerSound::RenderWeaponFire(FirePcm);
    BreakerSound::RenderHitConfirm(HitPcm);
    BreakerSound::RenderKill(KillPcm);

    FireWave = MakeWave();
    HitWave = MakeWave();
    KillWave = MakeWave();
    FireVoice->SetSound(FireWave);
    HitVoice->SetSound(HitWave);
    KillVoice->SetSound(KillWave);
}

void ABreakerSoundDirector::Trigger(UAudioComponent* Voice, USoundWaveProcedural* Wave, const TArray<int16>& Pcm)
{
    if (!Voice || !Wave || Pcm.IsEmpty()) return;
    // Retrigger cuts: flush whatever of the previous play is still queued and
    // start the clip over. Stop() first so a voice whose source ended when
    // its queue ran dry comes back — Play() on an already-playing component
    // would restart it anyway, but on a finished one it is the only path.
    Voice->Stop();
    Wave->ResetAudio();
    Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
    Voice->Play();
}

void ABreakerSoundDirector::PlayWeaponFire() { Trigger(FireVoice, FireWave, FirePcm); }
void ABreakerSoundDirector::PlayHitConfirm() { Trigger(HitVoice, HitWave, HitPcm); }
void ABreakerSoundDirector::PlayKill()       { Trigger(KillVoice, KillWave, KillPcm); }
