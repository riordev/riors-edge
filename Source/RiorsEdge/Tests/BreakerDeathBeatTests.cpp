#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerDeathBeatMath.h"
#include "Characters/BreakerCharacter.h"

// THE DEATH BEAT (O193). Pure maths, no world: the rhythm of dying — weapon
// down, camera down and grey, black, teleport under the black, fade in with
// input on the first visible frame — has to be provable without dying.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDeathBeatTimelineTest,
    "RiorsEdge.Game.DeathBeatTimeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDeathBeatTimelineTest::RunTest(const FString& Parameters)
{
    using namespace BreakerDeathBeat;
    const FBreakerDeathBeatTimeline Timeline;   // the shipped defaults
    const float Lower = Timeline.LowerAndDropSeconds;
    const float WeaponLower = Timeline.WeaponLowerSeconds;
    const float TeleportAt = TeleportAtSeconds(Timeline);
    const float Total = TotalSeconds(Timeline);
    constexpr float Step = 1.0f / 120.0f;

    // (a) Phases run Falling -> Black -> FadeIn -> Done and never come back.
    {
        int32 LastPhase = static_cast<int32>(EBreakerDeathBeatPhase::Falling);
        bool bMonotone = true;
        bool bSawBlack = false, bSawFadeIn = false, bSawDone = false;
        const int32 Steps = FMath::CeilToInt(Total / Step) + 12;
        for (int32 i = 0; i <= Steps; ++i)
        {
            const FBreakerDeathBeatSample S = Sample(Timeline, i * Step);
            const int32 Phase = static_cast<int32>(S.Phase);
            if (Phase < LastPhase) bMonotone = false;
            LastPhase = Phase;
            bSawBlack |= S.Phase == EBreakerDeathBeatPhase::Black;
            bSawFadeIn |= S.Phase == EBreakerDeathBeatPhase::FadeIn;
            bSawDone |= S.Phase == EBreakerDeathBeatPhase::Done;
        }
        TestTrue(TEXT("Phases never revisit an earlier phase"), bMonotone);
        TestTrue(TEXT("The sweep visits Black"), bSawBlack);
        TestTrue(TEXT("The sweep visits FadeIn"), bSawFadeIn);
        TestTrue(TEXT("The sweep ends Done"), bSawDone);
        TestEqual(TEXT("Elapsed 0 is Falling"),
            static_cast<int32>(Sample(Timeline, 0.0f).Phase), static_cast<int32>(EBreakerDeathBeatPhase::Falling));
    }

    // (b) The first frame of the beat: nothing has moved yet, input is already dead.
    {
        const FBreakerDeathBeatSample S = Sample(Timeline, 0.0f);
        TestEqual(TEXT("Elapsed 0: weapon at ready"), S.WeaponLowerFraction, 0.0f);
        TestEqual(TEXT("Elapsed 0: no camera drop"), S.CameraDropCm, 0.0f);
        TestEqual(TEXT("Elapsed 0: full colour"), S.Saturation, 1.0f);
        TestEqual(TEXT("Elapsed 0: no fade"), S.FadeAlpha, 0.0f);
        TestFalse(TEXT("Elapsed 0: input off"), S.bInputEnabled);
        TestTrue(TEXT("Elapsed 0: HUD visible"), S.bHudVisible);
    }

    // (c) The end of the fall: everything has arrived and the frame is black.
    {
        const FBreakerDeathBeatSample S = Sample(Timeline, Lower);
        TestEqual(TEXT("At Lower: weapon fully lowered"), S.WeaponLowerFraction, 1.0f);
        TestEqual(TEXT("At Lower: camera at full drop"), S.CameraDropCm, Timeline.CameraDropCm);
        TestEqual(TEXT("At Lower: colour gone"), S.Saturation, 0.0f);
        TestEqual(TEXT("At Lower: fade complete"), S.FadeAlpha, 1.0f);
        TestEqual(TEXT("At Lower: the phase is Black"),
            static_cast<int32>(S.Phase), static_cast<int32>(EBreakerDeathBeatPhase::Black));
        // And the fall itself is a monotone glide, not a snap.
        float LastLower = -1.0f;
        bool bLowerMonotone = true;
        for (float T = 0.0f; T < Lower; T += Step)
        {
            const float L = Sample(Timeline, T).WeaponLowerFraction;
            if (L < LastLower) bLowerMonotone = false;
            LastLower = L;
        }
        TestTrue(TEXT("Falling: the weapon only ever lowers"), bLowerMonotone);
        // The weapon runs on its own short clock and finishes first.
        TestTrue(TEXT("Falling: mid-lower is neither ready nor holstered"),
            Sample(Timeline, WeaponLower * 0.5f).WeaponLowerFraction > 0.0f
            && Sample(Timeline, WeaponLower * 0.5f).WeaponLowerFraction < 1.0f);
        TestEqual(TEXT("At WeaponLower: weapon holstered"),
            Sample(Timeline, WeaponLower).WeaponLowerFraction, 1.0f);
        TestTrue(TEXT("At WeaponLower: the camera drop is still running"),
            Sample(Timeline, WeaponLower).CameraDropCm < Timeline.CameraDropCm);
        // The cut to black is hard: no fade anywhere in the fall.
        bool bNoFadeInFall = true;
        for (float T = 0.0f; T < Lower; T += Step)
        {
            if (Sample(Timeline, T).FadeAlpha != 0.0f) bNoFadeInFall = false;
        }
        TestTrue(TEXT("Falling: fade stays 0 for the whole fall"), bNoFadeInFall);
    }

    // (d) Black: full fade, input off, HUD hidden, for the whole stretch.
    {
        bool bAllBlack = true;
        for (float T = Lower; T < TeleportAt - UE_KINDA_SMALL_NUMBER; T += Step)
        {
            const FBreakerDeathBeatSample S = Sample(Timeline, T);
            if (S.Phase != EBreakerDeathBeatPhase::Black || S.FadeAlpha != 1.0f || S.bInputEnabled || S.bHudVisible)
            {
                bAllBlack = false;
            }
        }
        TestTrue(TEXT("Black: fade 1, input off, HUD hidden throughout"), bAllBlack);
    }

    // (e) The first FadeIn sample: input and HUD return with the world. The
    // fade is exactly 1 at the phase boundary — this is the black frame the
    // teleport lands under — and strictly lifting from the next step on.
    {
        const FBreakerDeathBeatSample First = Sample(Timeline, TeleportAt);
        TestEqual(TEXT("First FadeIn sample is FadeIn"),
            static_cast<int32>(First.Phase), static_cast<int32>(EBreakerDeathBeatPhase::FadeIn));
        TestTrue(TEXT("First FadeIn sample: input on"), First.bInputEnabled);
        TestTrue(TEXT("First FadeIn sample: HUD visible"), First.bHudVisible);
        TestTrue(TEXT("First FadeIn sample: fade no darker than black"), First.FadeAlpha <= 1.0f);
        TestEqual(TEXT("First FadeIn sample: camera at rest"), First.CameraDropCm, 0.0f);
        TestEqual(TEXT("First FadeIn sample: weapon at ready"), First.WeaponLowerFraction, 0.0f);
        TestEqual(TEXT("First FadeIn sample: full colour"), First.Saturation, 1.0f);
        const FBreakerDeathBeatSample Next = Sample(Timeline, TeleportAt + Step);
        TestTrue(TEXT("One frame into FadeIn the black is lifting"), Next.FadeAlpha < 1.0f);
        TestTrue(TEXT("One frame into FadeIn the world is not yet fully back"), Next.FadeAlpha > 0.0f);
    }

    // (f) The teleport instant is the end of the black and the first FadeIn time.
    TestEqual(TEXT("TeleportAt == Lower + Black"), TeleportAt, Timeline.LowerAndDropSeconds + Timeline.BlackSeconds);
    TestEqual(TEXT("The instant before TeleportAt is still Black"),
        static_cast<int32>(Sample(Timeline, TeleportAt - Step).Phase), static_cast<int32>(EBreakerDeathBeatPhase::Black));

    // (g) The dead time stays a rhythm, not a punishment.
    TestTrue(TEXT("Lower + Black within [1.5, 3.0] s"), TeleportAt >= 1.5f && TeleportAt <= 3.0f);

    // (h) After Done, every channel is at rest and input is on.
    {
        const FBreakerDeathBeatSample S = Sample(Timeline, Total + Step);
        TestEqual(TEXT("Done phase"), static_cast<int32>(S.Phase), static_cast<int32>(EBreakerDeathBeatPhase::Done));
        TestEqual(TEXT("Done: weapon at ready"), S.WeaponLowerFraction, 0.0f);
        TestEqual(TEXT("Done: no drop"), S.CameraDropCm, 0.0f);
        TestEqual(TEXT("Done: no roll"), S.CameraRollDegrees, 0.0f);
        TestEqual(TEXT("Done: no pitch"), S.CameraPitchDegrees, 0.0f);
        TestEqual(TEXT("Done: full colour"), S.Saturation, 1.0f);
        TestEqual(TEXT("Done: no fade"), S.FadeAlpha, 0.0f);
        TestTrue(TEXT("Done: input on"), S.bInputEnabled);
        TestTrue(TEXT("Done: HUD visible"), S.bHudVisible);
        // A negative clock is "no beat": the rest state, byte for byte.
        const FBreakerDeathBeatSample Idle = Sample(Timeline, -1.0f);
        TestEqual(TEXT("Idle: Done"), static_cast<int32>(Idle.Phase), static_cast<int32>(EBreakerDeathBeatPhase::Done));
        TestEqual(TEXT("Idle: weapon at ready"), Idle.WeaponLowerFraction, 0.0f);
        TestTrue(TEXT("Idle: input on"), Idle.bInputEnabled);
    }

    // (i) Shipped configuration: the character's CDO carries the spec's
    // timeline (O193, O202), field by field, as literals.
    {
        const FBreakerDeathBeatTimeline& Shipped = GetDefault<ABreakerCharacter>()->DeathBeat;
        TestEqual(TEXT("Shipped WeaponLowerSeconds == 0.3"), Shipped.WeaponLowerSeconds, 0.3f);
        TestEqual(TEXT("Shipped LowerAndDropSeconds == 0.8"), Shipped.LowerAndDropSeconds, 0.8f);
        TestEqual(TEXT("Shipped BlackSeconds == 1.2"), Shipped.BlackSeconds, 1.2f);
        TestEqual(TEXT("Shipped FadeInSeconds == 0.4"), Shipped.FadeInSeconds, 0.4f);
        TestEqual(TEXT("Shipped CameraDropCm == 60"), Shipped.CameraDropCm, 60.0f);
        TestEqual(TEXT("Shipped CameraPitchDegrees == -12"), Shipped.CameraPitchDegrees, -12.0f);
        TestEqual(TEXT("Shipped CameraRollDegrees matches the default"), Shipped.CameraRollDegrees, Timeline.CameraRollDegrees);
        TestEqual(TEXT("Shipped TeleportAt == 2.0"), TeleportAtSeconds(Shipped), 2.0f);
        TestEqual(TEXT("Shipped Total == 2.4"), TotalSeconds(Shipped), 2.4f);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
