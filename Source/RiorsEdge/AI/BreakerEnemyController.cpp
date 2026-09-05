#include "AI/BreakerEnemyController.h"
#include "AI/BreakerLocomotionMath.h"
#include "AI/BreakerNavBounds.h"
#include "Navigation/PathFollowingComponent.h"

ABreakerEnemyController::ABreakerEnemyController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // The pawn faces where the behaviour says (Tick's SetActorRotation); the
    // controller never turns it.
    bSetControlRotationFromPawnOrientation = false;
    bWantsPlayerState = false;
}

void ABreakerEnemyController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    bHasGoal = false;
    BreakerNavBounds::EnsureCoverage(GetWorld());
}

bool ABreakerEnemyController::IsChasing() const
{
    const UPathFollowingComponent* Following = GetPathFollowingComponent();
    return Following && Following->GetStatus() != EPathFollowingStatus::Idle;
}

bool ABreakerEnemyController::Chase(const FVector& Goal, float AcceptanceRadius)
{
    if (!GetPawn()) return false;
    const bool bIdle = !IsChasing();
    if (bHasGoal && !BreakerLocomotionMath::ShouldReplan(LastGoal, Goal, bIdle))
    {
        return true;
    }
    const EPathFollowingRequestResult::Type Result = MoveToLocation(Goal, AcceptanceRadius,
        /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true,
        /*bCanStrafe*/ true, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ true);
    if (Result == EPathFollowingRequestResult::Failed)
    {
        bHasGoal = false;
        return false;
    }
    LastGoal = Goal;
    bHasGoal = true;
    // AlreadyAtGoal is a success that moves nothing; the mover's acceptance
    // radius sits inside the arrival ring, so the behaviour takes over here.
    return true;
}

void ABreakerEnemyController::StopChase()
{
    if (IsChasing()) StopMovement();
    bHasGoal = false;
}
