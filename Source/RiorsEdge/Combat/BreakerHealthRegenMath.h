#pragma once

#include "CoreMinimal.h"

namespace BreakerHealthRegen
{
    inline constexpr float PercentMaxHealthPerSecond = 2.0f; // O2 PLACEHOLDER within owner's 1–2% physical maximum health/s range.
    inline constexpr float DelaySeconds = 4.0f; // Owner: four seconds without combat.

    inline float CombatAge(float SecondsSinceIncoming, float SecondsSinceOutgoing)
    {
        return FMath::Min(SecondsSinceIncoming, SecondsSinceOutgoing);
    }

    inline float Step(float Health, float MaxHealth, float SecondsSinceCombat, float DeltaSeconds,
        float Rate = PercentMaxHealthPerSecond, float Delay = DelaySeconds)
    {
        if (Health <= 0.0f || Health >= MaxHealth || DeltaSeconds <= 0.0f || Rate <= 0.0f) return Health;
        // Only the portion of this frame beyond the delay earns recovery.
        const float ActiveSeconds = FMath::Min(DeltaSeconds, FMath::Max(0.0f, SecondsSinceCombat - FMath::Max(0.0f, Delay)));
        return FMath::Min(MaxHealth, Health + MaxHealth * Rate * .01f * ActiveSeconds);
    }
}
