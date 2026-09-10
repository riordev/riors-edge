#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/BreakerFlinchMath.h"

// ---------------------------------------------------------------------------
// THE HITCH.
//
// Owner: "enemies should stagger or flinch when shot". What a test can hold is
// the property that decides whether this is a reaction or a bug: the body must
// return to EXACTLY where it started, however many hits land and however fast.
// A hitch that accumulates walks a mesh off its own capsule, and it would do it
// slowly enough to look like a different bug entirely.
//
// What it cannot hold is whether being shot now feels like it lands.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFlinchTest,
    "RiorsEdge.Combat.Flinch.ReturnsToRest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFlinchTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFlinch;

    // ---- INSTANT, THEN GONE ----------------------------------------------
    // A hitch that eases IN is a lean, not a hit. Full at the instant of
    // impact is the whole difference.
    TestEqual(TEXT("the hitch is full at the instant of the hit"),
        Amount(0.0f, DurationSeconds), 1.0f, 0.001f);
    TestEqual(TEXT("and exactly zero when the clock runs out"),
        Amount(DurationSeconds, DurationSeconds), 0.0f);
    TestEqual(TEXT("and stays zero past it, so it cannot be revived by a late tick"),
        Amount(DurationSeconds * 40.0f, DurationSeconds), 0.0f);
    TestEqual(TEXT("a hit that has not happened moves nothing"),
        Amount(-0.5f, DurationSeconds), 0.0f);
    // A zeroed dial turns the effect off rather than dividing by nothing.
    TestEqual(TEXT("a zero duration draws no hitch"), Amount(0.0f, 0.0f), 0.0f);
    TestEqual(TEXT("and a negative one does not either"), Amount(0.0f, -1.0f), 0.0f);

    // ---- IT ONLY FALLS ----------------------------------------------------
    // A hitch that came back would read as two hits from one round.
    float Previous = Amount(0.0f, DurationSeconds);
    for (int32 Step = 1; Step <= 60; ++Step)
    {
        const float Elapsed = DurationSeconds * Step / 60.0f;
        const float Here = Amount(Elapsed, DurationSeconds);
        if (!TestTrue(*FString::Printf(TEXT("the hitch only settles, at %.4fs"), Elapsed),
            Here <= Previous + KINDA_SMALL_NUMBER)) return false;
        TestTrue(TEXT("and never leaves the unit range"), Here >= 0.0f && Here <= 1.0f);
        Previous = Here;
    }

    // ---- THE POSE RETURNS TO EXACTLY REST ---------------------------------
    // The one that matters. At and past the duration the offset and the pitch
    // are not "small", they are ZERO — the component writes the captured base
    // pose rather than nudging toward it, and this is the arithmetic that
    // makes that write correct.
    for (const FVector& Direction : { FVector(1, 0, 0), FVector(-0.4f, 0.9f, 0.0f), FVector::ZeroVector })
    {
        const float Settled = Amount(DurationSeconds, DurationSeconds);
        TestTrue(TEXT("a settled hitch offsets nothing"),
            Offset(Direction, Settled, ReachCm).IsNearlyZero());
        TestEqual(TEXT("and rocks nothing"), Pitch(Settled, PitchDegrees), 0.0f);
    }

    // ---- IT CANNOT REACH FURTHER THAN ITS OWN DIAL ------------------------
    // Bounded at every point on the curve, for every direction, including the
    // weak-point multiplier — which is the largest this can ever get.
    const float Largest = ReachCm * WeakPointScale;
    for (int32 Step = 0; Step <= 60; ++Step)
    {
        const float Elapsed = DurationSeconds * Step / 60.0f;
        const float Scaled = Amount(Elapsed, DurationSeconds) * WeakPointScale;
        const FVector Moved = Offset(FVector(0.6f, -0.8f, 0.0f), Scaled, ReachCm);
        // The down-lean adds to the length, so the bound is the planar reach
        // times the diagonal it can make with it rather than the reach alone.
        if (!TestTrue(*FString::Printf(TEXT("the hitch stays inside its reach at %.4fs (%.2f cm)"),
                Elapsed, static_cast<float>(Moved.Size())),
            static_cast<float>(Moved.Size()) <= Largest * 1.05f)) return false;
        TestTrue(TEXT("and inside its rock"),
            FMath::Abs(Pitch(Scaled, PitchDegrees)) <= PitchDegrees * WeakPointScale + KINDA_SMALL_NUMBER);
    }

    // ---- BACK ALONG THE SHOT, AND DOWN ------------------------------------
    // A struck thing drops into the blow. A hitch that lifted would read as
    // the body being launched, which is a different verb.
    {
        const FVector Moved = Offset(FVector(1.0f, 0.0f, 0.0f), 1.0f, ReachCm);
        TestTrue(TEXT("the body moves away from the shooter"), Moved.X > 0.0f);
        TestTrue(TEXT("and settles downward, not upward"), Moved.Z < 0.0f);
        // The direction is normalised, so a caller handing in an unnormalised
        // vector cannot make the hitch bigger than the dial allows.
        const FVector Long = Offset(FVector(40.0f, 0.0f, 0.0f), 1.0f, ReachCm);
        TestEqual(TEXT("a long direction vector does not lengthen the hitch"),
            static_cast<float>(Long.Size()), static_cast<float>(Moved.Size()), 0.001f);
    }
    // A weak point is worth more of the same shape, not a different one.
    TestTrue(TEXT("a weak point rocks the body harder"), WeakPointScale > 1.0f);

    // ---- SHORTER THAN THE INTERVAL IT ARRIVES ON --------------------------
    // Sustained fire lands four or five rounds a second. A hitch that outlived
    // its own interval would stop being a reaction and become a wobble.
    TestTrue(TEXT("the hitch is over before the next round lands"), DurationSeconds < 0.2f);
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
