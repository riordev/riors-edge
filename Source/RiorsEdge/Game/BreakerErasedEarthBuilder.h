#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerErasedEarthBuilder.generated.h"

class ABreakerEnemy;

struct FBreakerSurvivorRoutePoint
{
    FVector Location = FVector::ZeroVector;
    // Reach this safe checkpoint, then wait until its pocket is cleared.
    int32 RequiredPocket = INDEX_NONE;
};

struct FBreakerErasedEarthEnemySpawn
{
    FVector Location = FVector::ZeroVector;
    TSubclassOf<ABreakerEnemy> EnemyClass;
    int32 PocketIndex = INDEX_NONE;
};

struct FBreakerErasedEarthLayout
{
    // Actor-centre positions, 90cm above the authored walking surfaces.
    FVector PlayerArrival = FVector::ZeroVector;
    FVector SurvivorShelter = FVector::ZeroVector;
    FVector Extraction = FVector::ZeroVector;
    FRotator ArrivalFacing = FRotator::ZeroRotator;
    TArray<FBreakerSurvivorRoutePoint> Route;
    TArray<FBreakerErasedEarthEnemySpawn> Enemies;
    int32 PocketCount = 3;
};

// The first erased Earth: a garden settlement, broken stone causeway and
// extraction terrace. Geometry only; GameMode owns finite enemies and proof.
UCLASS()
class RIORSEDGE_API UBreakerErasedEarthBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    static FBreakerErasedEarthLayout Build(UWorld* World, const FTransform& Origin = FTransform::Identity);
};
