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

    // -----------------------------------------------------------------------
    // THE FRONT POOL (O198). A bearer's frontal shield is a pool of a fraction
    // of its max health that absorbs frontal damage until it is spent, and is
    // then gone for the fight. It sits AHEAD of the attribute shield (the
    // Warded ward) in the shield step: the pool pays first, whatever spills
    // goes on to the ward, and what the ward cannot hold reaches health.
    //
    // THERE IS NO RECHARGE FUNCTION HERE, BY CONSTRUCTION. RechargeStep above
    // is the player-side ward's; the front pool has no source once armed, so a
    // "refill" can only ever be ArmFrontShield at a vitals restore. A recharge
    // written for this pool is the rule being un-ruled.
    // -----------------------------------------------------------------------

    struct FFrontSpend
    {
        // What is left in the pool after this hit.
        float Remaining = 0.0f;
        // The part of the hit the pool could not hold; the ward's problem next.
        float Spill = 0.0f;
        // True on the hit that took the pool to zero — the tell fires once.
        bool bBroke = false;
    };

    // The pool a bearer arms: its max health times the archetype's fraction.
    inline float FrontPool(float MaxHealth, float Fraction)
    {
        return FMath::Max(0.0f, MaxHealth) * FMath::Clamp(Fraction, 0.0f, 1.0f);
    }

    // The pool pays first. ShieldDamage is the whole amount the shield step
    // routed; the pool keeps what it can and hands the rest on.
    inline FFrontSpend SpendFrontPool(float Pool, float ShieldDamage)
    {
        FFrontSpend Spend;
        const float Before = FMath::Max(0.0f, Pool);
        const float Paid = FMath::Min(Before, FMath::Max(0.0f, ShieldDamage));
        Spend.Remaining = Before - Paid;
        Spend.Spill = FMath::Max(0.0f, ShieldDamage) - Paid;
        Spend.bBroke = Before > 0.0f && Spend.Remaining <= 0.0f;
        return Spend;
    }

    // Rounds of a flat per-round damage needed to break a pool: ceil. A pool
    // of nothing breaks in zero rounds; a round of nothing never breaks it.
    inline int32 RoundsToBreak(float Pool, float DamagePerRound)
    {
        if (Pool <= 0.0f) return 0;
        if (DamagePerRound <= 0.0f) return TNumericLimits<int32>::Max();
        return FMath::CeilToInt(Pool / DamagePerRound);
    }
}
