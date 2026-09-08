#pragma once
#include "CoreMinimal.h"
class UWorld;
class ABreakerEnemy;
namespace BreakerFernhallCourtyard { struct FPlan; }
TArray<ABreakerEnemy*> BreakerSpawnFernhallCourtyardEncounter(UWorld* World, const BreakerFernhallCourtyard::FPlan& Plan);
