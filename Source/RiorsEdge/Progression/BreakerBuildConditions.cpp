#include "Progression/BreakerBuildConditions.h"

#include "Classes/BreakerMomentumComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Movement/BreakerCharacterMovementComponent.h"

FBreakerBuildConditionState FBreakerBuildConditionState::EvaluateForActor(const AActor* Actor)
{
    FBreakerBuildConditionState State;
    if (!Actor) return State;

    if (const UBreakerCharacterMovementComponent* Movement = Actor->FindComponentByClass<UBreakerCharacterMovementComponent>())
    {
        State.Set(EBreakerBuildCondition::Airborne, Movement->IsFalling());
        State.Set(EBreakerBuildCondition::Sliding, Movement->IsSliding());
        State.Set(EBreakerBuildCondition::WallRiding, Movement->IsWallRiding());

        // GetLastDashTime is world time and starts at a large negative
        // sentinel, so a character that has never dashed is never "recently
        // dashed" without a special case.
        if (const UWorld* World = Actor->GetWorld())
        {
            const float SinceDash = World->GetTimeSeconds() - Movement->GetLastDashTime();
            State.Set(EBreakerBuildCondition::RecentlyDashed, SinceDash >= 0.0f && SinceDash <= RecentDashSeconds);
        }
    }

    if (const UBreakerMomentumComponent* Momentum = Actor->FindComponentByClass<UBreakerMomentumComponent>())
    {
        // IsActiveForOwner is the loop's own "am I Swift and switched on" test;
        // without it a cached Redline state could outlive a class change.
        State.Set(EBreakerBuildCondition::Redline,
            Momentum->IsActiveForOwner() && Momentum->GetMomentumState() == EBreakerMomentumState::Redline);
    }

    return State;
}

FBreakerBuildConditionState FBreakerBuildConditionState::All()
{
    FBreakerBuildConditionState State;
    for (int32 Index = 0; Index < ConditionCount; ++Index)
    {
        State.Set(static_cast<EBreakerBuildCondition>(Index), true);
    }
    return State;
}
