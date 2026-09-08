#pragma once
#include "CoreMinimal.h"

// O228: halve the contribution, never the lane's neutral value. Discrete
// contributions remain fractional until their owning lane quantizes them.
struct FBreakerWindowLaneMath
{
    static constexpr double TailSeconds = 2.0;
    static float Scale(double Now, double End, bool bTail)
    {
        if (End < 0 || Now < End) return 1.0f;
        return bTail && Now < End + TailSeconds ? .5f : 0.0f;
    }
    static float Multiplier(float Value, float ContributionScale)
    { return 1.0f + (Value - 1.0f) * ContributionScale; }
};
