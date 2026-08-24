#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerMonsterChassis.h"

// ---------------------------------------------------------------------------
// A REWARD PREDICATE MUST NOT SKIP THE RANKS ABOVE THE ONE IT NAMES.
//
// Both loot sites on ABreakerEnemy asked IsElite(), which is EXACTLY the Elite
// rank. ModifierBearing is by definition a modifier-bearing elite and Boss is
// above both, so the rarity floor and the item-level bonus skipped the two
// ranks that are harder than the one they were written for: a three-modifier
// champion dropped with no floor and no bonus, strictly worse than the ordinary
// elite it is an upgrade of.
//
// THIS IS SEPARATE FROM THE RULING ON WHAT THOSE REWARDS SHOULD BE. Whatever
// the reward ladder turns out to be, "harder rank gets less" is wrong under all
// of them, so the predicate is fixed on its own rather than waiting behind a
// design decision it does not depend on.
//
// The pair is deliberate and both halves are asserted here, because the danger
// now runs the other way: IsElite() is still correct for the telemetry buckets,
// which must NOT average a champion in with an ordinary elite, and a later pass
// that "tidies" the two into one would break the kill-time sample instead.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRewardRankPredicateTest,
    "RiorsEdge.Combat.Reward.RankPredicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRewardRankPredicateTest::RunTest(const FString& Parameters)
{
    // The ordering the reward predicate depends on. EBreakerMonsterRank is
    // serialized by value and authored in ascending difficulty, so a comparison
    // is safe -- but only while that stays true, which is what this asserts.
    TestTrue(TEXT("Trash sorts below Elite"),
        EBreakerMonsterRank::Trash < EBreakerMonsterRank::Elite);
    TestTrue(TEXT("Elite sorts below ModifierBearing"),
        EBreakerMonsterRank::Elite < EBreakerMonsterRank::ModifierBearing);
    TestTrue(TEXT("ModifierBearing sorts below Boss"),
        EBreakerMonsterRank::ModifierBearing < EBreakerMonsterRank::Boss);

    ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>();
    if (!TestNotNull(TEXT("An enemy is constructible"), Enemy)) return false;

    struct FCase
    {
        EBreakerMonsterRank Rank;
        const TCHAR* Name;
        bool bIsElite;          // the narrow predicate, for telemetry
        bool bIsEliteOrBetter;  // the reward predicate
    };
    const FCase Cases[] = {
        { EBreakerMonsterRank::Trash,           TEXT("Trash"),           false, false },
        { EBreakerMonsterRank::Elite,           TEXT("Elite"),           true,  true  },
        { EBreakerMonsterRank::ModifierBearing, TEXT("ModifierBearing"), false, true  },
        { EBreakerMonsterRank::Boss,            TEXT("Boss"),            false, true  },
    };

    for (const FCase& Case : Cases)
    {
        Enemy->SetMonsterRank(Case.Rank);
        TestEqual(*FString::Printf(TEXT("%s: IsElite is the narrow rank test"), Case.Name),
            Enemy->IsElite(), Case.bIsElite);
        TestEqual(*FString::Printf(TEXT("%s: IsEliteOrBetter is what reward asks"), Case.Name),
            Enemy->IsEliteOrBetter(), Case.bIsEliteOrBetter);
    }

    // The statement the defect violated, said plainly rather than left implicit
    // in the table above: no rank harder than Elite may be refused what Elite
    // is given. This is the assertion that survives a re-ranking.
    for (const FCase& Case : Cases)
    {
        if (Case.Rank <= EBreakerMonsterRank::Elite) continue;
        Enemy->SetMonsterRank(Case.Rank);
        TestTrue(*FString::Printf(
            TEXT("%s is harder than Elite, so it cannot be refused Elite's reward gate"), Case.Name),
            Enemy->IsEliteOrBetter());
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
