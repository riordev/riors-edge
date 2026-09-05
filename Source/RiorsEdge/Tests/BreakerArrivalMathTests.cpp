#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerArrivalMath.h"
#include "Game/BreakerGameInstance.h"

// The arrival gate: every travel arrives under a cover, and the cover lifts
// only once the destination has rendered enough frames AND enough seconds,
// then fades. Proven without a world; the shipped figures are read from the
// one source the runtime reads.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerArrivalSettleHoldTest,
    "RiorsEdge.Game.Arrival.SettleHold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerArrivalSettleHoldTest::RunTest(const FString& Parameters)
{
    const FBreakerArrivalHold& H = UBreakerGameInstance::ShippedArrivalHold();
    const FBreakerArrivalHold Defaults;

    // Shipped configuration.
    TestTrue(TEXT("The frame gate is real (MinSettleFrames > 0)"), H.MinSettleFrames > 0);
    TestTrue(TEXT("The seconds gate is real (MinHoldSeconds > 0)"), H.MinHoldSeconds > 0.0f);
    TestTrue(TEXT("The reveal is a fade, not a cut (FadeInSeconds > 0)"), H.FadeInSeconds > 0.0f);
    TestEqual(TEXT("The seconds gate is the briefing's arrive hold, 0.9"), H.MinHoldSeconds, 0.9f, 1e-4f);
    TestEqual(TEXT("Shipped MinHoldSeconds is the default-constructed value"), H.MinHoldSeconds, Defaults.MinHoldSeconds, 1e-6f);
    TestEqual(TEXT("Shipped MinSettleFrames is the default-constructed value"), H.MinSettleFrames, Defaults.MinSettleFrames);
    TestEqual(TEXT("Shipped FadeInSeconds is the default-constructed value"), H.FadeInSeconds, Defaults.FadeInSeconds, 1e-6f);

    const uint64 Frames = static_cast<uint64>(H.MinSettleFrames);

    // The gates.
    TestEqual(TEXT("No world, no reveal, however long or however many frames"),
        BreakerArrivalRevealAlpha(false, 1e9, static_cast<uint64>(1e9), H), 0.0f);
    TestEqual(TEXT("Seconds met, frames one short: still covered"),
        BreakerArrivalRevealAlpha(true, H.MinHoldSeconds * 2.0, Frames - 1, H), 0.0f);
    TestEqual(TEXT("Frames met, seconds half way: still covered"),
        BreakerArrivalRevealAlpha(true, H.MinHoldSeconds * 0.5, static_cast<uint64>(1e6), H), 0.0f);
    TestEqual(TEXT("Both gates met exactly: the ramp starts at 0"),
        BreakerArrivalRevealAlpha(true, H.MinHoldSeconds, Frames, H), 0.0f, 1e-6f);
    TestEqual(TEXT("A full fade after both gates: the cover is gone"),
        BreakerArrivalRevealAlpha(true, static_cast<double>(H.MinHoldSeconds) + H.FadeInSeconds, Frames, H), 1.0f, 1e-6f);
    TestEqual(TEXT("Half the fade reveals half the world (linear)"),
        BreakerArrivalRevealAlpha(true, static_cast<double>(H.MinHoldSeconds) + H.FadeInSeconds * 0.5, Frames, H), 0.5f, 1e-4f);
    TestEqual(TEXT("Long after the fade the alpha stays at 1"),
        BreakerArrivalRevealAlpha(true, 1e6, static_cast<uint64>(1e6), H), 1.0f);

    // Monotone non-decreasing over a sweep of the clock with frames met, and
    // over a sweep of frames with the clock met.
    {
        float Last = 0.0f;
        bool bMonotone = true;
        const double End = static_cast<double>(H.MinHoldSeconds) + H.FadeInSeconds * 2.0;
        for (double T = 0.0; T <= End; T += End / 400.0)
        {
            const float A = BreakerArrivalRevealAlpha(true, T, Frames, H);
            if (A + 1e-7f < Last) { bMonotone = false; break; }
            Last = A;
        }
        TestTrue(TEXT("Alpha never decreases as the clock advances"), bMonotone);
    }
    {
        float Last = 0.0f;
        bool bMonotone = true;
        const double Clock = static_cast<double>(H.MinHoldSeconds) + H.FadeInSeconds;
        for (uint64 F = 0; F <= Frames * 2; ++F)
        {
            const float A = BreakerArrivalRevealAlpha(true, Clock, F, H);
            if (A + 1e-7f < Last) { bMonotone = false; break; }
            Last = A;
        }
        TestTrue(TEXT("Alpha never decreases as frames accumulate"), bMonotone);
    }

    // A frame-starved machine: the frame gate lands after the clock has run
    // past the fade. The reveal is then immediate — a compressed fade, never
    // an early reveal.
    TestEqual(TEXT("Frames met late: covered on the frame before the gate"),
        BreakerArrivalRevealAlpha(true, 10.0, Frames - 1, H), 0.0f);
    TestEqual(TEXT("Frames met late: revealed on the frame of the gate"),
        BreakerArrivalRevealAlpha(true, 10.0, Frames, H), 1.0f);

    return true;
}

#endif
