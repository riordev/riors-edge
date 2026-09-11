#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/BreakerSoundMath.h"
#include "Weapons/BreakerWeaponArchetype.h"

// ---------------------------------------------------------------------------
// EIGHT GUNS, EIGHT SOUNDS.
//
// Owner: "my gun sounds like a nerf gun", and "i dont know what weapon is in
// my hand". The second is the one this file exists for. Every archetype used
// to play the identical rendered burst, so the assertion that matters is not
// that each sound is clean — the shape test next door already covers that for
// the shared cue — but that no two of them are the SAME.
//
// A test cannot hear anything. What it can measure is the three properties a
// listener actually separates guns by, all of which are arithmetic:
//
//   LENGTH   how long the report lasts
//   WEIGHT   root-mean-square level across the whole render
//   COLOUR   zero-crossing rate, which rises with brightness — a crack crosses
//            zero far more often per second than a thump does
//
// So this asserts that the eight differ in those, and that they differ in the
// DIRECTIONS the voices claim: a sidearm brighter than a rocket, a rocket
// longer than an SMG, a shotgun heavier and darker than a sidearm. Whether any
// of them sounds good is the owner's ears and nothing here pretends otherwise.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWeaponVoiceTest,
    "RiorsEdge.Audio.WeaponVoice.EightDistinctReports",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    struct FBreakerVoiceMeasure
    {
        int32 Samples = 0;
        double Rms = 0.0;
        double CrossingsPerSecond = 0.0;
    };

    FBreakerVoiceMeasure BreakerMeasureVoice(EBreakerWeaponArchetype Archetype)
    {
        TArray<int16> Pcm;
        BreakerSound::RenderArchetypeFire(Pcm, Archetype);
        FBreakerVoiceMeasure Out;
        Out.Samples = Pcm.Num();
        if (Pcm.IsEmpty()) return Out;

        double SumSquares = 0.0;
        int32 Crossings = 0;
        for (int32 Index = 0; Index < Pcm.Num(); ++Index)
        {
            const double Value = static_cast<double>(Pcm[Index]) / 32767.0;
            SumSquares += Value * Value;
            if (Index > 0 && ((Pcm[Index - 1] < 0) != (Pcm[Index] < 0))) ++Crossings;
        }
        Out.Rms = FMath::Sqrt(SumSquares / Pcm.Num());
        Out.CrossingsPerSecond = static_cast<double>(Crossings) * BreakerSound::SampleRate / Pcm.Num();
        return Out;
    }
}

bool FBreakerWeaponVoiceTest::RunTest(const FString& Parameters)
{
    const int32 Count = static_cast<int32>(EBreakerWeaponArchetype::Count);
    TArray<FBreakerVoiceMeasure> Measures;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const EBreakerWeaponArchetype Archetype = static_cast<EBreakerWeaponArchetype>(Index);
        const FBreakerVoiceMeasure Measure = BreakerMeasureVoice(Archetype);
        const FString Name = BreakerWeaponArchetypeNames::Display(Archetype);
        AddInfo(FString::Printf(TEXT("WEAPON VOICE  %-12s %5d samples, rms %.4f, %.0f crossings/s"),
            *Name, Measure.Samples, Measure.Rms, Measure.CrossingsPerSecond));

        // A SOUND AT ALL. A voice that renders silence is worse than the shared
        // burst it replaced: the player fires and hears nothing.
        if (!TestTrue(*FString::Printf(TEXT("%s renders samples"), *Name), Measure.Samples > 0)) return false;
        if (!TestTrue(*FString::Printf(TEXT("%s carries real energy"), *Name), Measure.Rms > 0.01)) return false;

        // NO CLIPPING, and this is the one that the per-part gains could
        // actually break: the mix is normalised precisely so raising one part
        // cannot raise the whole sound past full scale.
        TArray<int16> Pcm;
        BreakerSound::RenderArchetypeFire(Pcm, Archetype);
        int32 AtCeiling = 0;
        for (const int16 Sample : Pcm) AtCeiling += FMath::Abs(static_cast<int32>(Sample)) >= 32767 ? 1 : 0;
        TestEqual(*FString::Printf(TEXT("%s never reaches full scale"), *Name), AtCeiling, 0);

        // STARTS AND ENDS AT SILENCE. A waveform that begins or ends off zero
        // is an audible click, and a click on every shot is the loudest defect
        // a gun can have.
        TestEqual(*FString::Printf(TEXT("%s starts silent"), *Name), static_cast<int32>(Pcm[0]), 0);
        TestEqual(*FString::Printf(TEXT("%s ends silent"), *Name), static_cast<int32>(Pcm.Last()), 0);

        // DETERMINISTIC, which is what lets any of the above be a test at all.
        TArray<int16> Again;
        BreakerSound::RenderArchetypeFire(Again, Archetype);
        TestTrue(*FString::Printf(TEXT("%s renders identically twice"), *Name), Pcm == Again);

        Measures.Add(Measure);
    }

    // ---- NO TWO GUNS SOUND THE SAME --------------------------------------
    // The whole point. Two archetypes that agree on length, weight AND colour
    // are indistinguishable in play, which is the state this pass ended.
    for (int32 A = 0; A < Count; ++A)
    {
        for (int32 B = A + 1; B < Count; ++B)
        {
            const FString First = BreakerWeaponArchetypeNames::Display(static_cast<EBreakerWeaponArchetype>(A));
            const FString Second = BreakerWeaponArchetypeNames::Display(static_cast<EBreakerWeaponArchetype>(B));
            const bool bLength = Measures[A].Samples != Measures[B].Samples;
            const bool bWeight = FMath::Abs(Measures[A].Rms - Measures[B].Rms) > 0.005;
            const bool bColour = FMath::Abs(Measures[A].CrossingsPerSecond - Measures[B].CrossingsPerSecond) > 200.0;
            TestTrue(*FString::Printf(TEXT("%s and %s are told apart by length, weight or colour"),
                *First, *Second), bLength || bWeight || bColour);
        }
    }

    // ---- AND THEY DIFFER IN THE DIRECTIONS THE VOICES CLAIM --------------
    // Distinctness alone would pass on eight arbitrary sounds. These assert
    // that the differences mean what the file says they mean, so a retune that
    // makes the rocket brighter than the sidearm fails here and says why.
    const auto& Sidearm = Measures[static_cast<int32>(EBreakerWeaponArchetype::Sidearm)];
    const auto& Rocket = Measures[static_cast<int32>(EBreakerWeaponArchetype::Rocket)];
    const auto& Shotgun = Measures[static_cast<int32>(EBreakerWeaponArchetype::Shotgun)];
    const auto& SMG = Measures[static_cast<int32>(EBreakerWeaponArchetype::SMG)];
    const auto& Sniper = Measures[static_cast<int32>(EBreakerWeaponArchetype::Sniper)];

    TestTrue(TEXT("a sidearm is brighter than a rocket"),
        Sidearm.CrossingsPerSecond > Rocket.CrossingsPerSecond);
    TestTrue(TEXT("and brighter than a shotgun"),
        Sidearm.CrossingsPerSecond > Shotgun.CrossingsPerSecond);
    TestTrue(TEXT("a rocket lasts longer than an SMG burst"), Rocket.Samples > SMG.Samples);
    TestTrue(TEXT("and longer than a sniper's report"), Rocket.Samples > Sniper.Samples);
    TestTrue(TEXT("a sniper lasts longer than an SMG"), Sniper.Samples > SMG.Samples);
    TestTrue(TEXT("a shotgun hits harder than a sidearm"), Shotgun.Rms > Sidearm.Rms);

    // THE RIFLE'S SYNTH VOICE IS THE FLOOR, NOT WHAT SHIPS. This file measures
    // renders, not routing: in the shipped configuration the director routes
    // the Rifle to the weapon_fire.wav recording, and this synth voice plays
    // only for a clone with no audio assets or a file that failed to load.
    // An earlier pass pinned this as "the rifle is unmoved" — it was not; the
    // recording had been cut out of the route and this synth played instead.
    // What is pinned is that the floor still renders at the shared
    // FireDurationSeconds, so RenderWeaponFire and FireVoiceFor(Rifle) agree.
    TestEqual(TEXT("the rifle's synth floor keeps the shared report length"),
        BreakerSound::FireVoiceFor(EBreakerWeaponArchetype::Rifle).DurationSeconds,
        BreakerSound::FireDurationSeconds);

    // ---- THE TWO NOISE COLOURS -------------------------------------------
    // BrightNoise is a first difference and DarkNoise a quarter-rate
    // interpolation, so one must actually be brighter than the other or the
    // whole voice system is one colour wearing two names.
    {
        int32 BrightCrossings = 0, DarkCrossings = 0;
        float PreviousBright = BreakerSound::BrightNoise(0);
        float PreviousDark = BreakerSound::DarkNoise(0);
        constexpr int32 Window = 4000;
        for (uint32 Index = 1; Index < Window; ++Index)
        {
            const float Bright = BreakerSound::BrightNoise(Index);
            const float Dark = BreakerSound::DarkNoise(Index);
            if ((PreviousBright < 0.0f) != (Bright < 0.0f)) ++BrightCrossings;
            if ((PreviousDark < 0.0f) != (Dark < 0.0f)) ++DarkCrossings;
            PreviousBright = Bright;
            PreviousDark = Dark;
        }
        AddInfo(FString::Printf(TEXT("NOISE COLOUR  bright %d crossings, dark %d, over %d samples"),
            BrightCrossings, DarkCrossings, Window));
        TestTrue(TEXT("the bright noise is actually brighter than the dark noise"),
            BrightCrossings > DarkCrossings * 2);
        // Both stay in range, or the mix normalisation cannot hold.
        for (uint32 Index = 0; Index < 512; ++Index)
        {
            TestTrue(TEXT("bright noise stays in range"), FMath::Abs(BreakerSound::BrightNoise(Index)) <= 1.0f);
            TestTrue(TEXT("dark noise stays in range"), FMath::Abs(BreakerSound::DarkNoise(Index)) <= 1.0f);
        }
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
