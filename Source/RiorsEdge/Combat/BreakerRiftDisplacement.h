#pragma once
#include "CoreMinimal.h"
class AActor;

namespace BreakerRiftDisplacement
{
    struct FResult
    {
        FVector Start = FVector::ZeroVector;
        FVector End = FVector::ZeroVector;
        float DistanceCm = 0;
        bool bReachedWall = false;
        TWeakObjectPtr<AActor> TerminalActor;
    };
    RIORSEDGE_API FResult ApplyDetailed(AActor* Target, const FVector& SourceLocation, float DistanceCm, bool bTowardSource);
    // Returns actual horizontal travel. This helper owns geometry only;
    // immunity, damage, and interruption remain the caller's responsibility.
    RIORSEDGE_API float Apply(AActor* Target, const FVector& SourceLocation, float DistanceCm);
}
