#pragma once

#include "Combat/BreakerCombatTypes.h"

namespace BreakerElementShares
{
    inline bool IsValidElement(EBreakerElement Element)
    {
        return Element == EBreakerElement::Entropy || Element == EBreakerElement::Void || Element == EBreakerElement::Rift;
    }

    inline TArray<FBreakerElementShare> Resolve(const FBreakerDamageRequest& Request)
    {
        TArray<FBreakerElementShare> Result;
        if (Request.ElementShares.IsEmpty())
        {
            if (IsValidElement(Request.Element) && FMath::IsFinite(Request.ElementalFraction) && Request.ElementalFraction > 0)
                Result.Emplace(Request.Element, FMath::Min(Request.ElementalFraction, 1.0f));
            return Result;
        }
        // Double accumulation also keeps very large but finite authored values
        // from overflowing before proportional normalization.
        TArray<double> Weights;
        double Total = 0;
        for (const FBreakerElementShare& Share : Request.ElementShares)
        {
            if (!IsValidElement(Share.Element) || !FMath::IsFinite(Share.Fraction) || Share.Fraction <= 0) continue;
            int32 Index = Result.IndexOfByPredicate([&Share](const FBreakerElementShare& Entry) { return Entry.Element == Share.Element; });
            if (Index == INDEX_NONE)
            {
                Index = Result.Emplace(Share.Element, 0.0f);
                Weights.Add(0);
            }
            Weights[Index] += static_cast<double>(Share.Fraction);
            Total += static_cast<double>(Share.Fraction);
        }
        const double Divisor = FMath::Max(1.0, Total);
        for (int32 Index = 0; Index < Result.Num(); ++Index)
            Result[Index].Fraction = static_cast<float>(Weights[Index] / Divisor);
        return Result;
    }

    inline float TotalFraction(const TArray<FBreakerElementShare>& Shares)
    {
        double Total = 0;
        for (const FBreakerElementShare& Share : Shares) Total += Share.Fraction;
        return static_cast<float>(FMath::Clamp(Total, 0.0, 1.0));
    }
}
