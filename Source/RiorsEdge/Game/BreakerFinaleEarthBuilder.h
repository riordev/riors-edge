#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerFinaleEarthBuilder.generated.h"
class ABreakerEnemy;
struct FBreakerFinaleEnemySpawn
{
    FVector Location = FVector::ZeroVector;
    TSubclassOf<ABreakerEnemy> EnemyClass;
    int32 PocketIndex = INDEX_NONE;
};
struct FBreakerFinaleEarthLayout
{
    FVector PlayerArrival = FVector::ZeroVector;
    FVector InteractionLocation = FVector::ZeroVector;
    FRotator ArrivalFacing = FRotator::ZeroRotator;
    FRotator InteractionFacing = FRotator::ZeroRotator;
    TArray<FVector> Route;
    TArray<FBreakerFinaleEnemySpawn> Enemies;
    int32 PocketCount = 0;
};
UCLASS()
class RIORSEDGE_API UBreakerFinaleEarthBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    static FBreakerFinaleEarthLayout BuildStripped(UWorld* World, const FTransform& Origin = FTransform::Identity);
    static FBreakerFinaleEarthLayout BuildWon(UWorld* World, const FTransform& Origin = FTransform::Identity);
};
