#include "Misc/AutomationTest.h"
#include "Characters/BreakerShakeMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// The trauma model's arithmetic: what a headless run can prove about a
// camera effect — bounds, decay, the squared perceptual ramp, and that zero
// trauma is exactly zero offset (the net-zero aim guarantee rests on it).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerShakeMathTest,
    "RiorsEdge.Characters.CameraShake.TraumaModelBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerShakeMathTest::RunTest(const FString& Parameters)
{
    // --- Trauma is a clamped accumulator with linear drain -------------------
    TestEqual(TEXT("Trauma never exceeds one"), BreakerShake::AddTrauma(0.9f, 0.5f), 1.0f);
    TestEqual(TEXT("Negative adds are refused"), BreakerShake::AddTrauma(0.4f, -1.0f), 0.4f);
    TestTrue(TEXT("Decay drains linearly"),
        FMath::IsNearlyEqual(BreakerShake::DecayTrauma(1.0f, 2.0f, 0.25f), 0.5f));
    TestEqual(TEXT("Decay floors at zero"), BreakerShake::DecayTrauma(0.1f, 2.0f, 1.0f), 0.0f);

    // --- The squared ramp ----------------------------------------------------
    TestTrue(TEXT("Half trauma is a quarter amplitude"),
        FMath::IsNearlyEqual(BreakerShake::ShakeAmplitude(0.5f), 0.25f));

    // --- The offset is bounded and dies exactly at zero ----------------------
    TestTrue(TEXT("Zero trauma is exactly zero offset"),
        BreakerShake::ShakeOffset(0.0f, 12.34, 18.0f, 0.5f, 0.4f).IsZero());
    for (float T = 0.0f; T < 5.0f; T += 0.037f)
    {
        const FRotator Offset = BreakerShake::ShakeOffset(1.0f, T, 18.0f, 0.5f, 0.4f);
        TestTrue(TEXT("Pitch stays inside its ceiling"), FMath::Abs(Offset.Pitch) <= 0.5f + KINDA_SMALL_NUMBER);
        TestTrue(TEXT("Yaw stays inside its ceiling"), FMath::Abs(Offset.Yaw) <= 0.4f + KINDA_SMALL_NUMBER);
        TestEqual(TEXT("Roll is never touched"), Offset.Roll, 0.0);
    }

    // --- The profile's shake scale is a plain multiplier on trauma ----------
    // ABreakerCharacter::UpdateCameraShake feeds ShakeTrauma * cvar * setting;
    // a setting of 1.0 (the shipped default) must be byte-identical to the
    // unscaled call, and 0 must be exactly zero offset — the setting turns
    // shake off, it does not leave a residue.
    const FRotator Unscaled = BreakerShake::ShakeOffset(0.6f, 2.5, 18.0f, 0.5f, 0.4f);
    const FRotator Identity = BreakerShake::ShakeOffset(0.6f * 1.0f, 2.5, 18.0f, 0.5f, 0.4f);
    TestEqual(TEXT("A shake scale of 1.0 is the unscaled shake"), Identity, Unscaled);
    TestTrue(TEXT("A shake scale of 0 is exactly no shake"),
        BreakerShake::ShakeOffset(0.6f * 0.0f, 2.5, 18.0f, 0.5f, 0.4f).IsZero());
    return true;
}

#endif
