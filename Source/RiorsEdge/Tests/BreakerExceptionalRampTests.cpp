#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Items/BreakerDropTable.h"

// ---------------------------------------------------------------------------
// THE EARLY-LEVEL EXCEPTIONAL RAMP.
//
// Owner: "exceptionals basically drop instantly which they shouldnt (i think
// we should reduce their drop rate a little bit in the earlier levels 1-13)".
//
// A hard gate would have been the wrong reading of that: he asked for LESS,
// not for none, and the unlock at item level 8 already answers "none" for
// 1 to 7. So what is asserted here is the shape of the reduction — thinner
// where he said, unchanged where he did not, and never able to go the other
// way and make an early Exceptional MORE likely than a late one.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerExceptionalRampTest,
    "RiorsEdge.Items.Drops.ExceptionalEarlyRamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerExceptionalRampTest::RunTest(const FString& Parameters)
{
    const FBreakerDropTableParams Params;
    const auto Scalar = [&Params](int32 Level)
    {
        return UBreakerDropTableLibrary::ExceptionalWeightScalar(Level, Params);
    };

    // ---- THE SHIPPED CONFIGURATION COVERS THE RANGE HE NAMED -------------
    // The gate empties 1-7 and the ramp thins 8-13. If either dial moves so
    // that they no longer meet, some band of levels gets the full weight the
    // owner said was too much, and nothing else would notice.
    TestTrue(TEXT("the ramp starts where the gate stops"),
        Params.ExceptionalFullWeightItemLevel > Params.ExceptionalMinimumItemLevel);
    TestEqual(TEXT("and full weight arrives at 14, so 1-13 is the whole thinned range"),
        Params.ExceptionalFullWeightItemLevel, 14);
    TestTrue(TEXT("the early floor actually reduces something"),
        Params.ExceptionalEarlyFloor > 0.0f && Params.ExceptionalEarlyFloor < 1.0f);

    // ---- BELOW THE UNLOCK, NOTHING --------------------------------------
    for (int32 Level = 1; Level < Params.ExceptionalMinimumItemLevel; ++Level)
    {
        TestEqual(*FString::Printf(TEXT("level %d rolls no Exceptional at all"), Level), Scalar(Level), 0.0f);
    }

    // ---- ACROSS THE RAMP, THINNER AND RISING -----------------------------
    TestEqual(TEXT("the unlock level itself carries only the floor"),
        Scalar(Params.ExceptionalMinimumItemLevel), Params.ExceptionalEarlyFloor, 0.001f);
    float Previous = Scalar(Params.ExceptionalMinimumItemLevel);
    for (int32 Level = Params.ExceptionalMinimumItemLevel + 1;
         Level < Params.ExceptionalFullWeightItemLevel; ++Level)
    {
        const float Here = Scalar(Level);
        TestTrue(*FString::Printf(TEXT("level %d is thinner than the full table"), Level), Here < 1.0f);
        TestTrue(*FString::Printf(TEXT("and richer than level %d"), Level - 1), Here > Previous);
        Previous = Here;
    }

    // ---- AND UNTOUCHED ABOVE IT -----------------------------------------
    // The owner named 1-13 and nothing else. A ramp that kept climbing past
    // there would be a silent nerf to content he did not complain about.
    for (int32 Level = Params.ExceptionalFullWeightItemLevel; Level <= 120; ++Level)
    {
        if (!TestEqual(*FString::Printf(TEXT("level %d carries the full authored weight"), Level),
            Scalar(Level), 1.0f)) return false;
    }

    // ---- THE DIALS DEGRADE SANELY ---------------------------------------
    {
        // A full level at or below the unlock turns the ramp OFF rather than
        // dividing by nothing: the failure a designer wants from a zeroed dial
        // is the old behaviour, not a crash or an inverted curve.
        FBreakerDropTableParams Off = Params;
        Off.ExceptionalFullWeightItemLevel = Off.ExceptionalMinimumItemLevel;
        TestEqual(TEXT("a collapsed ramp restores the pre-ruling table"),
            UBreakerDropTableLibrary::ExceptionalWeightScalar(Off.ExceptionalMinimumItemLevel, Off), 1.0f);
        Off.ExceptionalFullWeightItemLevel = 1;
        TestEqual(TEXT("and so does an inverted one"),
            UBreakerDropTableLibrary::ExceptionalWeightScalar(Off.ExceptionalMinimumItemLevel, Off), 1.0f);
    }
    {
        // A floor outside the unit range is clamped rather than trusted: a
        // scalar above 1 would make an early Exceptional MORE likely than a
        // late one, which is the opposite of the ruling.
        FBreakerDropTableParams Wild = Params;
        Wild.ExceptionalEarlyFloor = 4.0f;
        const float High = UBreakerDropTableLibrary::ExceptionalWeightScalar(
            Wild.ExceptionalMinimumItemLevel, Wild);
        TestTrue(TEXT("an over-set floor cannot exceed the full weight"), High <= 1.0f);
        Wild.ExceptionalEarlyFloor = -2.0f;
        const float Low = UBreakerDropTableLibrary::ExceptionalWeightScalar(
            Wild.ExceptionalMinimumItemLevel, Wild);
        TestTrue(TEXT("and an under-set one cannot go negative"), Low >= 0.0f);
    }

    // ---- IT ACTUALLY REACHES THE ROLL ------------------------------------
    // The scalar could be perfect and unused. This is the assertion that the
    // shipped table really is thinner early: measured through the analytic
    // projection, which shares the weight path with the roll precisely so the
    // two cannot disagree about what a level drops.
    {
        // TRASH ONLY. Exceptional is the one top rarity reachable from trash,
        // so a trash-only hour is where the ramp is actually felt — an elite
        // kill would drag the figure around for reasons this is not about.
        FBreakerKillRateSample Kills;
        Kills.TrashKillsPerHour = 600.0f;
        Kills.EliteKillsPerHour = 0.0f;
        Kills.ModifierBearingKillsPerHour = 0.0f;
        Kills.BossKillsPerHour = 0.0f;
        const FBreakerLootRateProjection Early =
            UBreakerDropTableLibrary::ProjectLootRate(Kills, Params.ExceptionalMinimumItemLevel, 0.0f, Params);
        const FBreakerLootRateProjection Late =
            UBreakerDropTableLibrary::ProjectLootRate(Kills, Params.ExceptionalFullWeightItemLevel, 0.0f, Params);
        AddInfo(FString::Printf(TEXT("EXCEPTIONAL RAMP  ilvl %d pays %.2f/hr, ilvl %d pays %.2f/hr"),
            Params.ExceptionalMinimumItemLevel, Early.ExceptionalOrBetterPerHour,
            Params.ExceptionalFullWeightItemLevel, Late.ExceptionalOrBetterPerHour));
        TestTrue(TEXT("an early yard really does pay fewer Exceptionals than a later one"),
            Early.ExceptionalOrBetterPerHour < Late.ExceptionalOrBetterPerHour);
        TestTrue(TEXT("and still pays some, because he asked for fewer and not for none"),
            Early.ExceptionalOrBetterPerHour > 0.0f);
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
