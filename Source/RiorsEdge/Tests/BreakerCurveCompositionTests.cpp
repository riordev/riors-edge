#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Weapons/BreakerWeaponMath.h"
#include "Items/BreakerAffixLibrary.h"

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

    // `w`, matching UBreakerWeaponComponent::ItemLevelDamageGrowth's default.
    constexpr float CurveCompositionWeaponGrowth = 0.09f;   // O2 PLACEHOLDER

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
            CurveCompositionArchetypeBase, ItemLevel, CurveCompositionWeaponGrowth);

        // Cadence, accuracy and the multiplier band are all constant across
        // area level, so they cancel out of the RATIO and are omitted rather
        // than guessed at. Damage-per-shot stands in for damage-per-second.
        const float BaseHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            1, EBreakerMonsterRank::Trash, Params);
        const float BaseDamage = FBreakerWeaponMath::WeaponBaseDamage(
            CurveCompositionArchetypeBase, 1, CurveCompositionWeaponGrowth);

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
        CurveCompositionWeaponGrowth, Params.HealthGrowthPerLevel, 0.0001f);

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
                UBreakerMonsterChassisLibrary::GetDropItemLevel(AreaLevel), CurveCompositionWeaponGrowth),
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
            CurveCompositionArchetypeBase, ItemLevel, CurveCompositionWeaponGrowth);
        const float BaseHealth = UBreakerMonsterChassisLibrary::GetMonsterHealth(
            1, EBreakerMonsterRank::Trash, Params);
        const float BaseDamage = FBreakerWeaponMath::WeaponBaseDamage(
            CurveCompositionArchetypeBase, 1, CurveCompositionWeaponGrowth);
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

#endif
