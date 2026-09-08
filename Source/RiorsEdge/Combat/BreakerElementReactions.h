#pragma once
#include "CoreMinimal.h"
struct FBreakerActiveStatus;

namespace BreakerElementReactions
{
    RIORSEDGE_API float OverlapThresholdFraction();
    // The finite unpaid raw amount still scheduled before Rot's current end.
    RIORSEDGE_API float RemainingRotBudget(const FBreakerActiveStatus& Status);
}
