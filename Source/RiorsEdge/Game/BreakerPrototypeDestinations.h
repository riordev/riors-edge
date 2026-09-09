#pragma once
#include "CoreMinimal.h"
class UWorld;
class ABreakerEnemy;
class ABreakerFernhallCache;
class ABreakerTravelPoint;

// Explicitly authored prototype destinations. These are fixed regional
// layouts, not a generator or an alternate campaign progression framework.
namespace BreakerPrototypeDestinations
{
    struct FDefinition
    {
        FName Id;
        FString MapName;
        FString DisplayName;
        FString Description;
        TArray<FVector> Districts;
        TArray<FString> DistrictNames;
        TArray<int32> AreaLevels;
    };
    struct FLayout
    {
        bool bComplete = false;
        FVector Arrival = FVector::ZeroVector;
        FRotator Facing = FRotator::ZeroRotator;
        TArray<ABreakerTravelPoint*> Gates;
        TArray<ABreakerFernhallCache*> Caches;
        TArray<ABreakerEnemy*> Enemies;
        TArray<FVector> WalkingRoute;
        int32 DressingCount = 0;
    };
    // O2 PLACEHOLDER: shared safe physical approach, also used before pawn BeginPlay.
    inline FVector ArrivalLocation() { return FVector(-3200,0,112); }
    const TArray<FDefinition>& All();
    const FDefinition* Find(FName Id);
    const FDefinition* ForWorld(const UObject* Context);
    bool HasMapPackage(const FDefinition& Definition);
    FLayout Build(UWorld* World, FName Id);
}
