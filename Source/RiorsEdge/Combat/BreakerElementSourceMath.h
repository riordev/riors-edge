#pragma once
#include "Combat/BreakerCombatTypes.h"

namespace BreakerElementSource
{
    inline float RawPart(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result)
    {
        if (Result.bHasElementRawAllocation)
        {
            for (const auto& Part : Result.ElementRawDamage)
                if (Part.Element == Request.Element) return Part.RawDamage;
            return 0;
        }
        // Compatibility for direct kernel fixtures/callers that provide an unresolved result.
        return Result.RawDamage * FMath::Clamp(Request.ElementalFraction, 0.0f, 1.0f);
    }
    inline float BuildupMultiplier(const FBreakerDamageRequest& Request)
    {
        const auto& Source = Request.ElementSource;
        const float Specific = Request.Element == EBreakerElement::Entropy ? Source.EntropyBuildupIncreasedPercent
            : Request.Element == EBreakerElement::Void ? Source.VoidBuildupIncreasedPercent : Source.RiftBuildupIncreasedPercent;
        const float Percent = Source.ElementalBuildupIncreasedPercent + Specific;
        return FMath::IsFinite(Percent) ? FMath::Max(0.0f, 1 + Percent / 100) : 1;
    }
    inline float ResistanceFactor(const FBreakerDamageRequest& Request, float Resistance)
    {
        const float Penetration = Request.ElementSource.ElementalBuildupPenetrationPercent;
        return 1 - FMath::Clamp(Resistance - (FMath::IsFinite(Penetration) ? FMath::Max(0.0f, Penetration) : 0), 0.0f, 100.0f) / 100;
    }
    inline float Threshold(const FBreakerDamageRequest& Request, float BaseThreshold)
    {
        const float Multiplier = Request.ElementSource.ElementalThresholdMultiplier;
        return BaseThreshold * (FMath::IsFinite(Multiplier) ? FMath::Clamp(Multiplier, .01f, 1.0f) : 1.0f);
    }
}
