#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerBossEnemy.h"
#include "Combat/BreakerBossPhases.h"
#include "Combat/BreakerCoverBehavior.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerSkirmisherEnemy.h"
#include "Combat/BreakerWardenEnemy.h"

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
        TestEqual(TEXT("Early phases keep full frontal armour"),
            EBoss::GetPhaseFrontalArmor(Early, 90.0f, Params), 90.0f, 0.0001f);
    }

    TestTrue(TEXT("Commitment is faster"),
        EBoss::GetPhaseSpeedMultiplier(EBreakerBossPhase::Commitment, Params) > 1.0f);
    TestTrue(TEXT("Commitment sweeps more often"),
        EBoss::GetPhaseSweepCooldown(EBreakerBossPhase::Commitment, 2.2f, Params) < 2.2f);
    TestTrue(TEXT("Commitment slams more often"),
        EBoss::GetPhaseSlamCooldown(EBreakerBossPhase::Commitment, 7.0f, Params) < 7.0f);
    TestEqual(TEXT("Commitment halves the frontal armour"),
        EBoss::GetPhaseFrontalArmor(EBreakerBossPhase::Commitment, 90.0f, Params), 45.0f, 0.0001f);

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

    // A partial rear multiplier (the boss's phase 3 does not zero its armour,
    // it halves it) passes through rather than snapping to 0 or 1.
    TestEqual(TEXT("A partial rear multiplier is honoured"),
        ELib::GetFacingArmorMultiplier(Forward, Self, FVector(-1000.0f, 0.0f, 0.0f), 0.4f, 0.0f), 0.4f);

    // The archetypes that depend on it are actually wired to use it.
    const ABreakerWardenEnemy* Warden = GetDefault<ABreakerWardenEnemy>();
    if (!TestNotNull(TEXT("The Warden has a default object"), Warden)) return false;
    TestTrue(TEXT("The Warden is frontally armoured"), Warden->FrontalArmor > 0.0f);
    TestEqual(TEXT("The Warden's rear is unarmoured"), Warden->RearArmorFraction, 0.0f);
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
    // (Story-Source §1.5). This is the mechanical claim, not a label.
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
