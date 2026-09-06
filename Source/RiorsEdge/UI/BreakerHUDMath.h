#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"
#include "UI/BreakerUIStyle.h"

// ---------------------------------------------------------------------------
// THE COMBAT HUD'S TIMELINES, with no HUD under them.
//
// Every curve the HUD sheet (Assets/design/02-hud/spec.md) states as a state
// rule — the crosshair's spread and ADS collapse, the health chip's hold and
// recovery, the near-death pulse, the damage number's pop/settle/rise/fade,
// the weapon-name slide, the countdown's format — is a pure function of time
// and a token. They live here so a test can walk each one over its domain;
// what stays on ABreakerPlaytestHUD is projection, the canvas and the world.
//
// Named BreakerHUDMath, not BreakerHUD: unity builds merge translation units,
// and Combat/BreakerEnemyHealthBars.cpp records exactly that collision at its
// own namespace.
//
// Every threshold below reads a BreakerUI::Hud* token. None is restated here.
// ---------------------------------------------------------------------------
namespace BreakerHUDMath
{
    // --- Crosshair ----------------------------------------------------------
    // The gap the ticks are heading for, from the weapon's honest cone. Rest
    // at zero spread, fully open at HudCrosshairFullSpreadDegrees, linear
    // between and clamped past.
    inline float CrosshairGapTarget(float SpreadDegrees)
    {
        const float T = BreakerUI::HudCrosshairFullSpreadDegrees > 0.0f
            ? FMath::Clamp(SpreadDegrees / BreakerUI::HudCrosshairFullSpreadDegrees, 0.0f, 1.0f) : 0.0f;
        return FMath::Lerp(BreakerUI::HudCrosshairGapRest, BreakerUI::HudCrosshairGapSpread, T);
    }

    // One frame of the gap's travel toward its target: the full 16→40 in 60 ms
    // opening, the full 40→16 in 200 ms closing. Linear, never overshooting.
    inline float CrosshairGapFollow(float CurrentGap, float TargetGap, float DeltaSeconds)
    {
        const float Travel = BreakerUI::HudCrosshairGapSpread - BreakerUI::HudCrosshairGapRest;
        const bool bOpening = TargetGap > CurrentGap;
        const float Seconds = bOpening ? BreakerUI::HudCrosshairSpreadOutSeconds : BreakerUI::HudCrosshairSpreadBackSeconds;
        const float Rate = Seconds > 0.0f ? Travel / Seconds : Travel;
        const float Step = Rate * FMath::Max(DeltaSeconds, 0.0f);
        return bOpening ? FMath::Min(CurrentGap + Step, TargetGap) : FMath::Max(CurrentGap - Step, TargetGap);
    }

    // 0 = hip ticks, 1 = the ADS dot and ring, moving over HudCrosshairAdsSeconds
    // from the moment the aim state last changed, in whichever direction it went.
    inline float CrosshairAdsBlend(float SecondsSinceChange, bool bAiming)
    {
        const float T = BreakerUI::HudCrosshairAdsSeconds > 0.0f
            ? FMath::Clamp(SecondsSinceChange / BreakerUI::HudCrosshairAdsSeconds, 0.0f, 1.0f) : 1.0f;
        return bAiming ? T : 1.0f - T;
    }

    // --- The health chip ---------------------------------------------------
    // On a drop the fill drains at once and a hatched chip stays where the
    // health WAS, holds, then recovers linearly down to the live value. From
    // is the fraction the chip was showing at the moment of the drop; Time is
    // that moment. A chip with a negative Time has never been struck.
    struct FHealthChip
    {
        float From = 0.0f;
        double Time = -1000.0;
    };

    // The fraction the chip currently displays. Never below the live value.
    inline float HealthChipShown(const FHealthChip& Chip, float CurrentFraction, double Now)
    {
        if (Chip.Time < 0.0) return CurrentFraction;
        const float Age = static_cast<float>(Now - Chip.Time);
        if (Age < 0.0f) return CurrentFraction;
        if (Age < BreakerUI::HudHealthChipHoldSeconds) return FMath::Max(Chip.From, CurrentFraction);
        const float Recover = Age - BreakerUI::HudHealthChipHoldSeconds;
        if (Recover >= BreakerUI::HudHealthChipRecoverSeconds) return CurrentFraction;
        const float T = Recover / BreakerUI::HudHealthChipRecoverSeconds;
        return FMath::Max(FMath::Lerp(Chip.From, CurrentFraction, T), CurrentFraction);
    }

    // A new drop. The chip re-arms from whatever it was SHOWING, so a second
    // hit inside the hold keeps the first hit's high-water mark rather than
    // collapsing to the value between them.
    inline FHealthChip HealthChipOnDrop(const FHealthChip& Chip, float ShownBeforeDrop, double Now)
    {
        FHealthChip Struck = Chip;
        Struck.From = FMath::Clamp(ShownBeforeDrop, 0.0f, 1.0f);
        Struck.Time = Now;
        return Struck;
    }

    // --- Near-death ---------------------------------------------------------
    inline bool NearDeathVisible(float HealthFraction)
    {
        return HealthFraction < BreakerUI::HudHealthLowFraction;
    }

    inline bool VitalsValueIsHarm(float HealthFraction)
    {
        return HealthFraction < BreakerUI::HudHealthLowFraction;
    }

    // The frame's border: 8→16→8 over one pulse, ease-in-out, looping.
    inline float NearDeathFrameWidth(double Now)
    {
        const float Period = BreakerUI::HudNearDeathPulseSeconds;
        const float Phase = Period > 0.0f
            ? FMath::Fmod(static_cast<float>(FMath::Max(Now, 0.0)), Period) / Period : 0.0f;
        const float Triangle = 1.0f - FMath::Abs(2.0f * Phase - 1.0f);
        const float Eased = FMath::SmoothStep(0.0f, 1.0f, Triangle);
        return FMath::Lerp(BreakerUI::HudNearDeathFrameMin, BreakerUI::HudNearDeathFrameMax, Eased);
    }

    // --- Damage numbers -----------------------------------------------------
    // How long a number lives: the rise, plus the crit hold.
    inline constexpr float DamageNumberLifetime(bool bCritical)
    {
        return BreakerUI::MotionDamageRise + (bCritical ? BreakerUI::MotionCritHold : 0.0f);
    }

    struct FDamageNumberFrame
    {
        // Size multiplier: 1 at birth, PopScale at the end of the pop, 1 again
        // at the end of the settle.
        float Scale = 1.0f;
        // 0..1 of DamageRisePixels, easing out over MotionDamageRise.
        float RiseFraction = 0.0f;
        // 1 until the last MotionDamageFade of the lifetime, then linear to 0.
        float Alpha = 1.0f;
    };

    inline FDamageNumberFrame DamageNumberFrame(float Age, float Lifetime, float PopScale)
    {
        FDamageNumberFrame Frame;
        const float A = FMath::Max(Age, 0.0f);
        if (A < BreakerUI::MotionDamagePop)
        {
            Frame.Scale = FMath::Lerp(1.0f, PopScale, A / BreakerUI::MotionDamagePop);
        }
        else if (A < BreakerUI::MotionDamagePop + BreakerUI::MotionDamageSettle)
        {
            Frame.Scale = FMath::Lerp(PopScale, 1.0f, (A - BreakerUI::MotionDamagePop) / BreakerUI::MotionDamageSettle);
        }
        const float RiseT = FMath::Clamp(A / BreakerUI::MotionDamageRise, 0.0f, 1.0f);
        Frame.RiseFraction = 1.0f - FMath::Square(1.0f - RiseT);
        const float FadeStart = FMath::Max(Lifetime - BreakerUI::MotionDamageFade, 0.0f);
        const float FadeSpan = FMath::Max(Lifetime - FadeStart, UE_KINDA_SMALL_NUMBER);
        Frame.Alpha = A <= FadeStart ? 1.0f : FMath::Clamp(1.0f - (A - FadeStart) / FadeSpan, 0.0f, 1.0f);
        return Frame;
    }

    // --- Countdown ----------------------------------------------------------
    // mm:ss, ceiling, so 0.2 s left still reads 00:01 and never 00:00 with
    // time on the clock. Empty when nothing is counting (negative).
    inline FString FormatCountdown(float Seconds)
    {
        if (Seconds < 0.0f) return FString();
        const int32 Whole = FMath::CeilToInt(Seconds);
        return FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60);
    }

    // --- The weapon name on swap ---------------------------------------------
    // Visible for HudWeaponSwapSeconds after a swap lands, sliding 16px up the
    // rail axis and fading to nothing.
    struct FSwapSlide
    {
        bool bVisible = false;
        float OffsetPixels = 0.0f;   // negative = up
        float Alpha = 0.0f;
    };

    inline FSwapSlide WeaponNameSwap(float SecondsSinceSwap)
    {
        FSwapSlide Slide;
        if (SecondsSinceSwap < 0.0f || SecondsSinceSwap >= BreakerUI::HudWeaponSwapSeconds) return Slide;
        const float T = SecondsSinceSwap / BreakerUI::HudWeaponSwapSeconds;
        Slide.bVisible = true;
        Slide.OffsetPixels = -BreakerUI::HudWeaponSwapSlidePixels * T;
        Slide.Alpha = 1.0f - T;
        return Slide;
    }

    // --- Ability tiles ------------------------------------------------------
    // The cooldown drain: a panel-2 block from the tile's bottom whose height
    // is the remaining fraction. Zero when nothing is cooling.
    inline float AbilityDrainHeight(float Remaining, float Duration, float TileHeight)
    {
        if (Duration <= 0.0f || Remaining <= 0.0f) return 0.0f;
        return TileHeight * FMath::Clamp(Remaining / Duration, 0.0f, 1.0f);
    }

    // --- The rarity tally ---------------------------------------------------
    // Cells lit on a loot plate's tally, one per tier. Rarity carried by rail
    // AND tally, so the count is the second read when the colour is not.
    inline int32 RarityTallyCells(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return 2;
            case EBreakerItemRarity::Exceptional: return 3;
            case EBreakerItemRarity::Aberrant:    return 4;
            case EBreakerItemRarity::Anomalous:   return 5;
            default:                              return 1;
        }
    }

    // --- The magazine --------------------------------------------------------
    inline bool MagazineIsLow(int32 Magazine, int32 Capacity)
    {
        if (Capacity <= 0) return false;
        return static_cast<float>(Magazine) < static_cast<float>(Capacity) * BreakerUI::HudMagazineLowFraction;
    }

    // --- The hatch ------------------------------------------------------------
    // 135° stripes are lines of constant (x + y). A period measured across the
    // stripes is a step of Period·√2 along that diagonal, and the first stripe
    // at or before a rectangle's top-left corner anchors the pattern to the
    // screen rather than to the rectangle, so two adjacent hatches line up.
    inline float HatchDiagonalStep(float Period)
    {
        return Period * UE_SQRT_2;
    }

    inline float HatchStripeStart(float X, float Y, float Period)
    {
        const float Step = HatchDiagonalStep(Period);
        if (Step <= 0.0f) return X + Y;
        return FMath::FloorToFloat((X + Y) / Step) * Step;
    }
}
