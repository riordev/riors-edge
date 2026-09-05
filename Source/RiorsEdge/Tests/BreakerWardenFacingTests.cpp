#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerWardenEnemy.h"

// The turn-rate cap lives entirely in ABreakerEnemy::ComputeCappedFacing,
// which is pure and world-free by design — this exercises exactly that
// function, through the Warden, the archetype whose counterplay depends on it.
//
// What this file does NOT cover: that the base ABreakerEnemy::Tick's
// SetActorRotation call actually applies the capped facing to a live actor's
// rotation. That cannot be exercised without a UWorld and a running tick,
// which nothing in this suite constructs (see the file header convention in
// BreakerBossAndArchetypeTests.cpp).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerWardenFacingTurnRateTest,
    "RiorsEdge.Combat.WardenFacing.TurnRateCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerWardenFacingTurnRateTest::RunTest(const FString& Parameters)
{
    using EWarden = ABreakerWardenEnemy;

    // Facing directly away from the player (180 degrees) with a generous cap
    // and a full second: the whole turn completes and lands exactly on the
    // desired direction, not merely close to it.
    {
        const FVector Result = EWarden::ComputeCappedFacing(
            FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), 200.0f, 1.0f);
        TestTrue(TEXT("A generous budget completes a 180-degree turn exactly"),
            Result.Equals(FVector(-1.0f, 0.0f, 0.0f), 0.001f));
    }

    // A single frame at the 100 deg/s O2 seed cannot complete a 90-degree
    // turn — this is the whole point of the cap existing.
    {
        const FVector Current(1.0f, 0.0f, 0.0f);
        const FVector Desired(0.0f, 1.0f, 0.0f);
        const FVector Result = EWarden::ComputeCappedFacing(Current, Desired, 100.0f, 1.0f / 60.0f);
        const float AngleTurnedDegrees = FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(FVector::DotProduct(Current, Result.GetSafeNormal()), -1.0f, 1.0f)));
        TestTrue(TEXT("One frame at 100 deg/s turns roughly 1.67 degrees, nowhere near 90"),
            AngleTurnedDegrees < 5.0f);
        TestTrue(TEXT("The capped step still moves toward the desired direction"),
            FVector::DotProduct(Result.GetSafeNormal(), Desired) > FVector::DotProduct(Current, Desired));
    }

    // Turns the SHORT way: from facing +X toward -Y (a 90-degree turn to the
    // right) must not detour through +Y.
    {
        const FVector Result = EWarden::ComputeCappedFacing(
            FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), 30.0f, 1.0f / 60.0f);
        TestTrue(TEXT("A right turn steps toward -Y, not +Y"), Result.Y < 0.0f);
    }

    // Degenerate inputs behave rather than producing NaNs or a random spin.
    TestTrue(TEXT("A zero desired direction holds the current facing"),
        EWarden::ComputeCappedFacing(FVector(1.0f, 0.0f, 0.0f), FVector::ZeroVector, 100.0f, 1.0f)
            .Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));
    TestTrue(TEXT("A zero current facing snaps straight to the desired direction"),
        EWarden::ComputeCappedFacing(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f), 100.0f, 1.0f)
            .Equals(FVector(0.0f, 1.0f, 0.0f), 0.001f));
    TestTrue(TEXT("Already facing the target is a no-op"),
        EWarden::ComputeCappedFacing(FVector(1.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), 100.0f, 1.0f / 60.0f)
            .Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));
    TestTrue(TEXT("A negative DeltaSeconds is treated as no turn budget, not a reverse turn"),
        EWarden::ComputeCappedFacing(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), 100.0f, -1.0f)
            .Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));

    // The O2 seed's own arithmetic: at the Warden's SweepRangeCm, the 950 cm/s
    // sprint canon this codebase already cites (BreakerRangedEnemy.h:83,
    // BreakerBossAndArchetypeTests.cpp:182) gives the player strictly more
    // angular speed than the default cap, which is the entire point of the
    // seed. This does not construct a Warden (no world); it re-derives the
    // same inequality the class comment's arithmetic states.
    {
        const ABreakerWardenEnemy* WardenDefaults = GetDefault<ABreakerWardenEnemy>();
        if (TestNotNull(TEXT("The Warden has a default object"), WardenDefaults))
        {
            const float PlayerSprintCmPerSecond = 950.0f;
            const float EngagementRadiusCm = WardenDefaults->SweepRangeCm;
            const float PlayerAngularDegreesPerSecond =
                FMath::RadiansToDegrees(PlayerSprintCmPerSecond / EngagementRadiusCm);
            TestTrue(TEXT("The player can out-turn the Warden's default cap at sweep range"),
                PlayerAngularDegreesPerSecond > WardenDefaults->MaxTurnRateDegreesPerSecond);
        }
    }

    // The shipped configuration of the cap every archetype turns through: a
    // zero or non-finite cap would freeze every body's facing, or free it.
    {
        const ABreakerEnemy* EnemyDefaults = GetDefault<ABreakerEnemy>();
        if (TestNotNull(TEXT("The enemy has a default object"), EnemyDefaults))
        {
            TestTrue(TEXT("The default turn-rate cap is positive"),
                EnemyDefaults->MaxTurnRateDegreesPerSecond > 0.0f);
            TestTrue(TEXT("The default turn-rate cap is finite"),
                FMath::IsFinite(EnemyDefaults->MaxTurnRateDegreesPerSecond));

            // The patrol hold: a body steering at its target stops at one
            // capsule radius, so the mover's stopping distance from MoveSpeed
            // (v^2 / 2a at its deceleration) must fit inside that radius or the
            // body overshoots the hold and steers back through it every pass.
            const UBreakerEnemyMovementComponent* MoverDefaults = EnemyDefaults->GetEnemyMovement();
            if (TestNotNull(TEXT("The enemy default object carries its mover"), MoverDefaults))
            {
                const float MoveSpeed = EnemyDefaults->GetMoveSpeed();
                const float Deceleration = MoverDefaults->Deceleration;
                TestTrue(TEXT("The mover's default deceleration is positive"), Deceleration > 0.0f);
                if (Deceleration > 0.0f)
                {
                    const float StoppingDistanceCm = (MoveSpeed * MoveSpeed) / (2.0f * Deceleration);
                    TestTrue(TEXT("The patrol stopping distance fits inside the default capsule radius"),
                        StoppingDistanceCm < EnemyDefaults->GetBodyCapsuleRadius());
                }
            }
        }
    }

    return true;
}

#endif
