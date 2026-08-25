#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/BreakerHealthBands.h"
#include "Combat/BreakerBossPhases.h"

// ---------------------------------------------------------------------------
// The health-band arithmetic, proven where it lives. The band index is about
// to carry a build condition (TargetBandBroken) and the segmented bar, so the
// two facts a consumer leans on hardest are pinned here: which side of a
// boundary a value lands on, and that EVERY rank — Trash included — has bands
// enough for a break to exist at all.
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHealthBandIndexTest,
    "RiorsEdge.Attributes.HealthBands.IndexOf",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHealthBandIndexTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHealthBands;

    // The anchor pool: the shipped trash chassis at area level 1.
    constexpr float Max = 220.0f;

    // Full health is the top band; zero and below are the bottom band.
    TestEqual(TEXT("full health sits in the top band"), IndexOf(Max, Max, 4), 3);
    TestEqual(TEXT("zero health sits in the bottom band"), IndexOf(0.0f, Max, 4), 0);
    TestEqual(TEXT("negative health clamps to the bottom band"), IndexOf(-50.0f, Max, 4), 0);
    // Overheal cannot invent a band above the top one.
    TestEqual(TEXT("overheal clamps to the top band"), IndexOf(Max * 2.0f, Max, 4), 3);

    // A value exactly on a boundary belongs to the band BELOW: the segment
    // above it is fully drained. This is the definition "removed a band"
    // hangs on, so it is pinned as its own fact rather than implied.
    TestEqual(TEXT("an exact boundary belongs to the band below"), IndexOf(Max * 0.75f, Max, 4), 2);
    TestEqual(TEXT("just above a boundary is still the band above"), IndexOf(Max * 0.75f + 1.0f, Max, 4), 3);
    TestEqual(TEXT("just below a boundary is the band below"), IndexOf(Max * 0.75f - 1.0f, Max, 4), 2);

    // Degenerate inputs answer 0, never a crossing: two calls on garbage
    // compare equal instead of fabricating a band break.
    TestEqual(TEXT("no pool answers band 0"), IndexOf(100.0f, 0.0f, 4), 0);
    TestEqual(TEXT("a negative pool answers band 0"), IndexOf(100.0f, -220.0f, 4), 0);
    TestEqual(TEXT("no segments answers band 0"), IndexOf(100.0f, Max, 0), 0);

    // Monotone: as health falls the index never rises. Swept in whole-point
    // steps across the anchor pool so a step can never skip a boundary's
    // neighbourhood.
    int32 Previous = IndexOf(Max, Max, 4);
    int32 Crossings = 0;
    for (float Health = Max; Health >= 0.0f; Health -= 1.0f)
    {
        const int32 Index = IndexOf(Health, Max, 4);
        TestTrue(TEXT("the band index never rises as health falls"), Index <= Previous);
        if (Index < Previous) ++Crossings;
        Previous = Index;
    }
    // Full to zero crosses every interior boundary exactly once.
    TestEqual(TEXT("a full-to-zero sweep breaks SegmentCount-1 bands"), Crossings, 3);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerHealthBandSegmentCountTest,
    "RiorsEdge.Attributes.HealthBands.SegmentCountPerRank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerHealthBandSegmentCountTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHealthBands;

    // EVERY rank has at least two bands — the design assertion, not a bounds
    // check. At SegmentCountFor(Trash) == 1 the index could never cross on a
    // trash mob, TargetBandBroken would never fire there, and every band-gated
    // line would silently become an Elite-and-above line: the predicate
    // Core.Ruin.Siege already occupies. Bands are state on every enemy; the
    // bar draws what it can. Turning Trash bandless is a design edit that must
    // arrive through this pin, deliberately, not through a return value nobody
    // argued about.
    const EBreakerMonsterRank AllRanks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };
    for (const EBreakerMonsterRank Rank : AllRanks)
    {
        TestTrue(*FString::Printf(TEXT("rank %d has bands enough for a break (%d)"),
            static_cast<int32>(Rank), SegmentCountFor(Rank)), SegmentCountFor(Rank) >= 2);
    }

    // The boss's longer fight carries at least as many band beats as trash.
    TestTrue(TEXT("the boss carries at least trash's band count"),
        SegmentCountFor(EBreakerMonsterRank::Boss) >= SegmentCountFor(EBreakerMonsterRank::Trash));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBossBandsAvoidPhaseGatesTest,
    "RiorsEdge.Attributes.HealthBands.BossBandsAvoidPhaseGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBossBandsAvoidPhaseGatesTest::RunTest(const FString& Parameters)
{
    using namespace BreakerHealthBands;

    // O135: bands are damage feedback, phase gates are behaviour thresholds,
    // and the boss bar draws both — so no band boundary may sit NEAR a phase
    // gate, where "near" would render as two almost-coincident marks and read
    // as a defect. Exact coincidence would be a different, deliberate design
    // (gates at exact thirds, bands a multiple of three, ruled jointly);
    // near-coincidence is only ever an accident, and this pin is what keeps a
    // future retune of either number from arriving at one unargued.
    //
    // Asserted against the DEFAULT-CONSTRUCTED params — the shipped
    // configuration, which is the one the fielded Field Marshal runs.
    const FBreakerBossPhaseParams Params;
    const int32 Segments = SegmentCountFor(EBreakerMonsterRank::Boss);
    // 3% of the bar: ~12px of separation on a 400px boss bar. O2 PLACEHOLDER.
    constexpr float MinimumGateClearance = 0.03f;

    for (const float Gate : { Params.SuppressionGate, Params.CommitmentGate })
    {
        for (int32 Boundary = 1; Boundary < Segments; ++Boundary)
        {
            const float BandFraction = static_cast<float>(Boundary) / static_cast<float>(Segments);
            TestTrue(*FString::Printf(
                TEXT("boss band boundary %.4f keeps %.0f%% clearance from phase gate %.2f"),
                BandFraction, MinimumGateClearance * 100.0f, Gate),
                FMath::Abs(BandFraction - Gate) >= MinimumGateClearance);
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
