#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Game/BreakerGameMode.h"
#include "Playtest/BreakerKillBuckets.h"

namespace BreakerGameModeTestHelpers
{
    // A bare NewObject<ABreakerEnemy>() never runs BeginPlay and is never
    // registered into a world, so two presentation/GAS side effects that are
    // harmless on a real SpawnActor'd enemy are unsafe here:
    //   1. RefreshHalo() (Combat/BreakerModifierComponent.cpp) unconditionally
    //      RegisterComponent()s a halo mesh the instant any modifier is
    //      granted, which asserts with no owning world. bShowHalo turns that
    //      off — a presentation concern this test does not exercise anyway.
    //   2. Warded's shield write (SetModifierShield -> the GAS-GENERATED
    //      Attributes->SetMaxShield) goes through
    //      FActiveGameplayEffectsContainer, which asserts with no live
    //      AbilitySystemComponent owner (InitAbilityActorInfo, which only
    //      BeginPlay calls). There is no test-safe way to satisfy that from
    //      outside a world, so the fix is to never roll Warded here: every
    //      test below grants an EXACT, hand-picked modifier
    //      (ConfigureWithExactModifiers) instead of the seeded random roll
    //      (ConfigureWithModifiers) SpawnCombatEncounter actually uses.
    //      Fleetfoot is chosen because ApplyPersistentModifiers' Fleetfoot
    //      branch only ever touches this enemy's own MoveSpeed/WeaveStrength
    //      fields — no Attributes, no GAS. This is a TEST-RIG adaptation:
    //      which modifier lands is irrelevant to what is under test here
    //      (rank promotion and kill-bucket classification depend on RANK and
    //      COUNT, never on which specific modifier was granted), and in the
    //      shipped game every enemy IS SpawnActor'd first, so
    //      ConfigureWithModifiers's random Warded roll is safe there.
    void PrepareEnemyForModifierGrant(ABreakerEnemy* Enemy)
    {
        if (!Enemy) return;
        if (UBreakerEnemyModifierComponent* Modifiers = Enemy->GetModifierComponent())
        {
            Modifiers->bShowHalo = false;
        }
    }

    const TArray<EBreakerEnemyModifier>& SafeSingleModifier()
    {
        static const TArray<EBreakerEnemyModifier> Safe = { EBreakerEnemyModifier::Fleetfoot };
        return Safe;
    }
}

// THROUGH-THE-ACTOR: rank ModifierBearing needs a producer (O27's kill-bucket
// instrument — Playtest/BreakerKillBuckets.h). Before this pass, modifiers
// only ever landed on the elite, and ABreakerGameMode::GrantModifiers restores
// the authored rank afterwards (correct for an elite, since ModifierBearing
// x2.5 would otherwise DEMOTE it from Elite x3.0) — so rank ModifierBearing
// never existed at kill time and the bucket the header calls "the one number
// that says whether [O27] worked" was structurally empty.
//
// This configures a REAL ABreakerEnemy the same way the gym does for a
// carrier (SetAreaLevel, then a modifier grant that is NOT captured-and-
// restored) and asserts the resulting kill actually classifies into the
// ModifierBearing bucket, in the precedent of
// RiorsEdge.Combat.PowerCurve.EnemyDropLevel (BreakerCurveCompositionTests.cpp):
// a library function proving the rule is not the same as an actor proving the
// shipped configuration reaches it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerModifierCarrierThroughActorTest,
    "RiorsEdge.Game.ModifierCarrierThroughActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerModifierCarrierThroughActorTest::RunTest(const FString& Parameters)
{
    using namespace BreakerGameModeTestHelpers;

    ABreakerGameMode* GameMode = NewObject<ABreakerGameMode>();
    if (!GameMode)
    {
        AddError(TEXT("Could not construct a game mode to read its shipped configuration from."));
        return false;
    }
    TestTrue(TEXT("The gym ships with the modifier layer switched on"), GameMode->bGrantModifiers);
    TestTrue(TEXT("The gym ships with at least one non-elite modifier carrier"), GameMode->GymModifierCarrierCount >= 1);

    ABreakerEnemy* Carrier = NewObject<ABreakerEnemy>();
    if (!Carrier)
    {
        AddError(TEXT("Could not construct an enemy to configure as a carrier."));
        return false;
    }
    PrepareEnemyForModifierGrant(Carrier);

    // The same shape SpawnCombatEncounter uses for a carrier: SetAreaLevel,
    // then a modifier grant that is NOT captured-and-restored afterwards —
    // that restoration is what GrantModifiers does for an elite and is
    // precisely the behaviour a carrier must NOT have. (The grant itself is
    // exact rather than the seeded roll for the test-rig reason above; the
    // GAME MODE still calls the real seeded ConfigureWithModifiers.)
    Carrier->SetAreaLevel(GameMode->GymAreaLevel);
    TestEqual(TEXT("A freshly configured carrier starts rank Trash"), Carrier->GetMonsterRank(), EBreakerMonsterRank::Trash);
    TestTrue(TEXT("The carrier's modifier grant succeeds"), Carrier->ConfigureWithExactModifiers(SafeSingleModifier()));

    TestEqual(TEXT("The carrier KEEPS rank ModifierBearing — it is not restored to anything else"),
        Carrier->GetMonsterRank(), EBreakerMonsterRank::ModifierBearing);

    const UBreakerEnemyModifierComponent* Modifiers = Carrier->GetModifierComponent();
    TestNotNull(TEXT("Every enemy carries a modifier component"), Modifiers);
    const int32 ModifierCount = Modifiers ? Modifiers->GetModifierCount() : 0;
    TestTrue(TEXT("The modifier component agrees at least one landed"), ModifierCount > 0);

    // The whole point: THIS enemy's kill, classified the way the real kill
    // telemetry component classifies it, lands in ModifierBearing.
    const EBreakerKillBucket Bucket = UBreakerKillBucketLibrary::ClassifyKill(
        Carrier->GetMonsterRank(), Carrier->IsRangedForTelemetry(), ModifierCount);
    TestEqual(TEXT("A configured carrier's kill classifies into the ModifierBearing bucket"),
        Bucket, EBreakerKillBucket::ModifierBearing);

    // Contrast case, in the same test so a future edit cannot make both pass
    // by accident: an ELITE that also carries modifiers must stay in the
    // Elite bucket, never ModifierBearing (O9: Rank and Modifiers are
    // separate fields). This is what GrantModifiers' capture-and-restore is
    // for, and it is the elite path this carrier path was deliberately built
    // NOT to share.
    ABreakerEnemy* Elite = NewObject<ABreakerEnemy>();
    PrepareEnemyForModifierGrant(Elite);
    Elite->SetAreaLevel(GameMode->GymAreaLevel);
    Elite->ConfigureElite();
    const EBreakerMonsterRank AuthoredRank = Elite->GetMonsterRank();
    TestEqual(TEXT("ConfigureElite grants rank Elite"), AuthoredRank, EBreakerMonsterRank::Elite);
    TestTrue(TEXT("The elite's own modifier grant succeeds"), Elite->ConfigureWithExactModifiers(SafeSingleModifier()));
    TestNotEqual(TEXT("ConfigureWithExactModifiers alone WOULD demote the elite (the bug GrantModifiers exists to prevent)"),
        Elite->GetMonsterRank(), AuthoredRank);
    // The demotion-guard GrantModifiers applies, reproduced here directly so
    // the contrast does not depend on calling the game mode's private method.
    Elite->SetMonsterRank(AuthoredRank);
    const int32 EliteModifierCount = Elite->GetModifierComponent() ? Elite->GetModifierComponent()->GetModifierCount() : 0;
    const EBreakerKillBucket EliteBucket = UBreakerKillBucketLibrary::ClassifyKill(
        Elite->GetMonsterRank(), Elite->IsRangedForTelemetry(), EliteModifierCount);
    TestEqual(TEXT("An elite carrying modifiers still classifies as Elite, not ModifierBearing"),
        EliteBucket, EBreakerKillBucket::Elite);
    return true;
}

// O40c: REACHABILITY IS PART OF DEFINITION-OF-DONE — the gym's default,
// shipped spawn set (as configured, not as documented) must actually produce
// a kill in each of the four buckets O27's difficulty model needs read
// (Playtest/BreakerKillBuckets.h), in the RiorsEdge.Movement.JumpGrantMatrix
// mold: read the real EditAnywhere defaults off a default-constructed game
// mode and drive the real per-archetype configuration calls
// SpawnCombatEncounter() makes on real actors, rather than asserting a fact
// about the design doc. (Boss is excluded: it is reached through
// SpawnBossTest(), not the standing encounter, and O40c's other claim —
// every wave archetype the solver claims — is covered separately by
// RiorsEdge.Game.Waves.ArchetypeReachability.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerGymSpawnSetBucketReachabilityTest,
    "RiorsEdge.Game.GymSpawnSetBucketReachability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerGymSpawnSetBucketReachabilityTest::RunTest(const FString& Parameters)
{
    using namespace BreakerGameModeTestHelpers;

    ABreakerGameMode* GameMode = NewObject<ABreakerGameMode>();
    if (!GameMode)
    {
        AddError(TEXT("Could not construct a game mode to read its shipped configuration from."));
        return false;
    }

    TSet<EBreakerKillBucket> ReachedBuckets;
    auto Classify = [](const ABreakerEnemy* Enemy) -> EBreakerKillBucket
    {
        const UBreakerEnemyModifierComponent* Modifiers = Enemy->GetModifierComponent();
        return UBreakerKillBucketLibrary::ClassifyKill(
            Enemy->GetMonsterRank(), Enemy->IsRangedForTelemetry(), Modifiers ? Modifiers->GetModifierCount() : 0);
    };

    // 1. Melee trash: SpawnCombatEncounter's three plain ABreakerEnemy bodies —
    //    no rank promotion, no ranged flag.
    ABreakerEnemy* MeleeTrash = NewObject<ABreakerEnemy>();
    MeleeTrash->SetAreaLevel(GameMode->GymAreaLevel);
    ReachedBuckets.Add(Classify(MeleeTrash));

    // 2. Ranged trash: the two LATTICE flankers, unconfigured beyond area
    //    level, exactly as the encounter leaves them.
    ABreakerRangedEnemy* Ranged = NewObject<ABreakerRangedEnemy>();
    Ranged->SetAreaLevel(GameMode->GymAreaLevel);
    ReachedBuckets.Add(Classify(Ranged));

    // 3. Elite: the arena anchor. ConfigureElite, then the SAME
    //    configure-then-restore sequence GrantModifiers uses — the exact bug
    //    class BreakerGameMode.cpp:1122-1127 (pre-fix) got wrong once already.
    //    (Modifier grant is the exact, GAS-safe form for this bare test
    //    actor — see PrepareEnemyForModifierGrant above; the shipped
    //    GrantModifiers still calls the real seeded roll.)
    ABreakerEnemy* Elite = NewObject<ABreakerEnemy>();
    PrepareEnemyForModifierGrant(Elite);
    Elite->SetAreaLevel(GameMode->GymAreaLevel);
    Elite->ConfigureElite();
    const EBreakerMonsterRank AuthoredEliteRank = Elite->GetMonsterRank();
    if (GameMode->bGrantModifiers && Elite->ConfigureWithExactModifiers(SafeSingleModifier())
        && Elite->GetMonsterRank() != AuthoredEliteRank)
    {
        Elite->SetMonsterRank(AuthoredEliteRank);
    }
    ReachedBuckets.Add(Classify(Elite));

    // 4. Modifier-bearing: the new non-elite carrier(s) — O27's kill-bucket
    //    producer. Unlike the elite, the promotion is KEPT.
    ABreakerEnemy* Carrier = NewObject<ABreakerEnemy>();
    PrepareEnemyForModifierGrant(Carrier);
    Carrier->SetAreaLevel(GameMode->GymAreaLevel);
    if (GameMode->bGrantModifiers) Carrier->ConfigureWithExactModifiers(SafeSingleModifier());
    ReachedBuckets.Add(Classify(Carrier));

    TestTrue(TEXT("The gym's default spawn set reaches melee trash"), ReachedBuckets.Contains(EBreakerKillBucket::MeleeTrash));
    TestTrue(TEXT("The gym's default spawn set reaches ranged trash"), ReachedBuckets.Contains(EBreakerKillBucket::RangedTrash));
    TestTrue(TEXT("The gym's default spawn set reaches elite"), ReachedBuckets.Contains(EBreakerKillBucket::Elite));
    TestTrue(TEXT("The gym's default spawn set reaches modifier-bearing"), ReachedBuckets.Contains(EBreakerKillBucket::ModifierBearing));

    // Where a claim fails, that IS the finding (O40c) — so state the claim
    // this test actually depends on: the gym ships with modifiers switched on
    // and at least one carrier, or bucket 4 above is trivially unreachable.
    TestTrue(TEXT("The modifier layer is switched on by default (or bucket 4 above is meaningless)"), GameMode->bGrantModifiers);
    TestTrue(TEXT("At least one carrier is configured by default (or bucket 4 above is meaningless)"), GameMode->GymModifierCarrierCount >= 1);
    return true;
}

#endif
