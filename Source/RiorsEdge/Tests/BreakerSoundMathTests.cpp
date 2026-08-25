#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Audio/BreakerSoundMath.h"
#include "Audio/BreakerWaveFile.h"

#if WITH_DEV_AUTOMATION_TESTS

// The synth is pure arithmetic, so the suite proves the only audio facts a
// headless run can prove: each sound renders exactly the length it claims,
// no sample clips or wraps, every waveform starts and ends at silence (a
// nonzero first or last sample is an audible click), each carries actual
// energy rather than rendering as silence, and two renders are identical —
// the determinism that is the whole point of hashing the sample index
// instead of seeding a random stream.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSoundSynthShapeTest,
    "RiorsEdge.Audio.PlaceholderSynth.RendersCleanBoundedWaves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSoundSynthShapeTest::RunTest(const FString& Parameters)
{
    struct FCase
    {
        const TCHAR* Name;
        float DurationSeconds;
        void (*Render)(TArray<int16>&);
    };
    const FCase Cases[] = {
        {TEXT("WeaponFire"), BreakerSound::FireDurationSeconds, &BreakerSound::RenderWeaponFire},
        {TEXT("HitConfirm"), BreakerSound::HitDurationSeconds, &BreakerSound::RenderHitConfirm},
        {TEXT("Kill"), BreakerSound::KillDurationSeconds, &BreakerSound::RenderKill},
        {TEXT("TakeHit"), BreakerSound::TakeHitDurationSeconds, &BreakerSound::RenderTakeHit},
    };

    for (const FCase& Case : Cases)
    {
        TArray<int16> Pcm;
        Case.Render(Pcm);

        TestEqual(FString::Printf(TEXT("%s renders the length it claims"), Case.Name),
            Pcm.Num(), BreakerSound::SampleCount(Case.DurationSeconds));
        if (Pcm.IsEmpty()) continue;

        // The attack ramp and the release fade both bind: no click at either
        // edge. The first sample sits inside the 2 ms ramp's very first step
        // and the last inside the 10 ms fade's last, so "near zero" here is
        // one percent of full scale.
        TestTrue(FString::Printf(TEXT("%s starts at silence"), Case.Name),
            FMath::Abs(Pcm[0]) <= 327);
        TestTrue(FString::Printf(TEXT("%s ends at silence"), Case.Name),
            FMath::Abs(Pcm.Last()) <= 327);

        int32 Peak = 0;
        for (const int16 Sample : Pcm) Peak = FMath::Max(Peak, FMath::Abs(static_cast<int32>(Sample)));
        TestTrue(FString::Printf(TEXT("%s carries energy"), Case.Name), Peak > 3276);
        TestTrue(FString::Printf(TEXT("%s never clips"), Case.Name), Peak <= 32767);

        TArray<int16> Again;
        Case.Render(Again);
        TestTrue(FString::Printf(TEXT("%s renders deterministically"), Case.Name), Pcm == Again);
    }

    // --- The WAV reader: the sample path's pure half -------------------------
    // A synthetic RIFF round-trips exactly; stereo downmixes; everything the
    // reader does not speak (wrong magic, compressed, truncated) refuses
    // cleanly instead of crashing, which is what lets the director fall back
    // to the synth on any malformed file.
    {
        const auto BuildWav = [](int32 Rate, uint16 Channels, const TArray<int16>& Frames)
        {
            TArray<uint8> B;
            const int32 DataBytes = Frames.Num() * sizeof(int16);
            const auto PushU32 = [&B](uint32 V) { B.Append({static_cast<uint8>(V), static_cast<uint8>(V >> 8), static_cast<uint8>(V >> 16), static_cast<uint8>(V >> 24)}); };
            const auto PushU16 = [&B](uint16 V) { B.Append({static_cast<uint8>(V), static_cast<uint8>(V >> 8)}); };
            B.Append({'R','I','F','F'}); PushU32(36 + DataBytes); B.Append({'W','A','V','E'});
            B.Append({'f','m','t',' '}); PushU32(16); PushU16(1); PushU16(Channels);
            PushU32(Rate); PushU32(Rate * Channels * 2); PushU16(Channels * 2); PushU16(16);
            B.Append({'d','a','t','a'}); PushU32(DataBytes);
            B.Append(reinterpret_cast<const uint8*>(Frames.GetData()), DataBytes);
            return B;
        };

        const TArray<int16> Mono = {0, 1000, -1000, 32767, -32768, 5};
        const BreakerWave::FParsedWave ParsedMono = BreakerWave::ParseWav(BuildWav(44100, 1, Mono));
        TestTrue(TEXT("A mono PCM16 WAV round-trips"), ParsedMono.IsValid()
            && ParsedMono.SampleRate == 44100 && ParsedMono.Samples == Mono);

        const TArray<int16> Stereo = {1000, 3000, -2000, -4000};   // frames (L,R)
        const BreakerWave::FParsedWave ParsedStereo = BreakerWave::ParseWav(BuildWav(48000, 2, Stereo));
        TestTrue(TEXT("Stereo downmixes to the channel average"), ParsedStereo.IsValid()
            && ParsedStereo.Samples.Num() == 2
            && ParsedStereo.Samples[0] == 2000 && ParsedStereo.Samples[1] == -3000);

        TArray<uint8> Broken = BuildWav(44100, 1, Mono);
        Broken[0] = 'X';
        TestFalse(TEXT("Wrong magic refuses"), BreakerWave::ParseWav(Broken).IsValid());
        TArray<uint8> Tiny; Tiny.Init(0, 10);
        TestFalse(TEXT("A truncated file refuses"), BreakerWave::ParseWav(Tiny).IsValid());
        TArray<uint8> Compressed = BuildWav(44100, 1, Mono);
        Compressed[20] = 2;   // format tag != PCM
        TestFalse(TEXT("A non-PCM format refuses"), BreakerWave::ParseWav(Compressed).IsValid());
    }

    // --- The shipped samples parse -------------------------------------------
    // The director spawns lazily on the first shot, so no headless run ever
    // exercises its loader — this is the suite's replacement for that log
    // line. Gated on the audio directory existing: a clone with no audio
    // assets is legal by design (the synth is the floor), but a repo that
    // SHIPS the directory must ship all four files in a format the reader
    // speaks, or the fallback would engage silently over a broken commit.
    {
        const FString AudioDir = FPaths::ProjectContentDir() / TEXT("Breaker/Audio");
        if (IFileManager::Get().DirectoryExists(*AudioDir))
        {
            for (const TCHAR* Name : {TEXT("weapon_fire.wav"), TEXT("hit_confirm.wav"),
                TEXT("kill_confirm.wav"), TEXT("take_hit.wav")})
            {
                TArray<uint8> Bytes;
                TestTrue(FString::Printf(TEXT("%s ships"), Name),
                    FFileHelper::LoadFileToArray(Bytes, *(AudioDir / Name)));
                const BreakerWave::FParsedWave Parsed = BreakerWave::ParseWav(Bytes);
                TestTrue(FString::Printf(TEXT("%s parses as PCM16"), Name), Parsed.IsValid());
                TestTrue(FString::Printf(TEXT("%s is a real clip, not a click"), Name),
                    Parsed.IsValid() && Parsed.Samples.Num() > Parsed.SampleRate / 20);
            }
        }
    }
    return true;
}

#endif
