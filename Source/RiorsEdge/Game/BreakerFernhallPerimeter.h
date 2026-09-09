#pragma once
#include "CoreMinimal.h"
struct FBreakerZonePiece;
class UWorld;
void BreakerBuildFernhallPerimeter(UWorld* World,const TArray<FBreakerZonePiece>& Pieces);
