#include "Misc/AutomationTest.h"
#include "Audio/BreakerSoundMath.h"

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
    return true;
}

#endif
