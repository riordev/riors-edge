#pragma once
#include "CoreMinimal.h"
struct FBreakerZonePiece;
class UWorld;
namespace BreakerFernhallCourtyard
{
struct FPlan
{
    FString ReplacedBoundaryPiece;
    FVector Origin, Forward, Right;
    float GroundZ = 0;
    float EntranceFloorStart = 0;
    TArray<FBox> GroundFootprints;
    TArray<FVector> RoutePoints;
    TArray<FVector> MeleeSpawns;
    FVector RangedSpawn;
    TArray<FVector> CoverCenters;
    FVector At(float Outward, float Along, float Height = 0) const
    { return Origin + Forward * Outward + Right * Along + FVector(0,0,Height); }
};
bool MakePlan(const TArray<FBreakerZonePiece>& Pieces, FPlan& Out, FString& Error);
bool Build(UWorld* World, const FPlan& Plan);
}
