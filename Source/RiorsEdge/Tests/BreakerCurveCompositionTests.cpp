#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Weapons/BreakerWeaponMath.h"

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
// THE ENDGAME CLAMP — a KNOWN, UNRESOLVED contradiction, pinned deliberately.
// ---------------------------------------------------------------------------
// Power-Curve §1 says of area levels past the character cap: "because area
// level drives drop item level, climbing tiers is what keeps gear improving
// after the level cap. That is the mechanism by which 'all endgame character
// power comes from gear' actually functions."
//
// The code does not do that. GetDropItemLevel clamps to 50 — for a good local
// reason, stated at the function: affix tier tables are authored to 50 and
// rolling past the end of the tier curve produces illegal items. But the
// chassis keeps climbing to area level 100. So across the entire endgame the
// monster curve runs and the player's base-damage curve does not, and a
// level-100 area is ~75x harder than a level-50 one for gear that cannot get
// any better.
//
// That is not a bug in either function. It is a MISSING DESIGN: the endgame
// needs a power source that keeps climbing past ilvl 50 — deeper affix tiers,
// an ilvl-past-50 track, ascended rarities, or a separate endgame multiplier —
// and which one it gets is an owner ruling, not an implementation choice.
//
// This test exists so the gap cannot be forgotten, and so that whoever closes
// it gets a failing test telling them exactly which claim they are satisfying.
// It asserts the divergence IS there, which reads backwards until you notice
// that the alternative is a contradiction nobody is looking at.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEndgameItemLevelClampTest,
    "RiorsEdge.Combat.PowerCurve.EndgameClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEndgameItemLevelClampTest::RunTest(const FString& Parameters)
{
    using namespace BreakerCurveCompositionTest;
    const FBreakerMonsterChassisParams Params;

    TestEqual(TEXT("Drop item level still clamps at the character cap"),
        UBreakerMonsterChassisLibrary::GetDropItemLevel(100), 50);
    TestTrue(TEXT("The monster chassis still climbs past it"),
        UBreakerMonsterChassisLibrary::GetMonsterHealth(100, EBreakerMonsterRank::Trash, Params)
        > UBreakerMonsterChassisLibrary::GetMonsterHealth(50, EBreakerMonsterRank::Trash, Params) * 10.0f);

    const float AtCap = CurveCompositionRelativeTimeToKill(50, Params);
    const float AtCeiling = CurveCompositionRelativeTimeToKill(100, Params);
    AddInfo(FString::Printf(
        TEXT("ENDGAME GAP: baseline TTK is %.2fx at AL50 and %.0fx at AL100 — a %.0fx swing with no gear source to answer it. ")
        TEXT("Power-Curve 1 claims drop item level answers this; GetDropItemLevel clamps to 50. Needs an owner ruling."),
        AtCap, AtCeiling, AtCeiling / AtCap));

    // If this ever fails, the gap has been CLOSED. That is good news: delete
    // this test and update Power-Curve 1 to describe whatever now carries
    // endgame power.
    TestTrue(TEXT("The endgame power gap is still open (see the comment above before 'fixing' this)"),
        AtCeiling > AtCap * 10.0f);
    return true;
}

#endif
