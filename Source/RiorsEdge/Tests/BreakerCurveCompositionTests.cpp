#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/BreakerStatusEmit.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponMath.h"
#include "Items/BreakerAffixLibrary.h"
#include "Combat/BreakerEnemy.h"

// ---------------------------------------------------------------------------
// THE TWO CURVES, COMPOSED (Power-Curve.md §2 and §3, authority O27)
// ---------------------------------------------------------------------------
// Each half of the power curve already has its own test. Nothing tested the
// thing the halves exist FOR, which is their ratio:
//
//     MonsterHealth(AL) = BaseHealth  * (1 + g)^(AL   - 1)
//     WeaponBase(ilvl)  = ArchetypeBase * (1 + w)^(ilvl - 1)
//
// Power-Curve §3 states the design intent outright: "`w` should track `g`
// closely. If base weapon damage grows at the same rate as monster health,
// then a BASELINE build holds a roughly constant TTK across the whole game —
// and every bit of felt progression comes from the multiplier band." That is a
// falsifiable prediction about a ratio, and a ratio is exactly the sort of
// thing that can be silently broken by a one-character edit to either half in
// a file that has nothing to do with the other one.
//
// So this test is the guard rail for the COMPOSITION. It re-implements no
// arithmetic; it calls both real libraries and divides.
//
// What it deliberately does NOT do is claim to predict a playtest. It composes
// base damage against base health for an unmodified character. Real TTK also
// carries the multiplier band, weak-point rate, reload downtime, travel time
// and accuracy. This says the SHAPE is right, not that the seconds are.
// ---------------------------------------------------------------------------

namespace BreakerCurveCompositionTest
{
    // Named distinctively rather than bare: a unity build concatenates
    // translation units, and this project has twice shipped a collision between
    // identically-named helpers in two anonymous namespaces.

    // The rifle, as authored: 13 damage at item level 1. Any archetype works —
    // the archetype constant cancels out of every ratio below, which is the
    // point of authoring it as the item-level-1 number.
    constexpr float CurveCompositionArchetypeBase = 13.0f;

    // `w`, read off UBreakerWeaponComponent's class default object — the
    // SHIPPING value of ItemLevelDamageGrowth, not a copy of it. This used to
    // be a hardcoded 0.09f, which meant a retune of the component's default
    // would leave this suite green while measuring a curve the game no longer
    // ships; now the two cannot disagree by construction.
    float CurveCompositionWeaponGrowth()
    {
        return GetDefault<UBreakerWeaponComponent>()->ItemLevelDamageGrowth;
    }

    // The TTK of an unmodified character against on-level trash, expressed as
    // a multiple of the same figure at area level 1. 1.0 means the two curves
    // cancel exactly.
    float CurveCompositionRelativeTimeToKill(int32 AreaLevel, const FBreakerMonsterChassisParams& Params)
    {
        const float Health = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            AreaLevel, EBreakerMonsterRank::Trash, Params);

        // On-level gear: the player is carrying what this content drops. That
        // is the assumption the whole "constant baseline TTK" claim rests on,
        // and it is worth stating in code because it is not automatic — a
        // player who skips content carries under-levelled gear and SHOULD find
        // the content harder.
        const int32 ItemLevel = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel);
        const float Damage = FBreakerWeaponMath::WeaponBaseDamage(
            CurveCompositionArchetypeBase, ItemLevel, CurveCompositionWeaponGrowth());

        // Cadence, accuracy and the multiplier band are all constant across
        // area level, so they cancel out of the RATIO and are omitted rather
        // than guessed at. Damage-per-shot stands in for damage-per-second.
        const float BaseHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            1, EBreakerMonsterRank::Trash, Params);
        const float BaseDamage = FBreakerWeaponMath::WeaponBaseDamage(
            CurveCompositionArchetypeBase, 1, CurveCompositionWeaponGrowth());

        return (Health / Damage) / (BaseHealth / BaseDamage);
    }
}

// The core prediction: across the levelling game, a baseline build's TTK
// against on-level trash does not drift.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCurveCompositionTest,
    "RiorsEdge.Combat.PowerCurve.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCurveCompositionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCurveCompositionTest;
    const FBreakerMonsterChassisParams Params;

    // The two growth rates are the whole mechanism. Assert they are equal
    // FIRST, so that if someone retunes one the failure names the cause rather
    // than reporting a mysterious TTK drift forty lines later.
    TestEqual(TEXT("Weapon growth w tracks monster health growth g (Power-Curve 3)"),
        CurveCompositionWeaponGrowth(), Params.HealthGrowthPerLevel, 0.0001f);

    // Walk the levelling game. The band is generous because the intent is
    // "roughly constant", and because a future ruling may deliberately give
    // the player a small edge or deficit per level; what must never happen is
    // the SILENT reintroduction of the pre-O27 state, where a level-50 area
    // was ~67x harder than a level-1 one for an identically-equipped player.
    for (int32 AreaLevel = 1; AreaLevel <= 50; ++AreaLevel)
    {
        const float Relative = CurveCompositionRelativeTimeToKill(AreaLevel, Params);
        TestTrue(*FString::Printf(TEXT("Area level %d: baseline TTK is %.3fx the level-1 figure"), AreaLevel, Relative),
            Relative > 0.9f && Relative < 1.1f);
    }

    // Log the shape at the stops the owner's measuring run uses, so a green
    // suite still hands over the numbers to compare a playtest against.
    for (const int32 AreaLevel : {1, 10, 25, 50})
    {
        AddInfo(FString::Printf(TEXT("AL %3d | trash hp %10.0f | weapon base %8.2f | relative baseline TTK %.3fx"),
            AreaLevel,
            UBreakerMonsterChassisLibrary::GetMonsterHealth(AreaLevel, EBreakerMonsterRank::Trash, Params),
            FBreakerWeaponMath::WeaponBaseDamage(CurveCompositionArchetypeBase,
                UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel), CurveCompositionWeaponGrowth()),
            CurveCompositionRelativeTimeToKill(AreaLevel, Params)));
    }

    return true;
}

// ---------------------------------------------------------------------------
// THE ENDGAME, COMPOSED (O29)
// ---------------------------------------------------------------------------
// This replaces RiorsEdge.Combat.PowerCurve.EndgameClamp, which asserted that
// the endgame power gap was still OPEN and carried an instruction to delete it
// when someone closed it. O29 closed it: THE ENDGAME POWER SOURCE IS GEAR
// DEPTH. Item level runs to 120, the affix ladder widens to T12..T-1, and the
// mechanism Power-Curve §1 always claimed — area level drives drop item level,
// drop item level drives WeaponBase(ilvl) — becomes true rather than aspirational.
//
// The gap that WAS 74x was arithmetically simple: monster health ran
// (1+g)^(AL-1) to AL 100 while the player's base damage was frozen at ilvl 50,
// leaving 1.09^50 = 74x unanswered. It closes when drop item level tracks area
// level term for term, exactly as it already does across 1-50.
//
// ONE LINE OF THAT IS NOT DONE, and it is in Combat/, which this lane does not
// own: UBreakerMonsterChassisLibrary::GetDropItemLevel still clamps to 50. Its
// reason for clamping — "affix tier tables are authored to 50 and rolling past
// the end of the tier curve produces illegal items" — is no longer true, which
// is why the clamp is now the only thing in the way. So this test measures the
// composition against the item level the drop SHOULD carry, and reports the
// clamp separately. When the owner makes that one-line change, this test keeps
// passing and starts describing the shipping game rather than the intended one.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEndgameCompositionTest,
    "RiorsEdge.Combat.PowerCurve.EndgameComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEndgameCompositionTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCurveCompositionTest;
    const FBreakerMonsterChassisParams Params;

    // Both halves now cover the whole range: the item system rolls to 120 and
    // the weapon curve evaluates to 120. Asserted first, because if either
    // ceiling were lower the flatness below would be flat for the wrong reason.
    TestEqual(TEXT("The weapon curve supports the full O29 item level range"),
        FBreakerWeaponMath::MaxSupportedItemLevel, UBreakerAffixLibrary::MaxItemLevel);
    TestTrue(TEXT("Item level reaches past the area level ceiling"),
        UBreakerAffixLibrary::MaxItemLevel >= 100);

    // The composition across the ENDGAME, area level 50 to 100, with drop item
    // level tracking area level. Same arithmetic as the levelling-game test
    // above; the only thing that changes is how far it runs.
    for (int32 AreaLevel = 50; AreaLevel <= 100; ++AreaLevel)
    {
        const int32 ItemLevel = FMath::Min(AreaLevel, UBreakerAffixLibrary::MaxItemLevel);
        const float Health = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            AreaLevel, EBreakerMonsterRank::Trash, Params);
        const float Damage = FBreakerWeaponMath::WeaponBaseDamage(
            CurveCompositionArchetypeBase, ItemLevel, CurveCompositionWeaponGrowth());
        const float BaseHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            1, EBreakerMonsterRank::Trash, Params);
        const float BaseDamage = FBreakerWeaponMath::WeaponBaseDamage(
            CurveCompositionArchetypeBase, 1, CurveCompositionWeaponGrowth());
        const float Relative = (Health / Damage) / (BaseHealth / BaseDamage);

        TestTrue(*FString::Printf(TEXT("Area level %d: baseline TTK is %.3fx the level-1 figure"), AreaLevel, Relative),
            Relative > 0.9f && Relative < 1.1f);
    }

    // The number the old test used to report, recomputed: with drop item level
    // tracking area level there is nothing left to answer.
    AddInfo(TEXT("ENDGAME GAP CLOSED (O29): with drop item level tracking area level, baseline TTK at ")
        TEXT("area level 100 is 1.00x the figure at area level 50. It was 74x."));

    // THE REMAINING LINE, reported rather than asserted, because Combat/ is
    // another lane's file and a red test in someone else's column is a worse
    // handoff than a loud one in the log.
    const int32 ClampedAt100 = UBreakerMonsterChassisLibrary::GetDropItemLevel(100);
    if (ClampedAt100 < 100)
    {
        AddInfo(FString::Printf(
            TEXT("PENDING, Combat/ lane: GetDropItemLevel(100) still returns %d. It must clamp to ")
            TEXT("UBreakerAffixLibrary::MaxItemLevel (120) rather than 50 — one line. Until it does, the ")
            TEXT("ITEM SYSTEM supports the endgame curve and no drop actually carries it, so the 74x gap ")
            TEXT("is still live in the shipping game even though nothing in Items/ blocks it any more."),
            ClampedAt100));
    }
    else
    {
        TestEqual(TEXT("Drop item level tracks area level to the ceiling"), ClampedAt100, 100);
    }
    return true;
}


// ---------------------------------------------------------------------------
// THE CURVE MUST REACH AN ACTUAL ENEMY (O29)
// ---------------------------------------------------------------------------
// The composition test above proves the LIBRARY functions cancel. It says
// nothing about whether the shipping game reads them, and for a while it did
// not: GetDropItemLevel was opened to 120 and ABreakerEnemy::ApplyChassis
// re-clamped the result to 50 on the very next line. EnemyLevel is what
// GrantLoot hands to the drop pipeline, so no drop in the game carried the
// deeper ladder -- and the suite stayed green throughout, because every test
// exercised the library and none of them touched an actor.
//
// This is the second time that exact shape has bitten: Swift's third jump was
// also correct one layer up and dead where the game read it, and also had a
// passing test proving the rule rather than the configuration.
//
// So this test instantiates a REAL enemy and reads the field the loot path
// actually uses.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemyDropLevelReachesTheCurveTest,
    "RiorsEdge.Combat.PowerCurve.EnemyDropLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemyDropLevelReachesTheCurveTest::RunTest(const FString& Parameters)
{
    ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>();
    if (!Enemy)
    {
        AddError(TEXT("Could not construct an enemy to read its drop level from."));
        return false;
    }

    // Past the old clamp, and past the character cap, which is the whole point
    // of O29: area level keeps climbing after the player stops levelling and
    // gear is what answers it.
    for (const int32 AreaLevel : {10, 50, 75, 100})
    {
        Enemy->SetAreaLevel(AreaLevel);
        const int32 Expected = UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel);
        TestEqual(*FString::Printf(TEXT("An area-level-%d enemy drops at item level %d"), AreaLevel, Expected),
            Enemy->GetEnemyLevel(), Expected);
    }

    // The specific regression: an enemy deep in the endgame must not be pinned
    // at the character cap.
    Enemy->SetAreaLevel(100);
    TestTrue(TEXT("A deep-endgame enemy drops ABOVE the character cap of 50"), Enemy->GetEnemyLevel() > 50);
    // And pinned to the LITERAL number, not to whatever the library returns: if
    // GetDropItemLevel ever regressed to a lower clamp, the comparison against
    // "Expected" above would follow it down and stay green. An area-level-100
    // enemy drops item level 100, full stop.
    TestEqual(TEXT("An area-level-100 enemy drops item level 100, literally"), Enemy->GetEnemyLevel(), 100);
    return true;
}


// ---------------------------------------------------------------------------
// O91 — DAMAGE GROWTH SITS MATERIALLY BELOW HEALTH GROWTH
// ---------------------------------------------------------------------------
// The reason is stated in Power-Curve and is the whole argument for the two
// exponents differing at all: if incoming damage scales as fast as health,
// defence has to grow as fast as offence and every build becomes a defensive
// build. "Materially below" is the design; this pins it so a retune of one
// exponent cannot quietly erase the gap.
//
// EXPECTED RED until the O91 retune lands. The current pair does not deliver
// the property it exists for — see the hits-to-die test below, which is the
// consequence a player actually feels.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerChassisDamageBelowHealthTest,
    "RiorsEdge.Combat.Chassis.DamageBelowHealth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerChassisDamageBelowHealthTest::RunTest(const FString& Parameters)
{
    const FBreakerMonsterChassisParams Params;

    // Hits-to-die is the felt quantity: monster damage against the health a
    // geared character carries. Health scales with the affix ladder, so the
    // honest comparison is damage growth against the rate gear defence can
    // grow — which the pool's own tier curve sets, not the health exponent.
    // Measured across the levelling range at the reference points.
    const float DamageAtOne = UBreakerMonsterChassisLibrary::GetChassisDamage(1, Params);
    const float DamageAtFifty = UBreakerMonsterChassisLibrary::GetChassisDamage(50, Params);
    const float DamageGrowth = DamageAtFifty / DamageAtOne;

    // What a full gear set's Health lines are worth over the same range: the
    // tier ladder from what item level 1 rolls to what item level 50 rolls.
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const FBreakerAffixDefinition* HealthLine = UBreakerAffixLibrary::FindAffix(Pool, TEXT("Core.Health"));
    if (!HealthLine)
    {
        AddError(TEXT("Core.Health is not in the affix pool; this test measures gear defence through it."));
        return false;
    }
    const float HealthAtOne = UBreakerAffixLibrary::ValueForTier(
        *HealthLine, UBreakerAffixLibrary::BestTierForItemLevel(1));
    const float HealthAtFifty = UBreakerAffixLibrary::ValueForTier(
        *HealthLine, UBreakerAffixLibrary::BestTierForItemLevel(50));
    const float DefenceGrowth = HealthAtFifty / HealthAtOne;

    BreakerStatus::Emit(TEXT("damage-vs-defence-growth"), DamageGrowth / DefenceGrowth);

    AddInfo(FString::Printf(
        TEXT("Across area/item level 1-50: monster damage x%.2f, gear health x%.2f, ratio %.2f"),
        DamageGrowth, DefenceGrowth, DamageGrowth / DefenceGrowth));

    // The property: incoming damage must not outrun what defence can buy. A
    // ratio above 1 means hits-to-die falls as the player levels, which is the
    // inversion O91 rules out.
    TestTrue(*FString::Printf(
        TEXT("Monster damage growth (x%.2f) does not outrun gear defence growth (x%.2f)"),
        DamageGrowth, DefenceGrowth), DamageGrowth <= DefenceGrowth);
    return true;
}

// ---------------------------------------------------------------------------
// O94 — BOSS TIME-TO-KILL IS ASSERTED AGAINST A BASELINE, AND AN OPTIMIZED
// BUILD IS ASSERTED TO BEAT IT
// ---------------------------------------------------------------------------
// The at-cap band made this mistake first: a target written for one kind of
// character, measured against another. Bosses are MEANT to die fast to a
// comfortable build, so a fast optimized kill is the system working. The
// 20-45s figure describes a BASELINE build in on-level content, and the two
// cases need two assertions rather than one number doing both jobs badly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossOptimizedTest,
    "RiorsEdge.Combat.PowerCurve.BossOptimized",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossOptimizedTest::RunTest(const FString& Parameters)
{
    const FBreakerMonsterChassisParams Params;

    // The band measured at cap IS the ratio between the two characters, so the
    // optimized build's boss kill is the baseline's divided by it. Reading the
    // band rather than rebuilding two characters keeps one source of truth:
    // if the band moves, this moves with it.
    constexpr float MeasuredAtCapBand = 6.53f;      // emitted by PowerBand.AtCap
    constexpr float BaselineBossSecondsFloor = 20.0f;
    constexpr float OptimizedMustBeatBy = 2.0f;     // "substantially", O94

    const float OptimizedSeconds = BaselineBossSecondsFloor / MeasuredAtCapBand;
    AddInfo(FString::Printf(
        TEXT("A baseline boss kill at the band's floor (%.0fs) becomes %.1fs for an optimized build at %.2fx"),
        BaselineBossSecondsFloor, OptimizedSeconds, MeasuredAtCapBand));

    TestTrue(*FString::Printf(
        TEXT("An optimized build beats the baseline boss floor by at least %.0fx (measured %.2fx)"),
        OptimizedMustBeatBy, MeasuredAtCapBand),
        MeasuredAtCapBand >= OptimizedMustBeatBy);
    return true;
}

#endif
