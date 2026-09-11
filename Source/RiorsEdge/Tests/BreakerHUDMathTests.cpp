#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerHarmPresentationMath.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerWaveBudget.h"
#include "UI/BreakerDamageFeed.h"
#include "Progression/BreakerExperience.h"
#include "UI/BreakerHUDMath.h"
#include "UI/BreakerHUDResourceRow.h"
#include "UI/BreakerUIStyle.h"

// The combat HUD's drawing cannot be tested — no viewport, no way to assert a
// mark reads. Its TIMELINES can: every state rule the HUD sheet states is a
// pure function in UI/BreakerHUDMath.h, and these walk each one over its
// domain. The first test pins the shipped configuration against the tokens
// and the default-constructed attribute set, so a retune that disagrees with
// a ruling fails here rather than in a screenshot.

namespace
{
    constexpr float BreakerHUDMathTolerance = 0.001f;
}

// --------------------------------------------------------------------------
// HUD.ShippedTokens: the rulings, as numbers.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDShippedTokensTest,
    "RiorsEdge.UI.HUD.ShippedTokens",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDShippedTokensTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUI;

    // O199: the vitals row, against the default-constructed attribute set.
    const UBreakerAttributeSet* Defaults = GetDefault<UBreakerAttributeSet>();
    if (!TestNotNull(TEXT("The attribute set has a default object"), Defaults)) return false;
    {
        const FVitalsRow Row = FormatVitalsRow(Defaults->GetShield(), Defaults->GetMaxShield(),
            Defaults->GetHealth(), Defaults->GetMaxHealth());
        TestFalse(TEXT("A fresh character draws no shield"), Row.bDrawShield);
        TestEqual(TEXT("The health max reads after the value"), Row.MaxText, FormatTicker(Defaults->GetMaxHealth()));
        TestEqual(TEXT("The default max is 100"), Row.MaxText, FString(TEXT("100")));
    }
    TestEqual(TEXT("The max sits in the small numeric"), HudVitalsMaxPixels, 13.0f);
    TestEqual(TEXT("The value sits in the large numeric"), HudVitalsValuePixels, 32.0f);

    // THE PLAY SIZES, RE-MEASURED BY PLAY. O207 pinned 26/52 from the owner
    // calling them too big twice; a second playtest called them too big again
    // and named the reference — "look at how destiny does damage numbers and
    // replicate something similar" — so 18/34 supersedes it. Only another
    // playtest can move these, which is why they are pinned at all.
    TestEqual(TEXT("Body damage number 18"), DamageBodyPixels, 18.0f);
    TestEqual(TEXT("Crit damage number 34"), DamageCritPixels, 34.0f);
    // AND THE HIERARCHY IS THE POINT, not the absolute sizes: the ratios are
    // what tell a body shot from a weak point from a crit, so they are asserted
    // as ratios and cannot be lost to a future uniform shrink.
    TestTrue(TEXT("A weak point reads bigger than a body shot"),
        DamageWeakPointPixels > DamageBodyPixels * 1.3f);
    TestTrue(TEXT("A crit reads bigger than a weak point"),
        DamageCritPixels > DamageWeakPointPixels * 1.25f);
    TestTrue(TEXT("A damage-over-time tick reads smaller than a body shot"),
        DamageDoTPixels < DamageBodyPixels);
    // Owner: "only used when in effective ranges". A yard is 106 m long, so the
    // gate has to sit well inside one or it gates nothing.
    TestTrue(TEXT("Numbers stop being drawn well inside a yard"),
        DamageMaxDrawDistanceCm > 2000.0f && DamageMaxDrawDistanceCm < 8000.0f);
    TestEqual(TEXT("Magazine 32"), HudMagazinePixels, 32.0f);
    TestEqual(TEXT("Reserve 15"), HudReservePixels, 15.0f);
    TestEqual(TEXT("Direct merge window 120 ms"), BreakerDamageFeed::DirectMergeWindow, 0.12f);

    // O179: cyan is the movement verb and nothing else; chrome is bone.
    TestTrue(TEXT("Cyan names the movement verb"), Cyan.Equals(VerbMove, BreakerHUDMathTolerance));
    TestFalse(TEXT("System bone is not the movement verb"), System.Equals(VerbMove, BreakerHUDMathTolerance));
    TestTrue(TEXT("System bone is not a hue"),
        FMath::Abs(System.R - System.G) < 0.1f && FMath::Abs(System.G - System.B) < 0.15f);

    // The teal law: the top rarity IS the teal noun, and the name step is
    // reserved with it.
    TestTrue(TEXT("Unwritten rarity is teal-1"), RarityUnwritten.Equals(TealUnwritten, BreakerHUDMathTolerance));
    TestTrue(TEXT("Teal-1 is 0x2FBFA6"), TealUnwritten.Equals(Hex(0x2FBFA6), BreakerHUDMathTolerance));
    TestTrue(TEXT("The name teal is reserved"), IsReservedTeal(TealName));
    TestFalse(TEXT("System bone is not reserved teal"), IsReservedTeal(System));
    TestFalse(TEXT("The movement verb is not reserved teal"), IsReservedTeal(VerbMove));

    // The grid and the rails.
    TestEqual(TEXT("Safe gutter 40"), HudSafeMargin, 40.0f);
    TestEqual(TEXT("Identity rail 4"), HudRailIdentity, 4.0f);
    TestEqual(TEXT("Status rail 2"), HudRailStatus, 2.0f);
    TestEqual(TEXT("The hatch is 6 of 8"), HudHatchPeriod - HudHatchStripe, 2.0f);

    // The damage-number timeline's numbers.
    TestEqual(TEXT("Hold 100 ms"), MotionDamageHold, 0.10f);
    TestEqual(TEXT("Settle 120 ms"), MotionDamageSettle, 0.12f);
    TestEqual(TEXT("Rise 700 ms"), MotionDamageRise, 0.70f);
    TestEqual(TEXT("Fade 300 ms"), MotionDamageFade, 0.30f);
    TestEqual(TEXT("Crit hold 400 ms"), MotionCritHold, 0.40f);
    TestEqual(TEXT("Rise 24 px"), DamageRisePixels, 24.0f);
    // The snap-in and the stroke (owner, 2026-09-11: "more oomph").
    TestEqual(TEXT("A number is born at 1.35"), DamagePopScale, 1.35f);
    TestTrue(TEXT("A crit is born larger than a body hit"), DamageCritPopScale > DamagePopScale);
    TestEqual(TEXT("The outline is a tenth of the glyph"), DamageOutlineFraction, 0.10f);

    // The hit tell: harm, outside the spread ticks' tips, gone in 600 ms.
    TestTrue(TEXT("The hit tell is harm"), HudHitTell.Equals(Harm, BreakerHUDMathTolerance));
    TestTrue(TEXT("The hit tell clears the open crosshair"),
        HudHitTellRadius > HudCrosshairGapSpread + HudCrosshairTickLength);
    TestEqual(TEXT("The hit tell fades over 600 ms"), HudHitTellSeconds, 0.6f);

    // The tracker's distance sits under the beat line, smaller than it.
    TestTrue(TEXT("The distance line is smaller than the beat line"), HudQuestDistancePixels < HudQuestLinePixels);
    return true;
}

// --------------------------------------------------------------------------
// HUD.HitTell: where a hit came from, on the crosshair's ring.
//
// Owner, playtest 2026-09-11: "no indicator that I'm taking damage from
// behind". The bearing is world yaw from the camera to the source; on screen
// it is that yaw less the camera's, so behind is ±180 whichever way the
// player is facing. Each tell fades over HudHitTellSeconds; a second hit
// from the same direction refreshes the tell rather than stacking a new one,
// and a fifth distinct direction takes the oldest's slot.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDHitTellTest,
    "RiorsEdge.UI.HUD.HitTell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDHitTellTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    using namespace BreakerUI;

    // --- THE BEARING -----------------------------------------------------------
    const FVector Camera = FVector::ZeroVector;
    TestEqual(TEXT("A source at -X is behind a camera at yaw 0"),
        HitTellScreenDegrees(HitWorldYaw(Camera, FVector(-100.0f, 0.0f, 0.0f)), 0.0f), 180.0f, BreakerHUDMathTolerance);
    TestEqual(TEXT("A source at +Y is to the right of a camera at yaw 0"),
        HitTellScreenDegrees(HitWorldYaw(Camera, FVector(0.0f, 100.0f, 0.0f)), 0.0f), 90.0f, BreakerHUDMathTolerance);
    TestEqual(TEXT("A source at +X is ahead of a camera at yaw 0"),
        HitTellScreenDegrees(HitWorldYaw(Camera, FVector(100.0f, 0.0f, 0.0f)), 0.0f), 0.0f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Turn to yaw 90 and the source at +X is on the left"),
        HitTellScreenDegrees(HitWorldYaw(Camera, FVector(100.0f, 0.0f, 0.0f)), 90.0f), -90.0f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Height does not bend the bearing"),
        HitWorldYaw(Camera, FVector(0.0f, 100.0f, 500.0f)), 90.0f, BreakerHUDMathTolerance);
    // The arc's direction on the canvas, y down: ahead is up, behind is down.
    TestTrue(TEXT("Ahead points up"), HitTellArcPoint(0.0f).Equals(FVector2D(0.0f, -1.0f), BreakerHUDMathTolerance));
    TestTrue(TEXT("Right points right"), HitTellArcPoint(90.0f).Equals(FVector2D(1.0f, 0.0f), BreakerHUDMathTolerance));
    TestTrue(TEXT("Behind points down"), HitTellArcPoint(180.0f).Equals(FVector2D(0.0f, 1.0f), BreakerHUDMathTolerance));

    // --- THE FADE ----------------------------------------------------------------
    TestEqual(TEXT("Full at the hit"), HitTellAlpha(0.0f), 1.0f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Half at 300 ms"), HitTellAlpha(0.3f), 0.5f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Gone at 600 ms"), HitTellAlpha(0.6f), 0.0f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Before its own time is nothing"), HitTellAlpha(-0.1f), 0.0f);
    TestEqual(TEXT("A never-struck tell is nothing"), HitTellAlpha(static_cast<float>(10.0 - FHitTell().Time)), 0.0f);

    // --- MERGE AND EVICTION ----------------------------------------------------
    TArray<FHitTell> Tells;
    HitTellsOnHit(Tells, 0.0f, 10.0);
    TestEqual(TEXT("The first hit makes one tell"), Tells.Num(), 1);
    HitTellsOnHit(Tells, 10.0f, 10.1);
    TestEqual(TEXT("A hit 10 degrees off a live tell refreshes it"), Tells.Num(), 1);
    TestEqual(TEXT("The refreshed tell restarts its clock"), Tells[0].Time, 10.1);
    TestEqual(TEXT("The refreshed tell follows the source"), Tells[0].WorldYawDegrees, 10.0f);
    HitTellsOnHit(Tells, 100.0f, 10.2);
    HitTellsOnHit(Tells, -170.0f, 10.3);
    HitTellsOnHit(Tells, -80.0f, 10.4);
    TestEqual(TEXT("Four distinct bearings are four tells"), Tells.Num(), HudHitTellMax);
    HitTellsOnHit(Tells, 45.0f, 10.5);
    TestEqual(TEXT("A fifth bearing does not grow the array"), Tells.Num(), HudHitTellMax);
    bool bOldestGone = true;
    bool bNewestPresent = false;
    for (const FHitTell& Tell : Tells)
    {
        if (FMath::IsNearlyEqual(Tell.WorldYawDegrees, 10.0f)) bOldestGone = false;
        if (FMath::IsNearlyEqual(Tell.WorldYawDegrees, 45.0f) && Tell.Time == 10.5) bNewestPresent = true;
    }
    TestTrue(TEXT("The fifth bearing evicts the oldest"), bOldestGone);
    TestTrue(TEXT("and takes its slot"), bNewestPresent);
    // The merge wraps: -170 and 175 are ten degrees apart, not 345.
    HitTellsOnHit(Tells, 175.0f, 10.6);
    TestEqual(TEXT("A hit across the seam refreshes rather than stacks"), Tells.Num(), HudHitTellMax);
    // A hit on a dead tell's bearing is a new tell with a fresh clock, and
    // exactly one tell is live after it.
    TArray<FHitTell> Stale;
    HitTellsOnHit(Stale, 0.0f, 20.0);
    const double Later = 20.0 + HudHitTellSeconds + 1.0;
    HitTellsOnHit(Stale, 0.0f, Later);
    int32 Live = 0;
    for (const FHitTell& Tell : Stale)
    {
        if (HitTellAlpha(static_cast<float>(Later - Tell.Time)) > 0.0f) ++Live;
    }
    TestEqual(TEXT("One tell is live after a hit on a dead bearing"), Live, 1);
    return true;
}

// --------------------------------------------------------------------------
// The crosshair: gap from cone, travel rates, ADS collapse.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDCrosshairSpreadTest,
    "RiorsEdge.UI.HUD.CrosshairSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDCrosshairSpreadTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    using namespace BreakerUI;

    TestEqual(TEXT("No cone is the resting gap"), CrosshairGapTarget(0.0f), HudCrosshairGapRest);
    TestEqual(TEXT("The full cone is the open gap"), CrosshairGapTarget(HudCrosshairFullSpreadDegrees), HudCrosshairGapSpread);
    TestEqual(TEXT("Past the full cone clamps"), CrosshairGapTarget(HudCrosshairFullSpreadDegrees * 3.0f), HudCrosshairGapSpread);
    TestEqual(TEXT("Half the cone is half the travel"), CrosshairGapTarget(HudCrosshairFullSpreadDegrees * 0.5f),
        (HudCrosshairGapRest + HudCrosshairGapSpread) * 0.5f, BreakerHUDMathTolerance);
    TestEqual(TEXT("A negative cone reads as rest"), CrosshairGapTarget(-1.0f), HudCrosshairGapRest);

    // Opening: the full travel in 60 ms, never past the target.
    TestEqual(TEXT("Opening arrives in the out time"),
        CrosshairGapFollow(HudCrosshairGapRest, HudCrosshairGapSpread, HudCrosshairSpreadOutSeconds), HudCrosshairGapSpread, BreakerHUDMathTolerance);
    const float HalfOpen = CrosshairGapFollow(HudCrosshairGapRest, HudCrosshairGapSpread, HudCrosshairSpreadOutSeconds * 0.5f);
    TestTrue(TEXT("Half the out time is mid-travel"), HalfOpen > HudCrosshairGapRest && HalfOpen < HudCrosshairGapSpread);
    TestEqual(TEXT("Opening never overshoots"),
        CrosshairGapFollow(HudCrosshairGapRest, HudCrosshairGapSpread, 1.0f), HudCrosshairGapSpread);
    // Closing: the full travel in 200 ms, slower than opening.
    TestEqual(TEXT("Closing arrives in the back time"),
        CrosshairGapFollow(HudCrosshairGapSpread, HudCrosshairGapRest, HudCrosshairSpreadBackSeconds), HudCrosshairGapRest, BreakerHUDMathTolerance);
    const float PartClosed = CrosshairGapFollow(HudCrosshairGapSpread, HudCrosshairGapRest, HudCrosshairSpreadOutSeconds);
    TestTrue(TEXT("Closing is slower than opening"), PartClosed > HudCrosshairGapRest);
    TestEqual(TEXT("Closing never undershoots"),
        CrosshairGapFollow(HudCrosshairGapSpread, HudCrosshairGapRest, 1.0f), HudCrosshairGapRest);
    TestEqual(TEXT("Zero time moves nothing"), CrosshairGapFollow(20.0f, 40.0f, 0.0f), 20.0f);
    TestEqual(TEXT("At the target it holds"), CrosshairGapFollow(40.0f, 40.0f, 0.5f), 40.0f);

    // ADS: 0 at the flip, 1 after 80 ms; the release runs the other way.
    TestEqual(TEXT("Aim flip starts at the ticks"), CrosshairAdsBlend(0.0f, true), 0.0f);
    TestEqual(TEXT("Aim collapses in the ADS time"), CrosshairAdsBlend(HudCrosshairAdsSeconds, true), 1.0f);
    TestEqual(TEXT("Aim held stays collapsed"), CrosshairAdsBlend(10.0f, true), 1.0f);
    TestEqual(TEXT("Release starts collapsed"), CrosshairAdsBlend(0.0f, false), 1.0f);
    TestEqual(TEXT("Release returns in the ADS time"), CrosshairAdsBlend(HudCrosshairAdsSeconds, false), 0.0f);
    TestEqual(TEXT("Midway is midway"), CrosshairAdsBlend(HudCrosshairAdsSeconds * 0.5f, true), 0.5f, BreakerHUDMathTolerance);
    return true;
}

// --------------------------------------------------------------------------
// The health chip: drain 0 ms, hold 400 ms, recover 600 ms linear.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDHealthChipTest,
    "RiorsEdge.UI.HUD.HealthChip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDHealthChipTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    using namespace BreakerUI;

    FHealthChip Chip;
    TestEqual(TEXT("An unstruck chip shows the live value"), HealthChipShown(Chip, 0.6f, 100.0), 0.6f);

    // A drop from full to 0.6 at t = 10.
    const double T0 = 10.0;
    Chip = HealthChipOnDrop(Chip, HealthChipShown(Chip, 1.0f, T0), T0);
    TestEqual(TEXT("The chip re-arms from what was showing"), Chip.From, 1.0f);
    TestEqual(TEXT("The fill drains at once: shown is the chip, not the fill"), HealthChipShown(Chip, 0.6f, T0), 1.0f);
    TestEqual(TEXT("Holds through the hold"), HealthChipShown(Chip, 0.6f, T0 + HudHealthChipHoldSeconds * 0.5), 1.0f);
    TestEqual(TEXT("Still full at the end of the hold"), HealthChipShown(Chip, 0.6f, T0 + HudHealthChipHoldSeconds), 1.0f);
    TestEqual(TEXT("Halfway through recovery is halfway down"),
        HealthChipShown(Chip, 0.6f, T0 + HudHealthChipHoldSeconds + HudHealthChipRecoverSeconds * 0.5), 0.8f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Recovered at the end of the recovery"),
        HealthChipShown(Chip, 0.6f, T0 + HudHealthChipHoldSeconds + HudHealthChipRecoverSeconds), 0.6f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Long after, the live value"), HealthChipShown(Chip, 0.6f, T0 + 5.0), 0.6f);
    TestEqual(TEXT("Before its own time, the live value"), HealthChipShown(Chip, 0.6f, T0 - 1.0), 0.6f);

    // A second hit inside the hold keeps the first hit's high-water mark.
    const double T1 = T0 + 0.2;
    Chip = HealthChipOnDrop(Chip, HealthChipShown(Chip, 0.6f, T1), T1);
    TestEqual(TEXT("A second drop inside the hold keeps the high-water mark"), Chip.From, 1.0f);
    TestEqual(TEXT("The hold restarts from the second hit"), HealthChipShown(Chip, 0.3f, T1 + HudHealthChipHoldSeconds), 1.0f);

    // A heal during recovery: the chip never shows below the live value.
    const double T2 = T1 + HudHealthChipHoldSeconds + HudHealthChipRecoverSeconds * 0.9;
    TestTrue(TEXT("The chip never sits below the live value"), HealthChipShown(Chip, 0.95f, T2) >= 0.95f);
    return true;
}

// --------------------------------------------------------------------------
// Being hurt is told by the world, not by a frame (O270).
//
// Owner: "the red screen flash, how you have just a giant red border around
// your entire screen is just not good looking ... maybe reduce visibility in
// some way, think about Path of Exile's light radius". The HUD's part is one
// line — the bar wears harm under it — and the camera's part is composed in
// Characters/BreakerHarmPresentationMath.h off that same line.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDHarmPresentationTest,
    "RiorsEdge.UI.HUD.HarmPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDHarmPresentationTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    using namespace BreakerHarmPresentation;
    using namespace BreakerUI;

    TestTrue(TEXT("The value goes harm under the line"), VitalsValueIsHarm(0.19f));
    TestFalse(TEXT("The value stays bone at the line"), VitalsValueIsHarm(HudHealthLowFraction));

    // A HEALTHY, UNHIT FRAME COSTS NOTHING. The blend weight is zero, so the
    // renderer skips the slot entirely.
    {
        const FLook Look = Compose(-1.0f, 1.0f, HudHealthLowFraction, 3.0, 1.0f);
        TestEqual(TEXT("Nothing happening is no vignette"), Look.Vignette, 0.0f);
        TestEqual(TEXT("Nothing happening is full colour"), Look.Saturation, 1.0f);
        TestEqual(TEXT("and the slot is off"), Look.BlendWeight, 0.0f);
    }
    // A HIT CLOSES THE EDGES FOR A MOMENT, loudest on its first frame and
    // gone before the next shot lands at any shipped cadence.
    {
        TestEqual(TEXT("The pulse starts full"), HitPulse(0.0f), 1.0f, BreakerHUDMathTolerance);
        TestTrue(TEXT("The pulse eases out"), HitPulse(HitPulseSeconds * 0.5f) < 0.5f);
        TestEqual(TEXT("The pulse is over at its end"), HitPulse(HitPulseSeconds), 0.0f);
        TestEqual(TEXT("A hit that never happened is no pulse"), HitPulse(-1.0f), 0.0f);
        const FLook Look = Compose(0.0f, 1.0f, HudHealthLowFraction, 3.0, 1.0f);
        TestEqual(TEXT("A fresh hit closes the edges"), Look.Vignette, HitVignette, BreakerHUDMathTolerance);
        TestTrue(TEXT("and drains a little colour"), Look.Saturation < 1.0f);
        TestEqual(TEXT("and the slot is on"), Look.BlendWeight, 1.0f);
        TestTrue(TEXT("but never blacks the screen"), Look.Vignette < 1.0f && Look.Saturation > 0.5f);
    }
    // THE LIGHT RADIUS: nothing above the line, and closing in the further
    // under it you are — never total, a player at one hit point still has to
    // see the thing about to hit them.
    {
        TestEqual(TEXT("Above the line has no depth"), LowHealthDepth(0.5f, HudHealthLowFraction), 0.0f);
        TestEqual(TEXT("At the line has no depth"), LowHealthDepth(HudHealthLowFraction, HudHealthLowFraction), 0.0f);
        TestEqual(TEXT("Half way under is half"), LowHealthDepth(HudHealthLowFraction * 0.5f, HudHealthLowFraction), 0.5f, BreakerHUDMathTolerance);
        TestEqual(TEXT("Empty is full depth"), LowHealthDepth(0.0f, HudHealthLowFraction), 1.0f);
        const FLook AtLine = Compose(-1.0f, HudHealthLowFraction * 0.999f, HudHealthLowFraction, 0.0, 1.0f);
        const FLook Empty = Compose(-1.0f, 0.0f, HudHealthLowFraction, 0.0, 1.0f);
        TestTrue(TEXT("Under the line the edges are closing"), AtLine.Vignette >= LowVignetteAtLine - BreakerHUDMathTolerance);
        TestTrue(TEXT("and close further toward empty"), Empty.Vignette > AtLine.Vignette);
        TestTrue(TEXT("and the colour drains toward empty"), Empty.Saturation < AtLine.Saturation);
        TestTrue(TEXT("but the world never goes fully dark"), Empty.Vignette < 1.0f && Empty.Saturation > 0.3f);
        // The breath moves the edges, and stays inside the ceiling.
        float Lowest = 1.0f, Highest = 0.0f;
        for (double Now = 0.0; Now < BreathSeconds * 2.0; Now += 0.05)
        {
            const float V = Compose(-1.0f, 0.0f, HudHealthLowFraction, Now, 1.0f).Vignette;
            Lowest = FMath::Min(Lowest, V);
            Highest = FMath::Max(Highest, V);
        }
        TestTrue(TEXT("The low state breathes"), Highest - Lowest > BreathVignette * 0.9f);
        TestTrue(TEXT("and the breath never reaches total"), Highest < 1.0f);
    }
    // THE DEATH BEAT'S DRAIN COMPOSES, NEVER FIGHTS. Whatever else is
    // happening, the beat's saturation is a floor the look cannot rise above.
    {
        const FLook Look = Compose(-1.0f, 1.0f, HudHealthLowFraction, 0.0, 0.2f);
        TestEqual(TEXT("The beat's drain comes through"), Look.Saturation, 0.2f, BreakerHUDMathTolerance);
        TestEqual(TEXT("and the slot is on for it"), Look.BlendWeight, 1.0f);
        const FLook Both = Compose(0.0f, 0.0f, HudHealthLowFraction, 0.0, 0.2f);
        TestTrue(TEXT("A hit at the point of death cannot brighten the beat"), Both.Saturation <= 0.2f + BreakerHUDMathTolerance);
    }
    return true;
}

// --------------------------------------------------------------------------
// The damage-number timeline: snap in, settle, hold, rise, fade, the crit hold.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDDamageNumberTimelineTest,
    "RiorsEdge.UI.HUD.DamageNumberTimeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDDamageNumberTimelineTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    using namespace BreakerUI;

    const float Plain = DamageNumberLifetime(false);
    const float Crit = DamageNumberLifetime(true);
    TestEqual(TEXT("A plain number lives the hold and the rise"), Plain, MotionDamageHold + MotionDamageRise);
    TestEqual(TEXT("A crit holds longer"), Crit, MotionDamageHold + MotionDamageRise + MotionCritHold);

    constexpr float PopScale = 1.4f;
    const FDamageNumberFrame Birth = DamageNumberFrame(0.0f, Plain, PopScale);
    TestEqual(TEXT("Born at its peak"), Birth.Scale, PopScale, BreakerHUDMathTolerance);
    TestEqual(TEXT("Born at the impact"), Birth.RiseFraction, 0.0f);
    TestEqual(TEXT("Born opaque"), Birth.Alpha, 1.0f);

    TestEqual(TEXT("The settle lands at rest"), DamageNumberFrame(MotionDamageSettle, Plain, PopScale).Scale, 1.0f, BreakerHUDMathTolerance);
    const float MidSettle = DamageNumberFrame(MotionDamageSettle * 0.5f, Plain, PopScale).Scale;
    TestTrue(TEXT("Mid-settle is between peak and rest"), MidSettle > 1.0f && MidSettle < PopScale);
    TestTrue(TEXT("The settle eases out: most of it early"), MidSettle < (1.0f + PopScale) * 0.5f);
    TestEqual(TEXT("After the settle it stays at rest"), DamageNumberFrame(Plain * 0.5f, Plain, PopScale).Scale, 1.0f);
    TestEqual(TEXT("A number that does not pop stays at rest"), DamageNumberFrame(0.0f, Plain, 1.0f).Scale, 1.0f);

    // The hold: no rise until MotionDamageHold, then it starts.
    TestEqual(TEXT("Still through the hold"), DamageNumberFrame(MotionDamageHold - BreakerHUDMathTolerance, Plain, PopScale).RiseFraction, 0.0f);
    TestTrue(TEXT("Rising after the hold"), DamageNumberFrame(MotionDamageHold + 0.05f, Plain, PopScale).RiseFraction > 0.0f);

    // The rise is monotonic and reaches its full travel by hold + rise.
    float Previous = -1.0f;
    for (float Age = 0.0f; Age <= MotionDamageHold + MotionDamageRise + BreakerHUDMathTolerance; Age += 0.05f)
    {
        const float Rise = DamageNumberFrame(Age, Plain, PopScale).RiseFraction;
        TestTrue(*FString::Printf(TEXT("Rise is monotonic at %.2f"), Age), Rise >= Previous);
        Previous = Rise;
    }
    TestEqual(TEXT("The rise completes at hold + rise"), DamageNumberFrame(MotionDamageHold + MotionDamageRise, Plain, PopScale).RiseFraction, 1.0f, BreakerHUDMathTolerance);
    TestTrue(TEXT("The rise eases out: most of it early"), DamageNumberFrame(MotionDamageHold + MotionDamageRise * 0.5f, Plain, PopScale).RiseFraction > 0.5f);

    // The fade owns the last 300 ms, whatever the lifetime.
    TestEqual(TEXT("Opaque until the fade"), DamageNumberFrame(Plain - MotionDamageFade, Plain, PopScale).Alpha, 1.0f);
    TestEqual(TEXT("Half faded at half the fade"), DamageNumberFrame(Plain - MotionDamageFade * 0.5f, Plain, PopScale).Alpha, 0.5f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Gone at the end"), DamageNumberFrame(Plain, Plain, PopScale).Alpha, 0.0f, BreakerHUDMathTolerance);
    TestEqual(TEXT("A crit is still opaque where a plain number faded"), DamageNumberFrame(Plain, Crit, PopScale).Alpha, 1.0f);
    TestEqual(TEXT("A crit's fade is its own last 300 ms"), DamageNumberFrame(Crit - MotionDamageFade * 0.5f, Crit, PopScale).Alpha, 0.5f, BreakerHUDMathTolerance);
    TestEqual(TEXT("A crit holds at its full rise"), DamageNumberFrame(Crit - 0.01f, Crit, PopScale).RiseFraction, 1.0f, BreakerHUDMathTolerance);
    return true;
}

// --------------------------------------------------------------------------
// The countdown, the swap slide, the drain, the magazine, the hatch.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDCountdownTest,
    "RiorsEdge.UI.HUD.Countdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDCountdownTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    using namespace BreakerUI;

    TestEqual(TEXT("Nothing counting is empty"), FormatCountdown(-1.0f), FString());
    TestEqual(TEXT("Zero is 00:00"), FormatCountdown(0.0f), FString(TEXT("00:00")));
    TestEqual(TEXT("A fraction rounds up, never to zero with time left"), FormatCountdown(0.2f), FString(TEXT("00:01")));
    TestEqual(TEXT("Under a minute"), FormatCountdown(42.0f), FString(TEXT("00:42")));
    TestEqual(TEXT("59.2 rounds up to the minute"), FormatCountdown(59.2f), FString(TEXT("01:00")));
    TestEqual(TEXT("Over a minute"), FormatCountdown(65.0f), FString(TEXT("01:05")));
    TestEqual(TEXT("Minutes past 59 stay minutes"), FormatCountdown(3600.0f), FString(TEXT("60:00")));

    // The tracker's distance: the figure alone when the target is the beat's
    // own objective (the beat line above already names it), the label only
    // for a manually tracked marker nothing above names.
    TestEqual(TEXT("The beat's objective is a bare distance"),
        FormatTrackerDistance(TEXT("Breach Marshalling Yard"), 7700.0f, true), FString(TEXT("77m")));
    TestEqual(TEXT("A tracked marker keeps its label"),
        FormatTrackerDistance(TEXT("Breach Marshalling Yard"), 7700.0f, false), FString(TEXT("Breach Marshalling Yard · 77m")));
    TestEqual(TEXT("Distance rounds to the metre"), FormatTrackerDistance(TEXT("X"), 7749.0f, true), FString(TEXT("77m")));

    // The swap slide.
    const FSwapSlide Start = WeaponNameSwap(0.0f);
    TestTrue(TEXT("The name shows at the swap"), Start.bVisible);
    TestEqual(TEXT("It starts in place"), Start.OffsetPixels, 0.0f);
    TestEqual(TEXT("It starts opaque"), Start.Alpha, 1.0f);
    const FSwapSlide Mid = WeaponNameSwap(HudWeaponSwapSeconds * 0.5f);
    TestEqual(TEXT("Halfway it has slid half the travel, upward"), Mid.OffsetPixels, -HudWeaponSwapSlidePixels * 0.5f, BreakerHUDMathTolerance);
    TestEqual(TEXT("Halfway it is half faded"), Mid.Alpha, 0.5f, BreakerHUDMathTolerance);
    TestFalse(TEXT("At 1.2 s it is gone"), WeaponNameSwap(HudWeaponSwapSeconds).bVisible);
    TestFalse(TEXT("Before the swap it is not there"), WeaponNameSwap(-0.1f).bVisible);

    // The cooldown drain.
    TestEqual(TEXT("A full cooldown fills the tile"), AbilityDrainHeight(5.0f, 5.0f, 64.0f), 64.0f);
    TestEqual(TEXT("Half remaining is half the tile"), AbilityDrainHeight(2.5f, 5.0f, 64.0f), 32.0f);
    TestEqual(TEXT("Nothing remaining draws nothing"), AbilityDrainHeight(0.0f, 5.0f, 64.0f), 0.0f);
    TestEqual(TEXT("No duration draws nothing"), AbilityDrainHeight(1.0f, 0.0f, 64.0f), 0.0f);
    TestEqual(TEXT("Past the duration clamps"), AbilityDrainHeight(9.0f, 5.0f, 64.0f), 64.0f);

    // The magazine, under 25 %.
    TestTrue(TEXT("7 of 30 is low"), MagazineIsLow(7, 30));
    TestFalse(TEXT("8 of 30 is not"), MagazineIsLow(8, 30));
    TestTrue(TEXT("Empty is low"), MagazineIsLow(0, 30));
    TestFalse(TEXT("No capacity is never low"), MagazineIsLow(0, 0));

    // The hatch anchors to the screen, not the rectangle.
    const float Step = HatchDiagonalStep(HudHatchPeriod);
    TestEqual(TEXT("The origin starts a stripe"), HatchStripeStart(0.0f, 0.0f, HudHatchPeriod), 0.0f);
    TestEqual(TEXT("A rectangle starts on the stripe at or before its corner"),
        HatchStripeStart(100.0f, 50.0f, HudHatchPeriod), FMath::FloorToFloat(150.0f / Step) * Step, BreakerHUDMathTolerance);
    TestTrue(TEXT("The start is never past the corner"), HatchStripeStart(100.0f, 50.0f, HudHatchPeriod) <= 150.0f);
    TestTrue(TEXT("The start is within one step of the corner"), HatchStripeStart(100.0f, 50.0f, HudHatchPeriod) > 150.0f - Step);
    return true;
}

// --------------------------------------------------------------------------
// The rarity tally: one cell per tier, ascending, five at the top.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDRarityTallyTest,
    "RiorsEdge.UI.HUD.RarityTally",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDRarityTallyTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;

    TestEqual(TEXT("Standard lights one"), RarityTallyCells(EBreakerItemRarity::Standard), 1);
    TestEqual(TEXT("Uncommon two"), RarityTallyCells(EBreakerItemRarity::Uncommon), 2);
    TestEqual(TEXT("Exceptional three"), RarityTallyCells(EBreakerItemRarity::Exceptional), 3);
    TestEqual(TEXT("Aberrant four"), RarityTallyCells(EBreakerItemRarity::Aberrant), 4);
    TestEqual(TEXT("Unwritten five"), RarityTallyCells(EBreakerItemRarity::Unwritten), 5);
    TestTrue(TEXT("The tally ascends with rarity"),
        RarityTallyCells(EBreakerItemRarity::Standard) < RarityTallyCells(EBreakerItemRarity::Uncommon)
        && RarityTallyCells(EBreakerItemRarity::Uncommon) < RarityTallyCells(EBreakerItemRarity::Exceptional)
        && RarityTallyCells(EBreakerItemRarity::Exceptional) < RarityTallyCells(EBreakerItemRarity::Aberrant)
        && RarityTallyCells(EBreakerItemRarity::Aberrant) < RarityTallyCells(EBreakerItemRarity::Unwritten));
    return true;
}

// --------------------------------------------------------------------------
// The interact plate: 40 tall, the key tile 32 at rail + 4, the tally
// 4n + 2(n - 1), the width a function of the name and nothing else.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDInteractPlateTest,
    "RiorsEdge.UI.HUD.InteractPlate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDInteractPlateTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;

    const FInteractPlateLayout Keyed = InteractPlateLayout(100.0f, 1, true);
    TestEqual(TEXT("The plate is 40 tall"), Keyed.Height, 40.0f);
    TestEqual(TEXT("The rail is the identity rail"), Keyed.RailWidth, BreakerUI::HudRailIdentity);
    TestTrue(TEXT("The key tile is drawn"), Keyed.bKeyTile);
    TestEqual(TEXT("The key tile is 32"), Keyed.KeySize, 32.0f);
    TestEqual(TEXT("The key tile sits at rail + 4"), Keyed.KeyX, Keyed.RailWidth + 4.0f);
    TestTrue(TEXT("The tally follows the key"), Keyed.TallyX > Keyed.KeyX + Keyed.KeySize);
    TestTrue(TEXT("The name follows the tally"), Keyed.NameX > Keyed.TallyX + Keyed.TallyWidth);
    TestTrue(TEXT("The name fits inside the plate"), Keyed.Width >= Keyed.NameX + 100.0f);
    TestEqual(TEXT("Tally cells are 4 wide"), Keyed.TallyCellWidth, 4.0f);
    TestEqual(TEXT("Tally cells are 12 tall"), Keyed.TallyCellHeight, 12.0f);
    TestEqual(TEXT("Tally cells sit 2 apart"), Keyed.TallyGap, 2.0f);
    TestTrue(TEXT("The tally is centred on the plate"),
        FMath::IsNearlyEqual(Keyed.TallyY + Keyed.TallyCellHeight * 0.5f, Keyed.Height * 0.5f, BreakerHUDMathTolerance));

    // Tally width 4n + 2(n - 1) for one to five cells.
    for (int32 Cells = 1; Cells <= 5; ++Cells)
    {
        TestEqual(*FString::Printf(TEXT("%d cells measure 4n + 2(n - 1)"), Cells),
            InteractPlateLayout(100.0f, Cells, true).TallyWidth, 4.0f * Cells + 2.0f * (Cells - 1));
    }
    TestEqual(TEXT("No cells is no tally"), InteractPlateLayout(100.0f, 0, true).TallyWidth, 0.0f);

    // Width is monotone in the name's width.
    float Previous = 0.0f;
    for (float NameWidth = 0.0f; NameWidth <= 400.0f; NameWidth += 25.0f)
    {
        const float Width = InteractPlateLayout(NameWidth, 3, true).Width;
        TestTrue(*FString::Printf(TEXT("A wider name is a wider plate at %.0f"), NameWidth), Width > Previous);
        Previous = Width;
    }

    // Removing the key tile removes exactly its 32 and one 8 gap.
    const FInteractPlateLayout Unkeyed = InteractPlateLayout(100.0f, 1, false);
    TestFalse(TEXT("No key tile"), Unkeyed.bKeyTile);
    TestEqual(TEXT("No key tile removes exactly 40"), Keyed.Width - Unkeyed.Width, 40.0f);
    TestEqual(TEXT("Without the key the tally starts at rail + 4"), Unkeyed.TallyX, Unkeyed.RailWidth + 4.0f);
    TestEqual(TEXT("The height does not depend on the key"), Unkeyed.Height, Keyed.Height);
    return true;
}

// --------------------------------------------------------------------------
// Wave cells (O120): cells only where a total is authored. The rift run's
// total is its boss wave; the gym authors no run and draws none.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDWaveCellsTest,
    "RiorsEdge.UI.HUD.WaveCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDWaveCellsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;

    TestEqual(TEXT("No rift set draws no cells, whatever the gym's interval"), WaveCellTotal(false, 12), 0);
    TestEqual(TEXT("A rift set draws its boss wave's worth"), WaveCellTotal(true, 3), 3);
    TestEqual(TEXT("A nonsense interval draws none"), WaveCellTotal(true, -1), 0);

    // Shipped: the rift budget the game mode builds carries the game mode's
    // own RiftBossWave. The property is not public and Game/ is not this
    // lane's, so the default is read through reflection rather than retyped.
    const ABreakerGameMode* DefaultMode = GetDefault<ABreakerGameMode>();
    if (!TestNotNull(TEXT("The game mode has a default object"), DefaultMode)) return false;
    const FIntProperty* RiftBossWaveProperty = FindFProperty<FIntProperty>(ABreakerGameMode::StaticClass(), TEXT("RiftBossWave"));
    if (!TestNotNull(TEXT("The game mode carries RiftBossWave"), RiftBossWaveProperty)) return false;
    const int32 RiftBossWave = RiftBossWaveProperty->GetPropertyValue_InContainer(DefaultMode);
    TestEqual(TEXT("The shipped rift bosses on wave 3"), RiftBossWave, 3);
    const FBreakerWaveBudgetParams Rift = UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(RiftBossWave);
    TestEqual(TEXT("The rift budget's interval is the boss wave"), Rift.BossWaveInterval, RiftBossWave);
    TestEqual(TEXT("MakeRiftWaveBudget(3) bosses every 3"), UBreakerWaveBudgetLibrary::MakeRiftWaveBudget(3).BossWaveInterval, 3);
    TestEqual(TEXT("The shipped rift row is three cells"), WaveCellTotal(true, Rift.BossWaveInterval), 3);
    TestEqual(TEXT("The shipped gym row is empty"), WaveCellTotal(false, FBreakerWaveBudgetParams().BossWaveInterval), 0);
    return true;
}

// --------------------------------------------------------------------------
// HUD.NumericReadouts: the figures the owner asked to be able to read, and the
// shield's own colour.
//
// Owner, playtest 2026-09-10: "can we get numeric values for xp and mana that
// are shown ... the health should be a proper read and shield a light blue or
// something". Three separate claims, and all three are assertable without a
// viewport: the pair a resource row carries, the arithmetic behind the XP pair,
// and that the shield's colour is actually distinguishable from the health's.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDNumericReadoutsTest,
    "RiorsEdge.UI.HUD.NumericReadouts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDNumericReadoutsTest::RunTest(const FString& Parameters)
{
    using namespace BreakerUI;

    // --- THE SHIELD IS TOLD APART BY COLOUR, NOT BY POSITION ----------------
    // Both pools share one track under O199's combined bar, so the only thing
    // that can separate them is the fill. Text-2 against bone did not: these
    // assert a real separation rather than merely a different constant.
    TestFalse(TEXT("The shield blue is not the health bone"), VitalShield.Equals(System, 0.05f));
    TestFalse(TEXT("The shield blue is not text-2"), VitalShield.Equals(TextSecondary, 0.05f));
    // O179 stands: the movement verb keeps the cyan and the shield is a blue.
    TestFalse(TEXT("The shield blue is not the movement verb"), VitalShield.Equals(VerbMove, 0.05f));
    TestFalse(TEXT("The shield blue is not reserved teal"), IsReservedTeal(VitalShield));
    // BLUE MEANS BLUE: more blue than red and more blue than green, or the name
    // is a lie and a retune could drift it grey without failing anything.
    TestTrue(TEXT("The shield blue is bluest"), VitalShield.B > VitalShield.G && VitalShield.G > VitalShield.R);
    // And its track reads as a track: the empty half is darker than the fill.
    TestTrue(TEXT("The shield's empty track is darker than its fill"),
        VitalShieldDeep.GetLuminance() < VitalShield.GetLuminance());

    // --- THE RESOURCE ROW CARRIES A PAIR -----------------------------------
    {
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveManaRow(68.0f, 120.0f, -20.0f);
        BreakerHUD::SetResourceValue(Row, 68.0f, 120.0f);
        TestEqual(TEXT("Mana in credit prints its bank"), Row.ValueText, FString(TEXT("68")));
        TestEqual(TEXT("Mana in credit prints its pool"), Row.MaxText, FString(TEXT("120")));
    }
    {
        // OVERCAST KEEPS ITS SIGN. The one state where the bank goes below zero
        // is the one state where a bare figure would read as credit.
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveManaRow(-12.0f, 120.0f, -20.0f);
        BreakerHUD::SetResourceValue(Row, -12.0f, 120.0f);
        TestTrue(TEXT("Overcast prints a debt"), Row.ValueText.StartsWith(TEXT("-")));
        TestEqual(TEXT("Overcast still counts against the credit pool"), Row.MaxText, FString(TEXT("120")));
    }
    {
        // NO TOTAL MEANS NO TOTAL. A component with no published maximum prints
        // one figure rather than a confident zero.
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveMomentumRow(0.5f, EBreakerMomentumState::Settled);
        BreakerHUD::SetResourceValue(Row, 50.0f, 0.0f);
        TestEqual(TEXT("A pool with no maximum prints its value"), Row.ValueText, FString(TEXT("50")));
        TestTrue(TEXT("A pool with no maximum prints no total"), Row.MaxText.IsEmpty());
    }
    {
        // AN INACTIVE ROW PRINTS NOTHING. The empty row is drawn, never omitted
        // (the cluster must not change height between classes), and a figure on
        // it would be a number describing a pool the class does not have.
        BreakerHUD::FResourceRow Row = BreakerHUD::ResolveEmptyResourceRow();
        BreakerHUD::SetResourceValue(Row, 50.0f, 100.0f);
        TestTrue(TEXT("A class with no resource prints no value"), Row.ValueText.IsEmpty());
        TestTrue(TEXT("A class with no resource prints no total"), Row.MaxText.IsEmpty());
    }

    // --- THE XP PAIR IS INTO-LEVEL OVER NEEDED -----------------------------
    // The HUD derives "how far into this level" by subtracting the cumulative
    // cost of reaching it from the stored total. That is only honest if the
    // curve's two functions agree, so this walks the whole ladder.
    {
        const FBreakerExperienceCurve Curve;
        for (int32 Level = 1; Level < UBreakerExperienceLibrary::MaxCharacterLevel; ++Level)
        {
            const int32 Reached = UBreakerExperienceLibrary::TotalXpToReachLevel(Level, Curve);
            const int32 Needed = UBreakerExperienceLibrary::XpToNextLevel(Level, Curve);
            const int32 NextReached = UBreakerExperienceLibrary::TotalXpToReachLevel(Level + 1, Curve);
            if (!TestEqual(*FString::Printf(TEXT("Level %d's cost closes the gap to %d"), Level, Level + 1),
                Reached + Needed, NextReached)) return false;
            if (!TestTrue(*FString::Printf(TEXT("Level %d costs something"), Level), Needed > 0)) return false;

            // A character exactly halfway up this level: the figure the HUD
            // prints must sit inside the level it belongs to, never past its
            // own denominator and never negative.
            const int32 Total = Reached + Needed / 2;
            const int32 IntoLevel = Total - Reached;
            if (!TestTrue(*FString::Printf(TEXT("Halfway up level %d is inside it"), Level),
                IntoLevel >= 0 && IntoLevel < Needed)) return false;
            if (!TestEqual(*FString::Printf(TEXT("Halfway up level %d is still level %d"), Level, Level),
                UBreakerExperienceLibrary::LevelForTotalXp(Total, Curve), Level)) return false;
        }
        // AT THE CAP THERE IS NO NEXT LEVEL, which is why the HUD prints the
        // lifetime total alone there instead of a pair over a zero.
        TestEqual(TEXT("The cap needs nothing further"),
            UBreakerExperienceLibrary::XpToNextLevel(UBreakerExperienceLibrary::MaxCharacterLevel, Curve), 0);
    }
    return true;
}

// --------------------------------------------------------------------------
// HUD.WalletGainReadout: "+N RIFTGLASS" after the wallet grows, and nothing
// after it is seeded, spent, or left alone.
//
// Owner, playtest 2026-09-11: "no indicator for gaining riftglass". The HUD
// diffs the balance each frame, so the rule is entirely about which deltas
// count and how they fold into one figure over one hold.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDWalletGainReadoutTest,
    "RiorsEdge.UI.HUD.WalletGainReadout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDWalletGainReadoutTest::RunTest(const FString& Parameters)
{
    using BreakerHUDMath::FBreakerWalletGainReadout;
    constexpr double Hold = FBreakerWalletGainReadout::GainHoldSeconds;

    // --- SHIPPED CONFIGURATION ---------------------------------------------
    {
        const FBreakerWalletGainReadout Fresh;
        TestEqual(TEXT("A fresh readout has nothing pending"), Fresh.Pending, 0);
        TestFalse(TEXT("A fresh readout is not showing"), Fresh.IsShowing(0.0));
        TestEqual(TEXT("A fresh readout draws at zero alpha"), Fresh.Alpha(0.0), 0.0f);
        TestEqual(TEXT("The hold is 1.6 s"), FBreakerWalletGainReadout::GainHoldSeconds, 1.6f);
        TestTrue(TEXT("The fade fits inside the hold"),
            FBreakerWalletGainReadout::GainFadeSeconds > 0.0f
            && FBreakerWalletGainReadout::GainFadeSeconds < FBreakerWalletGainReadout::GainHoldSeconds);
    }

    // --- THE FIRST OBSERVE SEEDS SILENTLY ------------------------------------
    // The account folds its whole balance in at spawn. That is not a gain.
    FBreakerWalletGainReadout Readout;
    Readout.Observe(1250, 10.0);
    TestFalse(TEXT("The seeding balance prints nothing"), Readout.IsShowing(10.0));
    TestEqual(TEXT("The seeding balance leaves nothing pending"), Readout.Pending, 0);

    // --- TWO GAINS INSIDE ONE HOLD FOLD INTO ONE FIGURE ----------------------
    Readout.Observe(1251, 10.5);
    TestTrue(TEXT("A gain shows"), Readout.IsShowing(10.5));
    TestEqual(TEXT("A gain of one reads one"), Readout.Pending, 1);
    TestEqual(TEXT("A fresh gain draws at full alpha"), Readout.Alpha(10.5), 1.0f);
    Readout.Observe(1252, 11.0);
    TestEqual(TEXT("A second gain inside the hold accumulates"), Readout.Pending, 2);
    // The hold restarted at 11.0: the figure is still up when the first
    // gain's own hold would have ended.
    TestTrue(TEXT("The hold extends from the latest gain"), Readout.IsShowing(10.5 + Hold + 0.01));
    TestFalse(TEXT("The extended hold still ends"), Readout.IsShowing(11.0 + Hold));

    // --- A DECREASE IS A SPEND, NOT A GAIN ------------------------------------
    // Well after the hold, so the figure is spent: the fall re-bases and
    // prints nothing.
    Readout.Observe(1200, 20.0);
    TestFalse(TEXT("A spend prints nothing"), Readout.IsShowing(20.0));
    TestEqual(TEXT("A spend leaves nothing pending"), Readout.Pending, 0);
    // And it re-based: the next rise is measured from the lower balance.
    Readout.Observe(1205, 20.5);
    TestTrue(TEXT("A gain after a spend shows"), Readout.IsShowing(20.5));
    TestEqual(TEXT("A gain after a spend is measured from the spent balance"), Readout.Pending, 5);

    // --- THE FADE IS LINEAR OVER THE LAST PART OF THE HOLD --------------------
    {
        const double FadeStart = 20.5 + Hold - FBreakerWalletGainReadout::GainFadeSeconds;
        TestEqual(TEXT("Full until the fade starts"), Readout.Alpha(FadeStart - 0.05), 1.0f);
        TestTrue(TEXT("Half way through the fade is about half"),
            FMath::IsNearlyEqual(Readout.Alpha(FadeStart + FBreakerWalletGainReadout::GainFadeSeconds * 0.5), 0.5f, 0.02f));
        TestTrue(TEXT("The fade never rises"),
            Readout.Alpha(FadeStart + FBreakerWalletGainReadout::GainFadeSeconds * 0.75)
            < Readout.Alpha(FadeStart + FBreakerWalletGainReadout::GainFadeSeconds * 0.25));
    }

    // --- AFTER THE HOLD: NOT SHOWING, NOTHING PENDING -------------------------
    const double AfterHold = 20.5 + Hold + 0.05;
    TestFalse(TEXT("After the hold the figure is gone"), Readout.IsShowing(AfterHold));
    TestEqual(TEXT("After the hold the alpha is zero"), Readout.Alpha(AfterHold), 0.0f);
    Readout.Observe(1205, AfterHold);
    TestEqual(TEXT("An unchanged balance after the hold spends the figure"), Readout.Pending, 0);

    // --- A BOSS PURSE ON TOP OF A TRASH KILL READS THE SUM --------------------
    Readout.Observe(1206, 30.0);
    TestEqual(TEXT("A trash kill reads one"), Readout.Pending, 1);
    Readout.Observe(1246, 30.4);
    TestEqual(TEXT("A boss purse on top reads the sum"), Readout.Pending, 41);
    TestTrue(TEXT("The sum is showing"), Readout.IsShowing(30.4));

    // --- A NEW GAIN AFTER THE HOLD STARTS FROM ZERO ----------------------------
    Readout.Observe(1250, 30.4 + Hold + 1.0);
    TestEqual(TEXT("A gain after the hold does not carry the old figure"), Readout.Pending, 4);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
