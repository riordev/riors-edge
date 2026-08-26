#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/BreakerSupportAbilities.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerAlteredEnemy.h"

// ---------------------------------------------------------------------------
// THE THREE ENEMY-SIDE SEAMS (2026-08-16): keyed wind-up duration, keyed aim
// error, keyed outgoing damage — the FlatArmorReduction pattern one layer
// over. The contract every test here pins: keyed push replaces (never
// stacks), pop removes, composed value is the product of live keys, and an
// EMPTY lane composes to exactly 1.0 so unkeyed behaviour is bit-identical
// to the authored numbers. A bare NewObject enemy is safe here: the lanes
// are plain TMaps and touch no world, no BeginPlay state, no components.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemySeamLaneTest,
    "RiorsEdge.Combat.EnemySeams.KeyedLanes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemySeamLaneTest::RunTest(const FString& Parameters)
{
    ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>();

    // The bit-identical pin: an unkeyed lane composes to EXACTLY 1.0 — not
    // approximately — because every consumer multiplies an authored value by
    // it and x * 1.0f is an exact float identity.
    TestTrue(TEXT("Unkeyed wind-up lane composes to exactly 1.0"),
        Enemy->GetComposedWindupDurationMultiplier() == 1.0f);
    TestTrue(TEXT("Unkeyed aim-error lane composes to exactly 1.0"),
        Enemy->GetComposedAimErrorMultiplier() == 1.0f);
    TestTrue(TEXT("Unkeyed outgoing lane composes to exactly 1.0"),
        Enemy->GetComposedOutgoingDamageMultiplier() == 1.0f);
    TestTrue(TEXT("Unkeyed effective attack damage IS AttackDamage, bit for bit"),
        Enemy->GetEffectiveAttackDamage() == Enemy->GetAttackDamage());

    // Product composition across distinct keys.
    Enemy->PushWindupDurationMultiplier(TEXT("Test.A"), 1.6f);
    TestEqual(TEXT("One key composes to its own value"),
        Enemy->GetComposedWindupDurationMultiplier(), 1.6f, 0.0001f);
    Enemy->PushWindupDurationMultiplier(TEXT("Test.B"), 2.0f);
    TestEqual(TEXT("Two keys compose as a product"),
        Enemy->GetComposedWindupDurationMultiplier(), 3.2f, 0.0001f);

    // The anti-stack rule: re-pushing a key REPLACES it.
    Enemy->PushWindupDurationMultiplier(TEXT("Test.A"), 1.5f);
    TestEqual(TEXT("Re-pushing a key replaces, never stacks"),
        Enemy->GetComposedWindupDurationMultiplier(), 3.0f, 0.0001f);

    // Pop removes exactly one key; popping an unknown key is a no-op.
    Enemy->PopWindupDurationMultiplier(TEXT("Test.B"));
    TestEqual(TEXT("Pop removes its key from the product"),
        Enemy->GetComposedWindupDurationMultiplier(), 1.5f, 0.0001f);
    Enemy->PopWindupDurationMultiplier(TEXT("Test.NeverPushed"));
    TestEqual(TEXT("Popping a never-pushed key changes nothing"),
        Enemy->GetComposedWindupDurationMultiplier(), 1.5f, 0.0001f);
    Enemy->PopWindupDurationMultiplier(TEXT("Test.A"));
    TestTrue(TEXT("An emptied lane returns to exactly 1.0"),
        Enemy->GetComposedWindupDurationMultiplier() == 1.0f);

    // A None key is refused, and a negative multiplier clamps to zero rather
    // than inverting a lane.
    Enemy->PushAimErrorMultiplier(NAME_None, 5.0f);
    TestTrue(TEXT("A None key never lands"),
        Enemy->GetComposedAimErrorMultiplier() == 1.0f);
    Enemy->PushAimErrorMultiplier(TEXT("Test.Negative"), -2.0f);
    TestEqual(TEXT("A negative multiplier clamps to zero, never inverts"),
        Enemy->GetComposedAimErrorMultiplier(), 0.0f, 0.0001f);
    Enemy->PopAimErrorMultiplier(TEXT("Test.Negative"));

    // The outgoing lane lands in the one damage-build input.
    Enemy->PushOutgoingDamageMultiplier(TEXT("Test.Soften"), 0.75f);
    TestEqual(TEXT("A 0.75 outgoing key softens effective attack damage to 75%"),
        Enemy->GetEffectiveAttackDamage(), Enemy->GetAttackDamage() * 0.75f, 0.0001f);
    Enemy->PopOutgoingDamageMultiplier(TEXT("Test.Soften"));
    TestTrue(TEXT("Popped, effective attack damage is bit-identical again"),
        Enemy->GetEffectiveAttackDamage() == Enemy->GetAttackDamage());
    return true;
}

// ---------------------------------------------------------------------------
// The aim-error arithmetic: Authored * M + Unit * (M - 1), floored at zero.
// The excess-over-one term is what lets an accuracy cut land on LATTICE,
// whose authored aim is exact — a plain multiplier would multiply zero.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemySeamSpreadMathTest,
    "RiorsEdge.Combat.EnemySeams.EffectiveSpread",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemySeamSpreadMathTest::RunTest(const FString& Parameters)
{
    // Neutral is EXACT: both terms are float identities at M == 1, so an
    // unkeyed enemy's cone is its authored spread bit for bit.
    TestTrue(TEXT("A perfect-aim enemy at neutral opens no cone at all"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(0.0f, 1.0f, 10.0f) == 0.0f);
    TestTrue(TEXT("An authored spread at neutral passes through bit-identical"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(4.0f, 1.0f, 10.0f) == 4.0f);

    // Degradation opens fresh cone even on a zero-spread marksman...
    TestEqual(TEXT("One full unit of degradation opens the unit cone"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(0.0f, 2.0f, 10.0f), 10.0f, 0.0001f);
    // ...and both scales an authored spread and adds unit cone on top.
    TestEqual(TEXT("Authored spread scales AND gains unit cone (4*1.5 + 10*0.5)"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(4.0f, 1.5f, 10.0f), 11.0f, 0.0001f);

    // An accuracy BUFF (below one) tightens but never below zero.
    TestEqual(TEXT("A buff below one floors at zero, never a negative cone"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(4.0f, 0.5f, 10.0f), 0.0f, 0.0001f);
    TestEqual(TEXT("A garbage negative multiplier clamps to M=0 and still floors at zero"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(4.0f, -3.0f, 10.0f), 0.0f, 0.0001f);
    return true;
}

// ---------------------------------------------------------------------------
// The buy-node consumers, pinned at their observable surface: the authored
// consumer magnitudes obey each node's own law, and the exact keyed push each
// consumer makes produces the seam-side change the node text describes.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEnemySeamConsumerTest,
    "RiorsEdge.Progression.BranchNodes.EnemySeamConsumers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEnemySeamConsumerTest::RunTest(const FString& Parameters)
{
    // TK8 INTERDICTION: "Delays — never cancels." A multiplier below 1 would
    // SHORTEN a telegraph, which Encounter-Design §0 forbids, so the authored
    // magnitude must sit at or above 1 and the consumer clamps there too.
    const ABreakerDeployable* DeployableDefaults = GetDefault<ABreakerDeployable>();
    TestTrue(TEXT("TK8: the Disruptor's wind-up multiplier delays, never cancels (>= 1)"),
        DeployableDefaults->InterdictionWindupMultiplier >= 1.0f);
    TestTrue(TEXT("TK8: the delay is a real delay, not a neutral no-op"),
        DeployableDefaults->InterdictionWindupMultiplier > 1.0f);

    // The push TK8 makes, replayed on a rig: wind-ups stretch by exactly the
    // authored multiplier while the key lives, and revert exactly on pop —
    // "begun inside it", enforced by the occupancy push/pop pair.
    ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>();
    Enemy->PushWindupDurationMultiplier(TEXT("Deployable.Interdiction.1"), DeployableDefaults->InterdictionWindupMultiplier);
    TestEqual(TEXT("TK8: inside the field, wind-ups stretch by the authored multiplier"),
        Enemy->GetComposedWindupDurationMultiplier(), DeployableDefaults->InterdictionWindupMultiplier, 0.0001f);
    Enemy->PopWindupDurationMultiplier(TEXT("Deployable.Interdiction.1"));
    TestTrue(TEXT("TK8: outside the field, wind-ups are bit-identical to authored"),
        Enemy->GetComposedWindupDurationMultiplier() == 1.0f);

    // U6 SUPPRESS's accuracy half (formerly recorded absent): the cut is a
    // real degradation (> 1 through the excess-over-one seam), and the base
    // application delay is non-zero so WA3 R2's "lands instantly too" clause
    // has something to remove.
    const UBreakerAbility_Suppress* SuppressDefaults = GetDefault<UBreakerAbility_Suppress>();
    TestTrue(TEXT("U6: the accuracy cut degrades (multiplier above 1)"),
        SuppressDefaults->SuppressAccuracyMultiplier > 1.0f);
    TestTrue(TEXT("WA3 R2: the base application delay is real, so 'instantly' buys something"),
        SuppressDefaults->AccuracyApplyDelaySeconds > 0.0f);
    // The observable on a perfect-aim LATTICE: Suppress's cut opens a real
    // cone where the authored aim had none.
    Enemy->PushAimErrorMultiplier(UBreakerAbility_Suppress::AccuracyModifierKey(), SuppressDefaults->SuppressAccuracyMultiplier);
    TestTrue(TEXT("U6: the cut opens a real error cone on a perfect-aim enemy"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(0.0f, Enemy->GetComposedAimErrorMultiplier(), 10.0f) > 0.0f);
    Enemy->PopAimErrorMultiplier(UBreakerAbility_Suppress::AccuracyModifierKey());
    TestTrue(TEXT("U6: popped, the perfect aim is perfect again"),
        ABreakerEnemy::GetEffectiveSpreadDegrees(0.0f, Enemy->GetComposedAimErrorMultiplier(), 10.0f) == 0.0f);

    // WA6 TELL: "hit you softer while the mark lives" — softer (< 1), never
    // disarmed (> 0), and the softening reverts exactly when the mark dies.
    const UBreakerAbility_Mark* MarkDefaults = GetDefault<UBreakerAbility_Mark>();
    TestTrue(TEXT("WA6: the marked enemy hits softer (multiplier below 1)"),
        MarkDefaults->TellOutgoingMultiplier < 1.0f);
    TestTrue(TEXT("WA6: softer, never disarmed (multiplier above 0)"),
        MarkDefaults->TellOutgoingMultiplier > 0.0f);
    Enemy->PushOutgoingDamageMultiplier(UBreakerAbility_Mark::TellModifierKey(), MarkDefaults->TellOutgoingMultiplier);
    TestEqual(TEXT("WA6: while the mark lives, the enemy's damage build softens by the authored fraction"),
        Enemy->GetEffectiveAttackDamage(), Enemy->GetAttackDamage() * MarkDefaults->TellOutgoingMultiplier, 0.0001f);
    Enemy->PopOutgoingDamageMultiplier(UBreakerAbility_Mark::TellModifierKey());
    TestTrue(TEXT("WA6: the mark ends and the damage build is bit-identical again"),
        Enemy->GetEffectiveAttackDamage() == Enemy->GetAttackDamage());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftTerminatorMarkTest,
    "RiorsEdge.Combat.EnemySeams.RiftTerminatorMark",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftTerminatorMarkTest::RunTest(const FString& Parameters)
{
    // O168's raise, asserted where it can be: the SHIPPED CONFIGURATION.
    // The failure this pins is the one that would actually happen — every
    // enemy in the game raising a rift completion because the mark defaulted
    // on, or because a subclass set it. A rift that completes when any trash
    // mob dies is worse than one that never completes, because it pays.
    //
    // The raise itself needs a world (HandleDeath drops loot, feeds telemetry
    // and schedules timers), so it is not exercised here. What IS pinned is
    // that nothing is a terminator until something says so, across every class
    // the game fields — which is the half a world-free test can hold and the
    // half that a mistake would live in.
    for (UClass* EnemyClass : { ABreakerEnemy::StaticClass(), ABreakerAlteredEnemy::StaticClass(),
        ABreakerRangedEnemy::StaticClass(), ABreakerSkirmisherEnemy::StaticClass(),
        ABreakerWardenEnemy::StaticClass(), ABreakerBossEnemy::StaticClass() })
    {
        const ABreakerEnemy* Enemy = NewObject<ABreakerEnemy>(GetTransientPackage(), EnemyClass);
        if (!Enemy)
        {
            AddError(FString::Printf(TEXT("%s failed to construct"), *EnemyClass->GetName()));
            continue;
        }
        TestFalse(FString::Printf(TEXT("%s is not a rift terminator until something marks it"),
            *EnemyClass->GetName()), Enemy->IsRiftTerminator());
        TestFalse(FString::Printf(TEXT("%s raises nothing, because nothing has bound it"),
            *EnemyClass->GetName()), Enemy->OnRiftTerminatorDefeated.IsBound());
    }

    // The mark round-trips, and it is the only way in. No constructor, no
    // spawn parameter, no rank: whoever builds the rift marks the body.
    ABreakerEnemy* Marked = NewObject<ABreakerEnemy>();
    Marked->SetRiftTerminator(true);
    TestTrue(TEXT("The mark is settable"), Marked->IsRiftTerminator());
    Marked->SetRiftTerminator(false);
    TestFalse(TEXT("And clearable"), Marked->IsRiftTerminator());
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
