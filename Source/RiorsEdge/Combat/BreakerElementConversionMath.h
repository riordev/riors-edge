#pragma once
#include "Combat/BreakerCombatTypes.h"

namespace BreakerElementConversion
{
    struct FSelection
    {
        EBreakerElement Element = EBreakerElement::None;
        float Fraction = 0;
    };

    inline FSelection Select(float GearEntropy, float GearVoid, float AttunementEntropy, float GearRift = 0)
    {
        const auto Fraction = [](float Value)
        { return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f; };
        const float Entropy = FMath::Max(Fraction(GearEntropy), Fraction(AttunementEntropy));
        const float Void = Fraction(GearVoid);
        const float Rift = Fraction(GearRift);
        // O2 deterministic selection: one maximum share. Strict-greater
        // priority preserves ties in the order Entropy, Void, Rift. Current
        // full Attunement wins until its owned buff/tail expires.
        if (Rift > Entropy && Rift > Void) return { EBreakerElement::Rift, Rift };
        if (Void > Entropy) return { EBreakerElement::Void, Void };
        if (Entropy > 0) return { EBreakerElement::Entropy, Entropy };
        return {};
    }
}
