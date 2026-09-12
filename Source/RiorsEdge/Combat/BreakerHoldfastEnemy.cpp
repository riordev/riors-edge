#include "Combat/BreakerHoldfastEnemy.h"

#include "Combat/BreakerBodyPaint.h"
#include "Combat/BreakerEnemyModifiers.h"

ABreakerHoldfastEnemy::ABreakerHoldfastEnemy()
{
    // FAMILY (O214). A Vestige mass: no severance, no stage, no insignia. The
    // base boss's Altered/Early pair is the Marshal's claim — that it still
    // gives orders — and this body does not make it. What it does with the
    // same apparatus is recorded at the label below, not renamed here.
    Family = EBreakerEnemyFamily::Vestige;
    SeveranceStage = EBreakerSeveranceStage::NotApplicable;

    // THE BODY (O190: slot, fallback, recipe). The slot is BodyMeshAsset; the
    // fallback is the Vestige cast body Stan, the same five paths every base
    // melee body wears (Combat/BreakerEnemy.cpp), at the boss's 1.75x scale;
    // the George the Warden and the Marshal wear is an Altered heavy and
    // reads wrong on a Vestige. The recipe for the mass this stands in for:
    //   - one skeletal mesh, /Game/Breaker/Meshes/enemies/vestige/Holdfast,
    //     fitted to the boss capsule by BreakerEnemyBodyMath like any body;
    //   - an Idle, a Walk, a HitRecieve and a Death clip under the same
    //     armature naming as Stan's (O281: rest, move, struck, dead), so the
    //     four animation slots swap by path alone;
    //   - no separate shield mesh: the Warden's slab component stays, painted
    //     the family colour below, until the mass has a front of its own.
    // Swapping it is a content change with no C++ diff (enemies.md).
    // O2 PLACEHOLDER.
    BodyMeshAsset = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/Stan.Stan"));
    BodyIdleAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/StanRobotArmature_Idle.StanRobotArmature_Idle"));
    BodyRunAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/StanRobotArmature_Walk.StanRobotArmature_Walk"));
    BodyHitAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/StanRobotArmature_HitRecieve_1.StanRobotArmature_HitRecieve_1"));
    BodyDeathAnimation = FSoftObjectPath(TEXT("/Game/Breaker/Meshes/enemies/mechs/Stan/StanRobotArmature_Death.StanRobotArmature_Death"));

    // THE FRONT. O198 says the boss's shield follows the Warden's rule, and it
    // does: same pool, same fraction, same break. The tension is Encounter
    // Design §2.3, which describes that front as EQUIPMENT — a slab an Altered
    // carries — and a Vestige carries nothing. Painting the slab the family
    // colour is the honest reading of what is built: the front is the mass's
    // own face, not a thing it holds. A front that is not a slab at all is
    // the O190 recipe's last line, not a number here.
    ShieldColor = BreakerBodyPaint::VestigeFamilyPaint;

    // THE ADD GATE. The one number this class turns on, and it authors
    // nothing: it is the Warding Aura's reduction, read from the modifier
    // params, so "the mass holds while its raised stand" and "an aura-bearer
    // holds while it stands" are the same fraction until one is felt apart
    // from the other. O2 PLACEHOLDER by inheritance.
    PhaseParams.AddGateDamageReduction = FBreakerEnemyModifierParams().AuraDamageReduction;

    // The Holdfast keeps the 0.35 ratio while the Marshal's falls to 0.30 (O214
    // makes this Act I's own boss; the cut was asked of the Marshal alone), so
    // its 5,775 / 866.25 / 37-round pins stand: BossBand's 24.06 s holds
    // ungated, and the gated worst case (every add alive through both early
    // phases at the reduction above) is about 37 s, inside O18's 45.
    ArchetypeHealthMultiplier = 0.35f;   // O2 PLACEHOLDER
}

void ABreakerHoldfastEnemy::BeginPlay()
{
    Super::BeginPlay();
    // The diagnostics label only: GetEnemyStateLabel reaches the F3 overlay
    // (UI/BreakerPlaytestHUD) and nothing shipped; the shipped bar reads rank
    // and says BOSS. The engaged tick still overwrites this with the Marshal's
    // "ORDER: DEPLOY" / "ORDER: FIRE" on a raise, and that is a Vestige giving
    // orders — a fiction gap recorded here, not renamed: the beat's NAME is
    // the order machine's, and the Holdfast has no vocabulary of its own yet.
    StateLabel = TEXT("THE HOLDFAST");
}
