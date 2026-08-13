#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/BreakerEnemyProjectile.h"
#include "Combat/BreakerRangedBehavior.h"
#include "Combat/BreakerRangedEnemy.h"

// The ranged archetype's behaviour cannot be tested without a running game,
// but every DECISION it makes is pure maths and is tested here: the
// advance/hold/retreat band, the projectile lead solve, and the telegraph ramp.
// What no test can prove is whether the orb actually reads in flight.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRangedBandTest,
    "RiorsEdge.Combat.Ranged.EngagementBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRangedBandTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerRangedBehaviorLibrary;
    const float Min = 900.0f;
    const float Max = 1900.0f;

    // With no hysteresis the band is the plain three-way split.
    TestTrue(TEXT("Beyond the band it advances"),
        ELib::ClassifyBand(3000.0f, Min, Max, 0.0f, EBreakerRangedBand::Hold) == EBreakerRangedBand::Advance);
    TestTrue(TEXT("Inside the band it holds"),
        ELib::ClassifyBand(1400.0f, Min, Max, 0.0f, EBreakerRangedBand::Advance) == EBreakerRangedBand::Hold);
    TestTrue(TEXT("Inside the minimum it retreats"),
        ELib::ClassifyBand(300.0f, Min, Max, 0.0f, EBreakerRangedBand::Hold) == EBreakerRangedBand::Retreat);

    // Hysteresis: a holding enemy tolerates drifting just outside...
    TestTrue(TEXT("Holding tolerates a small overshoot past the max"),
        ELib::ClassifyBand(1980.0f, Min, Max, 150.0f, EBreakerRangedBand::Hold) == EBreakerRangedBand::Hold);
    TestTrue(TEXT("Holding tolerates a small undershoot past the min"),
        ELib::ClassifyBand(820.0f, Min, Max, 150.0f, EBreakerRangedBand::Hold) == EBreakerRangedBand::Hold);
    // ...but a real breach still flips it.
    TestTrue(TEXT("A real overshoot still starts an advance"),
        ELib::ClassifyBand(2200.0f, Min, Max, 150.0f, EBreakerRangedBand::Hold) == EBreakerRangedBand::Advance);

    // ...and a committed advance keeps closing until meaningfully inside,
    // which is what stops the boundary flicker.
    TestTrue(TEXT("An advancing enemy does not stop the instant it touches the edge"),
        ELib::ClassifyBand(1880.0f, Min, Max, 150.0f, EBreakerRangedBand::Advance) == EBreakerRangedBand::Advance);
    TestTrue(TEXT("An advancing enemy settles once meaningfully inside"),
        ELib::ClassifyBand(1700.0f, Min, Max, 150.0f, EBreakerRangedBand::Advance) == EBreakerRangedBand::Hold);
    TestTrue(TEXT("A retreating enemy keeps backing off past the raw minimum"),
        ELib::ClassifyBand(920.0f, Min, Max, 150.0f, EBreakerRangedBand::Retreat) == EBreakerRangedBand::Retreat);

    // A hysteresis wider than half the band must not be able to invert it.
    TestTrue(TEXT("Absurd hysteresis still classifies a far target as advance"),
        ELib::ClassifyBand(9000.0f, Min, Max, 100000.0f, EBreakerRangedBand::Hold) == EBreakerRangedBand::Advance);
    TestTrue(TEXT("Absurd hysteresis still classifies a point-blank target as retreat"),
        ELib::ClassifyBand(0.0f, Min, Max, 100000.0f, EBreakerRangedBand::Hold) == EBreakerRangedBand::Retreat);

    // A mis-authored band (min above max) is tolerated, not nonsense.
    TestTrue(TEXT("Swapped band bounds are normalised"),
        ELib::ClassifyBand(1400.0f, Max, Min, 0.0f, EBreakerRangedBand::Advance) == EBreakerRangedBand::Hold);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRangedBandConvergenceTest,
    "RiorsEdge.Combat.Ranged.BandConvergence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRangedBandConvergenceTest::RunTest(const FString& Parameters)
{
    // The two failure modes named in the request: a ranged enemy that walks
    // into melee is a slow melee enemy, and one that never moves is a turret.
    // Simulate the controller from far, near, and point-blank starts and
    // assert it settles inside the band from all three without oscillating.
    using ELib = UBreakerRangedBehaviorLibrary;
    const float Min = 900.0f;
    const float Max = 1900.0f;
    const float Hysteresis = 150.0f;
    const float Speed = 320.0f;
    const float Advance = 1.25f;
    const float Retreat = 1.35f;
    const float Step = 1.0f / 60.0f;

    const float Starts[] = { 6000.0f, 2400.0f, 1400.0f, 400.0f, 0.0f };
    for (const float Start : Starts)
    {
        float Distance = Start;
        EBreakerRangedBand Band = EBreakerRangedBand::Advance;

        // 20 simulated seconds is far more than the controller needs to cross
        // 6000cm at 400 cm/s.
        for (int32 Tick = 0; Tick < 1200; ++Tick)
        {
            Distance = ELib::StepEngagementDistance(Distance, Min, Max, Hysteresis,
                Speed, Advance, Retreat, Step, Band);
        }

        TestTrue(FString::Printf(TEXT("From %.0f it settles inside the band, never in melee"), Start),
            Distance >= Min - Hysteresis - 1.0f);
        TestTrue(FString::Printf(TEXT("From %.0f it settles inside the band, never at sniping range"), Start),
            Distance <= Max + Hysteresis + 1.0f);
        TestTrue(FString::Printf(TEXT("From %.0f it ends in Hold, not chasing or fleeing"), Start),
            Band == EBreakerRangedBand::Hold);
    }

    // And once settled it stays settled: no dithering across the boundary.
    float Settled = Max;
    EBreakerRangedBand Band = EBreakerRangedBand::Hold;
    for (int32 Tick = 0; Tick < 600; ++Tick)
    {
        Settled = ELib::StepEngagementDistance(Settled, Min, Max, Hysteresis, Speed, Advance, Retreat, Step, Band);
        TestTrue(TEXT("A settled enemy never leaves Hold on its own"), Band == EBreakerRangedBand::Hold);
    }
    TestEqual(TEXT("Holding costs no radial movement at all"), Settled, Max, 0.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRangedLeadTest,
    "RiorsEdge.Combat.Ranged.ProjectileLead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRangedLeadTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerRangedBehaviorLibrary;
    const FVector Shooter(0.0f, 0.0f, 0.0f);
    const FVector Target(1500.0f, 0.0f, 0.0f);
    const float Speed = 1100.0f;

    // A stationary target: the intercept is just the flight time and the aim
    // point is the target itself.
    float Time = 0.0f;
    TestTrue(TEXT("A stationary target always solves"),
        ELib::SolveInterceptTime(Shooter, Target, FVector::ZeroVector, Speed, Time));
    TestEqual(TEXT("Flight time is distance over speed"), Time, 1500.0f / Speed, 0.001f);
    TestTrue(TEXT("A stationary target needs no lead"),
        ELib::ComputeAimPoint(Shooter, Target, FVector::ZeroVector, Speed, 1.0f).Equals(Target, 0.01f));

    // A crossing target: the solved intercept must actually be an intercept,
    // i.e. the projectile and the target arrive at the same point.
    const FVector Crossing(0.0f, 800.0f, 0.0f);
    TestTrue(TEXT("A crossing target solves"),
        ELib::SolveInterceptTime(Shooter, Target, Crossing, Speed, Time));
    const FVector MeetPoint = Target + Crossing * Time;
    TestEqual(TEXT("The shot and the target reach the meeting point together"),
        static_cast<float>(FVector::Dist(Shooter, MeetPoint)), Speed * Time, 0.1f);

    // Full lead lands on the intercept; zero lead lands on the target now.
    TestTrue(TEXT("Full lead aims at the intercept"),
        ELib::ComputeAimPoint(Shooter, Target, Crossing, Speed, 1.0f).Equals(MeetPoint, 0.1f));
    TestTrue(TEXT("Zero lead aims at the target's current position"),
        ELib::ComputeAimPoint(Shooter, Target, Crossing, Speed, 0.0f).Equals(Target, 0.01f));

    // The shipped partial lead sits strictly between the two, which is the
    // whole design: a straight sprint gets punished, a direction change wins.
    const FVector Partial = ELib::ComputeAimPoint(Shooter, Target, Crossing, Speed, 0.35f);
    TestTrue(TEXT("Partial lead leads, but not fully"),
        Partial.Y > Target.Y + 1.0f && Partial.Y < MeetPoint.Y - 1.0f);

    // Unsolvable: a target running directly away faster than the projectile.
    // The aim must fall back to the current position rather than fly wild.
    const FVector Fleeing(2000.0f, 0.0f, 0.0f);
    TestFalse(TEXT("An unoutrunnable target has no intercept"),
        ELib::SolveInterceptTime(Shooter, Target, Fleeing, 900.0f, Time));
    TestTrue(TEXT("An unsolvable lead falls back to the current position"),
        ELib::ComputeAimPoint(Shooter, Target, Fleeing, 900.0f, 1.0f).Equals(Target, 0.01f));

    // Degenerate speed must not divide by zero or produce a NaN aim point.
    TestFalse(TEXT("A zero-speed projectile has no intercept"),
        ELib::SolveInterceptTime(Shooter, Target, Crossing, 0.0f, Time));
    TestTrue(TEXT("A zero-speed aim point is finite"),
        !ELib::ComputeAimPoint(Shooter, Target, Crossing, 0.0f, 1.0f).ContainsNaN());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRangedTelegraphTest,
    "RiorsEdge.Combat.Ranged.TelegraphRamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRangedTelegraphTest::RunTest(const FString& Parameters)
{
    using ELib = UBreakerRangedBehaviorLibrary;
    TestEqual(TEXT("The tell starts dark"), ELib::GetTelegraphAlpha(0.0f, 0.85f), 0.0f);
    TestEqual(TEXT("The tell is half-lit halfway"), ELib::GetTelegraphAlpha(0.425f, 0.85f), 0.5f, 0.0001f);
    TestEqual(TEXT("The tell is full at the shot"), ELib::GetTelegraphAlpha(0.85f, 0.85f), 1.0f, 0.0001f);
    TestEqual(TEXT("The tell clamps past the shot"), ELib::GetTelegraphAlpha(10.0f, 0.85f), 1.0f);
    TestEqual(TEXT("A negative elapsed clamps to dark"), ELib::GetTelegraphAlpha(-1.0f, 0.85f), 0.0f);
    TestEqual(TEXT("A zero wind-up is instantly full rather than a divide by zero"),
        ELib::GetTelegraphAlpha(0.0f, 0.0f), 1.0f);

    // Monotonic: the tell must only ever brighten, never flicker.
    float Previous = -1.0f;
    for (int32 Step = 0; Step <= 20; ++Step)
    {
        const float Alpha = ELib::GetTelegraphAlpha(Step * 0.05f, 0.85f);
        TestTrue(TEXT("The tell never dims mid-wind-up"), Alpha >= Previous);
        Previous = Alpha;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRangedFairnessTest,
    "RiorsEdge.Combat.Ranged.ShipsDodgeable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRangedFairnessTest::RunTest(const FString& Parameters)
{
    // A guard on the shipped defaults, not on the maths. The owner asked for a
    // projectile they can SEE; these are the numbers that claim makes, so a
    // future retune that quietly breaks the claim fails here instead of in a
    // playtest. All values are O2 PLACEHOLDER and may legitimately change —
    // if they do, change this test deliberately and say so.
    const ABreakerRangedEnemy* Defaults = GetDefault<ABreakerRangedEnemy>();
    if (!TestNotNull(TEXT("The ranged archetype has a default object"), Defaults)) return false;

    TestTrue(TEXT("The engagement band is a band, not a point"),
        Defaults->MaxEngagementDistance > Defaults->MinEngagementDistance);
    TestTrue(TEXT("Hysteresis cannot swallow the band"),
        Defaults->BandHysteresis < (Defaults->MaxEngagementDistance - Defaults->MinEngagementDistance) * 0.5f);
    TestTrue(TEXT("It backs off faster than it closes, so crowding it works"),
        Defaults->RetreatSpeedMultiplier > Defaults->AdvanceSpeedMultiplier);
    TestTrue(TEXT("Holding station still moves — it is not a turret"),
        Defaults->StrafeSpeedMultiplier > 0.0f);

    // Flight time at the closest legal shot is the tightest reaction budget
    // the archetype can present.
    const float NearFlight = Defaults->MinEngagementDistance / Defaults->ProjectileSpeed;
    const float FarFlight = Defaults->MaxEngagementDistance / Defaults->ProjectileSpeed;
    TestTrue(TEXT("Even the closest shot is visible in flight for over half a second"), NearFlight > 0.5f);
    TestTrue(TEXT("A shot across the band takes over a second"), FarFlight > 1.0f);

    // Telegraph plus flight is the total warning. O1 makes movement the only
    // defensive input, so the answer has to be positional and therefore slow.
    TestTrue(TEXT("Telegraph plus flight gives over a second of warning"),
        Defaults->WindupSeconds + NearFlight > 1.0f);
    TestTrue(TEXT("The wind-up is a spatial tell, not a reaction frame"),
        Defaults->WindupSeconds >= 0.5f);
    TestTrue(TEXT("The wind-up visibly slows it down"),
        Defaults->WindupMoveScale < 1.0f);
    TestTrue(TEXT("Wind-up plus cooldown leaves real gaps between shots"),
        Defaults->ShotCooldown >= Defaults->WindupSeconds);

    // The lead choice is deliberate and partial: not 0 (never hits a moving
    // player, reads as harmless) and not 1 (only a direction change beats it).
    TestTrue(TEXT("The lead is partial by design"),
        Defaults->LeadFraction > 0.0f && Defaults->LeadFraction < 1.0f);

    // And the projectile it fires is a real actor, large and unlit-bright.
    const ABreakerEnemyProjectile* Orb = GetDefault<ABreakerEnemyProjectile>();
    if (!TestNotNull(TEXT("The projectile class has a default object"), Orb)) return false;
    TestTrue(TEXT("The projectile lives long enough to cross the band"),
        Orb->MaximumLifetime * Defaults->ProjectileSpeed > Defaults->MaxEngagementDistance);
    TestTrue(TEXT("The projectile carries its own light so it reads against bright ground"),
        Orb->GlowIntensity > 0.0f && Orb->GlowRadius > 0.0f);
    return true;
}

#endif
