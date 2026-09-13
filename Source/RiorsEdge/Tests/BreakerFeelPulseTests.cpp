#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/BreakerCharacter.h"
#include "Characters/BreakerFeelPulseMath.h"
#include "Game/BreakerGameMode.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "UObject/UnrealType.h"

// ---------------------------------------------------------------------------
// THE MOVEMENT-FEEL ENVELOPES (O283): the two shapes every camera and hand
// cue in the layer rides, proven world-free — no pawn, no camera, no clock.
// Plus the shipped configuration: the three movement Feel dials and the six
// cast Feel dials (O284) on the character the game actually spawns, read off
// the CDO the same way RiorsEdge.Playtest.
// Assembly reads its defaults. The dials are protected UPROPERTYs, so they
// are read through reflection rather than by widening the class for a test.
// ---------------------------------------------------------------------------
namespace
{
    // Reads one float UPROPERTY off an object by name. -1 when the property is
    // missing, which no Feel dial ships at (the movement dials are clamped
    // >= 0; the one negative dial, the cast's camera pitch, ships at -1.5),
    // so a rename fails loudly instead of passing on a default.
    float BreakerFeelPulseReadDial(const UObject* Object, const TCHAR* PropertyName)
    {
        if (!Object) return -1.0f;
        const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
        if (!Property) return -1.0f;
        return Property->GetPropertyValue_InContainer(Object);
    }

    // Walks a speed trace through BrakePlantEdge one sample at a time, the
    // way the character feeds it from consecutive frames, and counts plants.
    // The trace is the sequence of grounded 2D speeds; the flag is the
    // grounded-with-no-input state held for every sample.
    int32 BreakerFeelPulseCountPlants(const TArray<float>& Trace, float Threshold, bool bGroundedNoInput)
    {
        int32 Plants = 0;
        for (int32 Index = 1; Index < Trace.Num(); ++Index)
        {
            if (BreakerFeel::BrakePlantEdge(Trace[Index - 1], Trace[Index], Threshold, bGroundedNoInput)) ++Plants;
        }
        return Plants;
    }

    // A linear speed ramp from Start to End sampled every 10 ms over
    // Seconds, ends inclusive. A stop from the walk cap takes a few frames;
    // a 10 ms sample is finer than any frame the game ships.
    TArray<float> BreakerFeelPulseRamp(float Start, float End, float Seconds)
    {
        TArray<float> Trace;
        const int32 Steps = FMath::Max(1, FMath::RoundToInt(Seconds / 0.01f));
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            Trace.Add(FMath::Lerp(Start, End, static_cast<float>(Step) / static_cast<float>(Steps)));
        }
        return Trace;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFeelPulseTest,
    "RiorsEdge.Characters.FeelPulse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFeelPulseTest::RunTest(const FString& Parameters)
{
    // --- PulseAlpha: linear attack, squared ease-out, dead outside -----------
    const float Attack = 0.05f;
    const float Recovery = 0.30f;

    TestEqual(TEXT("A pulse that has not begun is zero"), BreakerFeel::PulseAlpha(-0.01f, Attack, Recovery), 0.0f);
    TestEqual(TEXT("A pulse far before its start is zero"), BreakerFeel::PulseAlpha(-100.0f, Attack, Recovery), 0.0f);
    TestEqual(TEXT("The first sample is the floor of the attack"), BreakerFeel::PulseAlpha(0.0f, Attack, Recovery), 0.0f);
    TestEqual(TEXT("The attack is linear: halfway up reads one half"),
        BreakerFeel::PulseAlpha(Attack * 0.5f, Attack, Recovery), 0.5f, 0.0001f);
    TestEqual(TEXT("The peak is exactly one, exactly at the attack"), BreakerFeel::PulseAlpha(Attack, Attack, Recovery), 1.0f);

    // Strictly decreasing over the whole recovery: the camera settles, it
    // never hitches back up. Sampled densely so a plateau anywhere is a red.
    {
        float Previous = BreakerFeel::PulseAlpha(Attack, Attack, Recovery);
        bool bStrictlyDecreasing = true;
        for (int32 Step = 1; Step <= 100; ++Step)
        {
            const float Elapsed = Attack + Recovery * (static_cast<float>(Step) / 100.0f);
            const float Current = BreakerFeel::PulseAlpha(Elapsed, Attack, Recovery);
            if (!(Current < Previous)) { bStrictlyDecreasing = false; break; }
            Previous = Current;
        }
        TestTrue(TEXT("The recovery is strictly decreasing from peak to rest"), bStrictlyDecreasing);
    }

    // The ease-out is (1 - a)^2 over the recovery fraction a.
    TestEqual(TEXT("A quarter into the recovery reads (0.75)^2"),
        BreakerFeel::PulseAlpha(Attack + Recovery * 0.25f, Attack, Recovery), 0.5625f, 0.0001f);
    TestEqual(TEXT("Halfway through the recovery reads (0.5)^2"),
        BreakerFeel::PulseAlpha(Attack + Recovery * 0.5f, Attack, Recovery), 0.25f, 0.0001f);
    TestEqual(TEXT("Three quarters in reads (0.25)^2"),
        BreakerFeel::PulseAlpha(Attack + Recovery * 0.75f, Attack, Recovery), 0.0625f, 0.0001f);

    TestEqual(TEXT("The pulse is spent exactly at attack plus recovery"),
        BreakerFeel::PulseAlpha(Attack + Recovery, Attack, Recovery), 0.0f, 0.0001f);
    TestEqual(TEXT("A spent pulse stays at zero"), BreakerFeel::PulseAlpha(Attack + Recovery + 0.01f, Attack, Recovery), 0.0f);
    TestEqual(TEXT("A long-spent pulse stays at zero"), BreakerFeel::PulseAlpha(100.0f, Attack, Recovery), 0.0f);

    // Degenerate dials never divide: a zero attack peaks on its first
    // positive sample, and a zero recovery is spent the sample after the peak.
    {
        const float ZeroAttackFirstSample = BreakerFeel::PulseAlpha(0.001f, 0.0f, Recovery);
        TestTrue(TEXT("A zero attack is finite"), FMath::IsFinite(ZeroAttackFirstSample));
        TestTrue(TEXT("A zero attack peaks on its first positive sample"), ZeroAttackFirstSample > 0.99f && ZeroAttackFirstSample <= 1.0f);
        const float ZeroRecoveryAfterPeak = BreakerFeel::PulseAlpha(Attack + 0.001f, Attack, 0.0f);
        TestTrue(TEXT("A zero recovery is finite"), FMath::IsFinite(ZeroRecoveryAfterPeak));
        TestEqual(TEXT("A zero recovery is spent the sample after the peak"), ZeroRecoveryAfterPeak, 0.0f);
    }
    // The envelope is bounded everywhere it can be sampled.
    for (float Elapsed = -0.1f; Elapsed <= 1.0f; Elapsed += 0.005f)
    {
        const float Alpha = BreakerFeel::PulseAlpha(Elapsed, Attack, Recovery);
        TestTrue(TEXT("The pulse never leaves [0, 1]"), Alpha >= 0.0f && Alpha <= 1.0f);
    }

    // --- EaseToward: an exponential chase that never overshoots ------------
    const float Tau = 0.12f;
    const float Dt = 0.01f;

    // Monotone toward the target from below, never past it.
    {
        float Value = 0.0f;
        const float Target = 100.0f;
        bool bMonotone = true;
        for (int32 Step = 0; Step < 200; ++Step)
        {
            const float Next = BreakerFeel::EaseToward(Value, Target, Tau, Dt);
            if (Next < Value || Next > Target) { bMonotone = false; break; }
            Value = Next;
        }
        TestTrue(TEXT("Rising toward a target is monotone and never overshoots"), bMonotone);
    }
    // And from above, the mirror (the crouch camera eases DOWN).
    {
        float Value = 48.0f;
        const float Target = 0.0f;
        bool bMonotone = true;
        for (int32 Step = 0; Step < 200; ++Step)
        {
            const float Next = BreakerFeel::EaseToward(Value, Target, Tau, Dt);
            if (Next > Value || Next < Target) { bMonotone = false; break; }
            Value = Next;
        }
        TestTrue(TEXT("Falling toward a target is monotone and never overshoots"), bMonotone);
    }
    // One tau closes the gap to 1/e; five tau leave under one percent.
    TestEqual(TEXT("One tau closes the gap to 1/e"),
        BreakerFeel::EaseToward(0.0f, 1.0f, Tau, Tau), 1.0f - FMath::Exp(-1.0f), 0.001f);
    {
        float Value = 0.0f;
        const float Target = 100.0f;
        const int32 Steps = FMath::RoundToInt(5.0f * Tau / Dt);   // exactly five tau of frames
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            Value = BreakerFeel::EaseToward(Value, Target, Tau, Dt);
        }
        TestTrue(TEXT("After five tau the value is within one percent of the target"),
            FMath::Abs(Target - Value) < 0.01f * Target);
    }
    // Frame-rate independent: two half-steps land where one whole step does.
    {
        const float Whole = BreakerFeel::EaseToward(0.0f, 100.0f, Tau, 2.0f * Dt);
        const float Halves = BreakerFeel::EaseToward(BreakerFeel::EaseToward(0.0f, 100.0f, Tau, Dt), 100.0f, Tau, Dt);
        TestEqual(TEXT("Two half-frames land where one whole frame does"), Halves, Whole, 0.01f);
    }
    // A non-positive tau is a snap, never a stall; so is a non-positive dt.
    TestEqual(TEXT("A zero tau returns the target"), BreakerFeel::EaseToward(0.0f, 100.0f, 0.0f, Dt), 100.0f);
    TestEqual(TEXT("A negative tau returns the target"), BreakerFeel::EaseToward(0.0f, 100.0f, -1.0f, Dt), 100.0f);
    TestEqual(TEXT("A zero dt returns the target"), BreakerFeel::EaseToward(0.0f, 100.0f, Tau, 0.0f), 100.0f);
    TestEqual(TEXT("Already at the target stays at the target"), BreakerFeel::EaseToward(100.0f, 100.0f, Tau, Dt), 100.0f);

    // --- Shipped configuration ---------------------------------------------
    // The C++ default, and the Blueprint the game mode actually spawns (the
    // Assembly test's loading idiom), so a Blueprint override of a Feel dial
    // is a red here on the day it lands rather than a surprise in a playtest.
    const ABreakerCharacter* CppDefaults = GetDefault<ABreakerCharacter>();
    TestEqual(TEXT("Sprint FOV push ships at 6 degrees"), BreakerFeelPulseReadDial(CppDefaults, TEXT("SprintFOVPushDegrees")), 6.0f, 0.0001f);
    TestEqual(TEXT("Crouch camera ease ships at 0.12 s"), BreakerFeelPulseReadDial(CppDefaults, TEXT("CrouchCameraEaseSeconds")), 0.12f, 0.0001f);
    TestEqual(TEXT("Brake plant threshold ships at half the walk cap"), BreakerFeelPulseReadDial(CppDefaults, TEXT("BrakePlantMinSpeedFraction")), 0.5f, 0.0001f);

    // --- BrakePlantEdge: a stop is one edge, never a level -------------------
    // The plant fires on the sample where a grounded, input-free speed
    // crosses from at-or-above the threshold to below it, and on no other.
    // The threshold is the shipped one: half the shipped walk cap, read off
    // the two CDOs rather than typed, so the trace below IS the game's stop.
    {
        const float WalkCap = BreakerFeelPulseReadDial(GetDefault<UBreakerCharacterMovementComponent>(), TEXT("WalkSpeed"));
        TestEqual(TEXT("The shipped walk cap is 672"), WalkCap, 672.0f, 0.0001f);
        const float Threshold = BreakerFeelPulseReadDial(CppDefaults, TEXT("BrakePlantMinSpeedFraction")) * WalkCap;
        TestEqual(TEXT("The shipped plant threshold is 336"), Threshold, 336.0f, 0.0001f);

        // A full stop from the walk cap, 672 -> 0 over 0.3 s at 10 ms: one plant.
        const TArray<float> Stop = BreakerFeelPulseRamp(WalkCap, 0.0f, 0.3f);
        TestEqual(TEXT("A 672 -> 0 stop plants exactly once"), BreakerFeelPulseCountPlants(Stop, Threshold, true), 1);
        // The same stop sampled coarsely still plants once: the edge is a
        // crossing, not a dwell, so the sample rate cannot double it.
        TestEqual(TEXT("A coarse-sampled stop plants exactly once"),
            BreakerFeelPulseCountPlants(BreakerFeelPulseRamp(WalkCap, 0.0f, 0.03f), Threshold, true), 1);
        // The dead frames after the stop: 0 -> 0 forever plants nothing more.
        TArray<float> Rest = Stop;
        for (int32 Frame = 0; Frame < 100; ++Frame) Rest.Add(0.0f);
        TestEqual(TEXT("Standing still after the stop adds no plant"), BreakerFeelPulseCountPlants(Rest, Threshold, true), 1);

        // A strafe reversal, 672 -> 400 -> 672: the speed dips but never
        // drops under 336, so no edge and no plant.
        TArray<float> Reversal = BreakerFeelPulseRamp(WalkCap, 400.0f, 0.1f);
        Reversal.Append(BreakerFeelPulseRamp(400.0f, WalkCap, 0.1f));
        TestEqual(TEXT("A strafe reversal that never drops under 336 plants zero times"),
            BreakerFeelPulseCountPlants(Reversal, Threshold, true), 0);

        // A shuffle, 300 -> 0: never at the threshold, so nothing to cross.
        TestEqual(TEXT("A 300 -> 0 shuffle plants zero times"),
            BreakerFeelPulseCountPlants(BreakerFeelPulseRamp(300.0f, 0.0f, 0.2f), Threshold, true), 0);

        // The single-sample edge, both sides of it.
        TestTrue(TEXT("Crossing 336 from above plants"), BreakerFeel::BrakePlantEdge(336.0f, 335.9f, Threshold, true));
        TestFalse(TEXT("Sitting at 336 does not plant"), BreakerFeel::BrakePlantEdge(336.0f, 336.0f, Threshold, true));
        TestFalse(TEXT("Rising through 336 does not plant"), BreakerFeel::BrakePlantEdge(335.9f, 336.0f, Threshold, true));
        TestFalse(TEXT("Already under 336 does not plant"), BreakerFeel::BrakePlantEdge(335.0f, 0.0f, Threshold, true));

        // Airborne, sliding, traversing, or still pushing the stick: the
        // caller's grounded-no-input flag is false and no trace plants.
        TestEqual(TEXT("A 672 -> 0 stop with input still live never plants"),
            BreakerFeelPulseCountPlants(Stop, Threshold, false), 0);
        TestFalse(TEXT("The bare edge never plants when not grounded-no-input"),
            BreakerFeel::BrakePlantEdge(WalkCap, 0.0f, Threshold, false));
    }

    // O284: the cast leaves the hand. Six dials on the same envelope shape:
    // a nod of the camera, a short attack and a longer recovery, a widening
    // of the field of view, and the viewmodel's kick. All O2 PLACEHOLDER on
    // the character; this pins them so a Blueprint override is a red.
    struct FBreakerFeelCastDial { const TCHAR* Name; float Value; };
    const FBreakerFeelCastDial CastDials[] = {
        { TEXT("CastCameraPitchDegrees"),   -1.5f },
        { TEXT("CastKickAttackSeconds"),     0.06f },
        { TEXT("CastKickRecoverySeconds"),   0.12f },
        { TEXT("CastFOVPulseDegrees"),       3.0f },
        { TEXT("CastFOVPulseSeconds"),       0.2f },
        { TEXT("CastViewmodelKickUnits"),    2.0f },
    };
    for (const FBreakerFeelCastDial& Dial : CastDials)
    {
        TestEqual(FString::Printf(TEXT("%s ships at %.2f"), Dial.Name, Dial.Value),
            BreakerFeelPulseReadDial(CppDefaults, Dial.Name), Dial.Value, 0.0001f);
    }

    const ABreakerGameMode* GameMode = GetDefault<ABreakerGameMode>();
    if (TestNotNull(TEXT("Playtest game mode has a pawn class"), GameMode->DefaultPawnClass.Get()))
    {
        const ABreakerCharacter* ShippedPawn = Cast<ABreakerCharacter>(GameMode->DefaultPawnClass->GetDefaultObject());
        if (TestNotNull(TEXT("Active pawn exposes Breaker defaults"), ShippedPawn))
        {
            TestEqual(TEXT("The shipped pawn's sprint FOV push is the C++ default"),
                BreakerFeelPulseReadDial(ShippedPawn, TEXT("SprintFOVPushDegrees")), 6.0f, 0.0001f);
            TestEqual(TEXT("The shipped pawn's crouch camera ease is the C++ default"),
                BreakerFeelPulseReadDial(ShippedPawn, TEXT("CrouchCameraEaseSeconds")), 0.12f, 0.0001f);
            TestEqual(TEXT("The shipped pawn's brake plant threshold is the C++ default"),
                BreakerFeelPulseReadDial(ShippedPawn, TEXT("BrakePlantMinSpeedFraction")), 0.5f, 0.0001f);
            // The dash punch rides the same envelope shape and is idle at rest.
            TestEqual(TEXT("The dash envelope is idle at rest"), ShippedPawn->GetDashFeedbackAlpha(), 0.0f);
            // O284: the cast dials on the pawn the game spawns.
            for (const FBreakerFeelCastDial& Dial : CastDials)
            {
                TestEqual(FString::Printf(TEXT("The shipped pawn's %s is the C++ default"), Dial.Name),
                    BreakerFeelPulseReadDial(ShippedPawn, Dial.Name), Dial.Value, 0.0001f);
            }
            // GAP: the cast pulse's at-rest value is not asserted here.
            // ABreakerCharacter exposes no public GetCastFOVPulseDegrees();
            // the pulse state is private to the actor and this test will not
            // widen the class to read it. Land the accessor beside the dials
            // and add `TestEqual(..., ShippedPawn->GetCastFOVPulseDegrees(), 0.0f)`.
        }
    }
    return true;
}

#endif
