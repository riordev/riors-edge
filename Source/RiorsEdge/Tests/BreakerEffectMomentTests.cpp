#include "Misc/AutomationTest.h"
#include "UI/BreakerEffectMomentMath.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"

#if WITH_DEV_AUTOMATION_TESTS

// The four moments (GLASS-1). What a headless suite can prove: the colour law
// is O179's, the four assets resolve to four distinct places under one
// directory, and the pooled fallback that stands in for an unauthored system
// has a schedule a frame can sample. Whether a Niagara system PLAYS is the
// capture harness's job once the owner has authored one.

namespace
{
    const EBreakerEffectMoment BreakerMomentAll[BreakerFX::EffectMomentCount] = {
        EBreakerEffectMoment::Muzzle, EBreakerEffectMoment::Impact,
        EBreakerEffectMoment::Cast, EBreakerEffectMoment::Death,
    };

    bool BreakerMomentSameColor(const FLinearColor& A, const FLinearColor& B)
    {
        return A.Equals(B, 1.0e-4f);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentColourLawTest,
    "RiorsEdge.UI.EffectMoment.ColourLaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentColourLawTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFX;

    // O179: weapon economy is the weapon/heat role.
    TestTrue(TEXT("Muzzle is Orange"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Muzzle, false), BreakerUI::Orange));
    TestTrue(TEXT("A muzzle never promotes to Gold"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Muzzle, true), BreakerUI::Orange));
    TestTrue(TEXT("Impact is Orange"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Impact, false), BreakerUI::Orange));
    // O179: the weak-point promise is the reward role.
    TestTrue(TEXT("A weak-point impact is Gold"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Impact, true), BreakerUI::Gold));
    // Death wears the kill confirm's colours (BreakerPlaytestHUD's crosshair
    // confirm): one event, one colour on the HUD and in the world.
    TestTrue(TEXT("Death is Harm"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Death, false), BreakerUI::Harm));
    TestTrue(TEXT("A weak-point kill is Gold"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Death, true), BreakerUI::Gold));
    // Cast's default is the player/system role; the site supplies the verb.
    TestTrue(TEXT("Cast defaults to Cyan"), BreakerMomentSameColor(MomentColor(EBreakerEffectMoment::Cast, false), BreakerUI::Cyan));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentAssetPathsTest,
    "RiorsEdge.UI.EffectMoment.AssetPathsDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentAssetPathsTest::RunTest(const FString& Parameters)
{
    TSet<FString> Seen;
    for (EBreakerEffectMoment Moment : BreakerMomentAll)
    {
        const FString Path = BreakerFX::MomentAssetPath(Moment);
        TestTrue(FString::Printf(TEXT("%s lives under /Game/Breaker/FX/"), *Path),
            Path.StartsWith(TEXT("/Game/Breaker/FX/NS_")));
        // Object path form: /Game/Dir/Name.Name, what LoadObject takes.
        const FString Name = BreakerFX::MomentAssetName(Moment);
        TestTrue(FString::Printf(TEXT("%s names its object"), *Path),
            Path.EndsWith(FString::Printf(TEXT("/%s.%s"), *Name, *Name)));
        TestFalse(FString::Printf(TEXT("%s is not shared with another moment"), *Path), Seen.Contains(Path));
        Seen.Add(Path);
    }
    TestEqual(TEXT("Four moments, four assets"), Seen.Num(), BreakerFX::EffectMomentCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEffectMomentFallbackTest,
    "RiorsEdge.UI.EffectMoment.FallbackSchedule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEffectMomentFallbackTest::RunTest(const FString& Parameters)
{
    using namespace BreakerFX;

    // The impact's fallback is the tracer renderer's spark, which already
    // exists; drawing a glow on top would double every hit.
    TestFalse(TEXT("Impact draws no second fallback"), MomentFallback(EBreakerEffectMoment::Impact).bDrawn);

    float MuzzleDuration = 0.0f;
    float LongestOther = 0.0f;
    for (EBreakerEffectMoment Moment : BreakerMomentAll)
    {
        const FMomentFallback F = MomentFallback(Moment);
        if (!F.bDrawn) continue;
        TestTrue(TEXT("A drawn fallback lasts"), F.Timing.DurationSeconds > 0.0f);
        TestTrue(TEXT("A drawn fallback has size"), F.RadiusCm >= 0.5f);   // the renderer hides anything smaller
        TestTrue(TEXT("A drawn fallback has light"), F.Intensity > 0.0f);
        TestTrue(TEXT("Fades fit inside the clip"),
            F.Timing.FadeInSeconds + F.Timing.FadeOutSeconds <= F.Timing.DurationSeconds + KINDA_SMALL_NUMBER);
        // A frame in the middle of the clip sees it.
        const FEffectSample Mid = SampleEffect(F.Timing, F.Timing.DurationSeconds * 0.5f);
        TestTrue(TEXT("Visible at half life"), Mid.bVisible && Mid.Alpha > 0.0f);
        // And a frame after it does not.
        TestTrue(TEXT("Gone after the clip"), SampleEffect(F.Timing, F.Timing.DurationSeconds + 0.001f).bFinished);
        // A blink light, when asked for, is a real light.
        if (F.LightRadiusCm > 0.0f) TestTrue(TEXT("A lit fallback has intensity"), F.LightIntensity > 0.0f);

        if (Moment == EBreakerEffectMoment::Muzzle) MuzzleDuration = F.Timing.DurationSeconds;
        else LongestOther = FMath::Max(LongestOther, F.Timing.DurationSeconds);
    }
    // A muzzle flash must be gone before the next round at any cadence the
    // weapon table ships. The fastest shipped weapon is 900 rpm (one round
    // every 0.0667 s); a flash that outlives that reads as a lamp, not a shot.
    // It is the shortest moment.
    TestTrue(TEXT("Muzzle is the shortest moment"), MuzzleDuration > 0.0f && MuzzleDuration < LongestOther);
    TestTrue(TEXT("Muzzle clears a 900 rpm cadence"), MuzzleDuration <= 60.0f / 900.0f + KINDA_SMALL_NUMBER);

    // Shipped configuration: the pools exist and the pending ring can hold a
    // full spread's landed pellets plus the kill they made.
    TestTrue(TEXT("Moment pool is not empty"), ABreakerEffectRenderer::GetMomentSlots() > 0);
    TestTrue(TEXT("Pending ring holds eight pellets and a death"), ABreakerEffectRenderer::GetPendingMomentSlots() >= 9);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
