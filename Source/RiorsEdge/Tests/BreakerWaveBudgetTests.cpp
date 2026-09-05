#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/BreakerWaveBudget.h"

// THE WAVE BUDGET SOLVER — Encounter-Design §4.2/§4.3/§5.3. Pure maths with no
// world, which is the whole reason it lives in its own header: what a wave IS
// has to be provable without playing to wave 12.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveBudgetCurveTest,
    "RiorsEdge.Game.Waves.Budget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveBudgetCurveTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // §4.2's table, read straight off the document: Budget(n) = 6 + 4n.
    TestEqual(TEXT("Wave 1 budget"), ELib::GetWaveBudget(1, Params), 10);
    TestEqual(TEXT("Wave 2 budget"), ELib::GetWaveBudget(2, Params), 14);
    TestEqual(TEXT("Wave 3 budget"), ELib::GetWaveBudget(3, Params), 18);
    TestEqual(TEXT("Wave 4 budget"), ELib::GetWaveBudget(4, Params), 22);
    TestEqual(TEXT("Wave 5 budget"), ELib::GetWaveBudget(5, Params), 26);
    // Wave 6 is a REST wave, so it takes half of the 30 the curve gives.
    TestEqual(TEXT("Wave 6 is half budget because it rests"), ELib::GetWaveBudget(6, Params), 15);
    TestEqual(TEXT("Wave 11 budget"), ELib::GetWaveBudget(11, Params), 50);

    // Capped at 90, reached at wave 21.
    TestEqual(TEXT("Wave 21 hits the cap"), ELib::GetWaveBudget(21, Params), 90);
    TestEqual(TEXT("Wave 40 stays at the cap"), ELib::GetWaveBudget(40, Params), 90);
    TestEqual(TEXT("Wave 0 has no budget"), ELib::GetWaveBudget(0, Params), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveCadenceTest,
    "RiorsEdge.Game.Waves.Cadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveCadenceTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    TestEqual(TEXT("Wave 1 is standard"), ELib::GetWaveKind(1, Params), EBreakerWaveKind::Standard);
    TestEqual(TEXT("Wave 6 rests"), ELib::GetWaveKind(6, Params), EBreakerWaveKind::Rest);
    TestEqual(TEXT("Wave 18 rests"), ELib::GetWaveKind(18, Params), EBreakerWaveKind::Rest);

    // 12 is a multiple of BOTH intervals, and §4.2's table makes it the boss.
    // Reversing the two checks in GetWaveKind deletes the boss wave entirely,
    // which is exactly the kind of silent loss a cadence rule invites.
    TestEqual(TEXT("Wave 12 is the boss, not a rest"), ELib::GetWaveKind(12, Params), EBreakerWaveKind::Boss);
    TestEqual(TEXT("Wave 24 is the boss again"), ELib::GetWaveKind(24, Params), EBreakerWaveKind::Boss);

    // §4.3: loot only on rest and boss waves, or the gym becomes a farm and the
    // drop-rate data it exists to gather is worthless.
    TestFalse(TEXT("A standard wave drops nothing"), ELib::SolveWave(5, 1, Params).bDropsLoot);

    // IN A RIFT, EVERY WAVE DROPS. 4.3's rest-and-boss-only rule is the GYM's
    // measurement rule — it keeps the instrument's drop-rate data clean — and
    // applying it to the player's own loop taxes the loop to protect the
    // instrument. The arithmetic is what makes this load-bearing rather than
    // tidy: a run reaches wave 3, waves 1 and 2 carry every trash body in it,
    // and wave 3 is the boss alone, so under the gym's rule the player's whole
    // loot loop was ONE BOSS DROP PER RUN with every drop-rate constant in the
    // game unreachable.
    FBreakerWaveBudgetParams RiftParams = Params;
    RiftParams.bRiftInstance = true;
    for (int32 Wave = 1; Wave <= 12; ++Wave)
    {
        TestTrue(FString::Printf(TEXT("Rift wave %d drops"), Wave),
            ELib::SolveWave(Wave, 1, RiftParams).bDropsLoot);
    }
    // And the gym is untouched by it, which is the half that keeps the
    // instrument honest.
    TestFalse(TEXT("the gym's standard wave still drops nothing"),
        ELib::SolveWave(5, 1, Params).bDropsLoot);
    TestTrue(TEXT("A rest wave drops"), ELib::SolveWave(6, 1, Params).bDropsLoot);
    TestTrue(TEXT("A boss wave drops"), ELib::SolveWave(12, 1, Params).bDropsLoot);

    // A rest wave takes no elites (§4.2's table: wave 6, "0 elites"). The beat
    // exists so the player stops READING, and a modifier set is the most
    // reading the game asks for.
    TestEqual(TEXT("A rest wave carries no elite"), ELib::SolveWave(6, 1, Params).Elites, 0);
    // Wave 18 would otherwise take one on floor(18/4).
    TestEqual(TEXT("Wave 18 rests and still carries no elite"), ELib::SolveWave(18, 1, Params).Elites, 0);

    // The boss wave is the boss alone.
    const FBreakerWaveComposition Boss = ELib::SolveWave(12, 1, Params);
    TestTrue(TEXT("Wave 12 spawns the boss"), Boss.bBoss);
    TestEqual(TEXT("Wave 12 spawns nothing else"), Boss.TotalEnemies(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveAutoAdvanceTest,
    "RiorsEdge.Game.Waves.AutoAdvance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveAutoAdvanceTest::RunTest(const FString& Parameters)
{
    // A RIFT'S WAVES FOLLOW EACH OTHER WITHOUT A KEY; THE GYM'S WAIT FOR ONE.
    // The gym is the TTK instrument and F4 is its pacing; a run is the
    // player's loop and a yard that empties and then waits for a test-bench
    // key is the "empty yard" defect one wave later.
    using ELib = UBreakerWaveBudgetLibrary;
    float Delay = -1.0f;

    // The shipped configuration: default-constructed params are the gym's,
    // and the gym never advances itself, on any kind of wave.
    const FBreakerWaveBudgetParams Gym;
    TestFalse(TEXT("the gym's params are not a rift's"), Gym.bRiftInstance);
    for (const int32 Wave : {1, 5, 6, 12})
    {
        TestFalse(FString::Printf(TEXT("gym wave %d does not advance itself"), Wave),
            ELib::GetAutoAdvanceDelay(Wave, Gym, Delay));
    }

    // The rift the door builds: wave 1 and 2 carry on after the standard
    // breather, and nothing follows the boss on 3.
    const FBreakerWaveBudgetParams Rift = ELib::MakeRiftWaveBudget(3);
    TestTrue(TEXT("the door's params are a rift's"), Rift.bRiftInstance);
    TestFalse(TEXT("nothing has cleared before wave 1"), ELib::GetAutoAdvanceDelay(0, Rift, Delay));
    TestTrue(TEXT("rift wave 1 advances on the clear"), ELib::GetAutoAdvanceDelay(1, Rift, Delay));
    TestEqual(TEXT("after a standard wave the breather is the clear breather"),
        Delay, Rift.WaveClearBreatherSeconds);
    TestTrue(TEXT("rift wave 2 advances on the clear"), ELib::GetAutoAdvanceDelay(2, Rift, Delay));
    TestFalse(TEXT("nothing follows the boss wave: the run ends through O168"),
        ELib::GetAutoAdvanceDelay(3, Rift, Delay));

    // A longer rift tier that reaches a rest wave honours the rest breather
    // there, not the clear breather.
    FBreakerWaveBudgetParams Tier = ELib::MakeRiftWaveBudget(5);
    Tier.RestWaveInterval = 2;
    TestEqual(TEXT("wave 2 of the tier rests"), ELib::GetWaveKind(2, Tier), EBreakerWaveKind::Rest);
    TestTrue(TEXT("a rest wave still advances"), ELib::GetAutoAdvanceDelay(2, Tier, Delay));
    TestEqual(TEXT("after a rest wave the breather is the rest breather"),
        Delay, Tier.RestBreatherSeconds);
    TestTrue(TEXT("the rest breather is the longer of the two"),
        Tier.RestBreatherSeconds > Tier.WaveClearBreatherSeconds);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveCompositionTest,
    "RiorsEdge.Game.Waves.Composition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveCompositionTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // §4.2's first row, exactly: "1 | 10 | 10 Skitter | 0 elites".
    const FBreakerWaveComposition One = ELib::SolveWave(1, 1, Params);
    TestEqual(TEXT("Wave 1 is ten Skitters"), One.Skitters, 10);
    TestEqual(TEXT("Wave 1 has no ranged"), One.RangedSources(), 0);
    TestEqual(TEXT("Wave 1 has no Warden"), One.Wardens, 0);
    TestEqual(TEXT("Wave 1 has no elite"), One.Elites, 0);
    TestEqual(TEXT("Wave 1 spends its whole budget"), One.UnspentBudget, 0);

    // Introduction order: Lattice from 2, Warden from 3, Skirmisher from 4.
    // Nothing appears before its wave, which is what makes each archetype a
    // teaching beat rather than noise.
    TestEqual(TEXT("No Lattice before wave 2"), ELib::SolveWave(1, 1, Params).Lattices, 0);
    TestTrue(TEXT("A Lattice from wave 2"), ELib::SolveWave(2, 1, Params).Lattices > 0);
    TestEqual(TEXT("No Warden before wave 3"), ELib::SolveWave(2, 1, Params).Wardens, 0);
    TestEqual(TEXT("A Warden from wave 3"), ELib::SolveWave(3, 1, Params).Wardens, 1);
    TestEqual(TEXT("No Skirmisher before wave 4"), ELib::SolveWave(3, 1, Params).Skirmishers, 0);
    TestTrue(TEXT("A Skirmisher from wave 4"), ELib::SolveWave(4, 1, Params).Skirmishers > 0);

    // §4.2: elite count = floor(wave/4), and §5.3 caps live elites at 1 solo.
    TestEqual(TEXT("No elite at wave 3"), ELib::SolveWave(3, 1, Params).Elites, 0);
    TestEqual(TEXT("One elite at wave 4"), ELib::SolveWave(4, 1, Params).Elites, 1);
    TestEqual(TEXT("Still one elite at wave 20, solo"), ELib::SolveWave(20, 1, Params).Elites, 1);
    TestTrue(TEXT("An elite carries at least one modifier"), ELib::SolveWave(4, 1, Params).ModifiersPerElite >= 1);
    TestTrue(TEXT("Modifiers per elite never exceed the composition ceiling of 3"),
        ELib::SolveWave(60, 1, Params).ModifiersPerElite <= 3);

    // §5.3, every cap, across the whole repeating cycle and past the budget cap.
    for (int32 Wave = 1; Wave <= 48; ++Wave)
    {
        const FBreakerWaveComposition Composition = ELib::SolveWave(Wave, 1, Params);
        FString Reason;
        if (!ELib::IsCompositionLegal(Composition, 1, Params, Reason))
        {
            AddError(FString::Printf(TEXT("Wave %d is illegal: %s (%s)"),
                Wave, *Reason, *ELib::DescribeComposition(Composition)));
        }
        // Restated directly rather than only through the legality helper, so a
        // bug in the helper cannot make the caps look enforced.
        TestTrue(TEXT("Density ceiling of 12 solo"), Composition.TotalEnemies() <= 12);
        TestTrue(TEXT("Never more than 3 ranged sources"), Composition.RangedSources() <= 3);
        TestTrue(TEXT("Never more than one Warden-class anchor"),
            Composition.Wardens + (Composition.bBoss ? 1 : 0) <= 1);
        TestTrue(TEXT("Never more than one elite solo"), Composition.Elites <= 1);
        TestTrue(TEXT("Never overspends"), Composition.SpentBudget <= Composition.Budget);
        TestTrue(TEXT("A wave is never empty"), Composition.TotalEnemies() >= 1);
    }

    // §4.3's variety rule, checked where it bites: after wave 3 no archetype
    // may take more than 70% of the budget. Skitters are the only archetype
    // that can run away with one, because they are the cheap one.
    for (int32 Wave = 4; Wave <= 24; ++Wave)
    {
        const FBreakerWaveComposition Composition = ELib::SolveWave(Wave, 1, Params);
        if (Composition.Kind != EBreakerWaveKind::Standard) continue;
        TestTrue(TEXT("Skitters stay inside the 70% budget share"),
            Composition.Skitters * Params.SkitterCost
                <= FMath::FloorToInt32(Composition.Budget * Params.MaximumSingleArchetypeShare));
    }

    // Deterministic: the same wave solves the same way every time, which is
    // what makes a TTK sample from wave 7 comparable with last session's.
    TestEqual(TEXT("Solving twice gives the same head count"),
        ELib::SolveWave(9, 1, Params).TotalEnemies(), ELib::SolveWave(9, 1, Params).TotalEnemies());

    // Party scaling goes through the same solver. §5.3: 3 elites at five
    // players, and the ranged cap does NOT scale with party size.
    TestEqual(TEXT("Five players allow three elites"), ELib::GetMaximumElites(5, Params), 3);
    TestEqual(TEXT("Solo allows one elite"), ELib::GetMaximumElites(1, Params), 1);
    TestTrue(TEXT("The ranged cap holds at five players"),
        ELib::SolveWave(20, 5, Params).RangedSources() <= 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveBudgetCollisionTest,
    "RiorsEdge.Game.Waves.CapCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveBudgetCollisionTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // A FINDING, pinned by test so it cannot quietly stop being true.
    // Encounter-Design §4.2's budget curve and §5.3's density caps DISAGREE:
    // under 12 live enemies, 3 ranged sources, 1 Warden and 1 elite, a solo
    // wave physically cannot spend much past the mid thirties, while the curve
    // keeps climbing to 90. The caps win here, because they are the ones with
    // reasons written beside them — and the solver reports the shortfall rather
    // than resolving the contradiction silently in either direction.
    TestEqual(TEXT("Early waves spend everything"), ELib::SolveWave(1, 1, Params).UnspentBudget, 0);
    TestTrue(TEXT("Late waves cannot spend their budget under the 5.3 caps"),
        ELib::SolveWave(20, 1, Params).UnspentBudget > 0);
    // Compared between two STANDARD waves. Wave 30 is a rest wave and takes
    // half budget, so it has LESS to leave unspent than wave 20 — which is the
    // rest wave working, not the shortfall shrinking, and is worth stating
    // because the first version of this assertion compared against it and
    // failed for exactly that reason.
    TestTrue(TEXT("The shortfall grows with the wave"),
        ELib::SolveWave(26, 1, Params).UnspentBudget >= ELib::SolveWave(20, 1, Params).UnspentBudget);
    TestEqual(TEXT("Wave 20 and 26 are both standard waves"),
        ELib::GetWaveKind(26, Params), EBreakerWaveKind::Standard);

    // The caps are never broken to spend it, which is the half that matters.
    const FBreakerWaveComposition Late = ELib::SolveWave(40, 1, Params);
    TestTrue(TEXT("A capped wave is still legal"), Late.TotalEnemies() <= 12);

    return true;
}

// MODIFIER CARRIERS — O27's kill-bucket producer (Playtest/BreakerKillBuckets.h).
// Non-elite bodies promoted to rank ModifierBearing and KEPT there, exactly
// like the elite promotion is folded into Skitters rather than counted as an
// extra body — the Lattice precedent: taken OUT OF the melee budget, so
// density is unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveModifierCarrierTest,
    "RiorsEdge.Game.Waves.ModifierCarriers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveModifierCarrierTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // From wave 4 — nothing before it.
    TestEqual(TEXT("No modifier carrier before wave 4"), ELib::SolveWave(3, 1, Params).ModifierCarriers, 0);

    // A carrier is a PROMOTION: it must never add to the body count on its
    // own, exactly like an elite. Swept across the whole repeating cycle.
    for (int32 Wave = 1; Wave <= 40; ++Wave)
    {
        const FBreakerWaveComposition Composition = ELib::SolveWave(Wave, 1, Params);
        TestTrue(FString::Printf(TEXT("Wave %d: carriers are folded into Skitters, not added on top"), Wave),
            Composition.Elites + Composition.ModifierCarriers <= Composition.Skitters);
        TestTrue(FString::Printf(TEXT("Wave %d: carriers never exceed the cap of 2"), Wave),
            Composition.ModifierCarriers <= 2);
        // Density is unchanged: the cap this checks is the SAME 5.3 ceiling
        // every other wave already has to respect.
        TestTrue(FString::Printf(TEXT("Wave %d stays legal with carriers included"), Wave),
            Composition.TotalEnemies() <= ELib::GetMaximumLiveEnemies(1, Params));
    }

    // Rest waves take no carriers, for the same reason they take no elites —
    // the beat exists so the player stops reading.
    TestEqual(TEXT("A rest wave carries no modifier carrier"), ELib::SolveWave(6, 1, Params).ModifierCarriers, 0);

    // At the wave everything else is already competing hard for budget
    // (Warden + Skirmisher + Lattice + the first elite), the carrier may
    // legitimately be budget-starved to zero — which is the SAME §4.2/§5.3
    // collision this file already documents for the Skitter fill, not a bug
    // in this feature. It must recover once the budget curve outpaces the
    // fixed body cap.
    TestTrue(TEXT("Modifier carriers become affordable once the curve catches up"),
        ELib::SolveWave(8, 1, Params).ModifierCarriers > 0);

    return true;
}

// O40c: REACHABILITY IS PART OF DEFINITION-OF-DONE, in the
// RiorsEdge.Movement.JumpGrantMatrix mold — read the SHIPPED default
// FBreakerWaveBudgetParams and confirm a representative wave's SOLVED
// composition actually contains every archetype the wave's own introduction
// thresholds claim to have unlocked by then, rather than asserting a fact
// about the design doc. §4.2/§5.3 already document that the budget curve and
// the density caps contradict from about wave 8 onward and the solver
// REPORTS the shortfall rather than choosing silently — this is the test
// that would have caught an eligible archetype silently landing at zero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWaveArchetypeReachabilityTest,
    "RiorsEdge.Game.Waves.ArchetypeReachability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWaveArchetypeReachabilityTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;   // the shipped defaults, not hand-picked numbers

    // Wave 8: past every archetype's introduction threshold (Lattice from 2,
    // Warden from 3, Skirmisher from 4, the first elite at wave 4, and the
    // modifier carrier from wave 4) and comfortably inside the variety rule's
    // enforcement window (from wave 4).
    const int32 RepresentativeWave = 8;
    TestTrue(TEXT("The representative wave has introduced Lattices"), RepresentativeWave >= Params.LatticeFromWave);
    TestTrue(TEXT("The representative wave has introduced Wardens"), RepresentativeWave >= Params.WardenFromWave);
    TestTrue(TEXT("The representative wave has introduced Skirmishers"), RepresentativeWave >= Params.SkirmisherFromWave);
    TestTrue(TEXT("The representative wave has introduced modifier carriers"), RepresentativeWave >= Params.ModifierCarrierFromWave);
    TestTrue(TEXT("The representative wave has introduced elites"), RepresentativeWave >= Params.WavesPerElite);

    const FBreakerWaveComposition Composition = ELib::SolveWave(RepresentativeWave, /*PartySize=*/1, Params);

    FString Reason;
    if (!ELib::IsCompositionLegal(Composition, 1, Params, Reason))
    {
        AddError(FString::Printf(TEXT("Wave %d is illegal: %s (%s)"),
            RepresentativeWave, *Reason, *ELib::DescribeComposition(Composition)));
    }

    TestTrue(TEXT("Skitters (melee trash) claimed at this wave are present"), Composition.Skitters > 0);
    TestTrue(TEXT("Lattices (ranged trash) claimed at this wave are present"), Composition.Lattices > 0);
    TestTrue(TEXT("Skirmishers (ranged trash) claimed at this wave are present"), Composition.Skirmishers > 0);
    TestTrue(TEXT("Wardens claimed at this wave are present"), Composition.Wardens > 0);
    TestTrue(TEXT("Elite promotions claimed at this wave are present"), Composition.Elites > 0);
    TestTrue(TEXT("Modifier-carrier promotions claimed at this wave are present — O27's kill-bucket producer"),
        Composition.ModifierCarriers > 0);
    // Carriers and elites are promoted Skitters, exactly like the Lattice
    // precedent: they must not inflate the body count.
    TestTrue(TEXT("Elites and carriers are folded into the Skitter count, not added on top"),
        Composition.Elites + Composition.ModifierCarriers <= Composition.Skitters);

    return true;
}

// THE PARTY DIMENSION. O82 rules party size a real axis, and until this test
// existed the solver's party arithmetic had almost no coverage: every
// production call site passes 1, and the two assertions that reached past it
// checked GetMaximumElites at exactly 5 and one wave's ranged count. The
// interpolated middle, the per-player body ceiling, the per-player Warden cap
// and IsCompositionLegal above solo had never been executed at all. A ruled
// design axis with no coverage breaks the first time somebody trusts it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWavePartyScalingTest,
    "RiorsEdge.Game.Waves.PartyScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWavePartyScalingTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const FBreakerWaveBudgetParams Params;

    // --- The elite allowance, across the whole range -----------------------
    // 5.3 gives only the two ends (solo 1, five-player 3); the middle is
    // interpolated. Pinned pointwise so a change to the interpolation or the
    // rounding is VISIBLE rather than absorbed, and asserted as a shape too
    // — O115: two bounds alone constrain nothing between them.
    const int32 ExpectedElites[6] = { 0, 1, 2, 2, 3, 3 };   // index = party size
    for (int32 Party = 1; Party <= 5; ++Party)
    {
        TestEqual(FString::Printf(TEXT("Party %d allows %d elites"), Party, ExpectedElites[Party]),
            ELib::GetMaximumElites(Party, Params), ExpectedElites[Party]);
    }
    for (int32 Party = 2; Party <= 5; ++Party)
    {
        TestTrue(FString::Printf(TEXT("The elite allowance never falls as the party grows (%d)"), Party),
            ELib::GetMaximumElites(Party, Params) >= ELib::GetMaximumElites(Party - 1, Params));
    }
    // Both clamps. A zero or negative party is a caller bug, not a wave with
    // no elites, and a six-player party is not a thing 5.3 priced.
    TestEqual(TEXT("Party 0 clamps to solo"), ELib::GetMaximumElites(0, Params), 1);
    TestEqual(TEXT("A negative party clamps to solo"), ELib::GetMaximumElites(-3, Params), 1);
    TestEqual(TEXT("Party 9 clamps to the five-player allowance"), ELib::GetMaximumElites(9, Params), 3);

    // --- The body ceiling scales per player; the ranged cap does not -------
    // This is the axis 5.3 actually draws, and it is the one worth asserting:
    // density is per-player, but converging PROJECTILE SOURCES are capped flat
    // because four of them removes all safe ground no matter how many players
    // are standing in it.
    for (int32 Party = 1; Party <= 5; ++Party)
    {
        TestEqual(FString::Printf(TEXT("Party %d carries %d live bodies"), Party, Party * 12),
            ELib::GetMaximumLiveEnemies(Party, Params), Party * 12);
    }
    TestEqual(TEXT("Party 0 clamps to one player's density"), ELib::GetMaximumLiveEnemies(0, Params), 12);

    // --- Every party size, every wave, still legal -------------------------
    // The solver's own legality helper run at each party size, which is the
    // arithmetic that had never executed above 1.
    for (int32 Party = 1; Party <= 5; ++Party)
    {
        for (int32 Wave = 1; Wave <= 30; ++Wave)
        {
            const FBreakerWaveComposition Composition = ELib::SolveWave(Wave, Party, Params);
            FString Reason;
            if (!ELib::IsCompositionLegal(Composition, Party, Params, Reason))
            {
                AddError(FString::Printf(TEXT("Party %d wave %d is illegal: %s (%s)"),
                    Party, Wave, *Reason, *ELib::DescribeComposition(Composition)));
            }
            // Restated directly rather than only through the helper, so a bug
            // in the helper cannot make the caps look enforced.
            TestTrue(FString::Printf(TEXT("Party %d wave %d respects the per-player density ceiling"), Party, Wave),
                Composition.TotalEnemies() <= Party * 12);
            TestTrue(FString::Printf(TEXT("Party %d wave %d holds the FLAT ranged cap of 3"), Party, Wave),
                Composition.RangedSources() <= 3);
            TestTrue(FString::Printf(TEXT("Party %d wave %d respects one Warden-class anchor per player"), Party, Wave),
                Composition.Wardens + (Composition.bBoss ? 1 : 0) <= Party);
            TestTrue(FString::Printf(TEXT("Party %d wave %d respects its elite allowance"), Party, Wave),
                Composition.Elites <= ELib::GetMaximumElites(Party, Params));
            TestTrue(FString::Printf(TEXT("Party %d wave %d never overspends"), Party, Wave),
                Composition.SpentBudget <= Composition.Budget);
            TestTrue(FString::Printf(TEXT("Party %d wave %d is never empty"), Party, Wave),
                Composition.TotalEnemies() >= 1);
        }
    }

    // The per-player Warden cap is REACHED, not merely respected: a cap that
    // no wave ever touches is a cap no test is actually exercising.
    TestTrue(TEXT("A five-player wave fields more than one Warden"),
        ELib::SolveWave(20, 5, Params).Wardens > 1);

    // --- A FINDING, pinned by test so it cannot quietly stop being true ----
    // THE BUDGET CURVE CARRIES NO PARTY TERM WHILE THE CAPS DO, so at a low
    // wave a LARGER party receives a SMALLER, more expensive wave. At wave 3
    // the budget is 18 either way: solo spends it on one Warden, one Lattice
    // and nine Skitters, while five players get three Wardens and nothing else
    // — the Wardens exhaust the budget at the per-player cap and there is
    // nothing left to buy a body with. Both compositions are LEGAL; 5.3's caps
    // are ceilings and the solver is right not to invent budget to fill them.
    //
    // This is the same 4.2-versus-5.3 collision FBreakerWaveBudgetCollisionTest
    // already pins, seen from the party axis instead of the wave axis, and it
    // is recorded here as MEASURED rather than as intent. O133 names the open
    // question; nothing in the shipped configuration answers it, so this test
    // asserts what the solver does today and will go red the moment a party
    // term is added — which is exactly when it should be re-read.
    const FBreakerWaveComposition SoloEarly = ELib::SolveWave(3, 1, Params);
    const FBreakerWaveComposition PartyEarly = ELib::SolveWave(3, 5, Params);
    TestEqual(TEXT("The wave budget is identical at any party size"),
        SoloEarly.Budget, PartyEarly.Budget);
    TestTrue(TEXT("FINDING: a five-player wave 3 fields FEWER bodies than a solo wave 3"),
        PartyEarly.TotalEnemies() < SoloEarly.TotalEnemies());
    TestTrue(TEXT("FINDING: and spends them all on Warden-class anchors"),
        PartyEarly.Wardens == PartyEarly.TotalEnemies() && PartyEarly.Wardens > 1);

    return true;
}

// THE RIFT'S OWN PROFILE (One-AD). The gym was built first, so its assumptions
// are the project's defaults — and its TWELVE-WAVE endurance pacing was being
// applied to a THREE-WAVE run, so a rift never reached the wave that introduces
// anything. Nine constants, not the two that happened to be noticed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRiftWaveProfileTest,
    "RiorsEdge.Game.Waves.RiftProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRiftWaveProfileTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerWaveBudgetLibrary;
    const int32 BossWave = 3;
    const FBreakerWaveBudgetParams Rift = ELib::MakeRiftWaveBudget(BossWave);

    // THE RUN IS THREE WAVES AND THE BOSS ENDS IT.
    TestEqual(TEXT("the boss arrives on the run's last wave"),
        ELib::GetWaveKind(BossWave, Rift), EBreakerWaveKind::Boss);

    // NO REST BEAT INSIDE A RUN. 4.3's breather is endurance pacing; a rest
    // wave inside three waves spends a third of the run on it.
    for (int32 Wave = 1; Wave <= BossWave; ++Wave)
    {
        TestTrue(FString::Printf(TEXT("wave %d is not a rest wave"), Wave),
            ELib::GetWaveKind(Wave, Rift) != EBreakerWaveKind::Rest);
    }

    // EVERYTHING THE RUN CONTAINS ARRIVES INSIDE THE RUN. This is the whole
    // finding: under the gym's schedule every one of these is introduced on a
    // wave a rift never reaches.
    // THE SOLVED RUN, LOGGED. The seat asked for the new composition; this is
    // where it comes from, so the report and the solver cannot drift apart.
    for (int32 Wave = 1; Wave <= BossWave; ++Wave)
    {
        AddInfo(FString::Printf(TEXT("rift %s"), *ELib::DescribeComposition(ELib::SolveWave(Wave, 1, Rift))));
    }

    const FBreakerWaveComposition Last = ELib::SolveWave(BossWave - 1, 1, Rift);
    TestTrue(TEXT("Lattices are present on the last wave before the boss"), Last.Lattices > 0);
    TestTrue(TEXT("Skirmishers are present"), Last.Skirmishers > 0);
    TestTrue(TEXT("Wardens are present"), Last.Wardens > 0);
    TestTrue(TEXT("elite promotions are present"), Last.Elites > 0);

    // AND EVERY WAVE OF A RUN DROPS, including the ones the gym would silence.
    for (int32 Wave = 1; Wave <= BossWave; ++Wave)
    {
        TestTrue(FString::Printf(TEXT("rift wave %d drops"), Wave),
            ELib::SolveWave(Wave, 1, Rift).bDropsLoot);
    }

    // LEGAL BY THE SAME RULES. A compressed schedule that broke 5.3's caps
    // would be trading a readability rule for a pacing one.
    for (int32 Wave = 1; Wave <= BossWave; ++Wave)
    {
        const FBreakerWaveComposition Composition = ELib::SolveWave(Wave, 1, Rift);
        FString Reason;
        if (!ELib::IsCompositionLegal(Composition, 1, Rift, Reason))
        {
            AddError(FString::Printf(TEXT("rift wave %d is illegal: %s (%s)"),
                Wave, *Reason, *ELib::DescribeComposition(Composition)));
        }
    }

    // THE GYM IS UNTOUCHED, which is the half that keeps the instrument honest:
    // this is a second profile, not an edit to the first.
    const FBreakerWaveBudgetParams Gym;
    TestEqual(TEXT("the gym still introduces Skirmishers on wave 4"), Gym.SkirmisherFromWave, 4);
    TestEqual(TEXT("the gym still rests every 6"), Gym.RestWaveInterval, 6);
    TestEqual(TEXT("the gym still bosses every 12"), Gym.BossWaveInterval, 12);
    TestFalse(TEXT("and the gym is not a rift"), Gym.bRiftInstance);
    return true;
}

#endif
