#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Progression/BreakerProgressionComponent.h"
namespace BreakerResourceGeneration
{
    // Normal earned income only. Refunds and explicit direct grants do not
    // pass this seam; class-specific suspension and budgets remain upstream.
    inline float Multiplier(const AActor* Owner)
    {
        const auto* Progression = Owner ? Owner->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
        const float Value = Progression ? Progression->GetNodeStats().ClassResourceGenerationMultiplier : 1.0f;
        return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 1.0f;
    }
}
