#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRequirements.h"
#include "Items/BreakerItemTypes.h"
#include "Save/BreakerAccountSave.h"
// BreakerInventoryLayout::WearOrder — the stash grid sorts by the same slot
// order the equipment column walks, so the two screens agree on what "head to
// foot, then trinkets, then weapons" means.
#include "UI/BreakerMenu.h"

// ---------------------------------------------------------------------------
// STASH LAYOUT — the stash screen's arithmetic and its text rules, world-free.
//
// Everything here is a PRESENTATION rule over the account's stash
// (Save/BreakerAccountSave.h) and the character's backpack
// (Items/BreakerEquipmentComponent.h). Nothing here decides a game rule: whether
// a deposit or a withdrawal is legal is UBreakerEquipmentComponent's answer,
// gated inside DepositToStash / WithdrawFromStash. This namespace chooses a
// grid, a label and a sentence, and RiorsEdge.UI.Stash.Screen asserts it with
// no widget, no world and no Slate application.
// ---------------------------------------------------------------------------
namespace BreakerStashLayout
{
    // ---- THE DESIGN'S NUMBERS, NOT O2 PLACEHOLDERS ---------------------------
    // The grid is pinned to the caps: 10 x 7 IS StashCapacity (70) and 5 x 5 IS
    // BackpackCapacity (25), so every held item has a cell and no cell can
    // exist without a slot behind it. Change a cap and the grid must change
    // with it; the test below holds the products equal. Cell and gap sizes and
    // the two origins are the design canvas at 1920x1080.
    inline constexpr int32 StashColumns = 10;
    inline constexpr int32 StashRows = 7;
    inline constexpr float StashCell = 112.0f;
    inline constexpr float StashGap = 4.0f;
    inline constexpr int32 BackpackColumns = 5;
    inline constexpr int32 BackpackRows = 5;
    inline constexpr float BackpackCellW = 120.0f;
    inline constexpr float BackpackCellH = 96.0f;
    // Origins as two floats each: FVector2D is not a literal type, and these
    // feed a constexpr width below.
    inline constexpr float StashOriginX = 40.0f;
    inline constexpr float StashOriginY = 160.0f;
    inline constexpr float BackpackOriginX = 1240.0f;
    inline constexpr float BackpackOriginY = 160.0f;
    inline constexpr float HeaderHeight = 88.0f;

    static_assert(StashColumns * StashRows == UBreakerAccountSave::StashCapacity,
        "The stash grid is the stash cap drawn; change both or neither.");
    static_assert(BackpackColumns * BackpackRows == UBreakerEquipmentComponent::BackpackCapacity,
        "The backpack grid is the backpack cap drawn; change both or neither.");

    // Derived widths, so the screen builder never multiplies at a call site.
    inline constexpr float StashGridWidth = StashColumns * StashCell + (StashColumns - 1) * StashGap;
    inline constexpr float StashGridHeight = StashRows * StashCell + (StashRows - 1) * StashGap;
    inline constexpr float BackpackGridWidth = BackpackColumns * BackpackCellW + (BackpackColumns - 1) * StashGap;
    inline constexpr float BackpackGridHeight = BackpackRows * BackpackCellH + (BackpackRows - 1) * StashGap;
    // The design's body: left margin, stash grid, gutter to the backpack
    // origin, backpack grid, right margin equal to the left.
    inline constexpr float DesignBodyWidth = BackpackOriginX + BackpackGridWidth + StashOriginX;
    // The gutter the design leaves between the two grids.
    inline constexpr float DesignGutter = BackpackOriginX - (StashOriginX + StashGridWidth);

    // The cell's interior: a 48px icon square (no icon atlas exists — the
    // square is reserved, and empty, at the builder), a five-cell rarity
    // tally, and the slot word. The legendary mark is a Gold diamond drawn as
    // geometry, never a glyph.
    inline constexpr float IconSquare = 48.0f;
    inline constexpr int32 RarityTallyCells = 5;
    inline constexpr float RarityTallyCellW = 12.0f;
    inline constexpr float RarityTallyCellH = 4.0f;
    inline constexpr float LegendaryDiamond = 10.0f;

    // ---- The tab strip ----------------------------------------------------
    // Three tabs and NO MATERIALS: no material item exists, and a tab over an
    // empty category is content authored for plumbing that does not exist.
    enum class EStashTab : int32
    {
        All = 0,
        Weapons = 1,
        Armour = 2,
    };

    inline const TArray<FString>& TabLabels()
    {
        static const TArray<FString> Labels = { TEXT("ALL"), TEXT("WEAPONS"), TEXT("ARMOUR") };
        return Labels;
    }

    inline EStashTab TabFromIndex(int32 Index)
    {
        switch (Index)
        {
            case 1:  return EStashTab::Weapons;
            case 2:  return EStashTab::Armour;
            default: return EStashTab::All;
        }
    }

    // WEAPONS is exactly what the item model calls a weapon slot; ARMOUR is
    // everything else. That puts a Necklace under ARMOUR — the stash has no
    // TRINKETS tab (three tabs, from the design), and a necklace is not a gun.
    // The two predicates partition the eight slots; the test walks every slot
    // and asserts exactly one of them answers.
    inline bool PassesTab(EBreakerEquipSlot Slot, EStashTab Tab)
    {
        switch (Tab)
        {
            case EStashTab::Weapons: return FBreakerItemInstance::IsWeaponSlot(Slot);
            case EStashTab::Armour:  return !FBreakerItemInstance::IsWeaponSlot(Slot);
            default:                 return true;
        }
    }

    // ---- The counters -----------------------------------------------------
    inline FString StashCounter(int32 Count)
    {
        return FString::Printf(TEXT("%d / %d"), Count, UBreakerAccountSave::StashCapacity);
    }

    inline FString BackpackCounter(int32 Count)
    {
        return FString::Printf(TEXT("%d / %d"), Count, UBreakerEquipmentComponent::BackpackCapacity);
    }

    // The header's meta line: both counters, labelled, so the screen states
    // both caps before the player clicks anything.
    inline FString MetaLine(int32 StashCount, int32 BackpackCount)
    {
        return FString::Printf(TEXT("STASH %s · BACKPACK %s"), *StashCounter(StashCount), *BackpackCounter(BackpackCount));
    }

    // ---- What the stash grid shows ----------------------------------------
    // The stash MINUS every claim-marked guid: a withdrawal leaves its copy in
    // StashItems until a restore proves it reached a character save, and that
    // copy is locked — WithdrawFromStash refuses it — so drawing it would draw
    // a cell the player cannot act on. Sorted by WearOrder on slot ONLY
    // (stable, so two helmets keep their stash order); rarity and level do not
    // reorder the grid, because a grid that reshuffles on every deposit is a
    // grid the player cannot learn.
    inline int32 WearOrderIndex(EBreakerEquipSlot Slot)
    {
        const int32 Index = BreakerInventoryLayout::WearOrder().IndexOfByKey(Slot);
        return Index == INDEX_NONE ? BreakerInventoryLayout::WearOrder().Num() : Index;
    }

    inline TArray<FBreakerItemInstance> VisibleStashItems(const UBreakerAccountSave& Account)
    {
        TArray<FBreakerItemInstance> Visible;
        for (const FBreakerItemInstance& Item : Account.StashItems)
        {
            if (!Account.PendingWithdrawals.Contains(Item.ItemId)) Visible.Add(Item);
        }
        Visible.StableSort([](const FBreakerItemInstance& A, const FBreakerItemInstance& B)
        {
            return WearOrderIndex(A.Slot) < WearOrderIndex(B.Slot);
        });
        return Visible;
    }

    // TAKE TO BACKPACK is painted disabled at the cap. The same predicate the
    // component refuses on (Backpack.Num() >= BackpackCapacity), read here so
    // the button can say so BEFORE the click rather than after.
    inline bool CanTakeToBackpack(int32 BackpackCount)
    {
        return BackpackCount < UBreakerEquipmentComponent::BackpackCapacity;
    }

    inline bool CanMoveToStash(int32 StashCount)
    {
        return StashCount < UBreakerAccountSave::StashCapacity;
    }

    // ---- The refusal lines ------------------------------------------------
    // DepositToStash / WithdrawFromStash return a bool and write their reason
    // to the log. The screen owes the player the reason, so it re-reads the
    // same predicates the component gates on, in the component's own order,
    // and prints the first that fails. Empty means the screen expects the
    // call to pass. GAP, recorded here: the component does not return a
    // reason enum, so a refusal the screen did not predict (an authority
    // failure, an unreadable account) prints the generic line below. The fix
    // is a result enum on the component, LEDGER's file, not a second copy of
    // its rules here.
    inline const TCHAR* GenericRefusal() { return TEXT("THE STASH REFUSED THAT."); }

    inline FString DepositRefusal(bool bAtAnchor, int32 StashCount)
    {
        if (!bAtAnchor) return TEXT("THE STASH IS IN THE ANCHOR.");
        if (!CanMoveToStash(StashCount)) return FString::Printf(TEXT("THE STASH IS FULL AT %d."), UBreakerAccountSave::StashCapacity);
        return FString();
    }

    inline FString WithdrawRefusal(bool bAtAnchor, bool bClaimMarked, int32 ItemLevel, int32 CharacterLevel, int32 BackpackCount)
    {
        if (!bAtAnchor) return TEXT("THE STASH IS IN THE ANCHOR.");
        if (bClaimMarked) return TEXT("THAT ITEM IS STILL ON ITS WAY OUT.");
        if (!BreakerItemRequirements::CanEquipAtLevel(ItemLevel, CharacterLevel))
        {
            return FString::Printf(TEXT("REQUIRES LEVEL %d."), BreakerItemRequirements::RequiredLevelFor(ItemLevel));
        }
        if (!CanTakeToBackpack(BackpackCount))
        {
            return FString::Printf(TEXT("THE BACKPACK IS FULL AT %d."), UBreakerEquipmentComponent::BackpackCapacity);
        }
        return FString();
    }

    // The slot word on a cell. The loadout's own SlotName is file-local to
    // BreakerMenu.cpp, so the stash carries the same eight words here where a
    // test can hold them.
    inline const TCHAR* SlotWord(EBreakerEquipSlot Slot)
    {
        switch (Slot)
        {
            case EBreakerEquipSlot::Helmet:     return TEXT("HELMET");
            case EBreakerEquipSlot::BodyArmour: return TEXT("BODY ARMOUR");
            case EBreakerEquipSlot::Gloves:     return TEXT("GLOVES");
            case EBreakerEquipSlot::Boots:      return TEXT("BOOTS");
            case EBreakerEquipSlot::Necklace:   return TEXT("NECKLACE");
            case EBreakerEquipSlot::Waist:      return TEXT("WAIST");
            case EBreakerEquipSlot::Primary:    return TEXT("PRIMARY");
            case EBreakerEquipSlot::Secondary:  return TEXT("SECONDARY");
            default:                            return TEXT("SLOT");
        }
    }

    // How many of the five tally cells a rarity fills: Standard one, Anomalous
    // five. The enum's declared order is the ladder (append-only, so this
    // holds).
    inline int32 RarityTallyFilled(EBreakerItemRarity Rarity)
    {
        return FMath::Clamp(static_cast<int32>(Rarity) + 1, 1, RarityTallyCells);
    }
}
