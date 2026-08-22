#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerAffixLibrary.h"
#include "Tests/BreakerStatusEmit.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"
#include "Weapons/BreakerWeaponMath.h"

// ---------------------------------------------------------------------------
// THE PROMOTED FINDINGS
// ---------------------------------------------------------------------------
// Findings that were carried as prose until somebody promoted them to
// invariants, and then carried as invariant rows with no test — which is the
// worst of the three states a claim can be in, because a named test that was
// never written looks asserted and is not.
//
// All of it is pure maths against the shipped chassis: no world, no actor, no
// ability system, and nothing granted that the game does not grant. Where a
// figure is a target the game does not meet, the test is RED ON PURPOSE and
// enumerated in Scripts/status-pins.json with the condition that deletes it.
// ---------------------------------------------------------------------------

namespace BreakerPromotedFindingTest
{
    // Distinctively named for the unity build, per the twice-shipped rule about
    // anonymous-namespace collisions.

    // The rifle at item level 1, as authored. The archetype constant cancels
    // out of every ratio here; it survives only in the absolute boss figure,
    // which is why that one is a seed rather than a derivation.
    constexpr float PromotedArchetypeBase = 13.0f;

    float PromotedWeaponGrowth()
    {
        return GetDefault<UBreakerWeaponComponent>()->ItemLevelDamageGrowth;
    }

    // A full gear set's Health lines at the tier this item level can roll.
    // EIGHT slots, one line each: the affix pool puts Core.Health on every
    // slot, so a character who wanted nothing else could carry eight, and the
    // question this test asks is whether the CEILING of gear defence keeps up.
    // A smaller set only makes the answer worse.
    constexpr int32 PromotedHealthLineCount = 8;

    float PromotedGearHealthAt(int32 ItemLevel)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        const FBreakerAffixDefinition* Line = UBreakerAffixLibrary::FindAffix(Pool, TEXT("Core.Health"));
        if (!Line) return 0.0f;
        return PromotedHealthLineCount * UBreakerAffixLibrary::ValueForTier(
            *Line, UBreakerAffixLibrary::BestTierForItemLevel(ItemLevel));
    }

    // Everything a baseline character has to absorb a hit with, at a level.
    float PromotedEffectiveHealthAt(int32 AreaLevel)
    {
        const float Base = GetDefault<UBreakerAttributeSet>()->GetMaxHealth();
        return Base + PromotedGearHealthAt(UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel));
    }

    // Hits a trash enemy needs to kill a baseline character at this area level.
    float PromotedHitsToDieAt(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
    {
        const float Incoming = UBreakerMonsterChassisLibrary::GetChassisDamage(AreaLevel, Params);
        if (Incoming <= 0.0f) return 0.0f;
        return PromotedEffectiveHealthAt(AreaLevel) / Incoming;
    }
}

// ---------------------------------------------------------------------------
// HITS-TO-DIE DOES NOT FALL ACROSS THE LEVEL RANGE
// ---------------------------------------------------------------------------
// The property `d` exists to produce, stated as the thing a player feels rather
// than as an exponent. Combat.Chassis.DamageBelowHealth already measures the
// two GROWTH rates against each other; this measures the quantity itself, at
// points across the range, because a ratio can hold on average while the curve
// dips in the middle — and a dip is a difficulty cliff nobody authored.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDefenseCurveHitsToDieTest,
    "RiorsEdge.Combat.DefenseCurve.HitsToDie",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDefenseCurveHitsToDieTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    const int32 Points[] = {1, 10, 20, 30, 40, 50};
    float First = 0.0f, Worst = 0.0f;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Points); ++Index)
    {
        const float Hits = PromotedHitsToDieAt(Points[Index], Params);
        if (Index == 0) { First = Hits; Worst = Hits; }
        Worst = FMath::Min(Worst, Hits);
        AddInfo(FString::Printf(TEXT("HITS-TO-DIE  area level %2d: %.2f hits"), Points[Index], Hits));
    }

    const float AtCap = PromotedHitsToDieAt(50, Params);
    AddInfo(FString::Printf(
        TEXT("HITS-TO-DIE  level 1 %.2f -> level 50 %.2f (%.0f%% of where it started); worst point %.2f"),
        First, AtCap, 100.0f * AtCap / FMath::Max(First, UE_SMALL_NUMBER), Worst));

    // THE PROPERTY, and it is a floor rather than a band: a character may get
    // tougher relative to content, and O27 rather wants that at the top end.
    // What it may not do is get FLIMSIER, which is the inversion O91 rules out
    // — a character at cap squishier than one at level 10, in content they
    // out-gear.
    TestTrue(*FString::Printf(
        TEXT("Hits-to-die at the cap (%.2f) is at least what it was at level 1 (%.2f)"), AtCap, First),
        AtCap >= First);
    TestTrue(*FString::Printf(
        TEXT("Hits-to-die never dips below its level-1 value anywhere in the range (worst %.2f)"), Worst),
        Worst >= First - UE_KINDA_SMALL_NUMBER);
    return true;
}

// ---------------------------------------------------------------------------
// BOSS TIME-TO-KILL, FOR A BASELINE BUILD
// ---------------------------------------------------------------------------
// O94: the 20-45s figure describes a BASELINE build in on-level content, and
// its optimized twin is asserted separately (Combat.PowerCurve.BossOptimized)
// because a single target measured against an optimized character is measuring
// the wrong character.
//
// This is the one figure here that cannot be a ratio: seconds are absolute, so
// it needs a cadence and an archetype. Both are read from the shipped
// definitions rather than assumed, and the number they produce is a SEED — the
// chassis solves backwards from it, per the spec's own rule that TTK is an
// output and never an input.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossBandTest,
    "RiorsEdge.Combat.PowerCurve.BossBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossBandTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18's seed band for a baseline build.
    constexpr float BossSecondsFloor = 20.0f;
    constexpr float BossSecondsCeiling = 45.0f;

    const UBreakerWeaponDefinition* Definition = GetDefault<UBreakerWeaponDefinition>();
    const float RoundsPerMinute = Definition ? Definition->RoundsPerMinute : 600.0f;

    // Measured at the character cap, in on-level content: the point the band
    // was authored to describe.
    constexpr int32 AreaLevel = 50;
    const int32 ItemLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel);
    const float PerShot = FBreakerWeaponMath::WeaponBaseDamage(
        PromotedArchetypeBase, ItemLevel, PromotedWeaponGrowth());
    const float DamagePerSecond = PerShot * RoundsPerMinute / 60.0f;

    const float BossHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
        AreaLevel, EBreakerMonsterRank::Boss, Params);
    const float Seconds = BossHealth / FMath::Max(DamagePerSecond, UE_SMALL_NUMBER);

    AddInfo(FString::Printf(
        TEXT("BOSS BAND  area level %d: %.0f health against %.1f dps (%.2f per shot at %.0f rpm) => %.1fs (O18 target %.0f-%.0fs)"),
        AreaLevel, BossHealth, DamagePerSecond, PerShot, RoundsPerMinute, Seconds, BossSecondsFloor, BossSecondsCeiling));

    // A BASELINE build carries no multiplier band by construction — that is
    // what makes it the baseline — so this is weapon base against boss health
    // and nothing else. An optimized build's faster kill is correct behaviour
    // and is the other test's subject, not a failure of this one.
    TestTrue(*FString::Printf(TEXT("A baseline boss kill (%.1fs) is at least %.0fs"), Seconds, BossSecondsFloor),
        Seconds >= BossSecondsFloor);
    TestTrue(*FString::Printf(TEXT("A baseline boss kill (%.1fs) is at most %.0fs"), Seconds, BossSecondsCeiling),
        Seconds <= BossSecondsCeiling);
    return true;
}

#endif
