#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerWaveBudget.h"

// THE WAVE BUDGET SOLVER — Encounter-Design §4.2/§4.3/§5.3. Pure maths with no
// world, which is the whole reason it lives in its own header: what a wave IS
// has to be provable without playing to wave 12.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveBudgetCurveTest,
    "RiorsEdge.Game.Waves.Budget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveBudgetCurveTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // §4.2's table, read straight off the document: Budget(n) = 6 + 4n.
    TestEqual(TEXT("Wave 1 budget"), ELib::GetWaveBudget(1, Params), 10);
    TestEqual(TEXT("Wave 2 budget"), ELib::GetWaveBudget(2, Params), 14);
    TestEqual(TEXT("Wave 3 budget"), ELib::GetWaveBudget(3, Params), 18);
    TestEqual(TEXT("Wave 4 budget"), ELib::GetWaveBudget(4, Params), 22);
    TestEqual(TEXT("Wave 5 budget"), ELib::GetWaveBudget(5, Params), 26);
    // Wave 6 is a REST wave, so it takes half of the 30 the curve gives.
    TestEqual(TEXT("Wave 6 is half budget because it rests"), ELib::GetWaveBudget(6, Params), 15);
    TestEqual(TEXT("Wave 11 budget"), ELib::GetWaveBudget(11, Params), 50);

    // Capped at 90, reached at wave 21.
    TestEqual(TEXT("Wave 21 hits the cap"), ELib::GetWaveBudget(21, Params), 90);
    TestEqual(TEXT("Wave 40 stays at the cap"), ELib::GetWaveBudget(40, Params), 90);
    TestEqual(TEXT("Wave 0 has no budget"), ELib::GetWaveBudget(0, Params), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveCadenceTest,
    "RiorsEdge.Game.Waves.Cadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveCadenceTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    TestEqual(TEXT("Wave 1 is standard"), ELib::GetWaveKind(1, Params), EBreakerWaveKind::Standard);
    TestEqual(TEXT("Wave 6 rests"), ELib::GetWaveKind(6, Params), EBreakerWaveKind::Rest);
    TestEqual(TEXT("Wave 18 rests"), ELib::GetWaveKind(18, Params), EBreakerWaveKind::Rest);

    // 12 is a multiple of BOTH intervals, and §4.2's table makes it the boss.
    // Reversing the two checks in GetWaveKind deletes the boss wave entirely,
    // which is exactly the kind of silent loss a cadence rule invites.
    TestEqual(TEXT("Wave 12 is the boss, not a rest"), ELib::GetWaveKind(12, Params), EBreakerWaveKind::Boss);
    TestEqual(TEXT("Wave 24 is the boss again"), ELib::GetWaveKind(24, Params), EBreakerWaveKind::Boss);

    // §4.3: loot only on rest and boss waves, or the gym becomes a farm and the
    // drop-rate data it exists to gather is worthless.
    TestFalse(TEXT("A standard wave drops nothing"), ELib::SolveWave(5, 1, Params).bDropsLoot);
    TestTrue(TEXT("A rest wave drops"), ELib::SolveWave(6, 1, Params).bDropsLoot);
    TestTrue(TEXT("A boss wave drops"), ELib::SolveWave(12, 1, Params).bDropsLoot);

    // A rest wave takes no elites (§4.2's table: wave 6, "0 elites"). The beat
    // exists so the player stops READING, and a modifier set is the most
    // reading the game asks for.
    TestEqual(TEXT("A rest wave carries no elite"), ELib::SolveWave(6, 1, Params).Elites, 0);
    // Wave 18 would otherwise take one on floor(18/4).
    TestEqual(TEXT("Wave 18 rests and still carries no elite"), ELib::SolveWave(18, 1, Params).Elites, 0);

    // The boss wave is the boss alone.
    const FBreakerWaveComposition Boss = ELib::SolveWave(12, 1, Params);
    TestTrue(TEXT("Wave 12 spawns the boss"), Boss.bBoss);
    TestEqual(TEXT("Wave 12 spawns nothing else"), Boss.TotalEnemies(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveCompositionTest,
    "RiorsEdge.Game.Waves.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveCompositionTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // §4.2's first row, exactly: "1 | 10 | 10 Skitter | 0 elites".
    const FBreakerWaveComposition One = ELib::SolveWave(1, 1, Params);
    TestEqual(TEXT("Wave 1 is ten Skitters"), One.Skitters, 10);
    TestEqual(TEXT("Wave 1 has no ranged"), One.RangedSources(), 0);
    TestEqual(TEXT("Wave 1 has no Warden"), One.Wardens, 0);
    TestEqual(TEXT("Wave 1 has no elite"), One.Elites, 0);
    TestEqual(TEXT("Wave 1 spends its whole budget"), One.UnspentBudget, 0);

    // Introduction order: Lattice from 2, Warden from 3, Skirmisher from 4.
    // Nothing appears before its wave, which is what makes each archetype a
    // teaching beat rather than noise.
    TestEqual(TEXT("No Lattice before wave 2"), ELib::SolveWave(1, 1, Params).Lattices, 0);
    TestTrue(TEXT("A Lattice from wave 2"), ELib::SolveWave(2, 1, Params).Lattices > 0);
    TestEqual(TEXT("No Warden before wave 3"), ELib::SolveWave(2, 1, Params).Wardens, 0);
    TestEqual(TEXT("A Warden from wave 3"), ELib::SolveWave(3, 1, Params).Wardens, 1);
    TestEqual(TEXT("No Skirmisher before wave 4"), ELib::SolveWave(3, 1, Params).Skirmishers, 0);
    TestTrue(TEXT("A Skirmisher from wave 4"), ELib::SolveWave(4, 1, Params).Skirmishers > 0);

    // §4.2: elite count = floor(wave/4), and §5.3 caps live elites at 1 solo.
    TestEqual(TEXT("No elite at wave 3"), ELib::SolveWave(3, 1, Params).Elites, 0);
    TestEqual(TEXT("One elite at wave 4"), ELib::SolveWave(4, 1, Params).Elites, 1);
    TestEqual(TEXT("Still one elite at wave 20, solo"), ELib::SolveWave(20, 1, Params).Elites, 1);
    TestTrue(TEXT("An elite carries at least one modifier"), ELib::SolveWave(4, 1, Params).ModifiersPerElite >= 1);
    TestTrue(TEXT("Modifiers per elite never exceed the composition ceiling of 3"),
        ELib::SolveWave(60, 1, Params).ModifiersPerElite <= 3);

    // §5.3, every cap, across the whole repeating cycle and past the budget cap.
    for (int32 Wave = 1; Wave <= 48; ++Wave)
    {
        const FBreakerWaveComposition Composition = ELib::SolveWave(Wave, 1, Params);
        FString Reason;
        if (!ELib::IsCompositionLegal(Composition, 1, Params, Reason))
        {
            AddError(FString::Printf(TEXT("Wave %d is illegal: %s (%s)"),
                Wave, *Reason, *ELib::DescribeComposition(Composition)));
        }
        // Restated directly rather than only through the legality helper, so a
        // bug in the helper cannot make the caps look enforced.
        TestTrue(TEXT("Density ceiling of 12 solo"), Composition.TotalEnemies() <= 12);
        TestTrue(TEXT("Never more than 3 ranged sources"), Composition.RangedSources() <= 3);
        TestTrue(TEXT("Never more than one Warden-class anchor"),
            Composition.Wardens + (Composition.bBoss ? 1 : 0) <= 1);
        TestTrue(TEXT("Never more than one elite solo"), Composition.Elites <= 1);
        TestTrue(TEXT("Never overspends"), Composition.SpentBudget <= Composition.Budget);
        TestTrue(TEXT("A wave is never empty"), Composition.TotalEnemies() >= 1);
    }

    // §4.3's variety rule, checked where it bites: after wave 3 no archetype
    // may take more than 70% of the budget. Skitters are the only archetype
    // that can run away with one, because they are the cheap one.
    for (int32 Wave = 4; Wave <= 24; ++Wave)
    {
        const FBreakerWaveComposition Composition = ELib::SolveWave(Wave, 1, Params);
        if (Composition.Kind != EBreakerWaveKind::Standard) continue;
        TestTrue(TEXT("Skitters stay inside the 70% budget share"),
            Composition.Skitters * Params.SkitterCost
                <= FMath::FloorToInt32(Composition.Budget * Params.MaximumSingleArchetypeShare));
    }

    // Deterministic: the same wave solves the same way every time, which is
    // what makes a TTK sample from wave 7 comparable with last session's.
    TestEqual(TEXT("Solving twice gives the same head count"),
        ELib::SolveWave(9, 1, Params).TotalEnemies(), ELib::SolveWave(9, 1, Params).TotalEnemies());

    // Party scaling goes through the same solver. §5.3: 3 elites at five
    // players, and the ranged cap does NOT scale with party size.
    TestEqual(TEXT("Five players allow three elites"), ELib::GetMaximumElites(5, Params), 3);
    TestEqual(TEXT("Solo allows one elite"), ELib::GetMaximumElites(1, Params), 1);
    TestTrue(TEXT("The ranged cap holds at five players"),
        ELib::SolveWave(20, 5, Params).RangedSources() <= 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveBudgetCollisionTest,
    "RiorsEdge.Game.Waves.CapCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveBudgetCollisionTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // A FINDING, pinned by test so it cannot quietly stop being true.
    // Encounter-Design §4.2's budget curve and §5.3's density caps DISAGREE:
    // under 12 live enemies, 3 ranged sources, 1 Warden and 1 elite, a solo
    // wave physically cannot spend much past the mid thirties, while the curve
    // keeps climbing to 90. The caps win here, because they are the ones with
    // reasons written beside them — and the solver reports the shortfall rather
    // than resolving the contradiction silently in either direction.
    TestEqual(TEXT("Early waves spend everything"), ELib::SolveWave(1, 1, Params).UnspentBudget, 0);
    TestTrue(TEXT("Late waves cannot spend their budget under the 5.3 caps"),
        ELib::SolveWave(20, 1, Params).UnspentBudget > 0);
    TestTrue(TEXT("The shortfall grows with the wave"),
        ELib::SolveWave(30, 1, Params).UnspentBudget >= ELib::SolveWave(20, 1, Params).UnspentBudget);

    // The caps are never broken to spend it, which is the half that matters.
    const FBreakerWaveComposition Late = ELib::SolveWave(40, 1, Params);
    TestTrue(TEXT("A capped wave is still legal"), Late.TotalEnemies() <= 12);

    return true;
}

#endif
