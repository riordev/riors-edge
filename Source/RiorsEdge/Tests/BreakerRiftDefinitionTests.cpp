#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemy.h"
#include "Game/BreakerRiftDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

// The rift's derived readouts are pure functions of the same libraries the
// game spawns from, so the loading screen can never disagree with the world
// it fronts. This pins the derivations, including the ruled baseline: the
// monster multipliers are ratios against AREA LEVEL 1, the only baseline
// that doesn't move.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftDerivationTest,
    "RiorsEdge.Game.RiftDefinition.DerivationsMatchTheSpawningLibraries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftDerivationTest::RunTest(const FString& Parameters)
{
    const FBreakerMonsterChassisParams Params;   // shipped defaults

    // --- Baseline: area level 1 multiplies nothing ---------------------------
    TestTrue(TEXT("Health multiplier at area level 1 is exactly 1"),
        FMath::IsNearlyEqual(UBreakerRiftLibrary::GetMonsterHealthMultiplier(1, Params), 1.0f));
    TestTrue(TEXT("Damage multiplier at area level 1 is exactly 1"),
        FMath::IsNearlyEqual(UBreakerRiftLibrary::GetMonsterDamageMultiplier(1, Params), 1.0f));

    // --- The multiplier IS the curve ratio, chassis base cancelled -----------
    for (const int32 Level : {14, 40, 100})
    {
        const float Expected = UBreakerMonsterChassisLibrary::GetChassisHealth(Level, Params)
            / UBreakerMonsterChassisLibrary::GetChassisHealth(1, Params);
        TestTrue(FString::Printf(TEXT("Health multiplier at %d matches the chassis ratio"), Level),
            FMath::IsNearlyEqual(UBreakerRiftLibrary::GetMonsterHealthMultiplier(Level, Params), Expected, 0.001f));
        FBreakerMonsterChassisParams Doubled = Params;
        Doubled.BaseHealth = Params.BaseHealth * 2.0f;
        TestTrue(TEXT("The multiplier is independent of the chassis base"),
            FMath::IsNearlyEqual(UBreakerRiftLibrary::GetMonsterHealthMultiplier(Level, Doubled),
                UBreakerRiftLibrary::GetMonsterHealthMultiplier(Level, Params), 0.001f));
    }
    TestTrue(TEXT("The curve rises"),
        UBreakerRiftLibrary::GetMonsterHealthMultiplier(50, Params)
        > UBreakerRiftLibrary::GetMonsterHealthMultiplier(10, Params));

    // --- The item-level range is the drop pipeline's own answer --------------
    // Floor is GetDropItemLevel (identity across the area band), ceiling adds
    // the enemy CDO's authored elite bonus — read from the same default the
    // spawned enemy reads, never transcribed.
    const int32 EliteBonus = GetDefault<ABreakerEnemy>()->GetEliteDropItemLevelBonus();
    int32 Min = 0, Max = 0;
    UBreakerRiftLibrary::GetDropItemLevelRange(14, EliteBonus, Min, Max);
    TestEqual(TEXT("The floor at area level 14 is the pipeline's own centre"),
        Min, UBreakerMonsterChassisLibrary::GetDropItemLevel(14));
    TestEqual(TEXT("The ceiling adds the elite bonus"), Max, Min + FMath::Max(EliteBonus, 0));

    // The ladder clamp binds at the top exactly as ApplyChassis's does.
    UBreakerRiftLibrary::GetDropItemLevelRange(100, 999, Min, Max);
    TestEqual(TEXT("The ceiling can never leave the affix ladder"), Max, UBreakerAffixLibrary::MaxItemLevel);

    // --- The definition's own clamp ------------------------------------------
    FBreakerRiftDefinition Rift;
    TestFalse(TEXT("A fresh definition is unset"), Rift.IsSet());
    Rift.AreaLevel = 250;
    TestEqual(TEXT("The effective level clamps into the area band"), Rift.EffectiveAreaLevel(), 100);
    return true;
}

#endif
