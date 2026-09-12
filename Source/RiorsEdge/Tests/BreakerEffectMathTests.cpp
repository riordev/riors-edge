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

    // --- The ring turns and dashes (O282) ------------------------------------
    // A phase rotates the whole polygon about its centre: one full turn is
    // the same ring, and a half-side turn puts vertex 0 where side 0's
    // midpoint was. A dash keeps a stroke on its own chord, shortened about
    // the chord's middle, so a dashed rim is still the true footprint.
    {
        const FVector Center(1200.0f, -300.0f, 40.0f);
        const float Radius = 400.0f;
        const int32 Count = BreakerFX::GroundRingStrokes;
        const float HalfSide = PI / static_cast<float>(Count);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector Plain = BreakerFX::RingVertex(Center, Radius, Index, Count);
            const FVector FullTurn = BreakerFX::RingVertex(Center, Radius, Index, Count, 2.0f * PI);
            TestTrue(TEXT("A full turn is the same vertex"), FullTurn.Equals(Plain, 0.01));

            FVector A0, B0, A, B;
            BreakerFX::RingStroke(Center, Radius, Index, Count, A0, B0);
            BreakerFX::RingStroke(Center, Radius, Index, Count, A, B, 0.0f, 0.72f);
            const FVector Chord = B0 - A0;
            const float ChordLength = static_cast<float>(Chord.Size());
            const FVector Along = Chord / ChordLength;
            // On the chord: no distance off the line, and both ends inside it.
            const float OffA = static_cast<float>(FVector::CrossProduct(A - A0, Along).Size());
            const float OffB = static_cast<float>(FVector::CrossProduct(B - A0, Along).Size());
            TestTrue(TEXT("A dashed stroke lies on its chord"), OffA <= 0.01f && OffB <= 0.01f);
            const float TA = static_cast<float>(FVector::DotProduct(A - A0, Along)) / ChordLength;
            const float TB = static_cast<float>(FVector::DotProduct(B - A0, Along)) / ChordLength;
            TestTrue(TEXT("A dashed stroke stays inside its chord"),
                TA >= -1.0e-3f && TA <= 1.0f + 1.0e-3f && TB >= -1.0e-3f && TB <= 1.0f + 1.0e-3f);
            TestTrue(TEXT("A dash is its fraction of the chord"),
                FMath::IsNearlyEqual(static_cast<float>(FVector::Dist(A, B)), 0.72f * ChordLength, 0.01f));
            // A full dash is the undashed stroke.
            FVector FullA, FullB;
            BreakerFX::RingStroke(Center, Radius, Index, Count, FullA, FullB, 0.0f, 1.0f);
            TestTrue(TEXT("A dash of one is the whole side"), FullA.Equals(A0, 0.01) && FullB.Equals(B0, 0.01));
        }
        // Half a side of phase: vertex 0 sits at the angle of side 0's old
        // midpoint, at the true radius.
        const FVector OldMid = (BreakerFX::RingVertex(Center, Radius, 0, Count)
            + BreakerFX::RingVertex(Center, Radius, 1, Count)) * 0.5f;
        const FVector Expected = Center + (OldMid - Center).GetSafeNormal() * Radius;
        const FVector Turned = BreakerFX::RingVertex(Center, Radius, 0, Count, HalfSide);
        TestTrue(TEXT("A half-side phase puts vertex 0 at the old midpoint angle"), Turned.Equals(Expected, 0.01));
        TestTrue(TEXT("and the turned vertex is still on the true radius"),
            FMath::IsNearlyEqual(static_cast<float>(FVector::Dist(Turned, Center)), Radius, 0.01f));
    }

    // --- The swept arc covers exactly its included angle ---------------------
    // Vertex 0 sits at -half off Forward (the LEFT edge, so index-staggered
    // strokes sweep left to right), the last vertex at +half, every vertex at
    // the true range, and consecutive strokes share endpoints.
    {
        const FVector Origin(0.0f, 0.0f, 100.0f);
        const FVector Forward(1.0f, 0.0f, 0.0f);
        const float Arc = 120.0f;      // pure-geometry fixture; not read from the ability
        const float Range = 300.0f;    // pure-geometry fixture; not read from the ability
        const int32 Count = BreakerFX::SweptArcStrokes;
        FVector PreviousB = FVector::ZeroVector;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FVector A, B;
            BreakerFX::ArcStroke(Origin, Forward, Arc, Range, Index, Count, A, B);
            TestTrue(TEXT("Arc vertices sit at the true range"),
                FMath::IsNearlyEqual(static_cast<float>(FVector::Dist(A, Origin)), Range, 0.01f)
                && FMath::IsNearlyEqual(static_cast<float>(FVector::Dist(B, Origin)), Range, 0.01f));
            if (Index > 0) TestTrue(TEXT("Consecutive arc strokes share their endpoint"), A.Equals(PreviousB, 0.01));
            PreviousB = B;
        }
        const FVector LeftEdge = BreakerFX::ArcVertex(Origin, Forward, Arc, Range, 0, Count);
        const FVector RightEdge = BreakerFX::ArcVertex(Origin, Forward, Arc, Range, Count, Count);
        const float HalfRad = FMath::DegreesToRadians(Arc * 0.5f);
        TestTrue(TEXT("The first vertex sits at minus half the included angle"),
            FMath::IsNearlyEqual(static_cast<float>(LeftEdge.Y), -Range * FMath::Sin(HalfRad), 0.1f));
        TestTrue(TEXT("The last vertex sits at plus half the included angle"),
            FMath::IsNearlyEqual(static_cast<float>(RightEdge.Y), Range * FMath::Sin(HalfRad), 0.1f));
    }

    // --- The status tint map is total ----------------------------------------
    // Mapped tags tint; anything else — including an invalid tag — keeps the
    // caller's fallback rather than inventing a colour.
    {
        const FLinearColor Fallback(0.62f, 0.14f, 0.92f);
        const FLinearColor Bleed = BreakerFX::ColorForStatusTag(
            FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false), Fallback);
        const FLinearColor Poison = BreakerFX::ColorForStatusTag(
            FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false), Fallback);
        TestFalse(TEXT("Bleed tints away from the fallback"), Bleed.Equals(Fallback));
        TestFalse(TEXT("Poison tints away from the fallback"), Poison.Equals(Fallback));
        TestFalse(TEXT("Bleed and Poison are distinguishable"), Bleed.Equals(Poison));
        TestTrue(TEXT("An unmapped tag keeps the fallback"),
            BreakerFX::ColorForStatusTag(FGameplayTag(), Fallback).Equals(Fallback));
    }

    // --- Pool arithmetic, like the tracer's ----------------------------------
    TestTrue(TEXT("A 16-stroke ground ring fits the stroke pool twice over"),
        ABreakerEffectRenderer::GetStrokeSlots() >= 32);
    TestTrue(TEXT("The light pool is the small one"),
        ABreakerEffectRenderer::GetEffectLightSlots() < ABreakerEffectRenderer::GetGlowSlots());
    return true;
}

#endif
