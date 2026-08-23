#pragma once

#include "CoreMinimal.h"

// ===========================================================================
// THE CHARACTER SHEET'S ARITHMETIC, world-free.
//
// Everything the sheet prints that is a DERIVED number rather than a stored
// one lives here: no AActor, no UWorld, no component. The screen is a thin
// caller, which is the only reason any of this can be asserted at all.
//
// ON PRINTING DPS AT ALL. art-and-ui says "No damage-per-second meter, ever",
// and the reason it gives is real: a live meter turns a build-crafting game
// into a spreadsheet-optimisation game and pushes the community toward one
// correct answer. This is not that meter. It is a full-screen modal the player
// opens deliberately, on the same footing as the item comparison the spec
// already calls the most important screen in the game, and it prints the
// BREAKDOWN rather than a single score -- the same distinction O-ledger draws
// between an item card and an item score. The spec line is nonetheless now
// false as written and wants amending to "never on the HUD".
// ===========================================================================

namespace BreakerSheet
{
    // The additive bucket, expressed as the multiplier the aggregation law
    // produces: (1 + sum(Increased)/100). Passed in already composed, because
    // composing it is the attribute layer's job and not this header's.
    inline float ShotDamage(float ScaledBaseDamage, int32 Pellets, float IncreasedMultiplier)
    {
        return ScaledBaseDamage * static_cast<float>(FMath::Max(1, Pellets)) * FMath::Max(0.0f, IncreasedMultiplier);
    }

    // EXPECTED damage per hit, not a best case. Crit is a chance layer, so the
    // honest single number is the expectation: 1 + p(m - 1). Printing the crit
    // damage instead would report a number the player sees on some fraction of
    // hits and never on average, which is the shape of lie a sheet exists to
    // stop.
    inline float CritFactor(float CritChance01, float CritMultiplier)
    {
        const float Chance = FMath::Clamp(CritChance01, 0.0f, 1.0f);
        return 1.0f + Chance * FMath::Max(0.0f, CritMultiplier - 1.0f);
    }

    inline float RoundsPerSecond(float RoundsPerMinute)
    {
        return FMath::Max(0.0f, RoundsPerMinute) / 60.0f;
    }

    // Burst: the gun is firing and never stops. This is the number that
    // flatters a build and it is labelled as such on the screen.
    inline float BurstDps(float ShotDamageValue, float CritFactorValue, float RoundsPerMinute)
    {
        return ShotDamageValue * CritFactorValue * RoundsPerSecond(RoundsPerMinute);
    }

    // Sustained: a magazine, then a reload, forever. THIS is the number that
    // describes play, and the two differ by the whole reload economy -- a
    // weapon can win on burst and lose on sustained, which is exactly the
    // decision the archetype table exists to offer.
    inline float SustainedDps(float ShotDamageValue, float CritFactorValue, float RoundsPerMinute,
        int32 MagazineSize, float ReloadSeconds)
    {
        const float Rps = RoundsPerSecond(RoundsPerMinute);
        const int32 Magazine = FMath::Max(1, MagazineSize);
        if (Rps <= 0.0f) return 0.0f;
        const float EmptySeconds = static_cast<float>(Magazine) / Rps;
        const float CycleSeconds = EmptySeconds + FMath::Max(0.0f, ReloadSeconds);
        if (CycleSeconds <= 0.0f) return 0.0f;
        return (ShotDamageValue * CritFactorValue * static_cast<float>(Magazine)) / CycleSeconds;
    }

    // combat.md's curve, with its cap. K and the cap are the combat layer's
    // constants and are passed rather than restated, so this header cannot
    // drift out of agreement with the pipeline it describes.
    inline float ArmourMitigation(float Armor, float K = 100.0f, float Cap = 0.8f)
    {
        if (Armor <= 0.0f) return 0.0f;
        return FMath::Min(Armor / (Armor + FMath::Max(KINDA_SMALL_NUMBER, K)), Cap);
    }

    // EFFECTIVE HEALTH POOL: how much raw incoming damage the pool absorbs
    // once mitigation is applied. Shield and health are summed because both
    // are routed through the same mitigation step -- if that ever stops being
    // true, this function is where it shows up rather than in a screen.
    inline float EffectiveHealthPool(float Health, float Shield, float Mitigation)
    {
        const float Pool = FMath::Max(0.0f, Health) + FMath::Max(0.0f, Shield);
        const float Remaining = 1.0f - FMath::Clamp(Mitigation, 0.0f, 0.99f);
        return Remaining > KINDA_SMALL_NUMBER ? Pool / Remaining : Pool;
    }

    // How many hits of a given size the pool survives. The ceiling, not the
    // floor: a hit that leaves 1 health has not killed anybody.
    inline int32 HitsSurvived(float EffectiveHealth, float RawHitDamage)
    {
        if (RawHitDamage <= KINDA_SMALL_NUMBER) return 0;
        return FMath::Max(1, FMath::CeilToInt(EffectiveHealth / RawHitDamage));
    }
}
