#pragma once

#include "CoreMinimal.h"
// The verb enum is the only thing this header wants from the definition. An
// opaque `enum class` declaration cannot name enumerators, so the switch in
// AbilityRailColor needs the complete type; the definition header carries no
// world and no UI, so the timelines here stay as world-free as they were.
#include "Abilities/BreakerAbilityDefinition.h"
#include "Items/BreakerItemTypes.h"
#include "UI/BreakerUIStyle.h"

// ---------------------------------------------------------------------------
// THE COMBAT HUD'S TIMELINES, with no HUD under them.
//
// Every curve the HUD sheet (Assets/design/02-hud/spec.md) states as a state
// rule — the crosshair's spread and ADS collapse, the health chip's hold and
// recovery, the near-death pulse, the damage number's snap/settle/hold/rise/
// fade, the hit tell's bearing and fade, the weapon-name slide, the
// countdown's format — is a pure function of time
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
    inline float AbilityRecoveryFraction(float Remaining, float Duration)
    {
        return Duration > UE_SMALL_NUMBER ? 1.0f - FMath::Clamp(Remaining / Duration, 0.0f, 1.0f) : 1.0f;
    }

    // Screen coordinates: twelve o'clock first, then clockwise.
    inline FVector2D AbilityRadialPoint(float Fraction)
    {
        const float Angle = FMath::Clamp(Fraction, 0.0f, 1.0f) * 2.0f * UE_PI - UE_HALF_PI;
        return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
    }

    inline FString AbilityCooldownText(float Remaining)
    {
        if (Remaining <= 0.0f) return FString();
        if (Remaining >= 10.0f) return FString::FromInt(FMath::CeilToInt(Remaining));
        // Round up so an active cooldown never says zero.
        return FString::Printf(TEXT("%.1f"), FMath::CeilToFloat(Remaining * 10.0f) / 10.0f);
    }
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

    // --- The hit tell -------------------------------------------------------
    // Where a hit came from, as an arc on the crosshair's ring. Each tell
    // remembers the WORLD bearing of its source, not the screen angle: the
    // player turns toward the thing and the arc must swing to the front as
    // they do, which a latched screen angle cannot. A tell with a negative
    // Time has never been struck.
    struct FHitTell
    {
        double Time = -1000.0;
        float WorldYawDegrees = 0.0f;
    };

    // The XY bearing from the camera to the source, in degrees, Unreal's
    // frame (+X is yaw 0, +Y is yaw 90).
    inline float HitWorldYaw(const FVector& CameraLocation, const FVector& SourceLocation)
    {
        const FVector Delta = SourceLocation - CameraLocation;
        return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
    }

    // The bearing on screen: 0 straight ahead, +90 to the right, ±180 behind.
    inline float HitTellScreenDegrees(float WorldYaw, float CameraYaw)
    {
        return FMath::UnwindDegrees(WorldYaw - CameraYaw);
    }

    // The unit direction from the crosshair for a screen bearing, in canvas
    // coordinates (y down): 0 is up, +90 right, 180 down.
    inline FVector2D HitTellArcPoint(float ScreenDegrees)
    {
        const float Radians = FMath::DegreesToRadians(ScreenDegrees);
        return FVector2D(FMath::Sin(Radians), -FMath::Cos(Radians));
    }

    // 1 at the hit, linear to 0 at HudHitTellSeconds; 0 outside that window.
    inline float HitTellAlpha(float Age)
    {
        if (Age < 0.0f || Age >= BreakerUI::HudHitTellSeconds) return 0.0f;
        return 1.0f - Age / BreakerUI::HudHitTellSeconds;
    }

    // A hit lands. A LIVE tell within HudHitTellMergeDegrees of the bearing
    // is refreshed in place — the same enemy, still there — rather than
    // stacked; otherwise the hit gets its own tell, and past HudHitTellMax the
    // oldest gives up its slot. The array never grows past the cap.
    inline void HitTellsOnHit(TArray<FHitTell>& Tells, float WorldYaw, double Now)
    {
        for (FHitTell& Tell : Tells)
        {
            if (HitTellAlpha(static_cast<float>(Now - Tell.Time)) <= 0.0f) continue;
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(Tell.WorldYawDegrees, WorldYaw)) <= BreakerUI::HudHitTellMergeDegrees)
            {
                Tell.WorldYawDegrees = WorldYaw;
                Tell.Time = Now;
                return;
            }
        }
        FHitTell Fresh;
        Fresh.Time = Now;
        Fresh.WorldYawDegrees = WorldYaw;
        if (Tells.Num() < BreakerUI::HudHitTellMax)
        {
            Tells.Add(Fresh);
            return;
        }
        int32 Oldest = 0;
        for (int32 Index = 1; Index < Tells.Num(); ++Index)
        {
            if (Tells[Index].Time < Tells[Oldest].Time) Oldest = Index;
        }
        Tells[Oldest] = Fresh;
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
    // The line under which the health bar wears harm. The world's own answer
    // to the same line — the closing edges and the drained colour — lives in
    // Characters/BreakerHarmPresentationMath.h and reads this same number.
    inline bool VitalsValueIsHarm(float HealthFraction)
    {
        return HealthFraction < BreakerUI::HudHealthLowFraction;
    }

    // --- Damage numbers -----------------------------------------------------
    // How long a number lives: the hold, the rise, plus the crit hold.
    inline constexpr float DamageNumberLifetime(bool bCritical)
    {
        return BreakerUI::MotionDamageHold + BreakerUI::MotionDamageRise + (bCritical ? BreakerUI::MotionCritHold : 0.0f);
    }

    struct FDamageNumberFrame
    {
        // Size multiplier: PopScale AT birth — the snap-in — easing down to 1
        // by the end of the settle.
        float Scale = 1.0f;
        // 0..1 of DamageRisePixels: 0 through MotionDamageHold, then easing
        // out over MotionDamageRise.
        float RiseFraction = 0.0f;
        // 1 until the last MotionDamageFade of the lifetime, then linear to 0.
        float Alpha = 1.0f;
    };

    inline FDamageNumberFrame DamageNumberFrame(float Age, float Lifetime, float PopScale)
    {
        FDamageNumberFrame Frame;
        const float A = FMath::Max(Age, 0.0f);
        if (A < BreakerUI::MotionDamageSettle)
        {
            const float SettleT = A / BreakerUI::MotionDamageSettle;
            Frame.Scale = FMath::Lerp(PopScale, 1.0f, 1.0f - FMath::Square(1.0f - SettleT));
        }
        const float RiseAge = A - BreakerUI::MotionDamageHold;
        const float RiseT = RiseAge <= 0.0f ? 0.0f : FMath::Clamp(RiseAge / BreakerUI::MotionDamageRise, 0.0f, 1.0f);
        Frame.RiseFraction = 1.0f - FMath::Square(1.0f - RiseT);
        const float FadeStart = FMath::Max(Lifetime - BreakerUI::MotionDamageFade, 0.0f);
        const float FadeSpan = FMath::Max(Lifetime - FadeStart, UE_KINDA_SMALL_NUMBER);
        Frame.Alpha = A <= FadeStart ? 1.0f : FMath::Clamp(1.0f - (A - FadeStart) / FadeSpan, 0.0f, 1.0f);
        return Frame;
    }

    // The outline's stroke for a glyph already scaled to device pixels: a
    // fraction of the size, never under the floor. The draw and the
    // label-bounds calculation both read this, so a number's collision box
    // is exactly the box it paints.
    inline float DamageOutlineOffset(float ScaledSizePixels)
    {
        return FMath::Max(ScaledSizePixels * BreakerUI::DamageOutlineFraction, BreakerUI::DamageOutlineMinPixels);
    }

    // --- The tracker's distance line ------------------------------------------
    // The beat lines above already name the objective, so under them the
    // distance alone is the read: "77m". Only a manually tracked map marker —
    // which nothing above names — keeps its label in front of the figure.
    inline FString FormatTrackerDistance(const FString& Label, float DistanceCm, bool bLabelIsTheBeat)
    {
        const int32 Metres = FMath::RoundToInt(DistanceCm / 100.0f);
        if (bLabelIsTheBeat) return FString::Printf(TEXT("%dm"), Metres);
        return FString::Printf(TEXT("%s · %dm"), *Label, Metres);
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

    // The rail an ability tile carries (O179): colour by VERB, never by slot
    // or class. The ultimate slot is violet whatever its verb; a definition
    // that has not said its verb (None) gets the resting border, not a guess.
    inline FLinearColor AbilityRailColor(EBreakerAbilityVerb Verb, bool bUltimate)
    {
        if (bUltimate) return BreakerUI::Violet;
        switch (Verb)
        {
            case EBreakerAbilityVerb::Movement: return BreakerUI::VerbMove;
            case EBreakerAbilityVerb::Weapon:   return BreakerUI::Orange;
            case EBreakerAbilityVerb::Reward:   return BreakerUI::Gold;
            case EBreakerAbilityVerb::Taunt:    return BreakerUI::Harm;
            default:                            return BreakerUI::BorderRest;
        }
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
            case EBreakerItemRarity::Unwritten:   return 5;
            default:                              return 1;
        }
    }

    // --- The interact plate ---------------------------------------------------
    // One plate over the thing F would act on: a 4px identity rail, the key
    // tile (32, only on the one F would take), the tally (4×12 cells at a 2
    // gap), the name, a 12 right pad. 40 tall. Every offset is a function of
    // the name's measured width and the cell count, so the plate is sized
    // before anything is drawn and cannot read its own arrangement. Spec
    // pixels; the caller scales. O2 PLACEHOLDER, every number.
    struct FInteractPlateLayout
    {
        float Height = 40.0f;
        float RailWidth = 4.0f;
        bool bKeyTile = false;
        float KeyX = 0.0f;
        float KeySize = 32.0f;
        float TallyX = 0.0f;
        float TallyY = 0.0f;
        float TallyCellWidth = 4.0f;
        float TallyCellHeight = 12.0f;
        float TallyGap = 2.0f;
        float TallyWidth = 0.0f;
        float NameX = 0.0f;
        float Width = 0.0f;
    };

    inline FInteractPlateLayout InteractPlateLayout(float NameWidth, int32 TallyCells, bool bKeyTile)
    {
        constexpr float Gap = 4.0f;        // rail to first element
        constexpr float ElementGap = 8.0f; // between key, tally and name
        constexpr float RightPad = 12.0f;
        FInteractPlateLayout L;
        L.bKeyTile = bKeyTile;
        float X = L.RailWidth + Gap;
        if (bKeyTile)
        {
            L.KeyX = X;
            X += L.KeySize + ElementGap;
        }
        const int32 Cells = FMath::Max(TallyCells, 0);
        L.TallyX = X;
        L.TallyY = (L.Height - L.TallyCellHeight) * 0.5f;
        L.TallyWidth = Cells > 0 ? L.TallyCellWidth * Cells + L.TallyGap * (Cells - 1) : 0.0f;
        if (Cells > 0) X += L.TallyWidth + ElementGap;
        L.NameX = X;
        L.Width = X + FMath::Max(NameWidth, 0.0f) + RightPad;
        return L;
    }

    // --- Step marks on the resource track ---------------------------------
    // Grit's three bands are equal thirds of the bar, so the track carries a
    // mark at each band edge and the band is readable without the word.
    inline TArray<float> GritStepMarks()
    {
        return { 1.0f / 3.0f, 2.0f / 3.0f };
    }

    // --- Wave cells -------------------------------------------------------
    // O120: cells are drawn only where a total is authored. A rift run has one
    // — its boss wave — so the row shows that many cells; the gym has no run
    // and shows none. The interval is the rift budget's BossWaveInterval.
    inline int32 WaveCellTotal(bool bRiftSet, int32 BossWaveInterval)
    {
        return bRiftSet ? FMath::Max(BossWaveInterval, 0) : 0;
    }

    // --- The wallet gain ------------------------------------------------------
    // "+N RIFTGLASS" for a moment after the wallet grows. The HUD has no seam
    // from the wallet, so this DIFFS: each frame it is told the balance, and a
    // rise is a gain. The first observation seeds silently — the account folds
    // its whole balance in at spawn, and that is not a gain the player just
    // made. A fall re-bases silently — a spend at the Forge is not a gain
    // either. Gains inside one hold accumulate into one figure and restart
    // the hold, so a pack's worth of kills reads as one rising number rather
    // than a stutter of small ones. When the hold ends the figure is spent:
    // the next gain starts from zero.
    struct FBreakerWalletGainReadout
    {
        static constexpr float GainHoldSeconds = 1.6f;   // O2 PLACEHOLDER
        // The last part of the hold fades linearly to nothing, the ELITE DOWN
        // callout's shape.
        static constexpr float GainFadeSeconds = 0.4f;   // O2 PLACEHOLDER

        int32 Pending = 0;
        int32 LastSeen = 0;
        bool bSeeded = false;
        double HoldStart = -1000.0;

        bool IsShowing(double Now) const
        {
            const double Age = Now - HoldStart;
            return Pending > 0 && Age >= 0.0 && Age < GainHoldSeconds;
        }

        float Alpha(double Now) const
        {
            if (!IsShowing(Now)) return 0.0f;
            const float Age = static_cast<float>(Now - HoldStart);
            const float FadeStart = FMath::Max(GainHoldSeconds - GainFadeSeconds, 0.0f);
            const float FadeSpan = FMath::Max(GainHoldSeconds - FadeStart, UE_KINDA_SMALL_NUMBER);
            return Age <= FadeStart ? 1.0f : FMath::Clamp(1.0f - (Age - FadeStart) / FadeSpan, 0.0f, 1.0f);
        }

        void Observe(int32 WalletNow, double Now)
        {
            if (!bSeeded)
            {
                bSeeded = true;
                LastSeen = WalletNow;
                return;
            }
            if (!IsShowing(Now)) Pending = 0;
            const int32 Delta = WalletNow - LastSeen;
            LastSeen = WalletNow;
            if (Delta <= 0) return;
            Pending += Delta;
            HoldStart = Now;
        }
    };

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
