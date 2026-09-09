#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Game/BreakerGameMode.h"
#include "Game/BreakerWaveBudget.h"
#include "UI/BreakerDamageFeed.h"
#include "UI/BreakerHUDMath.h"
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

    // O207: the play sizes stand, and the aggregation window with them.
    TestEqual(TEXT("Body damage number 26"), DamageBodyPixels, 26.0f);
    TestEqual(TEXT("Crit damage number 52"), DamageCritPixels, 52.0f);
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
    TestEqual(TEXT("Pop 60 ms"), MotionDamagePop, 0.06f);
    TestEqual(TEXT("Settle 120 ms"), MotionDamageSettle, 0.12f);
    TestEqual(TEXT("Rise 700 ms"), MotionDamageRise, 0.70f);
    TestEqual(TEXT("Fade 300 ms"), MotionDamageFade, 0.30f);
    TestEqual(TEXT("Crit hold 400 ms"), MotionCritHold, 0.40f);
    TestEqual(TEXT("Rise 24 px"), DamageRisePixels, 24.0f);
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
// Near-death: visible under 20 %, the frame pulsing 8→16→8 over 1.6 s.
// --------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHUDNearDeathPulseTest,
    "RiorsEdge.UI.HUD.NearDeathPulse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHUDNearDeathPulseTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHUDMath;
    using namespace BreakerUI;

    TestTrue(TEXT("Under the threshold shows"), NearDeathVisible(0.19f));
    TestFalse(TEXT("At the threshold does not"), NearDeathVisible(HudHealthLowFraction));
    TestFalse(TEXT("Full health does not"), NearDeathVisible(1.0f));
    TestTrue(TEXT("The value goes harm at the same line"), VitalsValueIsHarm(0.19f));
    TestFalse(TEXT("The value stays bone at the line"), VitalsValueIsHarm(HudHealthLowFraction));

    TestEqual(TEXT("The pulse starts thin"), NearDeathFrameWidth(0.0), HudNearDeathFrameMin, BreakerHUDMathTolerance);
    TestEqual(TEXT("Half a period is the thick edge"), NearDeathFrameWidth(HudNearDeathPulseSeconds * 0.5), HudNearDeathFrameMax, BreakerHUDMathTolerance);
    TestEqual(TEXT("A whole period is thin again"), NearDeathFrameWidth(HudNearDeathPulseSeconds), HudNearDeathFrameMin, BreakerHUDMathTolerance);
    TestEqual(TEXT("It loops"), NearDeathFrameWidth(HudNearDeathPulseSeconds * 2.5), HudNearDeathFrameMax, BreakerHUDMathTolerance);
    for (double Now = 0.0; Now < HudNearDeathPulseSeconds * 3.0; Now += 0.05)
    {
        const float Width = NearDeathFrameWidth(Now);
        TestTrue(*FString::Printf(TEXT("Width stays inside 8..16 at t=%.2f (got %.2f)"), Now, Width),
            Width >= HudNearDeathFrameMin - BreakerHUDMathTolerance && Width <= HudNearDeathFrameMax + BreakerHUDMathTolerance);
    }
    // Ease-in-out: the quarter point sits below the linear midpoint.
    TestTrue(TEXT("The pulse eases"), NearDeathFrameWidth(HudNearDeathPulseSeconds * 0.125) < (HudNearDeathFrameMin + HudNearDeathFrameMax) * 0.5f);
    return true;
}

// --------------------------------------------------------------------------
// The damage-number timeline: pop, settle, rise, fade, the crit hold.
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
    TestEqual(TEXT("A plain number lives the rise"), Plain, MotionDamageRise);
    TestEqual(TEXT("A crit holds longer"), Crit, MotionDamageRise + MotionCritHold);

    constexpr float PopScale = 1.4f;
    const FDamageNumberFrame Birth = DamageNumberFrame(0.0f, Plain, PopScale);
    TestEqual(TEXT("Born at rest size"), Birth.Scale, 1.0f);
    TestEqual(TEXT("Born at the impact"), Birth.RiseFraction, 0.0f);
    TestEqual(TEXT("Born opaque"), Birth.Alpha, 1.0f);

    TestEqual(TEXT("The pop peaks at its end"), DamageNumberFrame(MotionDamagePop, Plain, PopScale).Scale, PopScale, BreakerHUDMathTolerance);
    TestEqual(TEXT("The settle lands at rest"), DamageNumberFrame(MotionDamagePop + MotionDamageSettle, Plain, PopScale).Scale, 1.0f, BreakerHUDMathTolerance);
    const float MidSettle = DamageNumberFrame(MotionDamagePop + MotionDamageSettle * 0.5f, Plain, PopScale).Scale;
    TestTrue(TEXT("Mid-settle is between peak and rest"), MidSettle > 1.0f && MidSettle < PopScale);
    TestEqual(TEXT("After the settle it stays at rest"), DamageNumberFrame(Plain * 0.5f, Plain, PopScale).Scale, 1.0f);
    TestEqual(TEXT("A number that does not pop stays at rest"), DamageNumberFrame(MotionDamagePop, Plain, 1.0f).Scale, 1.0f);

    // The rise is monotonic and reaches its full travel by the rise time.
    float Previous = -1.0f;
    for (float Age = 0.0f; Age <= MotionDamageRise + BreakerHUDMathTolerance; Age += 0.05f)
    {
        const float Rise = DamageNumberFrame(Age, Plain, PopScale).RiseFraction;
        TestTrue(*FString::Printf(TEXT("Rise is monotonic at %.2f"), Age), Rise >= Previous);
        Previous = Rise;
    }
    TestEqual(TEXT("The rise completes at the rise time"), DamageNumberFrame(MotionDamageRise, Plain, PopScale).RiseFraction, 1.0f, BreakerHUDMathTolerance);
    TestTrue(TEXT("The rise eases out: most of it early"), DamageNumberFrame(MotionDamageRise * 0.5f, Plain, PopScale).RiseFraction > 0.5f);

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

#endif // WITH_DEV_AUTOMATION_TESTS
