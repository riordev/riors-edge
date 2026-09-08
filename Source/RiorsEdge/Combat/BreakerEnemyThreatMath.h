#pragma once
#include "CoreMinimal.h"
namespace BreakerEnemyThreat
{
    inline float Earned(float HealthDamage, float ShieldDamage)
    {
        const float Amount = HealthDamage + ShieldDamage;
        return FMath::IsFinite(Amount) ? FMath::Max(0.0f, Amount) : 0.0f;
    }
    inline bool Prefer(float Score, bool bCurrent, float DistanceSq,
        float BestScore, bool bBestCurrent, float BestDistanceSq)
    {
        if (Score != BestScore) return Score > BestScore;
        if (bCurrent != bBestCurrent) return bCurrent;
        return DistanceSq < BestDistanceSq;
    }
}
