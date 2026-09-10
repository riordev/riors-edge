#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"
// BreakerInventoryLayout::ShouldShowLimitTell — the picker opens on exactly
// the preview the card's limit tell shows, so one predicate answers both.
#include "UI/BreakerMenu.h"
// BreakerStashLayout::SlotWord — the eight slot words, held once where a test
// can read them rather than copied a third time here.
#include "UI/BreakerStashLayout.h"

// ---------------------------------------------------------------------------
// SWAP PICKER LAYOUT — the equip-limit swap picker's numbers and text rules,
// world-free (O205; sheet Assets/design/05-inventory/05-item-and-inventory.html,
// "Equip-limit swap picker").
//
// Nothing here decides a game rule. Which pieces are offered, which one is
// pre-focused and whether a choice is legal are UBreakerEquipmentComponent's
// answers (SwapCandidates / IsValidSwapChoice / EquipItemDisplacing); this
// namespace chooses a width, a label and a sentence, and
// RiorsEdge.UI.EquipLimit.SwapPicker asserts it with no widget, no world and
// no Slate application.
// ---------------------------------------------------------------------------
namespace BreakerSwapPickerLayout
{
    // ---- Geometry, from the sheet's drawing ------------------------------
    // The sheet is the later drawing and wins over spec.md's 960x400 plate
    // with 96px rows. Every dimension is unfelt until the owner has played
    // it.
    // 640 IN THE SHEET, WIDENED TO HOLD THE ITEM'S REAL NAME. O267 gave a
    // rolled item a headline — strongest prefix, base, "of" strongest suffix —
    // and the picker was printing the base alone, so it disagreed with the
    // inventory card about what the player was looking at. At 640 a worst-case
    // headline cannot fit two lines beside the delta column; the arithmetic is
    // NameWrapWidth below and RiorsEdge.UI.SwapPicker.Layout asserts it. This
    // is the width that makes the name fit, not a width chosen by eye.
    inline constexpr float ModalWidth = 800.0f;        // O2 PLACEHOLDER
    // 64 held one kind line over one name line. The name now takes both lines
    // and the kind line is gone from the candidate rows, so the row is two
    // name lines plus its interior. The INCOMING row keeps its kind line —
    // "EQUIPPING" is a verb, not a repeat of the name — so it is one line
    // taller.
    inline constexpr float NameLineHeight = 26.0f;     // O2 PLACEHOLDER — NameSize plus leading
    inline constexpr float RowHeight = 72.0f;          // O2 PLACEHOLDER
    inline constexpr float IncomingRowHeight = 96.0f;  // O2 PLACEHOLDER
    inline constexpr float RowRail = 4.0f;             // O2 PLACEHOLDER — identity rail, left
    inline constexpr float TopRail = 2.0f;             // O2 PLACEHOLDER — status rail, top, rarity colour
    inline constexpr float IconSquare = 40.0f;         // O2 PLACEHOLDER — reserved; no icon atlas exists
    inline constexpr float RowButtonHeight = 32.0f;    // O2 PLACEHOLDER
    inline constexpr float PlatePadding = 24.0f;       // O2 PLACEHOLDER
    inline constexpr float RowPaddingX = 12.0f;        // O2 PLACEHOLDER
    inline constexpr float RowGap = 16.0f;             // O2 PLACEHOLDER
    inline constexpr int32 TitleSize = 32;             // O2 PLACEHOLDER
    inline constexpr int32 KindSize = 13;              // O2 PLACEHOLDER — mono caption
    inline constexpr int32 NameSize = 20;              // O2 PLACEHOLDER — display 600
    inline constexpr int32 LevelSize = 16;             // O2 PLACEHOLDER — mono 500
    inline constexpr int32 RowButtonSize = 16;         // O2 PLACEHOLDER

    // ---- THE NAME COLUMN, SOLVED BEFORE LAYOUT ---------------------------
    // ui.md: nothing reads its own arrangement, so the name wraps at a width
    // this file COMPUTES from the plate — never at an allotted one. Everything
    // in the row except the name is fixed or countable, which is what makes
    // the subtraction honest:
    //
    //   plate           ModalWidth less both PlatePaddings and the rail
    //   icon            IconSquare, then RowPaddingX before the name
    //   marks           one DeltaGlyphColumn per compared affix — COUNTABLE,
    //                   which is the only term that moves, so it is the
    //                   argument rather than a guess
    //   level, verb     the two fixed columns below
    //
    // The row previously printed the slot word ABOVE the name. That line is
    // now redundant — the headline contains the base ("... Body Armour of
    // ...") — so it is gone and the name has both lines.
    inline constexpr float RowLevelColumn = 44.0f;     // O2 PLACEHOLDER — "i100" at LevelSize
    inline constexpr float RowButtonWidth = 96.0f;     // O2 PLACEHOLDER — the widest verb plus its chrome
    inline constexpr float MinNameColumn = 160.0f;     // O2 PLACEHOLDER — below this the name stops being a name
    inline constexpr int32 MaxNameLines = 2;
    // Approximate advance of one glyph of the display face at NameSize, the
    // same estimate-and-wrap trade BreakerInventoryLayout::CaptionAdvance
    // documents: an underestimate costs a wrapped line, never a cut word.
    inline constexpr float NameAdvance = 12.4f;        // O2 PLACEHOLDER

    // The longest headline the grammar can build: two longest affix names, the
    // longest base, and " of ". This is the string the plate has to hold, and
    // it is derived from the affix pool's own cap rather than from a sample.
    inline constexpr int32 LongestBaseChars = 11;      // "BODY ARMOUR"
    inline constexpr int32 WorstCaseNameChars =
        BreakerInventoryLayout::LongestAffixNameChars * 2 + 1 + LongestBaseChars + 4;

    inline float EstimateNameWidth(int32 Characters)
    {
        return static_cast<float>(Characters) * NameAdvance;
    }

    inline float NameWrapWidth(int32 DeltaCount, float PlateWidth = ModalWidth)
    {
        const float Content = PlateWidth - 2.0f * PlatePadding - RowRail;
        const float Marks = static_cast<float>(FMath::Max(DeltaCount, 0)) * BreakerUI::DeltaGlyphColumn;
        const float Right = RowPaddingX * 2.0f + Marks + BreakerUI::Space4 + RowLevelColumn + RowButtonWidth;
        return FMath::Max(Content - IconSquare - RowPaddingX - Right, MinNameColumn);
    }
    // The sheet slides the plate down from its top rail in 160 ms and out
    // along it in 100 ms. RECORDED, NOT BUILT: the modal snaps in and out on
    // frame one exactly as the discard modal does. A slide needs a curve
    // sequence on the overlay slot, and a faked one (a fade, a scale) would be
    // a nearest-fit primitive for a motion the sheet specifies precisely.
    inline constexpr float SlideInMs = 160.0f;         // O2 PLACEHOLDER — not driven
    inline constexpr float SlideOutMs = 100.0f;        // O2 PLACEHOLDER — not driven

    // ---- When it opens ---------------------------------------------------
    // The same predicate as the card's limit tell: the component says the cap
    // is exceeded AND names the piece that leaves. A picker with no pre-focus
    // is worse than no picker, for the same reason a tell with no named
    // victim is worse than no tell.
    inline bool ShouldOpenSwapPicker(const FBreakerEquipPreview& Preview)
    {
        return BreakerInventoryLayout::ShouldShowLimitTell(Preview);
    }

    // ---- Text ------------------------------------------------------------
    // The rarity word as the loadout prints it. O50: the Unwritten ENUMERATOR
    // displays as UNWRITTEN; the enumerator itself never moves.
    inline const TCHAR* RarityWord(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return TEXT("UNCOMMON");
            case EBreakerItemRarity::Exceptional: return TEXT("EXCEPTIONAL");
            case EBreakerItemRarity::Aberrant:    return TEXT("ABERRANT");
            case EBreakerItemRarity::Unwritten:   return TEXT("UNWRITTEN");
            default:                              return TEXT("STANDARD");
        }
    }

    // "ABERRANT · 3 OF 3 WORN". Count and limit are the preview's own
    // RarityCount / RarityLimit — the screen holds no second opinion.
    inline FString Title(EBreakerItemRarity Rarity, int32 Worn, int32 Limit)
    {
        return FString::Printf(TEXT("%s · %d OF %d WORN"), RarityWord(Rarity), Worn, Limit);
    }

    // "INCOMING · WAIST".
    inline FString IncomingLine(EBreakerEquipSlot Slot)
    {
        return FString::Printf(TEXT("INCOMING · %s"), BreakerStashLayout::SlotWord(Slot));
    }

    // The row's verb. SWAP when the row is the piece in the incoming slot,
    // TAKE OFF otherwise (the swap is by rarity, not slot).
    //
    // RECORDED AT THE SITE: under PreviewEquipAgainst the piece in the
    // candidate's own slot is never a cap victim — it leaves with every equip
    // and its departure is credited against the tally first — so SwapCandidates
    // never contains a row whose slot matches the incoming piece, and SWAP
    // cannot arise. Every row reads TAKE OFF in practice; the branch exists so
    // the label follows the rule if the rule ever offers the in-slot piece.
    inline const TCHAR* RowButtonLabel(const FBreakerItemInstance& Row, const FBreakerItemInstance& Incoming)
    {
        return Row.Slot == Incoming.Slot ? TEXT("SWAP") : TEXT("TAKE OFF");
    }

    inline const TCHAR* KeepCurrentLabel() { return TEXT("KEEP CURRENT"); }
}
