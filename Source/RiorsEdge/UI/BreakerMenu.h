#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Items/BreakerItemTypes.h"

class ABreakerCharacter;
class SBorder;
class SBox;

enum class EBreakerMenuScreen : uint8
{
    Main,
    Pause,
    Settings,
    Loadout,
    Inventory,
    ClassSelect,
    SkillTrees,
    Dialogue
};

class RIORSEDGE_API SBreakerMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBreakerMenu) {}
        SLATE_ARGUMENT(ABreakerCharacter*, Character)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void ShowMainMenu();
    void ShowPauseMenu();
    void ShowInventory();
    void ShowDialogue(class ABreakerNPC* NPC);
    void HandleEscape();
    // Dev capture only: jump straight to a screen so a screenshot run can see
    // it. Every menu in this project has been authored, reworked and shipped
    // without anyone looking at it, which is how the skill tree reached the
    // owner clipping its own numbers.
    void ShowScreenForCapture(EBreakerMenuScreen Screen) { Rebuild(Screen); }

private:
    void Rebuild(EBreakerMenuScreen NewScreen);
    void ApplyScreen(EBreakerMenuScreen NewScreen);
    TSharedRef<SWidget> BuildMainScreen();
    TSharedRef<SWidget> BuildPauseScreen();
    TSharedRef<SWidget> BuildSettingsScreen();
    TSharedRef<SWidget> BuildLoadoutScreen();
    TSharedRef<SWidget> BuildInventoryScreen();
    TSharedRef<SWidget> BuildClassSelectScreen();
    TSharedRef<SWidget> BuildSkillTreesScreen();
    TSharedRef<SWidget> BuildDialogueScreen();
    // Shared EQUIPMENT | SKILL TREES tab strip; both character screens live
    // behind it so the I-key flow reaches trees in one click.
    TSharedRef<SWidget> BuildScreenTabs(EBreakerMenuScreen ActiveScreen);
    TSharedRef<SWidget> BuildFrame(const FText& Title, const FText& Subtitle, const TSharedRef<SWidget>& Body, float PanelWidth = 720.0f) const;
    // Zoned screen shell for the two wide screens (Loadout / Skill matrix):
    // an 88px header band at bg/raised carrying the title, the meta line and
    // the screen's own controls, the body beneath it, and an optional footer.
    // BuildFrame's centred plate is kept for the narrow screens.
    // PanelHeight caps the plate the same way PanelWidth caps its width; the
    // skill matrix derives both from the viewport so the screen cannot run off
    // the edge of a window smaller than the authored 1920x1080 canvas.
    TSharedRef<SWidget> BuildZonedFrame(const FText& Title, const FText& Meta, const TSharedRef<SWidget>& HeaderRight,
        const TSharedRef<SWidget>& Body, const TSharedRef<SWidget>& Footer, float PanelWidth, float PanelHeight = 1000.0f) const;
    // Event-driven limit tell: paints (or clears) the harm-red outline on the
    // equipment-column row a hovered backpack card would eject. Called from
    // OnHovered/OnUnhovered only — never from a tick or a paint attribute.
    void SetEquipSlotOutline(EBreakerEquipSlot Slot, bool bDoomed);
    // The bulk-discard confirmation modal: count, exclusions, destructive
    // label. Returns the scrim plus the plate, meant to sit in an SOverlay
    // above the whole screen.
    TSharedRef<SWidget> BuildDiscardModal(int32 ArmIndex, EBreakerItemRarity MinimumKept, int32 Count);
    TSharedRef<SWidget> MakeButton(const FText& Label, const FOnClicked& OnClicked, bool bPrimary = false) const;
    TSharedRef<SWidget> MakeGearCard(const FText& Slot, const FText& Name, const FText& Details, const FLinearColor& Accent) const;
    FReply GoBack();

    TWeakObjectPtr<ABreakerCharacter> Character;
    TSharedPtr<SBox> ContentHost;
    EBreakerMenuScreen CurrentScreen = EBreakerMenuScreen::Main;
    // -1 shows every slot; otherwise an EBreakerEquipSlot index.
    int32 BackpackSlotFilter = -1;
    // Two-click arm for the destructive cleanup buttons. A click sets
    // PendingCleanupArm and rebuilds; Rebuild() moves it into
    // CleanupArmedIndex and clears the pending value, so any other
    // interaction on the screen disarms it on the next rebuild.
    // -1 none, 0 = discard below Uncommon, 1 = discard below Exceptional.
    int32 CleanupArmedIndex = -1;
    int32 PendingCleanupArm = -1;
    // Second click on an armed cleanup chip opens the confirmation modal
    // instead of committing. -1 none, otherwise the arm index, which is what
    // the modal's Destroy button acts on. Unlike CleanupArmedIndex this is
    // sticky across rebuilds: a modal that vanished when the screen refreshed
    // would be worse than no modal.
    int32 DiscardModalIndex = -1;
    // Result line echoed under the cleanup row after a discard.
    FText InventoryStatus;
    // Hover disclosure for the equip-limit tell. A backpack card whose equip
    // would eject an equipped piece outlines that piece here on OnHovered and
    // restores it on OnUnhovered. Weak, because the widget tree owns these and
    // is rebuilt out from under the map on every screen change; imperative,
    // because a per-frame attribute driving widget state is the exact pattern
    // that produced the historical screen jitter.
    TMap<EBreakerEquipSlot, TWeakPtr<SBorder>> EquipSlotOutlines;
    // Skill trees: which tree the left selector has focused, and the last
    // purchase/respec message echoed under the node grid.
    int32 SelectedTreeIndex = 0;
    FText SkillTreeStatus;
    // Skill matrix board tab: 0 = Class (the path board), 1 = Core (the
    // constellation map). One tab pair, not a mode toggle — the header and
    // the detail rail persist across the swap.
    int32 SkillBoardTab = 0;
    // Which class BRANCH the path board draws: an index into the screen's
    // class-branch list, or -1 for the side-by-side compare view.
    //
    // This is a VIEW selection, not a commitment. Nothing in the data model
    // records a chosen subclass — FBreakerProgressionState has no branch field
    // and UBreakerClassDefinition::BranchTrees is a flat list with no notion of
    // one being selected — so the screen lets the player browse and compare
    // branches, and says plainly that it is browsing. See the "Subclass
    // selection" note in BuildSkillTreesScreen for what a real commitment
    // would need.
    int32 SkillBranchIndex = 0;
    // The fixed 420px hover-detail rail. Node hover handlers swap its content
    // through SetContent; it is never driven by a per-frame attribute, and it
    // never changes width, so the board cannot reflow when it populates.
    TSharedPtr<SBox> SkillDetailHost;
    EBreakerMenuScreen PendingScreen = EBreakerMenuScreen::Main;
    bool bRebuildScheduled = false;
    TWeakObjectPtr<class ABreakerNPC> DialogueNPC;
    FName DialogueNodeId = NAME_None;
    EBreakerMenuScreen RootScreen = EBreakerMenuScreen::Main;
};
