#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
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
    inline float FlatRate(const AActor* Owner)
    {
        const auto* Combat = Owner ? Owner->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!Owner || Owner->IsActorBeingDestroyed() || !Combat || Combat->IsDead()) return 0.0f;
        const auto* Progression = Owner->FindComponentByClass<UBreakerProgressionComponent>();
        const float Rate = Progression ? Progression->GetNodeStats().ClassResourceRegenPerSecond : 0.0f;
        return FMath::IsFinite(Rate) ? FMath::Max(0.0f, Rate) : 0.0f;
    }
    inline bool HoldsOutOfCombatDecay(const AActor* Owner)
    {
        const auto* Character = Cast<ABreakerCharacter>(Owner);
        const auto* Progression = Owner ? Owner->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
        return Character && Progression && Progression->GetNodeStats().bNoOutOfCombatResourceDecay
            && !Character->IsInResourceCombat();
    }
}
