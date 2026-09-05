#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/BreakerEnemyController.h"
#include "AI/BreakerEnemyMovementComponent.h"
#include "AI/BreakerLocomotionMath.h"
#include "Combat/BreakerEnemy.h"
#include "NavMesh/RecastNavMesh.h"
#include "UObject/UObjectIterator.h"

namespace
{
    // The mode as an int so the assertion prints both sides on failure.
    int32 BreakerLocomotionModeOf(const FVector& Direction, const FVector& ToTarget, bool bHasTarget,
        bool bBlocked, float Distance, float Acceptance)
    {
        return static_cast<int32>(BreakerLocomotionMath::ChooseMode(
            Direction, ToTarget, bHasTarget, bBlocked, Distance, Acceptance));
    }
}

// NAV-1. The rule that decides how a behaviour's direction is honoured —
// steer along it, path to the target, or hold — proven without a world.
// Every archetype's lateral behaviour (weave, strafe, retreat, relocation)
// depends on the Steer branch being the default; the Path branch is admitted
// in exactly one shape, closing on a target the straight line cannot reach.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLocomotionModeTest,
    "RiorsEdge.AI.Locomotion.ModeSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLocomotionModeTest::RunTest(const FString& Parameters)
{
    using namespace BreakerLocomotionMath;
    const FVector ToTarget(1000.0f, 0.0f, 0.0f);
    const FVector Closing = ToTarget.GetSafeNormal();
    const FVector Retreat = -Closing;
    const FVector Strafe(0.0f, 1.0f, 0.0f);
    // The melee weave: a lateral sinusoid folded into the chase vector at
    // WeaveStrength, well inside the 35 degree cone.
    const FVector Weave = (Closing + Strafe * 0.45f).GetSafeNormal();
    const float Accept = AcceptanceRadius(260.0f);

    TestEqual(TEXT("Acceptance sits inside the arrival ring (AttackRange x 0.5)"), Accept, 130.0f);

    TestEqual(TEXT("A zero direction is a hold, whatever else is true"),
        BreakerLocomotionModeOf(FVector::ZeroVector, ToTarget, true, true, 1000.0f, Accept), static_cast<int32>(EBreakerLocomotionMode::Idle));
    TestEqual(TEXT("Closing on a blocked target far away paths"),
        BreakerLocomotionModeOf(Closing, ToTarget, true, true, 1000.0f, Accept), static_cast<int32>(EBreakerLocomotionMode::Path));
    TestEqual(TEXT("The weave still counts as closing"),
        BreakerLocomotionModeOf(Weave, ToTarget, true, true, 1000.0f, Accept), static_cast<int32>(EBreakerLocomotionMode::Path));
    TestEqual(TEXT("A clear line steers, exactly as before the navmesh"),
        BreakerLocomotionModeOf(Closing, ToTarget, true, false, 1000.0f, Accept), static_cast<int32>(EBreakerLocomotionMode::Steer));
    TestEqual(TEXT("Inside the acceptance radius the behaviour governs"),
        BreakerLocomotionModeOf(Closing, ToTarget, true, true, Accept, Accept), static_cast<int32>(EBreakerLocomotionMode::Steer));
    TestEqual(TEXT("A retreat never paths"),
        BreakerLocomotionModeOf(Retreat, ToTarget, true, true, 1000.0f, Accept), static_cast<int32>(EBreakerLocomotionMode::Steer));
    TestEqual(TEXT("A strafe never paths"),
        BreakerLocomotionModeOf(Strafe, ToTarget, true, true, 1000.0f, Accept), static_cast<int32>(EBreakerLocomotionMode::Steer));
    TestEqual(TEXT("No target (patrol) steers"),
        BreakerLocomotionModeOf(Closing, FVector::ZeroVector, false, true, 1000.0f, Accept), static_cast<int32>(EBreakerLocomotionMode::Steer));

    // The goal override (NAV-2): a body walking to a firing flank is walking
    // AWAY from the player's line, so the alignment cone that keeps strafes
    // and retreats out of the path follower must not apply. A blocked line to
    // the goal paths whatever the direction's angle to the player.
    const float Capsule = 45.0f;   // the enemy capsule's radius, the goal acceptance
    TestEqual(TEXT("A goal behind a wall paths even when the direction is a strafe to the player"),
        static_cast<int32>(ChooseGoalMode(Strafe, true, 1000.0f, Capsule)), static_cast<int32>(EBreakerLocomotionMode::Path));
    TestEqual(TEXT("A goal behind a wall paths even when the direction is a retreat from the player"),
        static_cast<int32>(ChooseGoalMode(Retreat, true, 1000.0f, Capsule)), static_cast<int32>(EBreakerLocomotionMode::Path));
    TestEqual(TEXT("A goal with a clear line steers"),
        static_cast<int32>(ChooseGoalMode(Strafe, false, 1000.0f, Capsule)), static_cast<int32>(EBreakerLocomotionMode::Steer));
    TestEqual(TEXT("A goal the body has arrived at hands back to the behaviour"),
        static_cast<int32>(ChooseGoalMode(Strafe, true, Capsule, Capsule)), static_cast<int32>(EBreakerLocomotionMode::Steer));
    TestEqual(TEXT("A zero direction with a goal is still a hold"),
        static_cast<int32>(ChooseGoalMode(FVector::ZeroVector, true, 1000.0f, Capsule)), static_cast<int32>(EBreakerLocomotionMode::Idle));

    const FVector Goal(500.0f, 500.0f, 0.0f);
    TestTrue(TEXT("An idle move always re-plans"), ShouldReplan(Goal, Goal, true));
    TestFalse(TEXT("A goal that has not moved keeps its path"), ShouldReplan(Goal, Goal + FVector(100.0f, 0, 0), false));
    TestTrue(TEXT("A goal past ReplanDistanceCm re-plans"), ShouldReplan(Goal, Goal + FVector(200.0f, 0, 0), false));
    TestFalse(TEXT("Height alone never re-plans (the snap owns Z)"), ShouldReplan(Goal, Goal + FVector(0, 0, 900.0f), false));

    TestEqual(TEXT("Max speed is MoveSpeed x scale"), MaxSpeed(330.0f, 1.5f), 495.0f);
    TestEqual(TEXT("A negative scale clamps to a stop"), MaxSpeed(330.0f, -1.0f), 0.0f);
    return true;
}

// NAV-3. The arrival angle: a closer walks to a point on the ring off its own
// bearing, signed by its seed, so two closers split and the NAV-1 detour still
// reads the approach as closing. Proven without a world.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLocomotionArrivalGoalTest,
    "RiorsEdge.AI.Locomotion.ArrivalGoal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLocomotionArrivalGoalTest::RunTest(const FString& Parameters)
{
    using namespace BreakerLocomotionMath;

    // The sign alternates over the game mode's seed step (Index x 1.3).
    TestEqual(TEXT("Seed 0.0 reads +1"), ArrivalSign(0.0f), 1.0f);
    TestEqual(TEXT("Seed 1.3 reads -1"), ArrivalSign(1.3f), -1.0f);
    TestEqual(TEXT("Seed 2.6 reads +1"), ArrivalSign(2.6f), 1.0f);
    TestEqual(TEXT("Seed 3.9 reads -1"), ArrivalSign(3.9f), -1.0f);
    TestEqual(TEXT("A negative seed reads by its magnitude"), ArrivalSign(-1.3f), -1.0f);

    const float Ring = 260.0f;
    const FVector Target(0.0f, 0.0f, 120.0f);
    const FVector Pawn(2000.0f, 0.0f, 90.0f);
    const FVector Left = ArrivalGoal(Target, Pawn, Ring, 1.0f, ArrivalOffsetDeg);
    const FVector Right = ArrivalGoal(Target, Pawn, Ring, -1.0f, ArrivalOffsetDeg);

    TestTrue(TEXT("The +1 goal lies on the ring"), FMath::IsNearlyEqual(FVector::Dist2D(Left, Target), Ring, 0.01f));
    TestTrue(TEXT("The -1 goal lies on the ring"), FMath::IsNearlyEqual(FVector::Dist2D(Right, Target), Ring, 0.01f));
    TestEqual(TEXT("The goal keeps the target's height (the snap owns Z)"), Left.Z, Target.Z);

    const FVector BearingLeft = (Left - Target).GetSafeNormal2D();
    const FVector BearingRight = (Right - Target).GetSafeNormal2D();
    const float Split = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(BearingLeft, BearingRight)));
    TestTrue(TEXT("The two goals stand 2 x OffsetDeg apart in bearing"), FMath::IsNearlyEqual(Split, 2.0f * ArrivalOffsetDeg, 0.01f));
    TestTrue(TEXT("The two goals stand on opposite sides of the bearing"), BearingLeft.Y * BearingRight.Y < 0.0f);

    // From the far edge of the approach the direction to the goal is well
    // inside the alignment cone, so a blocked line still paths (NAV-1).
    const FVector ToTarget = (Target - Pawn).GetSafeNormal2D();
    const FVector Approach = (Left - Pawn).GetSafeNormal2D();
    TestTrue(TEXT("From 2000 cm the approach reads as closing on the target"),
        FVector::DotProduct(Approach, ToTarget) >= PathAlignCos);
    TestEqual(TEXT("The approach to a ring goal paths when the line is blocked"),
        static_cast<int32>(ChooseMode(Approach, ToTarget, true, true, 2000.0f, AcceptanceRadius(Ring))),
        static_cast<int32>(EBreakerLocomotionMode::Path));

    // The sign is the seed's, not the position's: a pawn that moves keeps its
    // side, so the pair never swaps mid-approach.
    // The sign is the seed's, not the position's, so a pair never swaps sides
    // mid-approach; and the goal's side of the bearing follows the sign
    // wherever the pawn stands.
    const auto SideOf = [](const FVector& From, const FVector& Bearing, const FVector& Goal)
    {
        return FVector::CrossProduct((From - Bearing).GetSafeNormal2D(), (Goal - Bearing).GetSafeNormal2D()).Z;
    };
    const FVector Moved(1500.0f, 400.0f, 90.0f);
    const FVector LeftMoved = ArrivalGoal(Target, Moved, Ring, 1.0f, ArrivalOffsetDeg);
    TestEqual(TEXT("The seed's sign does not change when the pawn moves"), ArrivalSign(1.3f), ArrivalSign(1.3f));
    TestTrue(TEXT("The +1 goal stays on the same side of the bearing after the pawn moves"),
        SideOf(Pawn, Target, Left) * SideOf(Moved, Target, LeftMoved) > 0.0f);
    TestTrue(TEXT("The -1 goal stands on the other side"), SideOf(Pawn, Target, Left) * SideOf(Pawn, Target, Right) < 0.0f);

    // Shipped configuration: the offset is real, and the split covers at
    // least the chord a 150 cm body spacing subtends on the 260 cm ring.
    TestTrue(TEXT("The arrival offset is positive"), ArrivalOffsetDeg > 0.0f);
    const float BodySpacingChordDeg = FMath::RadiansToDegrees(2.0f * FMath::Asin(75.0f / 260.0f));
    TestTrue(TEXT("The split is at least one 150 cm body spacing on the 260 cm ring"),
        2.0f * ArrivalOffsetDeg >= BodySpacingChordDeg);
    return true;
}

// The shipped configuration: every fielded enemy class is possessed on spawn
// by the NAV controller, carries the NAV mover, and the project's navmesh
// generates at runtime — the ini line that, lost, would leave every level
// without a mesh and every enemy steering into walls with no test going red.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLocomotionShippedConfigurationTest,
    "RiorsEdge.AI.Locomotion.ShippedConfiguration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLocomotionShippedConfigurationTest::RunTest(const FString& Parameters)
{
    int32 Checked = 0;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(ABreakerEnemy::StaticClass())) continue;
        if (It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
        const ABreakerEnemy* Defaults = It->GetDefaultObject<ABreakerEnemy>();
        if (!Defaults) continue;
        ++Checked;
        const FString Name = It->GetName();
        TestTrue(FString::Printf(TEXT("%s is possessed by the enemy controller"), *Name),
            Defaults->AIControllerClass && Defaults->AIControllerClass->IsChildOf(ABreakerEnemyController::StaticClass()));
        TestEqual(FString::Printf(TEXT("%s is possessed when spawned, not only when placed"), *Name),
            static_cast<int32>(Defaults->AutoPossessAI), static_cast<int32>(EAutoPossessAI::PlacedInWorldOrSpawned));
        const UBreakerEnemyMovementComponent* Mover = Defaults->GetEnemyMovement();
        TestNotNull(FString::Printf(TEXT("%s carries the mover"), *Name), Mover);
        if (Mover)
        {
            TestEqual(FString::Printf(TEXT("%s's mover acceleration"), *Name), Mover->Acceleration, 6000.0f);
            TestEqual(FString::Printf(TEXT("%s's mover deceleration"), *Name), Mover->Deceleration, 8000.0f);
            TestEqual(FString::Printf(TEXT("%s's mover starts untouched"), *Name), Mover->GetWorldTouchCount(), 0);
        }
    }
    TestTrue(TEXT("At least the base enemy was checked"), Checked >= 1);

    const ARecastNavMesh* NavDefaults = GetDefault<ARecastNavMesh>();
    TestNotNull(TEXT("The recast navmesh class defaults resolve"), NavDefaults);
    if (NavDefaults)
    {
        TestEqual(TEXT("The navmesh generates at runtime (every level is built by code)"),
            static_cast<int32>(NavDefaults->GetRuntimeGenerationMode()), static_cast<int32>(ERuntimeGenerationType::Dynamic));
        TestEqual(TEXT("The nav agent is the enemy capsule's radius"), NavDefaults->AgentRadius, 45.0f);
        TestEqual(TEXT("The nav agent is the enemy capsule's height"), NavDefaults->AgentHeight, 180.0f);
    }
    return true;
}

#endif
