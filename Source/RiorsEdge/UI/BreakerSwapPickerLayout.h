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
    inline constexpr float ModalWidth = 640.0f;        // O2 PLACEHOLDER
    inline constexpr float RowHeight = 64.0f;          // O2 PLACEHOLDER
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
    // The rarity word as the loadout prints it. O50: the Anomalous ENUMERATOR
    // displays as UNWRITTEN; the enumerator itself never moves.
    inline const TCHAR* RarityWord(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return TEXT("UNCOMMON");
            case EBreakerItemRarity::Exceptional: return TEXT("EXCEPTIONAL");
            case EBreakerItemRarity::Aberrant:    return TEXT("ABERRANT");
            case EBreakerItemRarity::Anomalous:   return TEXT("UNWRITTEN");
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
