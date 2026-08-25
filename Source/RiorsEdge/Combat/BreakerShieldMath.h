#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// The shield recharge, pure. The player-side source for the shield pool the
// damage library already spends (absorb-before-health, bShieldBroken,
// bBypassShield all shipped long before anything could fill the bar).
//
// THE ONE RULE: recharge fills SHIELD ONLY, exactly as leech and healing
// fill LIFE ONLY — the sustain asymmetry is the armour archetypes' whole
// mechanism (see Items/BreakerItemBaseStats.h). This function is called
// with the combat component's own seconds-since-damage, the same clock the
// enemy Warded modifier already recharges on; taking damage resets that
// clock at the source, so a shield under fire never climbs.
// ---------------------------------------------------------------------------
namespace BreakerShield
{
    // Out of combat this long before the bar starts refilling. The Warded
    // modifier's 4 s is the precedent. O2 PLACEHOLDER.
    constexpr float RechargeDelaySeconds = 4.0f;
    // Refill rate as a fraction of MaxShield per second: a broken bar is
    // whole after five undisturbed seconds past the delay. O2 PLACEHOLDER.
    constexpr float RechargeFractionPerSecond = 0.2f;

    // One frame of recharge. Returns the new shield value; equal to Current
    // whenever nothing should happen (no pool, still in the delay, already
    // full). Never overshoots the cap and never goes backwards.
    inline float RechargeStep(float Current, float Max, float SecondsSinceDamage, float DeltaSeconds,
        float DelaySeconds = RechargeDelaySeconds, float FractionPerSecond = RechargeFractionPerSecond)
    {
        if (Max <= 0.0f || DeltaSeconds <= 0.0f) return Current;
        if (SecondsSinceDamage < DelaySeconds) return Current;
        if (Current >= Max) return Current;
        return FMath::Min(Max, Current + Max * FMath::Max(FractionPerSecond, 0.0f) * DeltaSeconds);
    }
}
