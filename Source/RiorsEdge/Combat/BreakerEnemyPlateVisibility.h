#pragma once
#include "CoreMinimal.h"
#include "Engine/World.h"
// Rank-independent plate eligibility. Dynamic bodies do not count as cover.
namespace BreakerEnemyPlateVisibility
{
    inline bool IsVisible(UWorld* World, const FVector& Eye, const FVector& Head, const AActor* Enemy, const AActor* Viewer)
    {
        if (!World) return false;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(BreakerEnemyBarSight), false);
        Query.AddIgnoredActor(Enemy); Query.AddIgnoredActor(Viewer);
        return !World->LineTraceTestByObjectType(Eye, Head, FCollisionObjectQueryParams(ECC_WorldStatic), Query);
    }
}
