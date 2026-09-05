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

    const FVector Goal(500.0f, 500.0f, 0.0f);
    TestTrue(TEXT("An idle move always re-plans"), ShouldReplan(Goal, Goal, true));
    TestFalse(TEXT("A goal that has not moved keeps its path"), ShouldReplan(Goal, Goal + FVector(100.0f, 0, 0), false));
    TestTrue(TEXT("A goal past ReplanDistanceCm re-plans"), ShouldReplan(Goal, Goal + FVector(200.0f, 0, 0), false));
    TestFalse(TEXT("Height alone never re-plans (the snap owns Z)"), ShouldReplan(Goal, Goal + FVector(0, 0, 900.0f), false));

    TestEqual(TEXT("Max speed is MoveSpeed x scale"), MaxSpeed(330.0f, 1.5f), 495.0f);
    TestEqual(TEXT("A negative scale clamps to a stop"), MaxSpeed(330.0f, -1.0f), 0.0f);
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
