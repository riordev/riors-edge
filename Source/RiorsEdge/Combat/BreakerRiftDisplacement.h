#pragma once
#include "CoreMinimal.h"
class AActor;

namespace BreakerRiftDisplacement
{
    // Returns actual horizontal travel. This helper owns geometry only;
    // immunity, damage, and interruption remain the caller's responsibility.
    RIORSEDGE_API float Apply(AActor* Target, const FVector& SourceLocation, float DistanceCm);
}
