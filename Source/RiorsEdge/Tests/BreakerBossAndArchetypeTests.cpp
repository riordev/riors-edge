#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerBossPhases.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerCoverBehavior.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerHoldfastEnemy.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Save/BreakerMissionContent.h"

// The boss's phase machine, the facing-armour geometry and the cover chooser
// are all pure, and all three are the kind of thing that breaks silently in a
// fight nobody wants to replay forty times.
//
// What no test can prove: whether the apparatus raise reads as an order, or
// whether the gym has any geometry a skirmisher can hide behind.

using EBoss = UBreakerBossPhaseLibrary;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossPhaseTransitionTest,
    "RiorsEdge.Combat.Boss.PhaseTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossPhaseTransitionTest::RunTest(const FString& Parameters)
{
    const FBreakerBossPhaseParams Params;

    // Health GATES, not timers (Encounter-Design §3.4).
    TestTrue(TEXT("Full health is Deployment"),
        EBoss::GetPhaseForHealthFraction(1.0f, Params) == EBreakerBossPhase::Deployment);
    TestTrue(TEXT("Just above the first gate is still Deployment"),
        EBoss::GetPhaseForHealthFraction(0.67f, Params) == EBreakerBossPhase::Deployment);
    TestTrue(TEXT("At the first gate it is Suppression"),
        EBoss::GetPhaseForHealthFraction(0.66f, Params) == EBreakerBossPhase::Suppression);
    TestTrue(TEXT("Just above the second gate is still Suppression"),
        EBoss::GetPhaseForHealthFraction(0.34f, Params) == EBreakerBossPhase::Suppression);
    TestTrue(TEXT("At the second gate it is Commitment"),
        EBoss::GetPhaseForHealthFraction(0.33f, Params) == EBreakerBossPhase::Commitment);
    TestTrue(TEXT("Dead is Commitment"),
        EBoss::GetPhaseForHealthFraction(0.0f, Params) == EBreakerBossPhase::Commitment);

    // MONOTONIC. This is the whole reason AdvancePhase exists separately: a
    // heal, a shield, or a chassis rebuild under the boss must not re-run a
    // phase script. Without it the fight has no defined length.
    TestTrue(TEXT("A healed boss does not return to Deployment"),
        EBoss::AdvancePhase(EBreakerBossPhase::Suppression, 1.0f, Params) == EBreakerBossPhase::Suppression);
    TestTrue(TEXT("A healed phase-3 boss does not return to Suppression"),
        EBoss::AdvancePhase(EBreakerBossPhase::Commitment, 0.9f, Params) == EBreakerBossPhase::Commitment);
    TestTrue(TEXT("Damage still advances the phase"),
        EBoss::AdvancePhase(EBreakerBossPhase::Deployment, 0.5f, Params) == EBreakerBossPhase::Suppression);
    // A burst that crosses both gates in one frame lands in phase 3 and does
    // not skip its setup by passing through phase 2 invisibly — every party
    // sees every phase's ENTRY because the entry runs on the transition.
    TestTrue(TEXT("A burst through both gates lands in Commitment"),
        EBoss::AdvancePhase(EBreakerBossPhase::Deployment, 0.05f, Params) == EBreakerBossPhase::Commitment);

    // A mis-authored params block (gates inverted) must not skip a phase.
    FBreakerBossPhaseParams Inverted;
    Inverted.SuppressionGate = 0.33f;
    Inverted.CommitmentGate = 0.66f;
    TestTrue(TEXT("Inverted gates are normalised, not obeyed"),
        EBoss::GetPhaseForHealthFraction(0.5f, Inverted) == EBreakerBossPhase::Suppression);

    // Out-of-range fractions are clamped rather than misclassified.
    TestTrue(TEXT("Above 100% health is still Deployment"),
        EBoss::GetPhaseForHealthFraction(5.0f, Params) == EBreakerBossPhase::Deployment);
    TestTrue(TEXT("Negative health is Commitment"),
        EBoss::GetPhaseForHealthFraction(-1.0f, Params) == EBreakerBossPhase::Commitment);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossOrderTest,
    "RiorsEdge.Combat.Boss.Orders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossOrderTest::RunTest(const FString& Parameters)
{
    const FBreakerBossPhaseParams Params;

    // Each phase gives exactly one kind of order, and phase 3 gives none: §3.4
    // says it stops commanding, and that is what makes it dangerous.
    TestTrue(TEXT("Phase 1 deploys"),
        EBoss::GetOrderForPhase(EBreakerBossPhase::Deployment) == EBreakerBossOrder::Deploy);
    TestTrue(TEXT("Phase 2 fires"),
        EBoss::GetOrderForPhase(EBreakerBossPhase::Suppression) == EBreakerBossOrder::Fire);
    TestTrue(TEXT("Phase 3 gives no orders"),
        EBoss::GetOrderForPhase(EBreakerBossPhase::Commitment) == EBreakerBossOrder::None);
    TestTrue(TEXT("Phase 3 spawns no adds"), !EBoss::ShouldSpawnAdds(EBreakerBossPhase::Commitment));
    TestTrue(TEXT("Phases 1 and 2 do spawn adds"),
        EBoss::ShouldSpawnAdds(EBreakerBossPhase::Deployment) && EBoss::ShouldSpawnAdds(EBreakerBossPhase::Suppression));
    TestTrue(TEXT("Phase 3 has no order interval at all"),
        EBoss::GetOrderIntervalSeconds(EBreakerBossPhase::Commitment, Params) < 0.0f);

    // The order clock. Fires once per interval, never twice on a hitch.
    //
    // Simulated at 60 Hz for 62 seconds rather than exactly 60: the clock
    // accumulates 1/60 in float, so a 20s order lands a frame or two LATE and a
    // 60-second window catches only two of the three. That drift is correct
    // behaviour for a fixed-step accumulator and the test measures the cadence
    // rather than pretending the arithmetic is exact.
    float Clock = 0.0f;
    int32 Fired = 0;
    for (int32 Step = 0; Step < 62 * 60; ++Step)
    {
        if (EBoss::AdvanceOrderClock(Clock, 1.0f / 60.0f, Params.DeployIntervalSeconds)) ++Fired;
    }
    TestEqual(TEXT("A 20s cadence fires three times in just over a minute"), Fired, 3);

    // THE HITCH GUARD. A 60-second frame must deploy one pack, not three. §5.1
    // requires every spawn to be previewed, and banked orders cannot be.
    Clock = 0.0f;
    TestTrue(TEXT("A huge delta fires the order"),
        EBoss::AdvanceOrderClock(Clock, 60.0f, Params.DeployIntervalSeconds));
    TestFalse(TEXT("...and does not bank a second one"),
        EBoss::AdvanceOrderClock(Clock, 0.0f, Params.DeployIntervalSeconds));
    TestEqual(TEXT("The clock resets rather than carrying a remainder"), Clock, 0.0f);

    // A phase with no orders never fires and never accumulates.
    Clock = 0.0f;
    TestFalse(TEXT("A phase with no orders never fires"), EBoss::AdvanceOrderClock(Clock, 999.0f, -1.0f));
    TestEqual(TEXT("...and keeps its clock at zero"), Clock, 0.0f);

    // THE PUNISH WINDOW. §3.2: the apparatus is exposed "only during Orders" —
    // and permanently in phase 3, because it has stopped commanding.
    TestFalse(TEXT("Phase 1 hides the apparatus between orders"),
        EBoss::IsApparatusExposed(EBreakerBossPhase::Deployment, false));
    TestTrue(TEXT("Phase 1 exposes it during an order"),
        EBoss::IsApparatusExposed(EBreakerBossPhase::Deployment, true));
    TestFalse(TEXT("Phase 2 hides it between orders"),
        EBoss::IsApparatusExposed(EBreakerBossPhase::Suppression, false));
    TestTrue(TEXT("Phase 3 exposes it permanently"),
        EBoss::IsApparatusExposed(EBreakerBossPhase::Commitment, false));

    // Both raise windows are long enough to be answered with POSITION rather
    // than with a reaction (O1: passive defence, §0: telegraphs are not to be
    // shortened).
    TestTrue(TEXT("The deploy raise is a repositioning window, not a frame"),
        Params.DeployRaiseSeconds >= 1.0f);
    TestTrue(TEXT("The fire raise is a repositioning window, not a frame"),
        Params.FireRaiseSeconds >= 1.0f);
    // The pointed alcove is previewed before anything comes out of it (§5.1).
    TestTrue(TEXT("Adds arrive after the order, not with it"), Params.DeploySpawnDelaySeconds > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossCommitmentTest,
    "RiorsEdge.Combat.Boss.CommitmentRewrites",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossCommitmentTest::RunTest(const FString& Parameters)
{
    const FBreakerBossPhaseParams Params;

    // §3.4: in phase 3 "its damage output rises and its defence falls." The
    // trade is what makes the phase survivable at all; a boss that only got
    // faster would be a wall that also hits harder.
    for (const EBreakerBossPhase Early : { EBreakerBossPhase::Deployment, EBreakerBossPhase::Suppression })
    {
        TestEqual(TEXT("Early phases do not change speed"),
            EBoss::GetPhaseSpeedMultiplier(Early, Params), 1.0f);
        TestEqual(TEXT("Early phases keep the authored sweep cadence"),
            EBoss::GetPhaseSweepCooldown(Early, 2.2f, Params), 2.2f, 0.0001f);
        TestEqual(TEXT("Early phases keep the authored slam cooldown"),
            EBoss::GetPhaseSlamCooldown(Early, 7.0f, Params), 7.0f, 0.0001f);
    }

    TestTrue(TEXT("Commitment is faster"),
        EBoss::GetPhaseSpeedMultiplier(EBreakerBossPhase::Commitment, Params) > 1.0f);
    TestTrue(TEXT("Commitment sweeps more often"),
        EBoss::GetPhaseSweepCooldown(EBreakerBossPhase::Commitment, 2.2f, Params) < 2.2f);
    TestTrue(TEXT("Commitment slams more often"),
        EBoss::GetPhaseSlamCooldown(EBreakerBossPhase::Commitment, 7.0f, Params) < 7.0f);

    // §3.2's "deliberately slower than the player": phase 3's +40% must still
    // leave it under the 950 cm/s sprint, or the fight becomes a chase the
    // player cannot win and O1 leaves them nothing to do about it.
    const ABreakerBossEnemy* Boss = GetDefault<ABreakerBossEnemy>();
    if (!TestNotNull(TEXT("The boss has a default object"), Boss)) return false;
    TestTrue(TEXT("Even at its fastest the boss is slower than a sprinting player"),
        300.0f * EBoss::GetPhaseSpeedMultiplier(EBreakerBossPhase::Commitment, Params) < 950.0f);

    // §3.1's corollary, checked against the shipped defaults: the boss must not
    // be a sponge, because its interest lives in the adds. The archetype ratio
    // is below 1 precisely so the inherited Warden 3.2x does not compound.
    TestTrue(TEXT("The boss does not inherit the Warden's health ratio"),
        Boss->GetArchetypeHealthMultiplier() < 1.0f);
    TestTrue(TEXT("The boss does not respawn"), !Boss->DoesRespawn());
    // A boss that chain-detonated on death would kill its own surviving adds
    // and end the encounter for the player.
    TestTrue(TEXT("The boss does not chain-detonate"), !Boss->DoesExplodeOnDeath());
    // §5.3's density ceiling has to be enforced at the source that creates the
    // density.
    TestTrue(TEXT("Deploy respects a live-add ceiling"), Boss->MaximumLiveAdds > 0);
    TestTrue(TEXT("Gallery Lattices respect the hard cap of 3"), Boss->GalleryLatticeCount <= 3);
    // §3.2's DoT stack cap.
    TestEqual(TEXT("Damage over time is capped at 3 stacks on the boss"),
        Boss->BossDamageOverTimeStackCap, 3);
    // Four alcoves and two galleries (§3.3), so the round-robin actually
    // rotates rather than pointing at the same corner every time.
    TestTrue(TEXT("There are multiple alcoves to choose between"), Boss->AlcoveOffsets.Num() >= 2);
    TestTrue(TEXT("There are galleries for the Lattices"), Boss->GalleryOffsets.Num() >= 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossGrammarTest,
    "RiorsEdge.Combat.Boss.Grammar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossGrammarTest::RunTest(const FString& Parameters)
{
    // The grammar is derived from the shipped numbers, so this reads the
    // default params and the default boss rather than authoring a fight of
    // its own.
    const FBreakerBossPhaseParams Params;
    const ABreakerBossEnemy* Boss = GetDefault<ABreakerBossEnemy>();
    if (!TestNotNull(TEXT("The boss has a default object"), Boss)) return false;
    const FBreakerBossGrammar Grammar = EBoss::MakeShippedGrammar(
        Params, Boss->AddsPerDeploy, Boss->GalleryLatticeCount, Boss->SweepWindupSeconds);

    // Every TIMED punish window is announced first, in its own list. The one
    // permanent window (Commitment) has no tell of its own: its tell is the
    // gate the phase before it ends on, which is asserted below.
    auto CheckTelegraphed = [this](const TArray<FBreakerBossBeat>& List, const TCHAR* Name)
    {
        bool bSeenTelegraph = false;
        for (const FBreakerBossBeat& B : List)
        {
            if (B.Beat == EBreakerBossBeat::Telegraph) bSeenTelegraph = true;
            if (B.Beat == EBreakerBossBeat::PunishWindow && B.Seconds >= 0.0f)
            {
                TestTrue(FString::Printf(TEXT("%s: a timed punish window is preceded by a telegraph (%s)"),
                    Name, *B.Tag.ToString()), bSeenTelegraph);
            }
        }
    };
    CheckTelegraphed(Grammar.FightLevel, TEXT("Fight-level"));
    CheckTelegraphed(Grammar.ForPhase(EBreakerBossPhase::Deployment), TEXT("Deployment"));
    CheckTelegraphed(Grammar.ForPhase(EBreakerBossPhase::Suppression), TEXT("Suppression"));
    CheckTelegraphed(Grammar.ForPhase(EBreakerBossPhase::Commitment), TEXT("Commitment"));

    // The first punish window of the fight is the front break (O198), and it is
    // a real window rather than a frame.
    const FBreakerBossBeat* First = EBoss::FirstPunishWindow(Grammar);
    if (!TestNotNull(TEXT("The grammar has a punish window"), First)) return false;
    TestEqual(TEXT("The first punish window is the front break"), First->Tag, FName(TEXT("FrontBreak")));
    TestEqual(TEXT("The front-break window is the authored length"),
        First->Seconds, Params.FrontBreakPunishSeconds, 0.0001f);
    TestTrue(TEXT("The front-break window is open for a real duration"), Params.FrontBreakPunishSeconds > 0.0f);

    // Gates descend across the phases and Commitment has none.
    const float Gate1 = EBoss::NextGate(EBreakerBossPhase::Deployment, Params);
    const float Gate2 = EBoss::NextGate(EBreakerBossPhase::Suppression, Params);
    TestTrue(TEXT("Deployment's gate is below full health"), Gate1 < 1.0f && Gate1 > 0.0f);
    TestTrue(TEXT("Gates are strictly descending"), Gate2 < Gate1 && Gate2 > 0.0f);
    TestTrue(TEXT("Commitment has no next gate"), EBoss::NextGate(EBreakerBossPhase::Commitment, Params) < 0.0f);
    // The grammar's gate beats carry the same numbers.
    const TArray<FBreakerBossBeat>& Suppression = Grammar.ForPhase(EBreakerBossPhase::Suppression);
    TestTrue(TEXT("Suppression ends on the gate that is Commitment's tell"),
        Suppression.Num() > 0 && Suppression.Last().Beat == EBreakerBossBeat::PhaseGate
        && FMath::IsNearlyEqual(Suppression.Last().GateFraction, Gate2, 0.0001f));
    const TArray<FBreakerBossBeat>& Commitment = Grammar.ForPhase(EBreakerBossPhase::Commitment);
    TestTrue(TEXT("Commitment's window is permanent"),
        Commitment.Num() > 0 && Commitment[0].Beat == EBreakerBossBeat::PunishWindow && Commitment[0].Seconds < 0.0f);

    // Phase 3 stops commanding: nothing arrives after its entry, and the room
    // itself is what changes instead.
    bool bArenaChange = false;
    for (const FBreakerBossBeat& B : Commitment)
    {
        TestTrue(TEXT("No add wave after Commitment's entry"), B.Beat != EBreakerBossBeat::AddWave);
        if (B.Beat == EBreakerBossBeat::ArenaChange) bArenaChange = true;
    }
    TestTrue(TEXT("Commitment changes the arena"), bArenaChange);

    // §5.3's ceiling: no single wave can outrun the live-add cap.
    for (const FBreakerBossBeat& B : Grammar.ForPhase(EBreakerBossPhase::Deployment))
    {
        if (B.Beat == EBreakerBossBeat::AddWave)
        {
            TestTrue(TEXT("A deploy wave fits under the live-add ceiling"), B.AddCount <= Boss->MaximumLiveAdds);
            TestTrue(TEXT("Adds arrive after the raise, not with it"), B.Seconds > 0.0f);
        }
    }

    // The Marshal authors no add gate, so its grammar has none anywhere: the
    // gate is the Holdfast's beat and the Marshal is byte-identical to a boss
    // that never heard of one.
    TestEqual(TEXT("The Marshal ships without an add gate"), Boss->PhaseParams.AddGateDamageReduction, 0.0f);
    for (const TArray<FBreakerBossBeat>& List : Grammar.PerPhase)
    {
        for (const FBreakerBossBeat& B : List)
        {
            TestTrue(TEXT("The Marshal's grammar has no AddGate"), B.Beat != EBreakerBossBeat::AddGate);
        }
    }

    // The window rule the actor reads.
    TestFalse(TEXT("No order, no break window: closed"),
        EBoss::IsPunishWindowOpen(EBreakerBossPhase::Deployment, false, 0.0f));
    TestTrue(TEXT("A running break window holds it open"),
        EBoss::IsPunishWindowOpen(EBreakerBossPhase::Deployment, false, 0.1f));
    TestTrue(TEXT("An order raise holds it open"),
        EBoss::IsPunishWindowOpen(EBreakerBossPhase::Suppression, true, 0.0f));
    TestTrue(TEXT("Commitment holds it open with nothing else"),
        EBoss::IsPunishWindowOpen(EBreakerBossPhase::Commitment, false, 0.0f));

    // The break window closes exactly once and never reads negative.
    float Remaining = Params.FrontBreakPunishSeconds;
    int32 Closes = 0;
    for (int32 Frame = 0; Frame < 600; ++Frame)
    {
        if (EBoss::AdvanceBreakWindow(Remaining, 1.0f / 60.0f)) ++Closes;
        TestTrue(TEXT("The break window never goes negative"), Remaining >= 0.0f);
    }
    TestEqual(TEXT("The break window closes exactly once"), Closes, 1);
    TestEqual(TEXT("A closed window reads zero"), Remaining, 0.0f);
    TestFalse(TEXT("A closed window does not close again"), EBoss::AdvanceBreakWindow(Remaining, 1.0f));
    Remaining = 0.5f;
    TestTrue(TEXT("A hitch closes it once"), EBoss::AdvanceBreakWindow(Remaining, 60.0f));
    TestEqual(TEXT("A hitch leaves it at zero, not below"), Remaining, 0.0f);
    return true;
}

// THE HOLDFAST'S GRAMMAR (O214). The Marshal's checks against the Holdfast's
// default object, then the one beat it adds: an AddGate after every AddWave
// in the two commanding phases, none in Commitment, and the pure rule the
// actor reads for it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHoldfastGrammarTest,
    "RiorsEdge.Combat.Boss.Holdfast.Grammar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHoldfastGrammarTest::RunTest(const FString& Parameters)
{
    const ABreakerHoldfastEnemy* Holdfast = GetDefault<ABreakerHoldfastEnemy>();
    if (!TestNotNull(TEXT("The Holdfast has a default object"), Holdfast)) return false;
    // The Holdfast's OWN params, not a default block: the reduction lives on
    // the class and the grammar has to be derived from what it ships.
    const FBreakerBossPhaseParams& Params = Holdfast->PhaseParams;
    const FBreakerBossGrammar Grammar = EBoss::MakeShippedGrammar(
        Params, Holdfast->AddsPerDeploy, Holdfast->GalleryLatticeCount, Holdfast->SweepWindupSeconds);

    // ---- The Marshal's checks, on this body ------------------------------
    auto CheckTelegraphed = [this](const TArray<FBreakerBossBeat>& List, const TCHAR* Name)
    {
        bool bSeenTelegraph = false;
        for (const FBreakerBossBeat& B : List)
        {
            if (B.Beat == EBreakerBossBeat::Telegraph) bSeenTelegraph = true;
            if (B.Beat == EBreakerBossBeat::PunishWindow && B.Seconds >= 0.0f)
            {
                TestTrue(FString::Printf(TEXT("%s: a timed punish window is preceded by a telegraph (%s)"),
                    Name, *B.Tag.ToString()), bSeenTelegraph);
            }
        }
    };
    CheckTelegraphed(Grammar.FightLevel, TEXT("Fight-level"));
    CheckTelegraphed(Grammar.ForPhase(EBreakerBossPhase::Deployment), TEXT("Deployment"));
    CheckTelegraphed(Grammar.ForPhase(EBreakerBossPhase::Suppression), TEXT("Suppression"));
    CheckTelegraphed(Grammar.ForPhase(EBreakerBossPhase::Commitment), TEXT("Commitment"));

    const FBreakerBossBeat* First = EBoss::FirstPunishWindow(Grammar);
    if (!TestNotNull(TEXT("The grammar has a punish window"), First)) return false;
    TestEqual(TEXT("The first punish window is the front break (O198)"), First->Tag, FName(TEXT("FrontBreak")));
    TestEqual(TEXT("The front-break window is the authored length"), First->Seconds, Params.FrontBreakPunishSeconds, 0.0001f);
    TestTrue(TEXT("The front-break window is open for a real duration"), Params.FrontBreakPunishSeconds > 0.0f);

    const float Gate1 = EBoss::NextGate(EBreakerBossPhase::Deployment, Params);
    const float Gate2 = EBoss::NextGate(EBreakerBossPhase::Suppression, Params);
    TestTrue(TEXT("Deployment's gate is below full health"), Gate1 < 1.0f && Gate1 > 0.0f);
    TestTrue(TEXT("Gates are strictly descending"), Gate2 < Gate1 && Gate2 > 0.0f);
    TestTrue(TEXT("Commitment has no next gate"), EBoss::NextGate(EBreakerBossPhase::Commitment, Params) < 0.0f);
    const TArray<FBreakerBossBeat>& Suppression = Grammar.ForPhase(EBreakerBossPhase::Suppression);
    TestTrue(TEXT("Suppression ends on the gate that is Commitment's tell"),
        Suppression.Num() > 0 && Suppression.Last().Beat == EBreakerBossBeat::PhaseGate
        && FMath::IsNearlyEqual(Suppression.Last().GateFraction, Gate2, 0.0001f));
    const TArray<FBreakerBossBeat>& Commitment = Grammar.ForPhase(EBreakerBossPhase::Commitment);
    TestTrue(TEXT("Commitment's window is permanent"),
        Commitment.Num() > 0 && Commitment[0].Beat == EBreakerBossBeat::PunishWindow && Commitment[0].Seconds < 0.0f);
    bool bArenaChange = false;
    for (const FBreakerBossBeat& B : Commitment)
    {
        TestTrue(TEXT("No add wave after Commitment's entry"), B.Beat != EBreakerBossBeat::AddWave);
        if (B.Beat == EBreakerBossBeat::ArenaChange) bArenaChange = true;
    }
    TestTrue(TEXT("Commitment changes the arena"), bArenaChange);
    for (const FBreakerBossBeat& B : Grammar.ForPhase(EBreakerBossPhase::Deployment))
    {
        if (B.Beat == EBreakerBossBeat::AddWave)
        {
            TestTrue(TEXT("A deploy wave fits under the live-add ceiling"), B.AddCount <= Holdfast->MaximumLiveAdds);
            TestTrue(TEXT("Adds arrive after the raise, not with it"), B.Seconds > 0.0f);
        }
    }

    // ---- The beat it adds -------------------------------------------------
    TestTrue(TEXT("The Holdfast authors an add gate"), Params.AddGateDamageReduction > 0.0f);
    TestTrue(TEXT("...that is not a wall (O31)"), Params.AddGateDamageReduction < 1.0f);
    // Authors nothing: it is the Warding Aura's reduction until felt apart.
    TestEqual(TEXT("The gate is the aura's reduction, not a new number"),
        Params.AddGateDamageReduction, FBreakerEnemyModifierParams().AuraDamageReduction, 0.0001f);

    // An AddGate follows EVERY AddWave in the commanding phases: the wave is
    // what the gate is made of, so a wave without a gate after it would be
    // adds that do not hold, and a gate without a wave before it would hold
    // on nothing.
    for (const EBreakerBossPhase Commanding : { EBreakerBossPhase::Deployment, EBreakerBossPhase::Suppression })
    {
        const TArray<FBreakerBossBeat>& List = Grammar.ForPhase(Commanding);
        const FString Name = EBoss::GetPhaseName(Commanding);
        int32 Waves = 0;
        int32 Gates = 0;
        for (int32 Index = 0; Index < List.Num(); ++Index)
        {
            if (List[Index].Beat == EBreakerBossBeat::AddWave)
            {
                ++Waves;
                TestTrue(*FString::Printf(TEXT("%s: an AddGate follows the %s wave"), *Name, *List[Index].Tag.ToString()),
                    List.IsValidIndex(Index + 1) && List[Index + 1].Beat == EBreakerBossBeat::AddGate);
            }
            if (List[Index].Beat == EBreakerBossBeat::AddGate)
            {
                ++Gates;
                TestTrue(*FString::Printf(TEXT("%s: the gate follows a wave"), *Name),
                    Index > 0 && List[Index - 1].Beat == EBreakerBossBeat::AddWave);
                TestEqual(*FString::Printf(TEXT("%s: the gate carries the shipped reduction"), *Name),
                    List[Index].Reduction, Params.AddGateDamageReduction, 0.0001f);
            }
        }
        TestTrue(*FString::Printf(TEXT("%s has at least one wave"), *Name), Waves >= 1);
        TestEqual(*FString::Printf(TEXT("%s has one gate per wave"), *Name), Gates, Waves);
    }
    for (const FBreakerBossBeat& B : Commitment)
    {
        TestTrue(TEXT("Commitment has no AddGate"), B.Beat != EBreakerBossBeat::AddGate);
    }
    for (const FBreakerBossBeat& B : Grammar.FightLevel)
    {
        TestTrue(TEXT("The gate is a phase beat, not a fight-level one"), B.Beat != EBreakerBossBeat::AddGate);
    }

    // The gated step. Live adds hold the phase; none, and it passes; and it
    // is monotonic under the same heal/shield/rebuild story the Marshal's is.
    TestTrue(TEXT("Below the gate with a live add, Deployment holds"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Deployment, 0.5f, Params, 1) == EBreakerBossPhase::Deployment);
    TestTrue(TEXT("Below the gate with no adds, Deployment passes"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Deployment, 0.5f, Params, 0) == EBreakerBossPhase::Suppression);
    TestTrue(TEXT("Suppression holds on a live Lattice"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Suppression, 0.1f, Params, 1) == EBreakerBossPhase::Suppression);
    TestTrue(TEXT("Suppression passes with the galleries dead"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Suppression, 0.1f, Params, 0) == EBreakerBossPhase::Commitment);
    TestTrue(TEXT("A burst through both gates with no adds lands in Commitment"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Deployment, 0.05f, Params, 0) == EBreakerBossPhase::Commitment);
    TestTrue(TEXT("A healed Suppression boss does not return to Deployment, adds or not"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Suppression, 1.0f, Params, 0) == EBreakerBossPhase::Suppression
        && EBoss::AdvancePhaseGated(EBreakerBossPhase::Suppression, 1.0f, Params, 3) == EBreakerBossPhase::Suppression);
    TestTrue(TEXT("Commitment stays Commitment whatever is alive"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Commitment, 0.9f, Params, 5) == EBreakerBossPhase::Commitment);
    TestTrue(TEXT("A negative count is no adds"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Deployment, 0.5f, Params, -1) == EBreakerBossPhase::Suppression);
    // The Marshal's zero reduction is the ungated step exactly, adds or not.
    const FBreakerBossPhaseParams Ungated;
    TestTrue(TEXT("At zero reduction the gate never holds"),
        EBoss::AdvancePhaseGated(EBreakerBossPhase::Deployment, 0.5f, Ungated, 8) == EBoss::AdvancePhase(EBreakerBossPhase::Deployment, 0.5f, Ungated));

    // The multiplier the actor pushes.
    TestEqual(TEXT("No adds: no reduction"), EBoss::AddGateIncomingMultiplier(EBreakerBossPhase::Deployment, 0, Params), 1.0f);
    TestEqual(TEXT("Commitment: no reduction with adds alive"), EBoss::AddGateIncomingMultiplier(EBreakerBossPhase::Commitment, 4, Params), 1.0f);
    TestEqual(TEXT("Deployment with adds: 1 - Reduction"),
        EBoss::AddGateIncomingMultiplier(EBreakerBossPhase::Deployment, 1, Params), 1.0f - Params.AddGateDamageReduction, 0.0001f);
    TestEqual(TEXT("Suppression with a Lattice: 1 - Reduction"),
        EBoss::AddGateIncomingMultiplier(EBreakerBossPhase::Suppression, 2, Params), 1.0f - Params.AddGateDamageReduction, 0.0001f);
    TestEqual(TEXT("The Marshal's zero reduction multiplies by one"),
        EBoss::AddGateIncomingMultiplier(EBreakerBossPhase::Deployment, 8, Ungated), 1.0f);
    // Never zero, whatever is authored: O31.
    FBreakerBossPhaseParams Wall;
    Wall.AddGateDamageReduction = 1.0f;
    TestTrue(TEXT("A reduction of one is still not immunity"),
        EBoss::AddGateIncomingMultiplier(EBreakerBossPhase::Deployment, 1, Wall) > 0.0f);
    return true;
}

// O31 ON THE BOSSES: no encounter may have a build that cannot participate.
// Every shipped boss class, the same assertions: a window to punish, a front
// that breaks, a DoT cap that is a cap and not a ban, a gate that is not a
// wall, a modifier map that starts clean, and a live-add ceiling a deploy
// fits under. Then the shipped identity of each.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossEveryBuildParticipatesTest,
    "RiorsEdge.Combat.Boss.EveryBuildParticipates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossEveryBuildParticipatesTest::RunTest(const FString& Parameters)
{
    for (const ABreakerBossEnemy* Boss : { static_cast<const ABreakerBossEnemy*>(GetDefault<ABreakerBossEnemy>()),
        static_cast<const ABreakerBossEnemy*>(GetDefault<ABreakerHoldfastEnemy>()) })
    {
        if (!TestNotNull(TEXT("The boss has a default object"), Boss)) continue;
        const FString Name = Boss->GetClass()->GetName();
        const FBreakerBossPhaseParams& Params = Boss->PhaseParams;
        const FBreakerBossGrammar Grammar = EBoss::MakeShippedGrammar(
            Params, Boss->AddsPerDeploy, Boss->GalleryLatticeCount, Boss->SweepWindupSeconds);

        // A window exists and the last one never closes: a burst build and a
        // sustained build both get their turn at the weak point.
        TestNotNull(*FString::Printf(TEXT("%s: the grammar has a punish window"), *Name), EBoss::FirstPunishWindow(Grammar));
        const TArray<FBreakerBossBeat>& Commitment = Grammar.ForPhase(EBreakerBossPhase::Commitment);
        TestTrue(*FString::Printf(TEXT("%s: Commitment's window is permanent"), *Name),
            Commitment.Num() > 0 && Commitment[0].Beat == EBreakerBossBeat::PunishWindow && Commitment[0].Seconds < 0.0f);

        // The front is a pool, not a wall (O198): a real fraction, under one.
        TestTrue(*FString::Printf(TEXT("%s: the front is a fraction of max health"), *Name),
            Boss->FrontShieldFractionOfMaxHealth > 0.0f && Boss->FrontShieldFractionOfMaxHealth < 1.0f);
        // DoT builds participate: a cap of at least one stack.
        TestTrue(*FString::Printf(TEXT("%s: DoTs stack at least once"), *Name), Boss->BossDamageOverTimeStackCap >= 1);
        // The add gate is never immunity, for any count of adds.
        TestTrue(*FString::Printf(TEXT("%s: the add gate is below one"), *Name), Params.AddGateDamageReduction < 1.0f);
        for (const int32 Count : { 0, 1, 2, 8, 100 })
        {
            for (const EBreakerBossPhase Phase : { EBreakerBossPhase::Deployment, EBreakerBossPhase::Suppression, EBreakerBossPhase::Commitment })
            {
                TestTrue(*FString::Printf(TEXT("%s: the gate multiplier is above zero at %d adds"), *Name, Count),
                    EBoss::AddGateIncomingMultiplier(Phase, Count, Params) > 0.0f);
            }
        }
        // The body starts ungated: a fresh component composes to exactly one,
        // so the only way a gate stands is a push the actor made on a crossing.
        const UBreakerCombatComponent* Fresh = NewObject<UBreakerCombatComponent>();
        TestEqual(*FString::Printf(TEXT("%s: a fresh combat component composes incoming to one"), *Name),
            Fresh->GetComposedIncomingDamageMultiplier(), 1.0f);
        // §5.3's ceiling, and a deploy fits under it.
        TestTrue(*FString::Printf(TEXT("%s: there is a live-add ceiling"), *Name), Boss->MaximumLiveAdds > 0);
        TestTrue(*FString::Printf(TEXT("%s: a deploy fits under it"), *Name), Boss->AddsPerDeploy <= Boss->MaximumLiveAdds);
    }

    // ---- Shipped identity -------------------------------------------------
    const ABreakerBossEnemy* Marshal = GetDefault<ABreakerBossEnemy>();
    const ABreakerHoldfastEnemy* Holdfast = GetDefault<ABreakerHoldfastEnemy>();
    if (!TestNotNull(TEXT("The Marshal has a default object"), Marshal)) return false;
    if (!TestNotNull(TEXT("The Holdfast has a default object"), Holdfast)) return false;
    TestEqual(TEXT("The Marshal has no add gate"), Marshal->PhaseParams.AddGateDamageReduction, 0.0f);

    // O214: a Vestige mass, no stage, boss rank, not a sponge, no respawn, no
    // detonation, and not wearing the Altered heavy's body.
    TestTrue(TEXT("The Holdfast is a Vestige"), Holdfast->GetFamily() == EBreakerEnemyFamily::Vestige);
    TestTrue(TEXT("...with no severance stage"), Holdfast->GetSeveranceStage() == EBreakerSeveranceStage::NotApplicable);
    TestTrue(TEXT("...at boss rank"), Holdfast->GetMonsterRank() == EBreakerMonsterRank::Boss);
    TestTrue(TEXT("...and not a sponge"), Holdfast->GetArchetypeHealthMultiplier() < 1.0f);
    TestFalse(TEXT("...and does not respawn"), Holdfast->DoesRespawn());
    TestFalse(TEXT("...and does not chain-detonate"), Holdfast->DoesExplodeOnDeath());
    TestFalse(TEXT("...and does not wear George"), Holdfast->BodyMeshAsset.ToString().Contains(TEXT("George")));
    TestTrue(TEXT("...and has a body slot filled (O190)"), Holdfast->BodyMeshAsset.IsValid());

    // The mission file names it, and the name resolves to this class without
    // the loader knowing what a class is.
    TestTrue(TEXT("\"Holdfast\" resolves to the Holdfast"),
        ABreakerBossEnemy::ClassForBossName(FName(TEXT("Holdfast"))) == ABreakerHoldfastEnemy::StaticClass());
    TestTrue(TEXT("\"FieldMarshal\" resolves to the Marshal"),
        ABreakerBossEnemy::ClassForBossName(FName(TEXT("FieldMarshal"))) == ABreakerBossEnemy::StaticClass());
    TestTrue(TEXT("An unknown name resolves to nothing"), !ABreakerBossEnemy::ClassForBossName(FName(TEXT("Nobody"))));
    TestTrue(TEXT("No name resolves to nothing"), !ABreakerBossEnemy::ClassForBossName(NAME_None));
    int32 BossBeats = 0;
    for (const FBreakerMissionDefinition& Mission : UBreakerMissionLibrary::GetMissions())
    {
        for (const FBreakerMissionBeat& Beat : Mission.Beats)
        {
            if (Beat.Kind != EBreakerMissionBeatKind::Boss) continue;
            ++BossBeats;
            TestTrue(*FString::Printf(TEXT("%s's Boss beat names a body that exists"), *Mission.MissionId.ToString()),
                ABreakerBossEnemy::ClassForBossName(Beat.Boss) != nullptr);
            if (Mission.Act == 1)
            {
                TestTrue(TEXT("Act I ends on The Holdfast (O214)"),
                    ABreakerBossEnemy::ClassForBossName(Beat.Boss) == ABreakerHoldfastEnemy::StaticClass());
            }
        }
    }
    TestTrue(TEXT("There is a Boss beat to name"), BossBeats >= 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerFacingArmorTest,
    "RiorsEdge.Combat.Archetypes.FacingArmor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerFacingArmorTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerDamageLibrary;
    const FVector Forward(1.0f, 0.0f, 0.0f);
    const FVector Self(0.0f, 0.0f, 0.0f);

    // A multiplier of exactly 1 is the OFF switch, and it must short-circuit
    // before any geometry runs: every hit in the game passes through here.
    TestEqual(TEXT("A rear multiplier of 1 changes nothing, from any angle"),
        ELib::GetFacingArmorMultiplier(Forward, Self, FVector(-1000.0f, 0.0f, 0.0f), 1.0f, 0.0f), 1.0f);

    // Frontal hits keep full armour; rear hits lose it (§2.3).
    TestEqual(TEXT("A hit from directly in front keeps full armour"),
        ELib::GetFacingArmorMultiplier(Forward, Self, FVector(1000.0f, 0.0f, 0.0f), 0.0f, 0.0f), 1.0f);
    TestEqual(TEXT("A hit from directly behind bypasses armour entirely"),
        ELib::GetFacingArmorMultiplier(Forward, Self, FVector(-1000.0f, 0.0f, 0.0f), 0.0f, 0.0f), 0.0f);

    // The FLANK is the interesting case, and the threshold is what tunes it.
    // A hit from slightly FORWARD of perpendicular (dot ~= +0.01) is the exact
    // boundary case: at threshold 0 it is still frontal, and the shipped
    // threshold of 0.15 widens the vulnerable arc to include it. That widening
    // is what makes "circle it" the answer rather than "stand precisely behind
    // it", which no player can do against a Warden that always turns to face.
    const FVector SlightlyForwardFlank(10.0f, 1000.0f, 0.0f);
    TestEqual(TEXT("At threshold 0, slightly forward of perpendicular is still frontal"),
        ELib::GetFacingArmorMultiplier(Forward, Self, SlightlyForwardFlank, 0.0f, 0.0f), 1.0f);
    TestEqual(TEXT("The shipped threshold widens the arc onto that flank"),
        ELib::GetFacingArmorMultiplier(Forward, Self, SlightlyForwardFlank, 0.0f, 0.15f), 0.0f);

    // Exactly perpendicular, and behind it, are vulnerable at threshold 0.
    TestEqual(TEXT("A hit from exactly perpendicular is vulnerable at threshold 0"),
        ELib::GetFacingArmorMultiplier(Forward, Self, FVector(0.0f, 1000.0f, 0.0f), 0.0f, 0.0f), 0.0f);
    const FVector RearFlank(-10.0f, 1000.0f, 0.0f);
    TestEqual(TEXT("A hit from behind perpendicular is vulnerable"),
        ELib::GetFacingArmorMultiplier(Forward, Self, RearFlank, 0.0f, 0.0f), 0.0f);
    TestEqual(TEXT("A negative threshold narrows the vulnerable arc to the true rear"),
        ELib::GetFacingArmorMultiplier(Forward, Self, RearFlank, 0.0f, -0.5f), 1.0f);

    // Height must not decide facing: a player who gets ABOVE an enemy has not
    // flanked it, and letting Z into the dot would say they had.
    TestEqual(TEXT("A hit from directly above the front is still a frontal hit"),
        ELib::GetFacingArmorMultiplier(Forward, Self, FVector(1000.0f, 0.0f, 5000.0f), 0.0f, 0.0f), 1.0f);

    // Degenerate inputs fall back to "no change" rather than to a random arc.
    TestEqual(TEXT("A hit from exactly the enemy's own position changes nothing"),
        ELib::GetFacingArmorMultiplier(Forward, Self, Self, 0.0f, 0.0f), 1.0f);
    TestEqual(TEXT("A zero forward vector changes nothing"),
        ELib::GetFacingArmorMultiplier(FVector::ZeroVector, Self, FVector(-1000.0f, 0.0f, 0.0f), 0.0f, 0.0f), 1.0f);

    // A partial rear multiplier passes through rather than snapping to 0 or 1:
    // the geometry honours whatever fraction its caller hands it.
    TestEqual(TEXT("A partial rear multiplier is honoured"),
        ELib::GetFacingArmorMultiplier(Forward, Self, FVector(-1000.0f, 0.0f, 0.0f), 0.4f, 0.0f), 0.4f);

    // The archetypes that depend on it are actually wired to use it.
    const ABreakerWardenEnemy* Warden = GetDefault<ABreakerWardenEnemy>();
    if (!TestNotNull(TEXT("The Warden has a default object"), Warden)) return false;
    TestEqual(TEXT("The Warden's front is a pool of 15% of its max health (O198)"),
        Warden->FrontShieldFractionOfMaxHealth, 0.15f, 0.0001f);
    // Both telegraphs are spatial windows, not reaction frames (O1, §0).
    TestTrue(TEXT("The sweep draw-back is a real window"), Warden->SweepWindupSeconds >= 0.4f);
    TestTrue(TEXT("The slam ring is a long, readable growth"), Warden->SlamWindupSeconds >= 0.8f);
    // The slam exists precisely so standing behind it is not free.
    TestTrue(TEXT("The slam reaches further than the sweep"), Warden->SlamRadiusCm > Warden->SweepRangeCm);
    // A plain Warden leaves no hazards: lingering ground is the Cascading
    // MODIFIER's identity, and giving every Warden one would make the modifier
    // invisible when it appeared.
    TestFalse(TEXT("A plain Warden leaves no lingering hazard"), Warden->bSlamLeavesHazard);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCoverBehaviorTest,
    "RiorsEdge.Combat.Archetypes.CoverBehaviour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoverBehaviorTest::RunTest(const FString& Parameters)
{
    using ECover = UBreakerCoverLibrary;
    FBreakerCoverParams Params;
    const FVector Origin(0.0f, 0.0f, 0.0f);
    const FVector Threat(2000.0f, 0.0f, 0.0f);

    // Candidates are generated around the enemy, inside the search radius.
    const TArray<FVector> Candidates = ECover::GenerateCoverCandidates(Origin, Params, 99);
    TestEqual(TEXT("It generates the requested number of candidates"), Candidates.Num(), Params.CandidateCount);
    for (const FVector& Candidate : Candidates)
    {
        TestTrue(TEXT("No candidate is outside the search radius"),
            FVector::Dist2D(Candidate, Origin) <= Params.SearchRadiusCm + 1.0f);
    }
    // Desynced per seed, so a pack does not break in one direction as a single
    // object — the failure mode the three-gear chase was built to kill.
    const TArray<FVector> Other = ECover::GenerateCoverCandidates(Origin, Params, 100);
    bool bDiffers = false;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        if (!Candidates[Index].Equals(Other[Index], 1.0f)) { bDiffers = true; break; }
    }
    TestTrue(TEXT("Two enemies in the same place choose different rings"), bDiffers);
    // ...but a given seed is reproducible, or a bad choice cannot be debugged.
    TestTrue(TEXT("The same seed generates the same ring"),
        ECover::GenerateCoverCandidates(Origin, Params, 99)[0].Equals(Candidates[0], 0.01f));

    // Range band rejection: a cover point in the player's face is not cover,
    // and one across the map is a different fight.
    TestEqual(TEXT("A point too close to the threat is rejected"),
        ECover::ScoreCoverCandidate(Threat + FVector(100.0f, 0.0f, 0.0f), Origin, Threat, Params),
        ECover::GetRejectedCoverScore());
    TestEqual(TEXT("A point too far from the threat is rejected"),
        ECover::ScoreCoverCandidate(FVector(-50000.0f, 0.0f, 0.0f), Origin, Threat, Params),
        ECover::GetRejectedCoverScore());

    // Between two in-band points it takes the nearer one: committing to a long
    // relocation reads as fleeing and removes the player's chance to push.
    const FVector Near(0.0f, 800.0f, 0.0f);
    const FVector Far(0.0f, -1200.0f, 0.0f);
    TestTrue(TEXT("A nearer in-band point scores better"),
        ECover::ScoreCoverCandidate(Near, Origin, Threat, Params)
        < ECover::ScoreCoverCandidate(Far, Origin, Threat, Params));

    FVector Chosen;
    TestTrue(TEXT("It chooses from the blocked candidates"),
        ECover::ChooseCoverPoint({ Far, Near }, Origin, Threat, Params, Chosen));
    TestTrue(TEXT("...and it chooses the nearer one"), Chosen.Equals(Near, 1.0f));

    // NO COVER is a legal answer and must be reported, not faked. An open field
    // is a real map, and an enemy that freezes in one is a bug.
    TestFalse(TEXT("An empty candidate list finds no cover"),
        ECover::ChooseCoverPoint({}, Origin, Threat, Params, Chosen));
    TestFalse(TEXT("A list of only out-of-band points finds no cover"),
        ECover::ChooseCoverPoint({ FVector(-50000.0f, 0.0f, 0.0f) }, Origin, Threat, Params, Chosen));

    // A mis-authored band (min above max) is normalised rather than rejecting
    // everything.
    Params.PreferredMinRangeCm = 2600.0f;
    Params.PreferredMaxRangeCm = 700.0f;
    TestTrue(TEXT("A swapped range band still accepts an in-band point"),
        ECover::ScoreCoverCandidate(Near, Origin, Threat, Params) < ECover::GetRejectedCoverScore());
    return true;
}

// NAV-2. The firing flank is the Skirmisher's cover point turned inside out:
// a spot BESIDE a piece from which the player can be seen. The geometry is
// pure — two points per anchor, perpendicular to the anchor-to-threat line,
// at the standoff — and the choice reuses the same band rejection and travel
// scoring as hiding does, so one rule governs both verbs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerCoverFlankTest,
    "RiorsEdge.Combat.Archetypes.CoverFlank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerCoverFlankTest::RunTest(const FString& Parameters)
{
    using ECover = UBreakerCoverLibrary;
    FBreakerCoverParams Params;
    TestEqual(TEXT("The standoff ships at the distance the field already stands a body off an anchor"),
        Params.FlankStandoffCm, 260.0f);

    const FVector Threat(2000.0f, 0.0f, 0.0f);
    // One anchor mid-band, one almost on the threat.
    const FVector InBand(500.0f, 0.0f, 0.0f);
    const FVector TooClose(1800.0f, 0.0f, 0.0f);
    const TArray<FVector> Candidates = ECover::BuildFlankCandidates({ InBand, TooClose }, Threat, Params);
    TestEqual(TEXT("Every anchor yields two flanks"), Candidates.Num(), 4);

    const TArray<FVector> Anchors = { InBand, InBand, TooClose, TooClose };
    for (int32 Index = 0; Index < Candidates.Num() && Index < Anchors.Num(); ++Index)
    {
        const FVector Offset = Candidates[Index] - Anchors[Index];
        const FVector ToThreat = (Threat - Anchors[Index]).GetSafeNormal2D();
        TestEqual(FString::Printf(TEXT("Flank %d sits at the standoff"), Index),
            static_cast<float>(Offset.Size2D()), Params.FlankStandoffCm, 0.01f);
        TestEqual(FString::Printf(TEXT("Flank %d is perpendicular to the threat line"), Index),
            static_cast<float>(FVector::DotProduct(Offset.GetSafeNormal2D(), ToThreat)), 0.0f, 0.001f);
        TestEqual(FString::Printf(TEXT("Flank %d keeps the anchor's height"), Index),
            static_cast<float>(Candidates[Index].Z), static_cast<float>(Anchors[Index].Z), 0.001f);
    }
    // The two flanks of one anchor are on opposite sides.
    TestTrue(TEXT("An anchor's two flanks face each other across it"),
        (Candidates[0] - InBand).Equals(-(Candidates[1] - InBand), 0.01f));

    // A flank inside the minimum band is not a firing position: it is the
    // player's face.
    TestEqual(TEXT("A flank inside the minimum band is rejected"),
        ECover::ScoreCoverCandidate(Candidates[2], FVector::ZeroVector, Threat, Params), ECover::GetRejectedCoverScore());
    TestEqual(TEXT("...and so is its twin"),
        ECover::ScoreCoverCandidate(Candidates[3], FVector::ZeroVector, Threat, Params), ECover::GetRejectedCoverScore());

    // From a body standing on the +Y side, the +Y flank of the in-band anchor
    // is the nearer legal one and wins.
    const FVector Current(0.0f, 300.0f, 0.0f);
    FVector Chosen;
    TestTrue(TEXT("It chooses a flank when one is in band"),
        ECover::ChooseCoverPoint(Candidates, Current, Threat, Params, Chosen));
    TestTrue(TEXT("...the nearer in-band flank"),
        Chosen.Equals(FVector(500.0f, 260.0f, 0.0f), 0.01f));

    // No anchors, no flanks; and an anchor standing on the threat has no
    // perpendicular to offer.
    TestEqual(TEXT("Empty input yields no candidates"),
        ECover::BuildFlankCandidates({}, Threat, Params).Num(), 0);
    TestFalse(TEXT("Empty input chooses nothing"),
        ECover::ChooseCoverPoint(ECover::BuildFlankCandidates({}, Threat, Params), Current, Threat, Params, Chosen));
    TestEqual(TEXT("An anchor on the threat yields nothing"),
        ECover::BuildFlankCandidates({ Threat }, Threat, Params).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSkirmisherDefaultsTest,
    "RiorsEdge.Combat.Archetypes.SkirmisherShipsFair",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerSkirmisherDefaultsTest::RunTest(const FString& Parameters)
{
    // A guard on the shipped defaults, not on the maths — the same shape as the
    // LATTICE fairness test. All values are O2 PLACEHOLDER and may legitimately
    // change; if they do, change this test deliberately and say so.
    const ABreakerSkirmisherEnemy* Skirmisher = GetDefault<ABreakerSkirmisherEnemy>();
    if (!TestNotNull(TEXT("The skirmisher has a default object"), Skirmisher)) return false;

    // It is an EARLY-severance Altered, which is what turns cover and flinch on
    // (Assets/story-source.md §1.5). This is the mechanical claim, not a label.
    TestTrue(TEXT("The skirmisher is Altered"), Skirmisher->GetFamily() == EBreakerEnemyFamily::Altered);
    TestTrue(TEXT("...at early severance, which is what grants cover and flinch"),
        Skirmisher->GetSeveranceStage() == EBreakerSeveranceStage::Early);

    // It has no contact attack: it is a shooter, and closing on it must be the
    // player's play rather than its own.
    TestEqual(TEXT("It has no contact attack"), Skirmisher->GetAttackRange(), 0.0f);
    TestEqual(TEXT("It never lunges"), Skirmisher->GetLungeRange(), 0.0f);

    // The exposure window is the whole counterplay: it must be long enough to
    // punish under passive defence (O1) and short enough to be a trade.
    TestTrue(TEXT("Exposure is long enough to be punished"), Skirmisher->MaximumExposureSeconds >= 1.5f);
    TestTrue(TEXT("Cover time is short enough that the loop keeps moving"), Skirmisher->PeekDelaySeconds <= 2.0f);
    // A relocation always terminates, so a point behind unreachable geometry
    // cannot strand it walking into a wall forever.
    TestTrue(TEXT("A relocation always times out"), Skirmisher->RelocateTimeoutSeconds > 0.0f);

    // The flinch is a tempo reward, not a stun-lock. A long flinch on a burst
    // weapon lets the player suppress the archetype out of the fight.
    TestTrue(TEXT("The flinch is short"), Skirmisher->FlinchSeconds <= 0.75f);
    TestTrue(TEXT("The flinch cannot be chained by a high-RPM weapon"),
        Skirmisher->FlinchCooldownSeconds > Skirmisher->FlinchSeconds);

    // A full burst is roughly ONE chassis attack, so the archetype is not
    // secretly three times as dangerous as its area level claims.
    const float BurstFraction = Skirmisher->DamagePerRoundFraction * Skirmisher->RoundsPerBurst;
    TestTrue(TEXT("A full burst is about one chassis attack"),
        BurstFraction > 0.75f && BurstFraction < 1.35f);

    // Its round is fast and flat — the readable difference from LATTICE's slow
    // orb, which is the Vestige answer to the same range. The two ranged
    // archetypes therefore fail to different kinds of movement.
    TestTrue(TEXT("The soldier's round is much faster than the Lattice orb"),
        Skirmisher->ProjectileSpeed > 2000.0f);
    // There is still a tell before the first round of a burst, on top of the
    // whole silhouette standing up.
    TestTrue(TEXT("There is an aim tell before the burst"), Skirmisher->AimSeconds > 0.0f);
    // It wants a real standoff: cover the player is already standing next to is
    // not cover.
    TestTrue(TEXT("Its preferred band is a band, not a point"),
        Skirmisher->Cover.PreferredMaxRangeCm > Skirmisher->Cover.PreferredMinRangeCm);
    TestTrue(TEXT("It searches a local area, not the whole arena"),
        Skirmisher->Cover.SearchRadiusCm <= Skirmisher->Cover.PreferredMaxRangeCm);

    // The Warden is MID severance: it carries equipment but has lost the
    // tactics, so it must NOT claim the early-stage behaviours.
    const ABreakerWardenEnemy* Warden = GetDefault<ABreakerWardenEnemy>();
    if (!TestNotNull(TEXT("The Warden has a default object"), Warden)) return false;
    TestTrue(TEXT("The Warden is Altered"), Warden->GetFamily() == EBreakerEnemyFamily::Altered);
    TestTrue(TEXT("The Warden is mid severance, matching how it fights"),
        Warden->GetSeveranceStage() == EBreakerSeveranceStage::Mid);

    // The boss is the most lucid thing in the game, because it still commands.
    const ABreakerBossEnemy* Boss = GetDefault<ABreakerBossEnemy>();
    if (!TestNotNull(TEXT("The boss has a default object"), Boss)) return false;
    TestTrue(TEXT("The Field Marshal is an early-severance Altered"),
        Boss->GetFamily() == EBreakerEnemyFamily::Altered && Boss->GetSeveranceStage() == EBreakerSeveranceStage::Early);
    return true;
}

#endif
