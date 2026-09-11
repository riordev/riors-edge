#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemyBarMath.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerItemTypes.h"
#include "Settings/BreakerGameSettings.h"
#include "UI/BreakerUIStyle.h"

// The nameplate's drawing cannot be tested — no viewport, no way to assert a
// mark reads. Its ARITHMETIC can: every rule the nameplate sheet states is a
// pure function in Combat/BreakerEnemyBarMath.h, and this walks each one over
// its domain and pins the shipped constants by value, so a retune that
// disagrees with a ruling fails here rather than in a screenshot.

namespace
{
    constexpr float BreakerEnemyBarMathTolerance = 0.001f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyBarMathTest,
    "RiorsEdge.Combat.EnemyBarMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyBarMathTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEnemyBarMath;

    // --- The shipped constants, by value -------------------------------------
    TestEqual(TEXT("Near 1200"), NearCm, 1200.0f);
    TestEqual(TEXT("Far 3500"), FarCm, 3500.0f);
    TestEqual(TEXT("Draw 6000"), DrawCm, 6000.0f);
    TestEqual(TEXT("Chip and shield hide 2500"), ChipAndShieldHideCm, 2500.0f);
    TestEqual(TEXT("Floor scale 0.5"), FloorScale, 0.5f);
    TestEqual(TEXT("Standard bar 96 wide"), StandardBarW, 96.0f);
    TestEqual(TEXT("Standard bar 6 tall"), StandardBarH, 6.0f);
    TestEqual(TEXT("Champion bar 160 wide"), ChampionBarW, 160.0f);
    TestEqual(TEXT("Champion bar 8 tall"), ChampionBarH, 8.0f);
    TestEqual(TEXT("Boss bar 640 wide"), BossBarW, 640.0f);
    TestEqual(TEXT("Boss bar 24 tall"), BossBarH, 24.0f);
    TestEqual(TEXT("Border 1"), BorderPx, 1.0f);
    TestEqual(TEXT("Boss border 2"), BossBorderPx, 2.0f);
    TestEqual(TEXT("Plate floor 5"), MinPlateH, 5.0f);
    TestEqual(TEXT("Boss plate floor 12"), BossMinPlateH, 12.0f);
    TestEqual(TEXT("Champion contact dot 4"), ChampionContactDotPx, 4.0f);
    TestEqual(TEXT("Boss beyond DrawCm draws at the floor"), BeyondDrawBossScale, FloorScale);
    TestEqual(TEXT("Name drop 2000, unused until a name source lands"), NameDropCm, 2000.0f);
    TestEqual(TEXT("Active underline 2"), MarkActiveUnderlinePx, 2.0f);
    TestEqual(TEXT("Active underline 2 below the cell"), MarkActiveUnderlineGapPx, 2.0f);
    TestEqual(TEXT("Fill floor 3"), MinFillH, 3.0f);
    TestEqual(TEXT("Shield line 2"), ShieldLinePx, 2.0f);
    TestEqual(TEXT("Halo width ratio 1.6"), HaloWidthRatio, 1.6f);
    TestEqual(TEXT("Halo height 28"), HaloHeightPx, 28.0f);
    TestEqual(TEXT("Halo stroke 2"), HaloStrokePx, 2.0f);
    TestEqual(TEXT("Champion diamond 10"), ChampionDiamondPx, 10.0f);
    TestEqual(TEXT("Plate 12 above the head"), PlateAboveHeadPx, 12.0f);
    TestEqual(TEXT("Column gap 6"), ColumnGapPx, 6.0f);
    TestEqual(TEXT("Mark cell 16"), MarkCellPx, 16.0f);
    TestEqual(TEXT("Mark gap 8"), MarkGapPx, 8.0f);
    TestEqual(TEXT("Mark gap floor 4"), MarkGapMinPx, 4.0f);
    TestEqual(TEXT("Three marks at most"), MaximumMarks, 3);

    // --- Range scale -----------------------------------------------------------
    TestEqual(TEXT("Full size at Near"), ScaleFor(1200.0f), 1.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Full size inside Near"), ScaleFor(300.0f), 1.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Halfway is three quarters"), ScaleFor(2350.0f), 0.75f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Floor at Far"), ScaleFor(3500.0f), 0.5f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Floor holds past Far"), ScaleFor(5000.0f), ScaleFor(3500.0f), BreakerEnemyBarMathTolerance);
    // The profile's larger-nameplates switch is a multiplier on the range
    // scale, applied by the nameplate TU: one authored step up, and the
    // math's floors are not the toggle's to move.
    TestEqual(TEXT("Larger nameplates at Near is one step up"),
        ScaleFor(1200.0f) * UBreakerGameSettings::LargerNameplateScale, 1.5f, BreakerEnemyBarMathTolerance);
    // BarSizeFor takes no profile: the unscaled fill at Near is the sheet's 4
    // whatever the toggle says, and the floor beneath it is still 3.
    TestEqual(TEXT("The unscaled fill is unchanged by the toggle"),
        BarSizeFor(EBreakerMonsterRank::Trash, 1.0f).FillH, 4.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Fill floor 3 stands under the toggle"), MinFillH, 3.0f);

    // --- Bar size by rank ---------------------------------------------------------
    {
        const FBarSize Trash = BarSizeFor(EBreakerMonsterRank::Trash, 1.0f);
        const FBarSize Elite = BarSizeFor(EBreakerMonsterRank::Elite, 1.0f);
        const FBarSize Champion = BarSizeFor(EBreakerMonsterRank::ModifierBearing, 1.0f);
        const FBarSize Boss = BarSizeFor(EBreakerMonsterRank::Boss, 1.0f);
        TestEqual(TEXT("Trash 96 wide"), Trash.W, 96.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Trash 6 tall"), Trash.H, 6.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Elite 96 wide"), Elite.W, 96.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Elite 6 tall"), Elite.H, 6.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion 160 wide"), Champion.W, 160.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion 8 tall"), Champion.H, 8.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss 640 wide"), Boss.W, 640.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss 24 tall"), Boss.H, 24.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss border 2"), Boss.Border, 2.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss fill inside a 2px border"), Boss.FillH, 20.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Trash border 1"), Trash.Border, 1.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Elite border 1"), Elite.Border, 1.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion border 1"), Champion.Border, 1.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Standard fill inside a 1px border"), Trash.FillH, 4.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion fill inside a 1px border"), Champion.FillH, 6.0f, BreakerEnemyBarMathTolerance);
    }
    {
        const FBarSize Trash = BarSizeFor(EBreakerMonsterRank::Trash, 0.5f);
        const FBarSize Champion = BarSizeFor(EBreakerMonsterRank::ModifierBearing, 0.5f);
        const FBarSize Boss = BarSizeFor(EBreakerMonsterRank::Boss, 0.5f);
        TestEqual(TEXT("Trash 48 wide at the floor"), Trash.W, 48.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Trash plate floors at 5"), Trash.H, 5.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion 80 wide at the floor"), Champion.W, 80.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion plate floors at 5"), Champion.H, 5.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss 320 wide at the floor"), Boss.W, 320.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss 12 tall at the floor"), Boss.H, 12.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss border never scales"), Boss.Border, 2.0f, BreakerEnemyBarMathTolerance);
        TestTrue(TEXT("Trash fill never under 3"), Trash.FillH >= MinFillH - BreakerEnemyBarMathTolerance);
        TestTrue(TEXT("Champion fill never under 3"), Champion.FillH >= MinFillH - BreakerEnemyBarMathTolerance);
        TestTrue(TEXT("Boss fill never under 3"), Boss.FillH >= MinFillH - BreakerEnemyBarMathTolerance);
    }

    // --- The fill's colour by rank (O203) -----------------------------------------
    {
        const FLinearColor Elite = BarFillColorFor(EBreakerMonsterRank::Elite);
        const FLinearColor Trash = BarFillColorFor(EBreakerMonsterRank::Trash);
        const FLinearColor Champion = BarFillColorFor(EBreakerMonsterRank::ModifierBearing);
        const FLinearColor Boss = BarFillColorFor(EBreakerMonsterRank::Boss);
        const FLinearColor Exceptional = BreakerUI::RarityColor(EBreakerItemRarity::Exceptional);

        // The elite's fill IS the third rarity's colour, read off the same
        // token the loot frame reads — not a hex authored beside it.
        TestTrue(TEXT("Elite fill is the Exceptional rarity colour"), Elite.Equals(Exceptional, BreakerEnemyBarMathTolerance));
        TestTrue(TEXT("Elite fill is not the trash fill"), !Elite.Equals(Trash, BreakerEnemyBarMathTolerance));
        // One colour per rank; the other three fill in the system bone, so
        // the champion's diamonds and the boss's phase geometry stay the read.
        TestTrue(TEXT("Trash fill is the system bone"), Trash.Equals(BreakerUI::System, BreakerEnemyBarMathTolerance));
        TestTrue(TEXT("Champion fill is the system bone"), Champion.Equals(BreakerUI::System, BreakerEnemyBarMathTolerance));
        TestTrue(TEXT("Boss fill is the system bone"), Boss.Equals(BreakerUI::System, BreakerEnemyBarMathTolerance));

        // Shipped configuration: the Exceptional token sits outside every
        // reserved verb band (O179) and outside the teal noun, so an elite's
        // bar cannot be misread as movement, a weak point, or a rift object.
        TestTrue(TEXT("Exceptional is not the movement cyan (O179)"), !Exceptional.Equals(BreakerUI::VerbMove, BreakerEnemyBarMathTolerance));
        TestTrue(TEXT("Exceptional is not the gold weak-point lane (O179)"), !Exceptional.Equals(BreakerUI::Gold, BreakerEnemyBarMathTolerance));
        TestTrue(TEXT("Exceptional is not the orange weapon lane (O179)"), !Exceptional.Equals(BreakerUI::Orange, BreakerEnemyBarMathTolerance));
        TestTrue(TEXT("Exceptional is not harm-red (O179)"), !Exceptional.Equals(BreakerUI::Harm, BreakerEnemyBarMathTolerance));
        TestTrue(TEXT("Exceptional is not the ultimate violet (O179)"), !Exceptional.Equals(BreakerUI::Violet, BreakerEnemyBarMathTolerance));
        TestFalse(TEXT("Exceptional is not a reserved teal"), BreakerUI::IsReservedTeal(Exceptional));
        TestTrue(TEXT("Exceptional is not the system bone"), !Exceptional.Equals(BreakerUI::System, BreakerEnemyBarMathTolerance));
    }

    // --- The boss's phase geometry (O156: the phase lives on the body) ----------
    {
        // The gate marks are the boss's own params, default-constructed here so
        // the shipped gates are pinned against the shipped plate.
        const FBossMarkFractions Gates = BossMarkFractions(FBreakerBossPhaseParams{});
        TestEqual(TEXT("Lower mark at the Commitment gate"), Gates.Commitment, 0.33f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Upper mark at the Suppression gate"), Gates.Suppression, 0.66f, BreakerEnemyBarMathTolerance);
        TestTrue(TEXT("The marks are ordered"), Gates.Commitment < Gates.Suppression);

        FBreakerBossPhaseParams Retuned;
        Retuned.CommitmentGate = 0.25f;
        Retuned.SuppressionGate = 0.75f;
        const FBossMarkFractions RetunedGates = BossMarkFractions(Retuned);
        TestEqual(TEXT("The marks follow the params, not a copy"), RetunedGates.Commitment, 0.25f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("The marks follow the params, not a copy (upper)"), RetunedGates.Suppression, 0.75f, BreakerEnemyBarMathTolerance);

        TestEqual(TEXT("Three phases, three pips"), BossPhaseCount(), 3);

        // Walk every phase: pips before it are done, its own is current, the
        // rest are upcoming.
        for (int32 Phase = 0; Phase < BossPhaseCount(); ++Phase)
        {
            const EBreakerBossPhase Current = static_cast<EBreakerBossPhase>(Phase);
            for (int32 Pip = 0; Pip < BossPhaseCount(); ++Pip)
            {
                const EBreakerPipState Expected = Pip < Phase ? EBreakerPipState::Done
                    : Pip == Phase ? EBreakerPipState::Current : EBreakerPipState::Upcoming;
                TestTrue(FString::Printf(TEXT("Pip %d in phase %d"), Pip, Phase), PipStateFor(Pip, Current) == Expected);
            }
        }
        TestTrue(TEXT("Deployment: first pip current"), PipStateFor(0, EBreakerBossPhase::Deployment) == EBreakerPipState::Current);
        TestTrue(TEXT("Suppression: first pip done"), PipStateFor(0, EBreakerBossPhase::Suppression) == EBreakerPipState::Done);
        TestTrue(TEXT("Commitment: last pip current"), PipStateFor(2, EBreakerBossPhase::Commitment) == EBreakerPipState::Current);
        TestTrue(TEXT("Deployment: last pip upcoming"), PipStateFor(2, EBreakerBossPhase::Deployment) == EBreakerPipState::Upcoming);

        TestEqual(TEXT("Gate mark 4 wide"), BossMarkW, 4.0f);
        TestEqual(TEXT("Gate mark 32 tall near"), BossMarkHeightFor(1.0f), 32.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Gate mark 16 tall at the floor"), BossMarkHeightFor(0.5f), 16.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Gate mark edge 1"), BossMarkEdgePx, 1.0f);
        TestEqual(TEXT("Pip 24 wide near"), BossPipSizeFor(1.0f).X, 24.0, static_cast<double>(BreakerEnemyBarMathTolerance));
        TestEqual(TEXT("Pip 6 tall near"), BossPipSizeFor(1.0f).Y, 6.0, static_cast<double>(BreakerEnemyBarMathTolerance));
        TestEqual(TEXT("Pip 16 wide at the floor"), BossPipSizeFor(0.5f).X, 16.0, static_cast<double>(BreakerEnemyBarMathTolerance));
        TestEqual(TEXT("Pip 4 tall at the floor"), BossPipSizeFor(0.5f).Y, 4.0, static_cast<double>(BreakerEnemyBarMathTolerance));
        TestEqual(TEXT("BOSS 32 near"), BossNameFor(1.0f), 32.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("BOSS 20 at the floor"), BossNameFor(0.5f), 20.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("BOSS 20 at the beyond-draw scale"), BossNameFor(BeyondDrawBossScale), 20.0f, BreakerEnemyBarMathTolerance);
    }

    // --- The elite's ellipse and the champion's diamonds -------------------------
    TestEqual(TEXT("Halo 28 tall near"), HaloHeightFor(1.0f), 28.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Halo 14 tall at the floor"), HaloHeightFor(0.5f), 14.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Diamond gap 4 near"), ChampionDiamondGapFor(1.0f), 4.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Diamond gap 6 at the floor"), ChampionDiamondGapFor(0.5f), 6.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Diamond 10 near"), ChampionDiamondFor(1.0f), 10.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Diamond floors at 5"), ChampionDiamondFor(0.5f), 5.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Mark gap 8 near"), MarkGapFor(1.0f), 8.0f, BreakerEnemyBarMathTolerance);
    TestEqual(TEXT("Mark gap floors at 4"), MarkGapFor(0.5f), 4.0f, BreakerEnemyBarMathTolerance);

    // --- Every modifier has its own mark, and every mark stays in its cell ----
    {
        TArray<EBreakerEnemyMark> Seen;
        for (uint8 Raw = static_cast<uint8>(EBreakerEnemyModifier::None) + 1;
            Raw < static_cast<uint8>(EBreakerEnemyModifier::Count); ++Raw)
        {
            const EBreakerEnemyModifier Modifier = static_cast<EBreakerEnemyModifier>(Raw);
            const EBreakerEnemyMark Mark = MarkFor(Modifier);
            TestTrue(FString::Printf(TEXT("Modifier %d has a mark"), Raw), Mark != EBreakerEnemyMark::None);
            TestFalse(FString::Printf(TEXT("Modifier %d's mark is its own"), Raw), Seen.Contains(Mark));
            Seen.AddUnique(Mark);

            const TArray<FBreakerMarkPrimitive> Shape = ShapeFor(Mark);
            TestTrue(FString::Printf(TEXT("Mark %d has geometry"), static_cast<int32>(Mark)), Shape.Num() > 0);
            for (const FBreakerMarkPrimitive& P : Shape)
            {
                const FBox2D Bounds = MarkPrimitiveBounds(P);
                TestTrue(FString::Printf(TEXT("Mark %d stays inside its cell"), static_cast<int32>(Mark)),
                    Bounds.Min.X >= -BreakerEnemyBarMathTolerance && Bounds.Min.Y >= -BreakerEnemyBarMathTolerance
                    && Bounds.Max.X <= MarkCellPx + BreakerEnemyBarMathTolerance
                    && Bounds.Max.Y <= MarkCellPx + BreakerEnemyBarMathTolerance);
            }
        }
        TestEqual(TEXT("Ten modifiers, ten marks"), Seen.Num(),
            static_cast<int32>(EBreakerEnemyModifier::Count) - static_cast<int32>(EBreakerEnemyModifier::None) - 1);
        TestTrue(TEXT("None has no geometry"), ShapeFor(EBreakerEnemyMark::None).IsEmpty());
        TestTrue(TEXT("None maps to no mark"), MarkFor(EBreakerEnemyModifier::None) == EBreakerEnemyMark::None);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
