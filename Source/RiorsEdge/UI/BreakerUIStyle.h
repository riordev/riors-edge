#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// The design system's tokens — the one place UI values live.
//
// Transcribed from Assets/design/01-tokens/spec.md (palette, borders, rails,
// grid) and Assets/design/02-hud/spec.md (the combat HUD's geometry). Both the
// canvas HUD (ABreakerPlaytestHUD) and the Slate front end (SBreakerMenu) read
// these tokens, which is the whole point: a colour that exists twice drifts.
//
// The design canvas authors colour in sRGB hex, so every token converts once
// through FromSRGBColor. Do NOT hand-author linear values here — they will not
// match the canvas, and the canvas is the authority.
//
// Geometry marked `// 02-hud` is the HUD sheet's own number; `// O2
// PLACEHOLDER` is a guess the owner has not yet felt.
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

    // --- Background ramp (olive, hue 118) ----------------------------------
    inline const FLinearColor BgVoid = Hex(0x0E1103);   // bg-0: screen field, scrim
    inline const FLinearColor BgBase = Hex(0x151808);   // bg-1: default ground
    inline const FLinearColor BgRaised = Hex(0x1D2010); // bg-2: zone separation

    // --- Panel ramp --------------------------------------------------------
    inline const FLinearColor Panel00 = Hex(0x262918);  // plate face
    inline const FLinearColor Panel10 = Hex(0x2F3221);  // cards, rows, slots
    inline const FLinearColor Panel20 = Hex(0x393D2B);  // hover, headers, drains

    // --- Borders -----------------------------------------------------------
    // Three weights of neutral border: low at rest, mid for emphasis, high for
    // focus. Thickness is a separate axis (1px rest, 2px only when it carries
    // an accent or focus; rails are 4px identity / 2px status).
    inline const FLinearColor BorderRest = Hex(0x454840);      // border-low
    inline const FLinearColor BorderEmphasis = Hex(0x5C6056); // border-mid
    inline const FLinearColor BorderHigh = Hex(0x8A8F84);     // border-high

    // --- Text --------------------------------------------------------------
    inline const FLinearColor TextPrimary = Hex(0xEDEBE3);   // text-1
    inline const FLinearColor TextSecondary = Hex(0xB8B6AC); // text-2
    inline const FLinearColor TextMuted = Hex(0x8E8D80);     // text-3
    inline const FLinearColor TextDisabled = Hex(0x5C5C52);  // text-4

    // --- Function accents --------------------------------------------------
    // SYSTEM is bone: the player's own readouts, chrome, the crosshair, the
    // kill mark. It is deliberately not a hue, so every hue below can mean one
    // verb and nothing else.
    inline const FLinearColor System = Hex(0xE3DFD2);
    inline const FLinearColor SystemDim = Hex(0x6E6C62);
    // Cyan is the MOVEMENT VERB (O179: movement and cleansing), and only that.
    // `Cyan` remains as a name for the movement verb so the menu's chrome
    // keeps compiling; those chrome reads move to System when the menu's front
    // end moves to System. A new site that wants "the system accent" takes
    // System; a new site that wants "movement" takes VerbMove. Neither takes
    // Cyan.
    inline const FLinearColor VerbMove = Hex(0x6FC3E8);
    inline const FLinearColor Cyan = VerbMove;
    inline const FLinearColor Orange = Hex(0xE8842B);     // weapon / heat
    inline const FLinearColor OrangeDeep = Hex(0x6B3E14);
    inline const FLinearColor Gold = Hex(0xE6B33A);       // reward / weak point
    inline const FLinearColor GoldDeep = Hex(0x6A5118);
    inline const FLinearColor Harm = Hex(0xD9402F);
    inline const FLinearColor HarmDeep = Hex(0x5E1F18);
    inline const FLinearColor Violet = Hex(0xA98BEA);     // ultimates
    inline const FLinearColor VioletDim = Hex(0x463367);
    inline const FLinearColor RiftDamage = Hex(0x35E8FF); // damage numbers only (rift-hot)

    // --- The shield is blue (O269) ------------------------------------------
    // Owner: "the health should be a proper read and shield a light blue or
    // something". The combined bar put both pools on one track and told them
    // apart by bone against text-2 — two greys a hairline apart, which is the
    // same defect the four unlabelled rails had, one level down.
    //
    // IT IS NOT A VERB AND DOES NOT BREAK O179. Every HUE means one player
    // action; a pool is not an action, and the two pools a player HAS are the
    // one place the HUD has to say "these are different things" without a word
    // to spare. Health keeps the bone every readout of the player's own state
    // wears; the shield takes a blue that is not the movement cyan (0x6FC3E8 is
    // a cyan, this is a blue) and not the reserved teal band.
    inline const FLinearColor VitalShield = Hex(0x6E9BE0);
    inline const FLinearColor VitalShieldDeep = Hex(0x23324F);

    // --- Teal object law ---------------------------------------------------
    // Legal on rift geometry, suppression hardware, and Unwritten items.
    // Never on chrome: buttons, rails, focus rings, tracks, tooltips.
    inline const FLinearColor TealHardware = Hex(0x1E8F7C);
    inline const FLinearColor TealUnwritten = Hex(0x2FBFA6);
    inline const FLinearColor TealName = Hex(0x6ADFC9);   // top-rarity name text

    // TEAL IS A NOUN, NEVER AN ADJECTIVE, and this is the predicate a teal
    // assertion is written against. It does not make the LAW asserted: a
    // predicate is not coverage, and UI.Teal.ObjectLaw -- teal on no interface
    // element anywhere -- has no test. Its only caller today is
    // UI.Teal.SealedCluster, which walks one widget pair. The reserved band is
    // permitted on rift geometry, suppression hardware, and the top rarity's
    // frames, beams and name text; it is forbidden on buttons, borders, rails,
    // focus rings, tab underlines, progress tracks and tooltips.
    //
    // The distinction is what the pixel DESCRIBES, not what the panel is about.
    // The skill screen's Elements cluster painted its rail and border teal on
    // the reasoning that a rift is a world object — but the rail is chrome
    // describing a panel, which is the adjectival use the law exists to forbid.
    // The cluster already says SEALED in words, so the colour was carrying
    // nothing the screen did not already state.
    inline bool IsReservedTeal(const FLinearColor& Colour)
    {
        return Colour.Equals(TealHardware, 0.001f)
            || Colour.Equals(TealUnwritten, 0.001f)
            || Colour.Equals(TealName, 0.001f);
    }

    // --- Rarity ramp -------------------------------------------------------
    inline const FLinearColor RarityStandard = Hex(0xB8B6AC);
    inline const FLinearColor RarityUncommon = Hex(0x8FB865);
    inline const FLinearColor RarityExceptional = Hex(0x8B8FEC);
    inline const FLinearColor RarityAberrant = Hex(0xE07AAE);
    // Identical to TealUnwritten by construction: the top rarity IS the teal
    // noun, and tests assert the two agree.
    inline const FLinearColor RarityUnwritten = Hex(0x2FBFA6);

    inline FLinearColor RarityColor(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return RarityUncommon;
            case EBreakerItemRarity::Exceptional: return RarityExceptional;
            case EBreakerItemRarity::Aberrant:    return RarityAberrant;
            case EBreakerItemRarity::Unwritten:   return RarityUnwritten;
            default:                              return RarityStandard;
        }
    }

    // Unwritten is the one tier that also gets a full 1px border, because it
    // is the only rarity that is also a world object class.
    inline bool RarityGetsFullBorder(EBreakerItemRarity Rarity)
    {
        return Rarity == EBreakerItemRarity::Unwritten;
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
    // 4 / 8 / 12 / 16 / 24 / 40 / 64, the token sheet's grid.
    inline constexpr float Space4 = 4.0f;
    inline constexpr float Space8 = 8.0f;
    inline constexpr float Space12 = 12.0f;
    inline constexpr float Space16 = 16.0f;
    inline constexpr float Space24 = 24.0f;
    inline constexpr float Space40 = 40.0f;
    inline constexpr float Space64 = 64.0f;

    // The menu's plate rail. The front end still draws 3px; it moves to the
    // 4/2 pair below when the menu split lands.
    inline constexpr float RailThickness = 3.0f;
    // Identity rail 4px left, status rail 2px top (01-tokens).
    inline constexpr float HudRailIdentity = 4.0f;   // 01-tokens
    inline constexpr float HudRailStatus = 2.0f;     // 01-tokens
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

    // --- HUD geometry (spec pixels at 1920x1080, origin top-left) ----------
    // Positions never change between states; only content does (02-hud).
    inline constexpr float HudSafeMargin = 40.0f;             // 02-hud gutter

    // Vitals — bottom-left, 480 wide at (40, 936).
    inline constexpr float HudVitalsLeft = 40.0f;             // 02-hud
    inline constexpr float HudVitalsTop = 936.0f;             // 02-hud
    inline constexpr float HudVitalsWidth = 480.0f;           // 02-hud
    inline constexpr float HudVitalsValuePixels = 32.0f;      // 02-hud
    inline constexpr float HudVitalsMaxPixels = 13.0f;        // 02-hud
    inline constexpr float HudVitalsMaxGap = 8.0f;            // 02-hud
    inline constexpr float HudShieldTop = 976.0f;             // 02-hud
    inline constexpr float HudShieldHeight = 6.0f;            // 02-hud
    inline constexpr float HudHealthTop = 990.0f;             // 02-hud
    inline constexpr float HudHealthHeight = 16.0f;           // 02-hud
    // THE XP RAIL, under the health bar (owner, playtest 2026-09-10: "i cant
    // see my xp"). Thinner than the shield rail because it is the slowest bar
    // on the screen and must never compete with a vital: it is a thing you
    // glance at between fights, not during one. O2 PLACEHOLDER.
    // BELOW the resource track (1010 + 8 = 1018), not on it. 1012 put the rail
    // exactly inside the resource bar and the capture showed them stacked.
    // The design canvas is 1080 with a 40 px safe gutter, so 1026 is the last
    // row that clears both the track above and the margin below.
    inline constexpr float HudXpTop = 1026.0f;                // O2 PLACEHOLDER
    inline constexpr float HudXpHeight = 4.0f;                // O2 PLACEHOLDER
    inline constexpr float HudXpLevelPixels = 12.0f;          // O2 PLACEHOLDER
    inline constexpr float HudResourceTop = 1010.0f;          // 02-hud
    inline constexpr float HudResourceHeight = 8.0f;          // 02-hud
    inline constexpr float HudResourceNotchFraction = 0.70f;  // 02-hud
    inline constexpr float HudResourceNotchHeight = 14.0f;    // 02-hud
    inline constexpr float HudResourceMarkWidth = 12.0f;      // 02-hud
    inline constexpr float HudResourceMarkHeight = 8.0f;      // 02-hud
    inline constexpr float HudResourceBankedCell = 8.0f;      // 02-hud
    // The health chip: fill drains at once, the hatched chip holds, then
    // recovers linearly. Hatch harm-dim/harm-full at 4/2.
    inline constexpr float HudHealthChipHoldSeconds = 0.400f;    // 02-hud
    inline constexpr float HudHealthChipRecoverSeconds = 0.600f; // 02-hud
    inline constexpr float HudHealthChipHatchPeriod = 6.0f;      // 02-hud (4 + 2)
    inline constexpr float HudHealthChipHatchStripe = 4.0f;      // 02-hud
    // Under this fraction the value goes harm, the bar carries its tick, and
    // the near-death frame shows. One number, three reads.
    inline constexpr float HudHealthLowFraction = 0.20f;      // 02-hud

    // Near-death frame — full screen, border pulsing 8→16→8 over 1.6 s;
    // corner brackets 64×64, 4px L-shapes at 24px from each corner.
    inline constexpr float HudNearDeathFrameMin = 8.0f;       // 02-hud
    inline constexpr float HudNearDeathFrameMax = 16.0f;      // 02-hud
    inline constexpr float HudNearDeathPulseSeconds = 1.6f;   // 02-hud
    inline constexpr float HudNearDeathBracketSize = 64.0f;   // 02-hud
    inline constexpr float HudNearDeathBracketStroke = 4.0f;  // 02-hud
    inline constexpr float HudNearDeathBracketInset = 24.0f;  // 02-hud

    // Abilities — bottom-centre, 232 wide at (844, 936), bottoms on y = 1024.
    inline constexpr float HudAbilityTile = 64.0f;            // 02-hud
    inline constexpr float HudUltimateTile = 88.0f;           // 02-hud
    inline constexpr float HudAbilityOneX = 844.0f;           // 02-hud
    inline constexpr float HudUltimateX = 916.0f;             // 02-hud
    inline constexpr float HudAbilityTwoX = 1012.0f;          // 02-hud
    inline constexpr float HudAbilityBottom = 1024.0f;        // 02-hud
    inline constexpr float HudAbilityMark = 16.0f;            // 02-hud
    inline constexpr float HudUltimateMark = 24.0f;           // 02-hud
    inline constexpr float HudAbilityKeyPixels = 13.0f;       // 02-hud
    inline constexpr float HudAbilityKeyInset = 6.0f;         // 02-hud
    inline constexpr float HudUltimateKeyInsetX = 8.0f;       // 02-hud
    inline constexpr float HudUltimateKeyInsetY = 10.0f;      // 02-hud

    // Weapon — bottom-right, right edge x = 1880.
    inline constexpr float HudWeaponRight = 1880.0f;          // 02-hud
    inline constexpr float HudMagazineTop = 944.0f;           // 02-hud
    inline constexpr float HudReserveGap = 12.0f;             // 02-hud
    inline constexpr float HudAmmoRailX = 1640.0f;            // 02-hud
    inline constexpr float HudAmmoRailY = 1000.0f;            // 02-hud
    inline constexpr float HudAmmoRailWidth = 240.0f;         // 02-hud
    inline constexpr float HudAmmoRailHeight = 4.0f;          // 02-hud
    inline constexpr float HudMagazineLowFraction = 0.25f;    // 02-hud
    inline constexpr float HudWeaponNamePixels = 20.0f;       // 02-hud
    inline constexpr float HudWeaponSwapSeconds = 1.2f;       // 02-hud
    inline constexpr float HudWeaponSwapSlidePixels = 16.0f;  // 02-hud
    // Magazine and reserve sizes BELOW the sheet's 48/24 on the owner's
    // direct feedback after seeing them rendered (O207: play sizes stand).
    inline constexpr float HudMagazinePixels = 32.0f;         // O207, sheet 48
    inline constexpr float HudReservePixels = 15.0f;          // O207, sheet 24

    // Crosshair — 40×40 box, four ticks 2×12, gap 16; spread opens the gap to
    // 40 (60 ms out, 200 ms back); ADS collapses the ticks (80 ms) to a 2×2
    // dot inside a 24px ring.
    inline constexpr float HudCrosshairBox = 40.0f;               // 02-hud
    inline constexpr float HudCrosshairTickLength = 12.0f;        // 02-hud
    inline constexpr float HudCrosshairTickWidth = 2.0f;          // 02-hud
    inline constexpr float HudCrosshairGapRest = 16.0f;           // 02-hud
    inline constexpr float HudCrosshairGapSpread = 40.0f;         // 02-hud
    inline constexpr float HudCrosshairSpreadOutSeconds = 0.06f;  // 02-hud
    inline constexpr float HudCrosshairSpreadBackSeconds = 0.20f; // 02-hud
    inline constexpr float HudCrosshairAdsSeconds = 0.08f;        // 02-hud
    inline constexpr float HudCrosshairAdsDot = 2.0f;             // 02-hud
    inline constexpr float HudCrosshairAdsRing = 24.0f;           // 02-hud
    // The cone half-angle at which the gap reads fully open. The sheet says
    // "with movement/fire" and names no angle; this is the number that maps
    // the weapon's honest spread onto the 16→40 travel.
    inline constexpr float HudCrosshairFullSpreadDegrees = 6.0f;  // O2 PLACEHOLDER

    // Crosshair marks: hit 80 ms, kill 200 ms, weak-point kill 320 ms.
    inline constexpr float HudHitMarkSeconds = 0.08f;         // 02-hud
    inline constexpr float HudKillMarkSeconds = 0.20f;        // 02-hud
    inline constexpr float HudWeakPointMarkSeconds = 0.32f;   // 02-hud
    inline constexpr float HudHitDiagonal = 12.0f;            // 02-hud
    inline constexpr float HudWeakPointDiagonal = 16.0f;      // 02-hud
    inline constexpr float HudKillSquare = 6.0f;              // 02-hud
    inline constexpr float HudWeakPointDiamond = 8.0f;        // 02-hud

    // Periphery: zone name at (40, 40) display 24; countdown numeric 16 on the
    // line beneath; quest line right edge 1880, top 40, max 320 wide, body 14.
    // How far a travel point's label sits ABOVE its world anchor, in screen
    // pixels. The anchor is 260 cm up, which is generous at the door and
    // negligible at range, so without this the name lands on the crosshair
    // from across a yard. O2 PLACEHOLDER.
    inline constexpr float HudTravelLabelLiftPixels = 64.0f;
    // Past this the door keeps its NAME and drops its "AREA n" line: a
    // qualifier is for a door you are walking to. O2 PLACEHOLDER.
    inline constexpr float HudTravelDetailCm = 2500.0f;
    inline constexpr float HudZoneLeft = 40.0f;               // 02-hud
    inline constexpr float HudZoneTop = 40.0f;                // 02-hud
    inline constexpr float HudZonePixels = 24.0f;             // 02-hud
    inline constexpr float HudCountdownTop = 76.0f;           // 02-hud (the wave-cell row)
    inline constexpr float HudCountdownPixels = 16.0f;        // 02-hud
    inline constexpr float HudQuestTrackerWidth = 320.0f;     // 02-hud
    inline constexpr float HudQuestLineTop = 40.0f;           // 02-hud
    inline constexpr float HudQuestLinePixels = 14.0f;        // 02-hud

    // The hatch: flat 135° stripes, A for 6px then B for 2px (period 8).
    inline constexpr float HudHatchPeriod = 8.0f;             // 01-tokens
    inline constexpr float HudHatchStripe = 6.0f;             // 01-tokens

    // The effect column above the vitals: one dot and one value per line.
    inline constexpr float HudV2StatusDot = 9.0f;             // O2 PLACEHOLDER
    inline constexpr float HudV2StatusRowGap = 5.0f;          // O2 PLACEHOLDER
    inline constexpr float HudV2StatusPixels = 12.0f;         // O2 PLACEHOLDER

    inline constexpr float HudEnemyBarWidth = 180.0f;
    inline constexpr float HudEnemyBarHeight = 8.0f;

    // Damage number sizes.
    // The sheet's sizes were authored for a 1920x1080 mock viewed at desk
    // distance. In the game they cover the target you are shooting at, which
    // is worse than illegible — the owner called them too big on sight, twice,
    // and O207 rules the play measurement stands: 26 body, 52 crit. The
    // relative sizes are what carry the information, so the HIERARCHY is kept
    // and the width problem is solved by FormatDamage below, not by a third cut.
    // 26/52/40 -> 18/34/26 (owner, second playtest: "number font needs to go
    // down a little ... look at how destiny does damage numbers and replicate
    // something similar"). Destiny's numbers are small, thin and read as
    // confirmation rather than as an event; O207's play measurement is
    // superseded by a second play measurement, which is the only thing that
    // can supersede it.
    //
    // THE HIERARCHY IS PRESERVED EXACTLY. The ratios are unchanged to within a
    // rounding — body 1.0, weak 1.5, crit 1.9 — because the relative sizes are
    // what carry the information and shrinking them unevenly would delete the
    // separation between a body shot, a weak point and a crit.
    inline constexpr float DamageBodyPixels = 18.0f;      // O2 PLACEHOLDER
    inline constexpr float DamageCritPixels = 34.0f;      // O2 PLACEHOLDER
    inline constexpr float DamageWeakPointPixels = 26.0f; // O2 PLACEHOLDER
    // DoT ticks sit BELOW the body size: they are bookkeeping, not an event,
    // and at body size a three-target Bleed drowned the gunfire it rode over.
    inline constexpr float DamageDoTPixels = 13.0f;    // O2 PLACEHOLDER

    // HOW FAR A NUMBER IS WORTH PRINTING. Owner: "only used when in effective
    // ranges". Past this a number is a smear of pixels over a target too small
    // to read it against, and a yard is 106 m long, so a fight at the far
    // pocket used to spray unreadable glyphs across the middle distance.
    // The DAMAGE is unaffected — this is what is drawn, not what is dealt.
    inline constexpr float DamageMaxDrawDistanceCm = 4200.0f;   // O2 PLACEHOLDER
    // Killing blows multiply whatever size their kind already earned: a kill
    // is the heaviest read of its own family, never a fourth colour.
    inline constexpr float DamageKillScale = 1.25f;    // O2 PLACEHOLDER

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
    // The damage-number timeline (02-hud): pop 60 ms, settle 120 ms, rise
    // 24px over 700 ms, fade over the last 300 ms; a crit holds 400 ms longer.
    inline constexpr float MotionDamagePop = 0.06f;      // 02-hud
    inline constexpr float MotionDamageSettle = 0.12f;   // 02-hud
    inline constexpr float MotionDamageRise = 0.70f;     // 02-hud
    inline constexpr float MotionDamageFade = 0.30f;     // 02-hud
    inline constexpr float MotionCritHold = 0.40f;       // 02-hud (extra lifetime)
    inline constexpr float DamageRisePixels = 24.0f;     // 02-hud

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

    // --- The vitals row (O199) ----------------------------------------------
    // Health current in the large numeric with the health MAX after it in the
    // small numeric; shield current in the small numeric, drawn only when a
    // shield pool exists. A character with no shield pool at all draws no
    // shield — no track, no number — so the row never shows a dead "0" beside
    // a bar that can never fill. Positions are fixed either way, so the health
    // number does not move when a shield is gained or lost.
    struct FVitalsRow
    {
        FString ShieldText;
        bool bDrawShield = false;
        FString HealthText;
        FString MaxText;
    };

    inline FVitalsRow FormatVitalsRow(float Shield, float MaxShield, float Health, float MaxHealth)
    {
        FVitalsRow Row;
        Row.bDrawShield = MaxShield > 0.0f;
        Row.ShieldText = Row.bDrawShield ? FormatTicker(Shield) : FString();
        Row.HealthText = FormatTicker(Health);
        Row.MaxText = FormatTicker(MaxHealth);
        return Row;
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
