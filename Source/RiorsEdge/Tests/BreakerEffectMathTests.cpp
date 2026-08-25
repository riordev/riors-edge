#include "Misc/AutomationTest.h"
#include "UI/BreakerEffectMath.h"
#include "UI/BreakerEffectRenderer.h"

#if WITH_DEV_AUTOMATION_TESTS

// The effects renderer's schedule is pure arithmetic, and this is the half of
// "a primitive appears and disappears on schedule" a headless suite can
// prove. The other half — that the primitive is really in the world — is the
// capture harness's job, via -BreakerEffectProbe.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectScheduleTest,
    "RiorsEdge.UI.EffectSchedule.BornOnTimeDeadOnTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectScheduleTest::RunTest(const FString& Parameters)
{
    BreakerFX::FEffectTiming Timing;
    Timing.DurationSeconds = 6.0f;   // Rot's shape: long clip, soft edges
    Timing.FadeInSeconds = 0.25f;
    Timing.FadeOutSeconds = 0.5f;

    // --- Before birth: scheduled, not visible, not finished -----------------
    {
        const BreakerFX::FEffectSample Sample = BreakerFX::SampleEffect(Timing, -0.5f);
        TestFalse(TEXT("A scheduled effect is not visible"), Sample.bVisible);
        TestFalse(TEXT("A scheduled effect is not finished"), Sample.bFinished);
        TestEqual(TEXT("A scheduled effect is dark"), Sample.Alpha, 0.0f);
    }

    // --- The whole life, sampled finely -------------------------------------
    // Every frame inside (0, duration) is visible with alpha in (0, 1]; no
    // sample anywhere is outside [0, 1]; visible and finished are never both.
    for (float Age = -1.0f; Age < Timing.DurationSeconds + 1.0f; Age += 0.01f)
    {
        const BreakerFX::FEffectSample Sample = BreakerFX::SampleEffect(Timing, Age);
        TestTrue(TEXT("Alpha stays inside [0, 1]"), Sample.Alpha >= 0.0f && Sample.Alpha <= 1.0f);
        TestFalse(TEXT("Visible and finished are exclusive"), Sample.bVisible && Sample.bFinished);
        if (Age >= 0.011f && Age < Timing.DurationSeconds - 0.001f)
        {
            TestTrue(TEXT("Alive means visible"), Sample.bVisible);
            TestTrue(TEXT("Alive means lit"), Sample.Alpha > 0.0f);
        }
    }

    // --- The envelope's three regimes ----------------------------------------
    TestTrue(TEXT("Mid fade-in is part-lit"),
        FMath::IsNearlyEqual(BreakerFX::SampleEffect(Timing, 0.125f).Alpha, 0.5f, 0.01f));
    TestTrue(TEXT("The plateau is full brightness"),
        FMath::IsNearlyEqual(BreakerFX::SampleEffect(Timing, 3.0f).Alpha, 1.0f));
    TestTrue(TEXT("Mid fade-out is part-lit"),
        FMath::IsNearlyEqual(BreakerFX::SampleEffect(Timing, 5.75f).Alpha, 0.5f, 0.01f));

    // --- Death is exact and permanent ----------------------------------------
    {
        const BreakerFX::FEffectSample AtDeath = BreakerFX::SampleEffect(Timing, 6.0f);
        TestTrue(TEXT("Duration reached is finished"), AtDeath.bFinished);
        TestFalse(TEXT("Finished is not visible"), AtDeath.bVisible);
        TestTrue(TEXT("Long after death stays finished"),
            BreakerFX::SampleEffect(Timing, 60.0f).bFinished);
    }

    // --- Degenerate timings never divide or linger ---------------------------
    {
        BreakerFX::FEffectTiming Hard;   // no fades: pop in, pop out
        Hard.DurationSeconds = 0.1f;
        Hard.FadeInSeconds = 0.0f;
        Hard.FadeOutSeconds = 0.0f;
        TestTrue(TEXT("A fadeless clip is instantly full"),
            FMath::IsNearlyEqual(BreakerFX::SampleEffect(Hard, 0.0f).Alpha, 1.0f));
        TestTrue(TEXT("A fadeless clip still dies on time"),
            BreakerFX::SampleEffect(Hard, 0.1f).bFinished);

        BreakerFX::FEffectTiming Zero;   // zero duration: never visible
        Zero.DurationSeconds = 0.0f;
        TestTrue(TEXT("A zero-length clip is born finished"),
            BreakerFX::SampleEffect(Zero, 0.0f).bFinished);

        // Fades longer than the clip: the dimmer edge wins and the peak sits
        // below full, but the clip is still lit and still bounded.
        BreakerFX::FEffectTiming Overlapped;
        Overlapped.DurationSeconds = 0.2f;
        Overlapped.FadeInSeconds = 1.0f;
        Overlapped.FadeOutSeconds = 1.0f;
        const BreakerFX::FEffectSample Mid = BreakerFX::SampleEffect(Overlapped, 0.1f);
        TestTrue(TEXT("Overlapping fades still light the clip"), Mid.Alpha > 0.0f);
        TestTrue(TEXT("Overlapping fades peak below full"), Mid.Alpha < 1.0f);
    }

    // --- The Siphon rewrite works through this same function -----------------
    // A channel broken at age 2.1 rewrites duration to age + fade-out; the
    // clip must still be lit at the break, dying just after and dead exactly
    // at the rewritten end. No second lifetime mode.
    {
        BreakerFX::FEffectTiming Held;
        Held.DurationSeconds = 5.0f;     // the channel's authored maximum
        Held.FadeOutSeconds = 0.15f;
        const float BreakAge = 2.1f;
        Held.DurationSeconds = BreakAge + Held.FadeOutSeconds;
        TestTrue(TEXT("The beam is lit the frame the channel breaks"),
            BreakerFX::SampleEffect(Held, BreakAge).bVisible);
        TestTrue(TEXT("The broken beam is mid-fade just after"),
            BreakerFX::SampleEffect(Held, BreakAge + 0.075f).Alpha < 0.75f);
        TestTrue(TEXT("The broken beam is gone at the rewritten end"),
            BreakerFX::SampleEffect(Held, BreakAge + 0.15f).bFinished);
    }

    // --- The ground ring closes, at the true radius --------------------------
    // Every vertex sits exactly on the circle (the radius is never
    // approximated), consecutive strokes share endpoints exactly so the loop
    // has no gaps, and the last stroke ends where the first began.
    {
        const FVector Center(1200.0f, -300.0f, 40.0f);
        const float Radius = 400.0f;   // Rot's authored footprint
        const int32 Count = BreakerFX::GroundRingStrokes;
        FVector FirstA = FVector::ZeroVector, PreviousB = FVector::ZeroVector;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FVector A, B;
            BreakerFX::RingStroke(Center, Radius, Index, Count, A, B);
            TestTrue(TEXT("Ring vertex A sits on the true radius"),
                FMath::IsNearlyEqual(static_cast<float>(FVector::Dist(A, Center)), Radius, 0.01f));
            TestTrue(TEXT("Ring vertex B sits on the true radius"),
                FMath::IsNearlyEqual(static_cast<float>(FVector::Dist(B, Center)), Radius, 0.01f));
            TestTrue(TEXT("Ring strokes stay in the ground plane"),
                FMath::IsNearlyEqual(static_cast<float>(A.Z), static_cast<float>(Center.Z)));
            if (Index == 0) FirstA = A;
            else TestTrue(TEXT("Consecutive strokes share their endpoint"), A.Equals(PreviousB, 0.01));
            PreviousB = B;
        }
        TestTrue(TEXT("The ring closes"), PreviousB.Equals(FirstA, 0.01));
    }

    // --- Pool arithmetic, like the tracer's ----------------------------------
    TestTrue(TEXT("A 16-stroke ground ring fits the stroke pool twice over"),
        ABreakerEffectRenderer::GetStrokeSlots() >= 32);
    TestTrue(TEXT("The light pool is the small one"),
        ABreakerEffectRenderer::GetEffectLightSlots() < ABreakerEffectRenderer::GetGlowSlots());
    return true;
}

#endif
