#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// FIELDPLATE — the one place UI values live.
//
// Transcribed from Docs/Design/UI-Style-Guide-Fieldplate.md. Both the canvas
// HUD (ABreakerPlaytestHUD) and the Slate front end (SBreakerMenu) read these
// tokens, which is the whole point: a colour that exists twice drifts.
//
// The design canvas authors colour in sRGB hex, so every token converts once
// through FromSRGBColor. Do NOT hand-author linear values here — they will not
// match the canvas, and the canvas is the authority.
// ---------------------------------------------------------------------------
namespace BreakerUI
{
    inline FLinearColor Hex(uint32 RGB)
    {
        return FLinearColor::FromSRGBColor(FColor(
            static_cast<uint8>((RGB >> 16) & 0xFF),
            static_cast<uint8>((RGB >> 8) & 0xFF),
            static_cast<uint8>(RGB & 0xFF)));
    }

    inline FLinearColor Alpha(const FLinearColor& Color, float A)
    {
        return FLinearColor(Color.R, Color.G, Color.B, A);
    }

    // --- Background ramp ---------------------------------------------------
    inline const FLinearColor BgVoid = Hex(0x060B12);   // screen field, scrim
    inline const FLinearColor BgBase = Hex(0x0A111C);   // default ground
    inline const FLinearColor BgRaised = Hex(0x0F1826); // zone separation

    // --- Panel ramp --------------------------------------------------------
    inline const FLinearColor Panel00 = Hex(0x131E2E);  // plate face
    inline const FLinearColor Panel10 = Hex(0x18263A);  // cards, rows, slots
    inline const FLinearColor Panel20 = Hex(0x1F3047);  // hover, headers

    // --- Borders -----------------------------------------------------------
    // There is no 2px neutral border in the system: 1px rest, 1px emphasis,
    // 2px only when it carries an accent, 3px only for a rail.
    inline const FLinearColor BorderRest = Hex(0x1F3047);
    inline const FLinearColor BorderEmphasis = Hex(0x2A3E58);

    // --- Text --------------------------------------------------------------
    inline const FLinearColor TextPrimary = Hex(0xE6EDF5);
    inline const FLinearColor TextSecondary = Hex(0x9FB0C4);
    inline const FLinearColor TextMuted = Hex(0x63768C);
    inline const FLinearColor TextDisabled = Hex(0x3E4C5E);

    // --- Function accents --------------------------------------------------
    inline const FLinearColor Cyan = Hex(0x4FD8F5);       // player / system
    inline const FLinearColor Orange = Hex(0xFF8A3D);     // weapon / heat
    inline const FLinearColor OrangeDeep = Hex(0xC25A1E);
    inline const FLinearColor Gold = Hex(0xFFC64A);       // reward / weak point
    inline const FLinearColor GoldDeep = Hex(0xB98212);
    inline const FLinearColor Harm = Hex(0xFF4040);
    inline const FLinearColor HarmDeep = Hex(0xC22A2A);
    inline const FLinearColor Violet = Hex(0xB866FF);     // ultimates
    inline const FLinearColor RiftDamage = Hex(0xA8FBFF); // damage numbers only

    // --- Teal object law ---------------------------------------------------
    // Legal on rift geometry, suppression hardware, and Anomalous items.
    // Never on chrome: buttons, rails, focus rings, tracks, tooltips.
    inline const FLinearColor TealHardware = Hex(0x08B8A8);
    inline const FLinearColor TealAnomalous = Hex(0x26F2D9);

    // --- Rarity ramp -------------------------------------------------------
    inline const FLinearColor RarityStandard = Hex(0xDCE4EE);
    inline const FLinearColor RarityUncommon = Hex(0x408CFF);
    inline const FLinearColor RarityExceptional = Hex(0xB866FF);
    inline const FLinearColor RarityAberrant = Hex(0xFF4040);
    inline const FLinearColor RarityAnomalous = Hex(0x26F2D9);

    inline FLinearColor RarityColor(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return RarityUncommon;
            case EBreakerItemRarity::Exceptional: return RarityExceptional;
            case EBreakerItemRarity::Aberrant:    return RarityAberrant;
            case EBreakerItemRarity::Anomalous:   return RarityAnomalous;
            default:                              return RarityStandard;
        }
    }

    // Anomalous is the one tier that also gets a full 1px border, because it
    // is the only rarity that is also a world object class.
    inline bool RarityGetsFullBorder(EBreakerItemRarity Rarity)
    {
        return Rarity == EBreakerItemRarity::Anomalous;
    }

    // --- Destructive surfaces ---------------------------------------------
    // The one face reserved for irreversible confirmation, from the inventory
    // canvas: a near-black red plate under a harm-red border. It is not part
    // of the panel ramp and must never be used for an ordinary card.
    inline const FLinearColor DestructiveFace = Hex(0x2A1414);

    // --- Comparison glyphs -------------------------------------------------
    // Per-affix deltas on a loadout card (UI-Inventory-Spec "Card anatomy").
    // Cyan up for better, harm-red down for worse, muted equals for parity.
    // The glyphs are tokens because the shipping faces are not imported yet:
    // if the fallback face has no Geometric Shapes coverage this is the single
    // line to change, not a pass over the card builder.
    // This file is UTF-8 without a BOM, matching BreakerMenu.cpp, which
    // already ships non-ASCII inside TEXT() literals.
    // MEASURED, not assumed: the engine's Slate face
    // (Engine/Content/Slate/Fonts/Roboto-*.ttf) carries 878 codepoints and its
    // cmap has NO Geometric Shapes block — U+25B2/U+25BC, the arrows, and every
    // other triangle are absent, so the spec's triangles would have rendered as
    // tofu on the most-scanned line of the screen. ASCII until the shipping
    // faces are imported; colour is what actually carries the meaning here, and
    // the glyph sits alone in its own column, so a sign reads cleanly.
    // Restore the triangles in this one place once Barlow/JetBrains land.
    inline const TCHAR* DeltaBetterGlyph = TEXT("+");
    inline const TCHAR* DeltaWorseGlyph = TEXT("-");
    inline const TCHAR* DeltaParityGlyph = TEXT("=");
    // Fixed column so the affix names form a straight edge whatever the glyph.
    inline constexpr float DeltaGlyphColumn = 14.0f;

    // --- Spacing scale -----------------------------------------------------
    // 4 / 8 / 16 / 24 / 40 / 64. 12 and 32 are not tokens; where the HUD spec
    // names a 12px interior pad it says so itself and uses HudClusterPad.
    inline constexpr float Space4 = 4.0f;
    inline constexpr float Space8 = 8.0f;
    inline constexpr float Space16 = 16.0f;
    inline constexpr float Space24 = 24.0f;
    inline constexpr float Space40 = 40.0f;
    inline constexpr float Space64 = 64.0f;

    inline constexpr float RailThickness = 3.0f;
    inline constexpr float BorderThin = 1.0f;
    inline constexpr float BorderSelected = 2.0f;
    inline constexpr float MinHitTarget = 44.0f;

    // --- Type scale --------------------------------------------------------
    // Pixel sizes from the spec. Slate takes these as point sizes directly;
    // the canvas HUD converts through CanvasTextScale below.
    inline constexpr int32 TypeH1 = 34;
    inline constexpr int32 TypeH2 = 20;
    inline constexpr int32 TypeBody = 14;
    inline constexpr int32 TypeCaption = 11;
    inline constexpr int32 TypeNumberLarge = 56;

    // The engine's small font renders at roughly 12px per unit scale. Every
    // canvas size in the HUD is authored as a spec pixel value and passed
    // through here, so swapping in the real faces later is a one-constant fix
    // rather than a pass over every call site.
    inline constexpr float CanvasFontPixels = 12.0f;
    inline constexpr float CanvasTextScale(float SpecPixels)
    {
        return SpecPixels / CanvasFontPixels;
    }

    // --- HUD geometry (spec pixels at 1920x1080) ---------------------------
    inline constexpr float HudSafeMargin = 48.0f;
    inline constexpr float HudClusterWidth = 440.0f;
    inline constexpr float HudClusterHeight = 184.0f;
    inline constexpr float HudClusterPad = 12.0f;
    inline constexpr float HudClusterRowGap = 10.0f;
    inline constexpr float HudAbilitySquare = 56.0f;
    inline constexpr float HudAbilityGap = 12.0f;
    inline constexpr float HudAbilityRadius = 4.0f;
    inline constexpr float HudVitalsWidth = 420.0f;
    inline constexpr float HudShieldBarHeight = 11.0f;
    inline constexpr float HudHealthBarHeight = 8.0f;
    inline constexpr float HudValueColumnWidth = 84.0f;
    inline constexpr float HudArmorChipWidth = 18.0f;
    inline constexpr float HudArmorChipHeight = 5.0f;
    inline constexpr float HudMomentumTrackHeight = 12.0f;
    inline constexpr float HudEnemyBarWidth = 180.0f;
    inline constexpr float HudEnemyBarHeight = 8.0f;
    inline constexpr float HudCrosshairBox = 80.0f;

    // Damage number sizes, section 4 of the HUD spec.
    inline constexpr float DamageBodyPixels = 40.0f;
    inline constexpr float DamageCritPixels = 80.0f;
    inline constexpr float DamageWeakPointPixels = 64.0f;

    // Ultimate frame, section 5.
    inline constexpr float UltimateFrameInset = 8.0f;
    inline constexpr float UltimateBandHeight = 120.0f;
    inline constexpr float UltimateTitleTop = 132.0f;
    inline constexpr float UltimateTitleWidth = 260.0f;
    inline constexpr float UltimateTitleSeconds = 1.2f;
    inline constexpr float UltimateStepDownSeconds = 3.0f;

    // --- Motion (seconds) --------------------------------------------------
    inline constexpr float MotionDamagePop = 0.04f;
    inline constexpr float MotionDamageRise = 0.52f;
    inline constexpr float MotionCritHold = 0.06f;
    inline constexpr float DamageRisePixels = 40.0f;

    // Thousands take a space, never a comma: at 40px a comma collapses into a
    // dot. The spec asks for U+2009 THIN SPACE; this uses a normal space until
    // the real numeric face is imported. The rule the spec is protecting —
    // never a comma — holds either way.
    // NOTE for anyone tempted to "fix" this from the measurement above: that
    // one is the Slate TTF, which does carry U+2009. This function feeds the
    // CANVAS HUD, which draws with GEngine->GetSmallFont() — a different font
    // asset with its own charset. Verify that asset before changing this.
    inline FString FormatTicker(float Value)
    {
        const int32 Whole = FMath::RoundToInt(Value);
        FString Digits = FString::FromInt(FMath::Abs(Whole));
        for (int32 Index = Digits.Len() - 3; Index > 0; Index -= 3)
        {
            Digits.InsertAt(Index, TEXT(' '));
        }
        return Whole < 0 ? FString(TEXT("-")) + Digits : Digits;
    }
}
