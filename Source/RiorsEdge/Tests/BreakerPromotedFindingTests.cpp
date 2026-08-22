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

    // Seconds a BASELINE build takes to kill one enemy of this rank, in on-level
    // content. Absolute rather than relative, so it needs a cadence and an
    // archetype; both come from the shipped definitions. A baseline carries no
    // multiplier band by construction, which is what makes it the baseline.
    float PromotedBaselineSecondsToKill(int32 AreaLevel, EBreakerMonsterRank Rank,
        const FBreakerMonsterChassisParams& Params)
    {
        const UBreakerWeaponDefinition* Definition = GetDefault<UBreakerWeaponDefinition>();
        const float RoundsPerMinute = Definition ? Definition->RoundsPerMinute : 600.0f;
        const int32 ItemLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel);
        const float PerShot = FBreakerWeaponMath::WeaponBaseDamage(
            PromotedArchetypeBase, ItemLevel, PromotedWeaponGrowth());
        const float DamagePerSecond = PerShot * RoundsPerMinute / 60.0f;
        const float Health = UBreakerMonsterChassisLibrary::GetMonsterHealth(AreaLevel, Rank, Params);
        return Health / FMath::Max(DamagePerSecond, UE_SMALL_NUMBER);
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
// TRASH AND ELITE TIME-TO-KILL, AS MAGNITUDES
// ---------------------------------------------------------------------------
// Combat.PowerCurve.Composition asserts the baseline TTK curve is FLAT across
// area level and never asserts what it is flat AT. A curve can be perfectly
// level and land at twice its target everywhere, which is what happened here: a
// throughput shortfall on the two commonest enemies in the game survived every
// green run until a boss test tripped over it, and the boss looked like the
// fault.
//
// So these assert the magnitude the flatness test takes for granted, on the same
// chassis and weapon maths. They are cheaper than any other finding outstanding,
// and they are the figure every other TTK number in the project rests on.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerTrashTtkTest,
    "RiorsEdge.Combat.PowerCurve.TrashTtk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerTrashTtkTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18: trash a little under a second. A CEILING rather than a band — trash
    // exists to be trivialized by an optimized build, so a baseline killing it
    // faster than the seed is not a defect.
    constexpr float TrashSecondsCeiling = 1.0f;

    for (const int32 AreaLevel : {1, 25, 50})
    {
        AddInfo(FString::Printf(TEXT("TRASH TTK  area level %2d: %.2fs (O18 target under %.1fs)"),
            AreaLevel, PromotedBaselineSecondsToKill(AreaLevel, EBreakerMonsterRank::Trash, Params),
            TrashSecondsCeiling));
    }

    const float AtCap = PromotedBaselineSecondsToKill(50, EBreakerMonsterRank::Trash, Params);
    TestTrue(*FString::Printf(TEXT("A baseline kills on-level trash in under %.1fs (measured %.2fs)"),
        TrashSecondsCeiling, AtCap), AtCap <= TrashSecondsCeiling);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEliteTtkTest,
    "RiorsEdge.Combat.PowerCurve.EliteTtk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEliteTtkTest::RunTest(const FString& Parameters)
{
    using namespace BreakerPromotedFindingTest;
    const FBreakerMonsterChassisParams Params;

    // O18: elite around three. A band, because an elite dying too fast is a rank
    // that stopped meaning anything — the opposite failure from trash, which is
    // why the two are asserted differently.
    constexpr float EliteSecondsFloor = 2.0f;
    constexpr float EliteSecondsCeiling = 4.0f;

    const float AtCap = PromotedBaselineSecondsToKill(50, EBreakerMonsterRank::Elite, Params);
    AddInfo(FString::Printf(TEXT("ELITE TTK  area level 50: %.2fs (O18 target ~3s, band %.0f-%.0fs)"),
        AtCap, EliteSecondsFloor, EliteSecondsCeiling));

    TestTrue(*FString::Printf(TEXT("A baseline kills an on-level elite in at least %.0fs (measured %.2fs)"),
        EliteSecondsFloor, AtCap), AtCap >= EliteSecondsFloor);
    TestTrue(*FString::Printf(TEXT("A baseline kills an on-level elite in at most %.0fs (measured %.2fs)"),
        EliteSecondsCeiling, AtCap), AtCap <= EliteSecondsCeiling);
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

    constexpr int32 AreaLevel = 50;
    const float Seconds = PromotedBaselineSecondsToKill(AreaLevel, EBreakerMonsterRank::Boss, Params);
    const float TrashSeconds = PromotedBaselineSecondsToKill(AreaLevel, EBreakerMonsterRank::Trash, Params);
    const float TrashTarget = 0.9f;
    const float Shortfall = TrashSeconds / TrashTarget;

    // TWO ERRORS, REPORTED SEPARATELY, because closing either one alone looks
    // like progress and is not. The rank multiplier is half of this figure; the
    // other half is a throughput shortfall that trash and elite carry too, and
    // moving boss health to cover both would hide it on the enemies a player
    // meets most.
    AddInfo(FString::Printf(TEXT("BOSS BAND  area level %d: %.1fs (O18 target %.0f-%.0fs)"),
        AreaLevel, Seconds, BossSecondsFloor, BossSecondsCeiling));
    AddInfo(FString::Printf(
        TEXT("BOSS BAND  error 1, throughput: trash takes %.2fs against a %.1fs target, so the baseline is %.2fx short before a boss is involved"),
        TrashSeconds, TrashTarget, Shortfall));
    AddInfo(FString::Printf(
        TEXT("BOSS BAND  error 2, rank: x75 authored, against x%.0f-%.0f derived from this band over the trash target"),
        BossSecondsFloor / TrashTarget, BossSecondsCeiling / TrashTarget));
    AddInfo(FString::Printf(
        TEXT("BOSS BAND  throughput alone gives %.1fs, rank alone gives %.1fs, both together land the band"),
        Seconds / Shortfall, Seconds * (BossSecondsCeiling / TrashTarget) / 75.0f));

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
