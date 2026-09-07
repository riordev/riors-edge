#pragma once
#include "Combat/BreakerCombatTypes.h"

namespace BreakerElementConversion
{
    struct FSelection
    {
        EBreakerElement Element = EBreakerElement::None;
        float Fraction = 0;
    };

    inline FSelection Select(float GearEntropy, float GearVoid, float AttunementEntropy)
    {
        const auto Fraction = [](float Value)
        { return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f; };
        const float Entropy = FMath::Max(Fraction(GearEntropy), Fraction(AttunementEntropy));
        const float Void = Fraction(GearVoid);
        // O2 deterministic selection: one converted share, never both. Exact
        // ties keep Entropy. Current full Attunement therefore overrides even
        // a fully converted Void weapon until its owned buff/tail expires.
        if (Void > Entropy) return { EBreakerElement::Void, Void };
        if (Entropy > 0) return { EBreakerElement::Entropy, Entropy };
        return {};
    }
}
