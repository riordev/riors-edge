#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Playtest/BreakerKillBuckets.h"
#include "Playtest/BreakerPlaytestComponent.h"

// TTK BUCKETING. Pure logic with no world, which is the whole reason it lives
// in its own header: the instrument that the O2 value freeze is waiting on must
// be provable without a playtest.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKillBucketClassifyTest,
    "RiorsEdge.Playtest.KillBuckets.Classify",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKillBucketClassifyTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerKillBucketLibrary;

    TestEqual(TEXT("Trash melee"),
        ELib::ClassifyKill(EBreakerMonsterRank::Trash, false, 0), EBreakerKillBucket::MeleeTrash);
    TestEqual(TEXT("Trash ranged"),
        ELib::ClassifyKill(EBreakerMonsterRank::Trash, true, 0), EBreakerKillBucket::RangedTrash);
    TestEqual(TEXT("Elite"),
        ELib::ClassifyKill(EBreakerMonsterRank::Elite, false, 0), EBreakerKillBucket::Elite);

    // THE BUG THIS FILE EXISTS FOR. A Champion's rank is ModifierBearing, so
    // IsElite() is FALSE for it — the two-boolean call filed it under melee
    // trash, not (as the handover assumed) under elite. Same for the boss.
    TestEqual(TEXT("Modifier-bearing is its own bucket, not trash"),
        ELib::ClassifyKill(EBreakerMonsterRank::ModifierBearing, false, 2), EBreakerKillBucket::ModifierBearing);
    TestEqual(TEXT("Boss is its own bucket, not trash"),
        ELib::ClassifyKill(EBreakerMonsterRank::Boss, false, 0), EBreakerKillBucket::Boss);

    // A modifier-bearing RANGED enemy is still a modifier kill: what it costs
    // to kill is set by the rank multiplier, not by the range it was fought at.
    TestEqual(TEXT("Rank beats the ranged flag"),
        ELib::ClassifyKill(EBreakerMonsterRank::ModifierBearing, true, 1), EBreakerKillBucket::ModifierBearing);

    // The gym's arena anchor: an ELITE that also carries modifiers. Its health
    // came from the elite rank row (x3.0), not the modifier row (x2.5), so it
    // is an elite sample. Rank multiplies the chassis; modifiers change the verb.
    TestEqual(TEXT("An elite carrying modifiers stays an elite sample"),
        ELib::ClassifyKill(EBreakerMonsterRank::Elite, false, 3), EBreakerKillBucket::Elite);

    // A modifier count alone promotes, even if a spawner restored a rank.
    TestEqual(TEXT("A nonzero modifier count alone is enough"),
        ELib::ClassifyKill(EBreakerMonsterRank::Trash, false, 1), EBreakerKillBucket::ModifierBearing);

    // The legacy two-boolean path can still only name the three old buckets,
    // and that limit is the point: anything wanting the other two must say so.
    TestEqual(TEXT("Legacy melee"), ELib::ClassifyLegacyKill(false, false), EBreakerKillBucket::MeleeTrash);
    TestEqual(TEXT("Legacy ranged"), ELib::ClassifyLegacyKill(false, true), EBreakerKillBucket::RangedTrash);
    TestEqual(TEXT("Legacy elite beats ranged"), ELib::ClassifyLegacyKill(true, true), EBreakerKillBucket::Elite);

    // Names are a column heading in the clipboard report; they must be stable
    // and distinct or two populations merge again in the owner's feedback log.
    TSet<FString> Names;
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerKillBucket::Count); ++Index)
    {
        Names.Add(ELib::GetKillBucketName(static_cast<EBreakerKillBucket>(Index)));
    }
    TestEqual(TEXT("Every bucket has a distinct name"), Names.Num(), static_cast<int32>(EBreakerKillBucket::Count));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerKillBucketRoutingTest,
    "RiorsEdge.Playtest.KillBuckets.Routing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerKillBucketRoutingTest::RunTest(const FString& Parameters)
{
    // A sample must land in exactly ONE array. The failure this guards against
    // is the one that has now happened twice: two populations averaged together
    // produce a number that describes neither.
    FBreakerPlaytestStats Stats;
    for (int32 Index = 0; Index < static_cast<int32>(EBreakerKillBucket::Count); ++Index)
    {
        Stats.SamplesForBucket(static_cast<EBreakerKillBucket>(Index)).Add(1.0f + Index);
    }

    TestEqual(TEXT("Melee bucket took one sample"), Stats.TimeToKillSamples.Num(), 1);
    TestEqual(TEXT("Ranged bucket took one sample"), Stats.RangedTimeToKillSamples.Num(), 1);
    TestEqual(TEXT("Elite bucket took one sample"), Stats.EliteTimeToKillSamples.Num(), 1);
    TestEqual(TEXT("Modifier bucket took one sample"), Stats.ModifierTimeToKillSamples.Num(), 1);
    TestEqual(TEXT("Boss bucket took one sample"), Stats.BossTimeToKillSamples.Num(), 1);

    // And the values are not cross-wired: bucket N holds 1 + N.
    TestEqual(TEXT("Melee holds its own value"), Stats.TimeToKillSamples[0], 1.0f);
    TestEqual(TEXT("Boss holds its own value"), Stats.BossTimeToKillSamples[0],
        1.0f + static_cast<float>(EBreakerKillBucket::Boss));

    // The average is what the report prints, so a boss sample must not be able
    // to move the trash average at all.
    const float TrashAverage = FBreakerPlaytestStats::Average(Stats.TimeToKillSamples);
    Stats.SamplesForBucket(EBreakerKillBucket::Boss).Add(40.0f);
    TestEqual(TEXT("A boss kill cannot move the trash average"),
        FBreakerPlaytestStats::Average(Stats.TimeToKillSamples), TrashAverage);

    TestEqual(TEXT("An empty bucket averages zero rather than dividing by zero"),
        FBreakerPlaytestStats::Average(FBreakerPlaytestStats().BossTimeToKillSamples), 0.0f);

    return true;
}

#endif
