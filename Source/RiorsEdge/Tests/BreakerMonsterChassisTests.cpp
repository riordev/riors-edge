#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerMonsterChassis.h"

namespace BreakerChassisTest
{
    // Health at area level 100 is in the millions, where a single-precision ulp
    // is larger than any sane absolute tolerance, so ratio checks compare
    // RELATIVELY. Prefixed rather than bare because a unity build concatenates
    // translation units and a bare helper name would collide.
    static bool BreakerChassisNearlyProportional(float Actual, float Expected)
    {
        const float Scale = FMath::Max(FMath::Abs(Expected), 1.0f);
        return FMath::Abs(Actual - Expected) <= Scale * 1.0e-4f;
    }
}

// The monster chassis (O27 / Power-Curve.md §2). The curve itself is pure
// maths with no world and no actor, which is the whole reason it lives in its
// own header — the thing that MUST be provable is that a monster's difficulty
// is a function of the CONTENT and nothing else.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerChassisCurveTest,
    "RiorsEdge.Combat.Chassis.Curve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerChassisCurveTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerMonsterChassisLibrary;
    const FBreakerMonsterChassisParams Params;

    // Area level 1 IS the authored base: the curve is anchored on the chassis
    // session 5 actually measured, so nothing silently retuned when it landed.
    TestEqual(TEXT("Area level 1 is the base health"), ELib::GetChassisHealth(1, Params), Params.BaseHealth, 0.01f);
    TestEqual(TEXT("Area level 1 is the base damage"), ELib::GetChassisDamage(1, Params), Params.BaseDamage, 0.01f);

    // Geometric, not linear: each level multiplies by exactly (1 + g).
    for (int32 Level = 1; Level < 50; ++Level)
    {
        const float Ratio = ELib::GetChassisHealth(Level + 1, Params) / ELib::GetChassisHealth(Level, Params);
        TestEqual(*FString::Printf(TEXT("Health ratio at level %d is 1 + g"), Level),
            Ratio, 1.0f + Params.HealthGrowthPerLevel, 0.001f);
    }

    // Monotonic strictly upward in both curves over the whole authored range.
    for (int32 Level = 1; Level < ELib::BreakerMaxAreaLevel; ++Level)
    {
        TestTrue(*FString::Printf(TEXT("Health rises from level %d"), Level),
            ELib::GetChassisHealth(Level + 1, Params) > ELib::GetChassisHealth(Level, Params));
        TestTrue(*FString::Printf(TEXT("Damage rises from level %d"), Level),
            ELib::GetChassisDamage(Level + 1, Params) > ELib::GetChassisDamage(Level, Params));
    }

    // Incoming damage MUST scale slower than health. If it does not, defence
    // has to grow as fast as offence and every build becomes a defensive
    // build — the reason Power-Curve.md states d materially below g.
    TestTrue(TEXT("Damage growth is materially below health growth"),
        Params.DamageGrowthPerLevel < Params.HealthGrowthPerLevel * 0.8f);
    TestTrue(TEXT("Health outruns damage across the range"),
        (ELib::GetChassisHealth(50, Params) / ELib::GetChassisHealth(1, Params))
        > (ELib::GetChassisDamage(50, Params) / ELib::GetChassisDamage(1, Params)) * 2.0f);

    // The doc's stated shape: g = 9% is about x67 over 50 levels.
    const float FiftyLevelRatio = ELib::GetChassisHealth(50, Params) / ELib::GetChassisHealth(1, Params);
    TestTrue(TEXT("Fifty levels multiply health by roughly 67x"),
        FiftyLevelRatio > 60.0f && FiftyLevelRatio < 75.0f);

    // Area level is clamped, never wrapped or negative.
    TestEqual(TEXT("Level 0 clamps to the minimum"), ELib::GetChassisHealth(0, Params), Params.BaseHealth, 0.01f);
    TestEqual(TEXT("Level -50 clamps to the minimum"), ELib::GetChassisHealth(-50, Params), Params.BaseHealth, 0.01f);
    TestEqual(TEXT("Absurd levels clamp to the maximum"),
        ELib::GetChassisHealth(9999, Params), ELib::GetChassisHealth(ELib::BreakerMaxAreaLevel, Params), 0.01f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerChassisRankTest,
    "RiorsEdge.Combat.Chassis.Rank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerChassisRankTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerMonsterChassisLibrary;
    const FBreakerMonsterChassisParams Params;

    // Rank MULTIPLIES the chassis; it does not replace it. That is the
    // property that makes rank a difficulty axis rather than a second table of
    // hardcoded health numbers, and it is checked at several area levels
    // because a rank that only held at one level would be a replacement.
    using BreakerChassisTest::BreakerChassisNearlyProportional;
    for (const int32 Level : { 1, 7, 25, 50, 80 })
    {
        const float Trash = ELib::GetMonsterHealth(Level, EBreakerMonsterRank::Trash, Params);
        TestTrue(*FString::Printf(TEXT("Trash is the bare chassis at %d"), Level),
            BreakerChassisNearlyProportional(Trash, ELib::GetChassisHealth(Level, Params)));
        TestTrue(*FString::Printf(TEXT("Elite is the elite ratio at %d"), Level),
            BreakerChassisNearlyProportional(ELib::GetMonsterHealth(Level, EBreakerMonsterRank::Elite, Params),
                Trash * Params.EliteHealthMultiplier));
        TestTrue(*FString::Printf(TEXT("Modifier-bearing is its ratio at %d"), Level),
            BreakerChassisNearlyProportional(ELib::GetMonsterHealth(Level, EBreakerMonsterRank::ModifierBearing, Params),
                Trash * Params.ModifierHealthMultiplier));
        TestTrue(*FString::Printf(TEXT("Boss is its ratio at %d"), Level),
            BreakerChassisNearlyProportional(ELib::GetMonsterHealth(Level, EBreakerMonsterRank::Boss, Params),
                Trash * Params.BossHealthMultiplier));
    }

    // The rank ORDER is the design statement and is pinned independently of
    // the values, so a retune cannot accidentally make an elite squishier than
    // trash or a boss softer than an elite.
    TestTrue(TEXT("Elite is tougher than trash"),
        ELib::GetRankHealthMultiplier(EBreakerMonsterRank::Elite, Params)
        > ELib::GetRankHealthMultiplier(EBreakerMonsterRank::Trash, Params));
    TestTrue(TEXT("Boss is tougher than elite"),
        ELib::GetRankHealthMultiplier(EBreakerMonsterRank::Boss, Params)
        > ELib::GetRankHealthMultiplier(EBreakerMonsterRank::Elite, Params));
    TestTrue(TEXT("Modifier-bearing sits between trash and elite"),
        ELib::GetRankHealthMultiplier(EBreakerMonsterRank::ModifierBearing, Params)
        > ELib::GetRankHealthMultiplier(EBreakerMonsterRank::Trash, Params)
        && ELib::GetRankHealthMultiplier(EBreakerMonsterRank::ModifierBearing, Params)
        < ELib::GetRankHealthMultiplier(EBreakerMonsterRank::Elite, Params));

    // Rank damage rises with rank but far more gently than rank health: the
    // threat of an elite is that it survives your build, not that it one-shots.
    for (const EBreakerMonsterRank Rank : { EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss })
    {
        TestTrue(TEXT("Rank damage never outruns rank health"),
            ELib::GetRankDamageMultiplier(Rank, Params) < ELib::GetRankHealthMultiplier(Rank, Params));
        TestTrue(TEXT("Rank damage is at least trash damage"),
            ELib::GetRankDamageMultiplier(Rank, Params) >= 1.0f);
    }

    // The rank ratios are derivable from O18's targets rather than guessed:
    // trash <1s and elite ~3s IS an elite health ratio of 3, and a boss at
    // 20-45s against a 1s trash is a ratio inside 20-45.
    TestTrue(TEXT("Elite ratio sits in the doc's x3-4 band"),
        Params.EliteHealthMultiplier >= 3.0f && Params.EliteHealthMultiplier <= 4.0f);
    TestTrue(TEXT("Boss ratio sits in the doc's x20-40 band"),
        Params.BossHealthMultiplier >= 20.0f && Params.BossHealthMultiplier <= 40.0f);
    TestTrue(TEXT("Modifier-bearing ratio sits in the doc's x2-3 band"),
        Params.ModifierHealthMultiplier >= 2.0f && Params.ModifierHealthMultiplier <= 3.0f);

    // The archetype multiplier composes on top of rank without disturbing it —
    // Encounter-Design §2.2's 1.6x Lattice is the first user.
    const float LatticeTrash = ELib::GetMonsterHealth(12, EBreakerMonsterRank::Trash, Params, 1.6f);
    TestTrue(TEXT("Archetype multiplier composes with the chassis"),
        BreakerChassisNearlyProportional(LatticeTrash, ELib::GetChassisHealth(12, Params) * 1.6f));
    TestTrue(TEXT("Archetype and rank compose independently"),
        BreakerChassisNearlyProportional(ELib::GetMonsterHealth(12, EBreakerMonsterRank::Elite, Params, 1.6f),
            LatticeTrash * Params.EliteHealthMultiplier));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerChassisContentScaledTest,
    "RiorsEdge.Combat.Chassis.ContentScaled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerChassisContentScaledTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerMonsterChassisLibrary;
    const FBreakerMonsterChassisParams Params;

    // THE ruling. A monster's chassis is a function of the content and of
    // nothing else, so the same (area level, rank, params) triple must produce
    // the same numbers every time it is asked — no hidden state, no session
    // history, and structurally no way to consult a player, because none of
    // these functions can see one.
    for (const int32 Level : { 1, 3, 10, 22, 37, 50, 100 })
    {
        const float First = ELib::GetMonsterHealth(Level, EBreakerMonsterRank::Elite, Params, 1.6f);
        for (int32 Repeat = 0; Repeat < 8; ++Repeat)
        {
            TestEqual(TEXT("The chassis is stable across calls"),
                ELib::GetMonsterHealth(Level, EBreakerMonsterRank::Elite, Params, 1.6f), First, 0.0f);
        }
    }

    // Two areas at the same level are identical regardless of anything else
    // that might differ about the run.
    TestEqual(TEXT("Same area level, same trash health"),
        ELib::GetMonsterHealth(18, EBreakerMonsterRank::Trash, Params),
        ELib::GetMonsterHealth(18, EBreakerMonsterRank::Trash, Params), 0.0f);

    // Drop item level follows area level — that is the mechanism by which
    // rising item level corresponds to gameplay (O27) — but stops at the
    // character cap of 50 because affix tiers are authored to 50.
    TestEqual(TEXT("Drop item level follows area level"), ELib::GetDropItemLevel(23), 23);
    TestEqual(TEXT("Drop item level clamps at the character cap"), ELib::GetDropItemLevel(80), 50);
    TestEqual(TEXT("Drop item level has a floor of 1"), ELib::GetDropItemLevel(0), 1);
    for (int32 Level = 1; Level < ELib::BreakerMaxAreaLevel; ++Level)
    {
        TestTrue(TEXT("Drop item level never falls as area level rises"),
            ELib::GetDropItemLevel(Level + 1) >= ELib::GetDropItemLevel(Level));
    }

    // The reporting helper is arithmetic only and takes a MEASURED dps: it
    // must never be able to derive one.
    TestEqual(TEXT("TTK is health over dps"), ELib::GetTimeToKillSeconds(220.0f, 110.0f), 2.0f, 0.001f);
    TestTrue(TEXT("Zero dps never kills"), ELib::GetTimeToKillSeconds(220.0f, 0.0f) > 1.0e9f);

    // Session 5's measurement, restated: 220 health in 1.81s is ~121.5 dps for
    // an unchanged player. At area level 1 the curve reproduces that number
    // exactly, so the chassis change is provably feel-neutral at the anchor
    // and every deviation above it is the curve, not a retune.
    const float MeasuredDps = 220.0f / 1.81f;
    TestEqual(TEXT("Area level 1 reproduces the measured trash TTK"),
        ELib::GetTimeToKillSeconds(ELib::GetMonsterHealth(1, EBreakerMonsterRank::Trash, Params), MeasuredDps),
        1.81f, 0.01f);

    return true;
}

#endif
