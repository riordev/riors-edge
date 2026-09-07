#pragma once
#include "CoreMinimal.h"

namespace BreakerBuildup
{
    struct FDecayState
    {
        float Amount = 0;
        float GraceRemaining = 0;
        float FadeRemaining = 0;
    };

    // Keep grace and linear fade separate. Partitioning elapsed time into
    // frames must not change how much of an earned contribution remains.
    inline FDecayState Advance(FDecayState State, float Seconds)
    {
        if (!FMath::IsFinite(Seconds) || Seconds <= 0) return State;
        const float GraceSpent = FMath::Min(Seconds, State.GraceRemaining);
        State.GraceRemaining -= GraceSpent;
        const float FadeSpent = FMath::Min(Seconds - GraceSpent, State.FadeRemaining);
        if (State.FadeRemaining > 0)
            State.Amount *= FMath::Max(0.0f, 1.0f - FadeSpent / State.FadeRemaining);
        State.FadeRemaining -= FadeSpent;
        if (State.GraceRemaining <= 0 && State.FadeRemaining <= 0) State.Amount = 0;
        return State;
    }
}
