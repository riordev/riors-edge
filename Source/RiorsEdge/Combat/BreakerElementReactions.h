#pragma once
#include "CoreMinimal.h"
struct FBreakerActiveStatus;

namespace BreakerElementReactions
{
    // The finite unpaid raw amount still scheduled before Rot's current end.
    RIORSEDGE_API float RemainingRotBudget(const FBreakerActiveStatus& Status);
}
