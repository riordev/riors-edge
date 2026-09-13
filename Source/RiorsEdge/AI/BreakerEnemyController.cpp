#include "AI/BreakerEnemyController.h"
#include "AI/BreakerLocomotionMath.h"
#include "AI/BreakerNavBounds.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"

namespace
{
    constexpr double BreakerEnemyPathRetrySeconds = 0.75; // O2 PLACEHOLDER
}

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
    NextPathAttemptTime = 0.0;
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
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextPathAttemptTime) return !bIdle;
    NextPathAttemptTime = Now + BreakerEnemyPathRetrySeconds;
    const EPathFollowingRequestResult::Type Result = MoveToLocation(Goal, AcceptanceRadius,
        /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true,
        /*bCanStrafe*/ true, /*FilterClass*/ nullptr, /*bAllowPartialPath*/ false);
    if (Result != EPathFollowingRequestResult::RequestSuccessful)
    {
        bHasGoal = false;
        return false;
    }
    LastGoal = Goal;
    bHasGoal = true;
    return true;
}

void ABreakerEnemyController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);
    if (GetWorld()) NextPathAttemptTime = GetWorld()->GetTimeSeconds() + BreakerEnemyPathRetrySeconds;
}

void ABreakerEnemyController::StopChase()
{
    if (IsChasing()) StopMovement();
    bHasGoal = false;
}
