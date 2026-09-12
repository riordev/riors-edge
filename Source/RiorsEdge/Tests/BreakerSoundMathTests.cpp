#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Audio/BreakerSoundMath.h"
#include "Audio/BreakerSoundDirector.h"
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
        {TEXT("AbilityCast"), BreakerSound::AbilityCastDurationSeconds, &BreakerSound::RenderAbilityCast},
        {TEXT("PlayerDeath"), BreakerSound::PlayerDeathDurationSeconds, &BreakerSound::RenderPlayerDeath},
        {TEXT("EntropyActivation"), BreakerSound::EntropyActivationDurationSeconds, &BreakerSound::RenderEntropyActivation},
        {TEXT("VoidActivation"), .24f, &BreakerSound::RenderVoidActivation},
        {TEXT("VoidBurst"), .18f, &BreakerSound::RenderVoidBurst},
        {TEXT("RiftActivation"), .22f, &BreakerSound::RenderRiftActivation},
        {TEXT("ReactionCollapse"), .28f, &BreakerSound::RenderReactionCollapse},
        {TEXT("ReactionWither"), .30f, &BreakerSound::RenderReactionWither},
        {TEXT("ReactionTear"), .20f, &BreakerSound::RenderReactionTear},
        {TEXT("ChestOpen"), BreakerSound::ChestOpenDurationSeconds, &BreakerSound::RenderChestOpen},
    };
    // Every shipped fallback renderer participates in the waveform checks.
    TestEqual(TEXT("all fourteen authored fallback cues are rendered"), static_cast<int32>(UE_ARRAY_COUNT(Cases)), 14);

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

    // --- The player's death cue (O193): "one low sound" ----------------------
    // Shipped duration, and the only spectral fact a zero-crossing count can
    // prove: the implied fundamental across the whole render stays below
    // 200 Hz, which is what keeps the cue under the take-hit thud rather than
    // beside it. A 110 -> 55 Hz glide over 0.4 s crosses zero about 66 times;
    // 200 Hz would be 160. Zeros are skipped so the quantized tail does not
    // count as chatter.
    {
        TestEqual(TEXT("PlayerDeath ships at 0.4 s"), BreakerSound::PlayerDeathDurationSeconds, 0.4f);

        TArray<int16> Pcm;
        BreakerSound::RenderPlayerDeath(Pcm);
        int32 Crossings = 0;
        int32 LastSign = 0;
        for (const int16 Sample : Pcm)
        {
            const int32 Sign = Sample > 0 ? 1 : (Sample < 0 ? -1 : 0);
            if (Sign == 0) continue;
            if (LastSign != 0 && Sign != LastSign) ++Crossings;
            LastSign = Sign;
        }
        const float ImpliedHz = static_cast<float>(Crossings) / (2.0f * BreakerSound::PlayerDeathDurationSeconds);
        TestTrue(FString::Printf(TEXT("PlayerDeath's implied fundamental (%.1f Hz) stays below 200 Hz"), ImpliedHz),
            ImpliedHz < 200.0f);
        TestTrue(TEXT("PlayerDeath is a tone, not a click"), Crossings >= 20);
    }

    // --- The chest latch (O275): two taps, not one ----------------------------
    // Shipped figures, and the one structural fact a headless run can prove
    // about "two clicks 60 ms apart": the second tap is a second ONSET. The
    // peak inside the 10 ms after the second tap's instant must be louder
    // than the quietest 10 ms window between the two taps — a single decaying
    // click would only ever fall across that span.
    {
        TestEqual(TEXT("ChestOpen ships at 0.18 s"), BreakerSound::ChestOpenDurationSeconds, 0.18f);
        TestEqual(TEXT("ChestOpen's second tap lands at 60 ms"), BreakerSound::ChestOpenSecondTapSeconds, 0.06f);

        TArray<int16> Pcm;
        BreakerSound::RenderChestOpen(Pcm);
        const int32 Window = BreakerSound::SampleCount(0.010f);
        const int32 SecondTap = BreakerSound::SampleCount(BreakerSound::ChestOpenSecondTapSeconds);
        const auto PeakIn = [&Pcm](int32 From, int32 Count)
        {
            int32 Peak = 0;
            for (int32 I = From; I < FMath::Min(From + Count, Pcm.Num()); ++I)
                Peak = FMath::Max(Peak, FMath::Abs(static_cast<int32>(Pcm[I])));
            return Peak;
        };
        const int32 Lull = PeakIn(SecondTap - Window, Window);
        const int32 Second = PeakIn(SecondTap, Window);
        TestTrue(FString::Printf(TEXT("ChestOpen's second tap is an onset (lull %d, tap %d)"), Lull, Second),
            Second > Lull * 2);
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

    // --- The per-archetype fire cue's filename rule ---------------------------
    // The override is a FILENAME CONVENTION, so the convention is the contract:
    // every archetype must derive a DISTINCT, space-free name. A collision does
    // not fail — two guns quietly share one clip, which is the sameness this
    // override exists to end. Asserted across the whole enum rather than a
    // sample, so an archetype appended later is covered by existing.
    {
        TSet<FString> Seen;
        for (int32 Index = 0; Index < static_cast<int32>(EBreakerWeaponArchetype::Count); ++Index)
        {
            const EBreakerWeaponArchetype Archetype = static_cast<EBreakerWeaponArchetype>(Index);
            const FString FileName = BreakerSoundFiles::ArchetypeFire(Archetype);
            TestFalse(FString::Printf(TEXT("%s carries no space"), *FileName), FileName.Contains(TEXT(" ")));
            TestTrue(FString::Printf(TEXT("%s is the fire prefix"), *FileName),
                FileName.StartsWith(TEXT("weapon_fire_")) && FileName.EndsWith(TEXT(".wav")));
            TestFalse(FString::Printf(TEXT("%s is not already claimed by another archetype"), *FileName),
                Seen.Contains(FileName));
            Seen.Add(FileName);
        }
        TestEqual(TEXT("every archetype derived its own cue name"),
            Seen.Num(), static_cast<int32>(EBreakerWeaponArchetype::Count));
    }

    // --- The shipped samples parse -------------------------------------------
    // The director spawns lazily on the first shot, so no headless run ever
    // exercises its loader — this is the suite's replacement for that log
    // line. Gated on the audio directory existing: a clone with no audio
    // assets is legal by design (the synth is the floor), but a repo that
    // SHIPS the directory must ship every file the director names eagerly in
    // a format the reader speaks, or the fallback would engage silently over
    // a broken commit.
    //
    // FIVE: the files the director names eagerly at BeginPlay and that ship.
    // ability_cast.wav joined the day it was authored (a cut from the
    // converted Kenney set — provenance in SOURCES.txt), and chest_open.wav
    // (O275) the same way. take_hit.wav left
    // when the take-hit verb was retired (owner, playtest 2026-09-10; the
    // volume routing test records the voice is gone): the file may still sit
    // on disk, but nothing names it, so pinning it asserts nothing about the
    // game. The per-ability overrides (ability_<Id>.wav) never join, because
    // every one of them is optional by construction, and candidates/ is
    // invisible here on purpose — audition copies are consumed by nothing.
    // player_death.wav (O193) is named eagerly by the director but is not on
    // this list: no sample is authored and the synth is its floor. It joins
    // the day one is, under the same condition ability_cast.wav met.
    //
    // weapon_fire.wav is the one on this list that is now actually HEARD: it
    // is the Rifle's report. The other four are heard on their verbs.
    {
        const FString AudioDir = FPaths::ProjectContentDir() / TEXT("Breaker/Audio");
        if (IFileManager::Get().DirectoryExists(*AudioDir))
        {
            for (const TCHAR* Name : {TEXT("weapon_fire.wav"), TEXT("hit_confirm.wav"),
                TEXT("kill_confirm.wav"), TEXT("ability_cast.wav"), TEXT("chest_open.wav")})
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

// ---------------------------------------------------------------------------
// PER-PLAY PITCH. Owner: the sounds are "really flat". Every play of every
// verb was bit-identical, and this is the rule that ends that: one pitch
// multiplier per play, inside ±PitchSpread, from the same replayable hash the
// renders use. What a test can prove about it: it stays inside the spread
// (a multiplier outside it is a different sound, not a variation), no two
// consecutive plays match (the sameness was the finding), it centres on 1.0
// (a spread with a bias would detune every gun, not vary it), and the
// shipped spread is the number the owner will feel.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSoundPitchVariationTest,
    "RiorsEdge.Audio.PitchVariation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSoundPitchVariationTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("PitchSpread ships at 0.06"), BreakerSound::PitchSpread, 0.06f);

    const float Low = 1.0f - BreakerSound::PitchSpread;
    const float High = 1.0f + BreakerSound::PitchSpread;
    double Sum = 0.0;
    float Previous = BreakerSound::PitchForPlay(0);
    int32 OutOfRange = 0;
    int32 Repeats = 0;
    constexpr uint32 Plays = 1000;
    for (uint32 PlayIndex = 1; PlayIndex <= Plays; ++PlayIndex)
    {
        const float Pitch = BreakerSound::PitchForPlay(PlayIndex);
        if (Pitch < Low || Pitch > High) ++OutOfRange;
        if (Pitch == Previous) ++Repeats;
        Sum += Pitch;
        Previous = Pitch;
    }
    TestEqual(TEXT("every play's pitch stays inside +-PitchSpread"), OutOfRange, 0);
    TestEqual(TEXT("no two consecutive plays share a pitch"), Repeats, 0);
    const double Mean = Sum / Plays;
    TestTrue(FString::Printf(TEXT("the pitch centres on 1.0 (mean %.4f)"), Mean),
        FMath::Abs(Mean - 1.0) < 0.01);

    // Replayable: the whole point of a hash over a seeded stream.
    TestEqual(TEXT("the sequence is deterministic"),
        BreakerSound::PitchForPlay(37), BreakerSound::PitchForPlay(37));
    return true;
}

#endif
