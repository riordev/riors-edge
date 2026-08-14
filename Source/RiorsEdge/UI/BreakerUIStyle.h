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
    // --- Combat cluster type sizes ----------------------------------------
    // Tokens rather than literals at the call site, because these three are
    // the sizes the owner has actually asked to move and the next request
    // should be a one-line edit in one place.
    //
    // All three sit BELOW the HUD spec's authored figures (17 / 44 / 18), on
    // the owner's direct feedback after seeing them rendered: the state word
    // and the magazine were "a little too big and disjointed on both ends".
    // The spec's numbers were authored on a design canvas and never looked at
    // in-engine; these were.
    inline constexpr float HudResourceStatePixels = 13.0f;   // spec 17
    inline constexpr float HudMagazinePixels = 32.0f;        // spec 44
    inline constexpr float HudReservePixels = 15.0f;         // spec 18

    inline constexpr float HudCrosshairBox = 80.0f;

    // --- Minimap (UI-HUD-Spec section 6) -----------------------------------
    // LANDSCAPE, and that is the whole design decision. Level-Design section 5
    // gives the field a 25000 cm long axis against roughly 8000 cm of occupied
    // width, with every station strung along the spawn-forward axis. A square
    // minimap over that field spends most of its area on empty flank, so the
    // plate is 320x176 — a 1.82:1 window whose long side is the field's long
    // side, and the map is FIELD-ALIGNED (forward = right) rather than
    // rotating, because a rotating map throws that alignment away every time
    // the player turns.
    //
    // Both dimensions are on the 8px grid (40 and 22 cells).
    inline constexpr float HudMinimapWidth = 320.0f;
    inline constexpr float HudMinimapHeight = 176.0f;
    // Centimetres of world per SPEC pixel. 56 puts the half-window at 8960 cm
    // forward and 4928 cm lateral, which is chosen so the encounter pocket
    // (8500 cm out) is on the map from the safe ring rather than one pixel off
    // its edge, and so one 8px grid step is 448 cm — near enough a combat
    // pocket's quarter-radius to be a usable unit.
    inline constexpr float HudMinimapCmPerPixel = 56.0f;
    // Graticule pitch, in world cm. The combat pocket radius, so the grid is a
    // statement about fighting distance rather than an arbitrary ruler.
    inline constexpr float HudMinimapGridCm = 2000.0f;
    inline constexpr float HudMinimapBlipSize = 5.0f;
    inline constexpr float HudMinimapPlayerSize = 9.0f;

    // Damage number sizes, section 4 of the HUD spec.
    // The spec's 40/64/80 were authored for a 1920x1080 mock viewed at desk
    // distance. In the game they cover the target you are shooting at, which
    // is worse than illegible — the owner called them too big on sight. Cut
    // ~35% while keeping the crit/weak-point/body HIERARCHY intact, since the
    // relative sizes are what carry the information.
    //
    // HELD HERE, deliberately, on the SECOND "too large" report (2026-08-14).
    // These three are the only thing that separates a body shot from a weak
    // point from a crit at a glance, and they had already been cut once. What
    // changed between the two reports was not the type — it was O29, which put
    // item level at 120 and roughly doubled affix values, so a number that was
    // three digits when these sizes were chosen is now five or six. A six-digit
    // number carrying two THIN SPACES is eight glyphs wide; at the crit size
    // that is most of the target. The problem is WIDTH, and width is a
    // formatting question, so the fix is FormatDamage below rather than a third
    // cut that would eventually collapse the hierarchy into one size.
    inline constexpr float DamageBodyPixels = 26.0f;   // SPEC: 40
    inline constexpr float DamageCritPixels = 52.0f;   // SPEC: 80
    inline constexpr float DamageWeakPointPixels = 40.0f; // SPEC: 64

    // How much of a hit has to disappear into mitigation before the number
    // says so. Below this it is ordinary armour shaving and saying so every
    // frame would be noise; at or above it the player is hitting the wrong
    // part of something and needs to be told, which is the whole Warden read.
    inline constexpr float DamageAbsorbedThreshold = 0.20f;

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
    // dot. U+2009 THIN SPACE, as the spec asks: the canvas HUD now draws
    // through Slate's font path rather than the engine's bitmap small font,
    // and the Slate Roboto face carries U+2009 (measured from its cmap — see
    // the delta-glyph note above, where the same measurement is why the
    // triangles could NOT stay).
    // The headless fallback in DrawSpecText still uses the small font, whose
    // charset is unverified; nothing is on screen in that path.
    inline FString FormatTicker(float Value)
    {
        const int32 Whole = FMath::RoundToInt(Value);
        FString Digits = FString::FromInt(FMath::Abs(Whole));
        for (int32 Index = Digits.Len() - 3; Index > 0; Index -= 3)
        {
            Digits.InsertAt(Index, TCHAR(0x2009));  // THIN SPACE
        }
        return Whole < 0 ? FString(TEXT("-")) + Digits : Digits;
    }

    // --- Damage numbers abbreviate; every other ticker does not -------------
    // FormatTicker is right for a readout in a FIXED column — the magazine, a
    // health pool, a reserve — where the space is reserved whatever the value
    // does and the player may want the exact figure. A floating damage number
    // is the opposite: it is drawn over the thing being shot, its width is
    // whatever the value makes it, and nobody reads the units digit of a crit.
    //
    // Under O29 the values run to six digits, and FormatTicker's thin spaces
    // make that EIGHT glyphs — the correct answer for a column and the wrong
    // one for a number sitting on a target's chest. Abbreviation holds every
    // damage number to at most five glyphs at any magnitude, which is what the
    // authored sizes were chosen against.
    //
    // The banding keeps the precision where it is legible: three significant
    // figures up to 100k, then whole units, so "12.4k" and "148k" are both
    // four-to-five glyphs and neither lies about its magnitude.
    inline constexpr float DamageAbbreviateAt = 10000.0f;

    inline FString FormatDamage(float Value)
    {
        const float Magnitude = FMath::Abs(Value);
        if (Magnitude < DamageAbbreviateAt) return FormatTicker(Value);

        const TCHAR* Sign = Value < 0.0f ? TEXT("-") : TEXT("");
        if (Magnitude < 100000.0f)                                  // 12.4k
            return FString::Printf(TEXT("%s%.1fk"), Sign, Magnitude / 1000.0f);
        if (Magnitude < 1000000.0f)                                 // 148k
            return FString::Printf(TEXT("%s%.0fk"), Sign, Magnitude / 1000.0f);
        if (Magnitude < 10000000.0f)                                // 1.24M
            return FString::Printf(TEXT("%s%.2fM"), Sign, Magnitude / 1000000.0f);
        if (Magnitude < 100000000.0f)                               // 12.4M
            return FString::Printf(TEXT("%s%.1fM"), Sign, Magnitude / 1000000.0f);
        return FString::Printf(TEXT("%s%.0fM"), Sign, Magnitude / 1000000.0f);
    }
}
