#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerWaveBudget.h"
#include "Items/BreakerDropTable.h"

// ---------------------------------------------------------------------------
// THE RIFT RUN'S DROP PROFILE (One-AB's race item, taken by LEDGER). The seat
// estimated a run at 26 trash / 3 promoted / 1 boss and asked for the real
// figure from SolveWave. This file IS that figure, asserted so it cannot
// drift back into prose: the solver here is the same one the spawner calls,
// at the rift's boss interval (RiftBossWave defaults to 3 on the game mode;
// the interval is restated here because the game mode keeps it internal, and
// if the owner moves it this file describes interval-3 behaviour truthfully
// while the report names the dependency).
//
// The finding these rows encode: a rift run fields ZERO elites and ZERO
// modifier carriers, because both unlock at wave 4 and the run ends at 3.
// Every promoted-rank drop chance (elite 0.75, modifier-bearing 0.90) is
// therefore unreachable in the player's actual loot loop today, and O27's
// ModifierBearing kill bucket is structurally empty inside rifts.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftRunCompositionTest,
    "RiorsEdge.Items.RiftRunDropProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftRunCompositionTest::RunTest(const FString& Parameters)
{
    FBreakerWaveBudgetParams Params;   // the shipped defaults
    Params.BossWaveInterval = 3;       // RiftBossWave's default (the rift build)

    // Wave 1: budget 10, nothing but Skitters is unlocked yet.
    const FBreakerWaveComposition One = UBreakerWaveBudgetLibrary::SolveWave(1, 1, Params);
    TestEqual(TEXT("wave 1 is ten Skitters"), One.Skitters, 10);
    TestEqual(TEXT("wave 1 has no other bodies"), One.TotalEnemies(), 10);
    TestEqual(TEXT("wave 1 spends everything"), One.UnspentBudget, 0);

    // Wave 2: budget 14, one Lattice joins and the fill meets the 12-body
    // density ceiling.
    const FBreakerWaveComposition Two = UBreakerWaveBudgetLibrary::SolveWave(2, 1, Params);
    TestEqual(TEXT("wave 2 carries one Lattice"), Two.Lattices, 1);
    TestEqual(TEXT("wave 2 fills to the density ceiling"), Two.TotalEnemies(), 12);

    // Wave 3: the boss, alone — it deploys its own adds at its own source.
    const FBreakerWaveComposition Three = UBreakerWaveBudgetLibrary::SolveWave(3, 1, Params);
    TestTrue(TEXT("wave 3 is the boss"), Three.bBoss);
    TestEqual(TEXT("the boss wave spawns nothing beside it"), Three.TotalEnemies(), 1);

    // The finding: no promoted body anywhere in the run.
    TestEqual(TEXT("a rift run fields zero elites"), One.Elites + Two.Elites + Three.Elites, 0);
    TestEqual(TEXT("a rift run fields zero modifier carriers"),
        One.ModifierCarriers + Two.ModifierCarriers + Three.ModifierCarriers, 0);

    // The figure the seat asked for, from the authored chances: 22 trash-rank
    // kills at 0.10 plus the boss at 1.0 — about 3.2 items a run once every
    // rift wave drops, so the 25-slot backpack fills in about 7.8 runs, not
    // the estimate's 4.3. Asserted from the same defaults the game constructs.
    const FBreakerDropTableParams Drops;
    const int32 TrashKills = One.TotalEnemies() + Two.TotalEnemies();
    const float ExpectedItems = TrashKills * Drops.TrashDropChance + 1.0f * Drops.BossDropChance;
    TestEqual(TEXT("22 trash-rank kills across waves 1-2"), TrashKills, 22);
    TestTrue(TEXT("expected items a run sits near 3.2"), FMath::IsNearlyEqual(ExpectedItems, 3.2f, 0.01f));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
