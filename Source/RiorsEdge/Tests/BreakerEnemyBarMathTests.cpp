#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemyBarMath.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerMonsterChassis.h"

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
    TestEqual(TEXT("Border 1"), BorderPx, 1.0f);
    TestEqual(TEXT("Plate floor 5"), MinPlateH, 5.0f);
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
        TestEqual(TEXT("Boss takes the champion's width"), Boss.W, 160.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Boss takes the champion's height"), Boss.H, 8.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Standard fill inside a 1px border"), Trash.FillH, 4.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion fill inside a 1px border"), Champion.FillH, 6.0f, BreakerEnemyBarMathTolerance);
    }
    {
        const FBarSize Trash = BarSizeFor(EBreakerMonsterRank::Trash, 0.5f);
        const FBarSize Champion = BarSizeFor(EBreakerMonsterRank::ModifierBearing, 0.5f);
        TestEqual(TEXT("Trash 48 wide at the floor"), Trash.W, 48.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Trash plate floors at 5"), Trash.H, 5.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion 80 wide at the floor"), Champion.W, 80.0f, BreakerEnemyBarMathTolerance);
        TestEqual(TEXT("Champion plate floors at 5"), Champion.H, 5.0f, BreakerEnemyBarMathTolerance);
        TestTrue(TEXT("Trash fill never under 3"), Trash.FillH >= MinFillH - BreakerEnemyBarMathTolerance);
        TestTrue(TEXT("Champion fill never under 3"), Champion.FillH >= MinFillH - BreakerEnemyBarMathTolerance);
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
