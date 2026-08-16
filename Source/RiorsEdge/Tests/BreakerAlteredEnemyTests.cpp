#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerAlteredEnemy.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"

// SEVERED DRUDGE — construction invariants, the weak point's world-height
// arithmetic, and the capsule-vs-cosmetic geometry the whole class comment
// argues for. All of it is testable off the default object, with no world,
// the same shape as FBreakerFacingArmorTest and FBreakerSkirmisherDefaultsTest
// in BreakerBossAndArchetypeTests.cpp.
//
// WHAT THIS DOES NOT COVER, stated plainly: nothing here proves the
// silhouette actually READS as "low, heavy, asymmetric, weight-dragging" —
// that is a screenshot judgement, not an assertion. Nothing here exercises
// Tick, BeginPlay, the ground snap, or ApplyChassis against a live world, so
// the archetype's actual in-game health/damage numbers at a given area level
// are unverified here (BreakerMonsterChassisTests.cpp covers the chassis
// maths itself). And AreCosmeticExtentsWithinCapsule() checks the SHIPPED
// defaults only — it cannot prove a future edit that moves a primitive stays
// inside the capsule; it can only fail loudly if one does not.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAlteredEnemyFamilyTest,
    "RiorsEdge.Combat.AlteredEnemy.Family",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAlteredEnemyFamilyTest::RunTest(const FString& Parameters)
{
    const ABreakerAlteredEnemy* Altered = GetDefault<ABreakerAlteredEnemy>();
    if (!TestNotNull(TEXT("The altered enemy has a default object"), Altered)) return false;

    // Story-Source §1.5: two families, and this one is the Altered.
    TestTrue(TEXT("It is Altered, not Vestige"), Altered->GetFamily() == EBreakerEnemyFamily::Altered);
    // LATE severance: "nothing is left but the shape." This is the mechanical
    // claim, not a label — it is what turns cover discipline and flinching
    // off through UBreakerEnemyFamilyLibrary, exactly as
    // FBreakerSkirmisherDefaultsTest checks the EARLY case reads the opposite
    // way.
    TestTrue(TEXT("It is late-stage severance"), Altered->GetSeveranceStage() == EBreakerSeveranceStage::Late);
    TestFalse(TEXT("Late severance takes no cover"), Altered->UsesCoverDiscipline());
    TestFalse(TEXT("Late severance does not flinch"), Altered->FlinchesWhenHit());

    // Distinguishing it from the OTHER two Altered archetypes already
    // shipped, so a copy-paste that forgot to change the stage is caught.
    const ABreakerSkirmisherEnemy* Skirmisher = GetDefault<ABreakerSkirmisherEnemy>();
    const ABreakerWardenEnemy* Warden = GetDefault<ABreakerWardenEnemy>();
    if (!TestNotNull(TEXT("The skirmisher has a default object"), Skirmisher)) return false;
    if (!TestNotNull(TEXT("The Warden has a default object"), Warden)) return false;
    TestTrue(TEXT("It is a later stage than the early-severance Skirmisher"),
        Altered->GetSeveranceStage() != Skirmisher->GetSeveranceStage());
    TestTrue(TEXT("It is a later stage than the mid-severance Warden"),
        Altered->GetSeveranceStage() != Warden->GetSeveranceStage());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAlteredEnemyChassisTuningTest,
    "RiorsEdge.Combat.AlteredEnemy.ChassisTuning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAlteredEnemyChassisTuningTest::RunTest(const FString& Parameters)
{
    // A guard on the shipped defaults, not on the maths — the same shape as
    // FBreakerSkirmisherDefaultsTest. All values are O2 PLACEHOLDER and may
    // legitimately change; if they do, change this test deliberately.
    const ABreakerAlteredEnemy* Altered = GetDefault<ABreakerAlteredEnemy>();
    const ABreakerWardenEnemy* Warden = GetDefault<ABreakerWardenEnemy>();
    if (!TestNotNull(TEXT("The altered enemy has a default object"), Altered)) return false;
    if (!TestNotNull(TEXT("The Warden has a default object"), Warden)) return false;

    // The archetype's own class comment derivation: bulkier than the melee
    // baseline (1.0x), because it is the roster's heaviest silhouette, but
    // below the Warden's 3.2x, because the Warden's ratio prices in frontal
    // armour this archetype does not carry.
    TestTrue(TEXT("Health is above the unmodified melee baseline"), Altered->GetArchetypeHealthMultiplier() > 1.0f);
    TestTrue(TEXT("...but below the armoured Warden's ratio"),
        Altered->GetArchetypeHealthMultiplier() < Warden->GetArchetypeHealthMultiplier());
    TestTrue(TEXT("Damage is above the unmodified melee baseline"), Altered->GetArchetypeDamageMultiplier() > 1.0f);
    TestTrue(TEXT("...but below the Warden's telegraphed sweep ratio"),
        Altered->GetArchetypeDamageMultiplier() < Warden->GetArchetypeDamageMultiplier());

    // No new attack: the base three-gear chase runs, tuned rather than
    // replaced. Explicitly no juke and no committed leap — "weight-dragging"
    // contradicts both, the same reasoning the Warden uses for the same
    // fields.
    TestEqual(TEXT("It does not weave"), Altered->GetWeaveStrength(), 0.0f);
    TestEqual(TEXT("It never lunges"), Altered->GetLungeRange(), 0.0f);
    // Slower than the un-modified baseline (330), because it is dragging
    // weight, but not slower than the Warden's own anchor pace (260) —
    // this archetype is not meant to be a second anchor.
    TestTrue(TEXT("It is slower than the unmodified baseline"), Altered->GetMoveSpeed() < 330.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAlteredEnemyGeometryTest,
    "RiorsEdge.Combat.AlteredEnemy.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAlteredEnemyGeometryTest::RunTest(const FString& Parameters)
{
    const ABreakerAlteredEnemy* Altered = GetDefault<ABreakerAlteredEnemy>();
    if (!TestNotNull(TEXT("The altered enemy has a default object"), Altered)) return false;

    // THE CAPSULE OVERRIDE. The base class's own capsule is
    // InitCapsuleSize(45.0f, 90.0f) — confirmed by reading BreakerEnemy.cpp:53
    // — a 45 cm radius. This archetype's constructor overrides it precisely
    // because an asymmetric silhouette with one oversized limb does not fit
    // that radius (see the constructor's RightArmVisual comment for the
    // arithmetic that would fail at 45 cm).
    TestTrue(TEXT("The capsule was actually widened for the asymmetric silhouette"),
        Altered->GetCapsuleRadiusCm() > 45.0f);
    TestEqual(TEXT("The authored capsule radius is 60 cm"), Altered->GetCapsuleRadiusCm(), 60.0f);
    TestEqual(TEXT("The authored capsule half-height is 75 cm (150 cm tall)"),
        Altered->GetCapsuleHalfHeightCm(), 75.0f);
    // "Low, heavy" is shorter as well as wider, not merely bigger in every
    // direction — the base is 180 cm tall (2 * 90); this is 150 cm.
    TestTrue(TEXT("It is shorter than the base 180 cm silhouette"),
        Altered->GetCapsuleHalfHeightCm() * 2.0f < 180.0f);

    // THE WEAK POINT WORLD-HEIGHT ARITHMETIC. This is the number the class
    // comment calls out by name as the one that must not drift from its own
    // visual tell — WeakPointVisual is a child of WeakPoint and was never
    // repositioned independently, so there is exactly one number to pin.
    TestEqual(TEXT("The weak point sits 123 cm above the ground, on the centreline"),
        Altered->GetWeakPointWorldHeightCm(), 123.0f);
    // Radius is left at the base value (20 cm) — only the placement changed.
    TestEqual(TEXT("The weak point radius is unchanged from the base class"),
        Altered->GetWeakPointRadiusCm(), 20.0f);
    // The tell has to stay inside the capsule's top, or it would be floating
    // visibly outside the silhouette that is supposed to contain it.
    TestTrue(TEXT("The weak point's top stays inside the capsule ceiling"),
        Altered->GetWeakPointWorldHeightCm() + Altered->GetWeakPointRadiusCm()
            <= Altered->GetCapsuleHalfHeightCm() * 2.0f);

    // THE CONSTRAINT THE EXTERNAL BLOCKOUT FAILED. Genuinely worth asserting
    // on its own, separate from the by-hand arithmetic in the constructor
    // comments, because it is exactly the property that got the source asset
    // rejected: cosmetic mass with no matching hit registration, clipping
    // through walls.
    TestTrue(TEXT("Every cosmetic primitive's extent stays inside the capsule"),
        Altered->AreCosmeticExtentsWithinCapsule());

    // THE FLOATING-TELL DEFECT, closed and pinned. The first authored
    // placement (-30, 0, 54) left the weak point sphere ~4.3 cm clear of the
    // nearest cosmetic corner — a tell hanging in the air behind the ridge it
    // claims to be part of. The sphere must visibly reach the body cosmetics
    // (it now overlaps BodyVisual's rear-top edge by ~3.9 cm; see the
    // constructor's arithmetic), while the capsule-ceiling assertion above
    // keeps it from drifting outside the silhouette in compensation.
    TestTrue(TEXT("The weak point sphere visibly touches the body cosmetics"),
        Altered->DoesWeakPointTouchCosmetics());
    return true;
}

#endif
