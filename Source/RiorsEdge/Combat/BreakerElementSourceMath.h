#pragma once
#include "Combat/BreakerCombatTypes.h"
#include "Attributes/BreakerAttributeAggregation.h"

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
    inline float SpecificStatusIncreased(const FBreakerDamageRequest& Request)
    {
        return Request.Element == EBreakerElement::Entropy ? Request.ElementSource.RotDamageIncreasedPercent
            : Request.Element == EBreakerElement::Void ? Request.ElementSource.VoidBurstDamageIncreasedPercent
            : Request.ElementSource.RiftBurstDamageIncreasedPercent;
    }
    inline float StatusBudget(const FBreakerDamageRequest& Request, float RawBudget)
    {
        if (!Request.bHasSourceSplit) return RawBudget;
        const float Existing = 1 + (Request.SourceIncreasedPercent + Request.ElementSource.ElementalDamageIncreasedPercent) / 100;
        const float Composed = Existing + SpecificStatusIncreased(Request) / 100;
        return Existing > 0 && FMath::IsFinite(Composed) ? RawBudget * FMath::Max(0.0f, Composed) / Existing : 0;
    }
    inline void SnapshotReactionCredit(const FBreakerDamageRequest& Request, FBreakerStatusApplicationSpec& Spec)
    {
        if (!Request.bHasSourceSplit) return;
        const float Existing = 1 + (Request.SourceIncreasedPercent + Request.ElementSource.ElementalDamageIncreasedPercent
            + SpecificStatusIncreased(Request)) / 100;
        const float Composed = Existing + Request.ElementSource.ReactionDamageIncreasedPercent / 100;
        // Temporary delivery windows may already spend selected scope headroom.
        // Match direct allocation, then reserve only the remainder for this credit.
        const auto ValidMore = [](float Value) { return FMath::IsFinite(Value) ? FMath::Max(1.0f, Value) : 1.0f; };
        const float Ceiling = FBreakerAttributeAggregator::ComposedMoreCeiling();
        const float DeliveryMore = ValidMore(Request.SourceMoreProduct);
        const float ElementMore = FMath::Min(ValidMore(Request.ElementSource.ElementalMoreProduct)
            * (Request.Element == EBreakerElement::Void ? ValidMore(Request.ElementSource.VoidMoreProduct) : 1.0f),
            FMath::Max(1.0f, Ceiling / DeliveryMore));
        const float ReactionMore = FMath::Min(ValidMore(Request.ElementSource.ReactionMoreProduct),
            FMath::Max(1.0f, Ceiling / (DeliveryMore * ElementMore)));
        Spec.bHasReactionCreditSnapshot = true;
        Spec.ReactionCreditMultiplier = Existing > 0 && FMath::IsFinite(Composed)
            ? FMath::Max(0.0f, Composed) / Existing * ReactionMore : 0;
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
