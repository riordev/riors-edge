#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABreakerCharacter;
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
    TSharedRef<SWidget> BuildZonedFrame(const FText& Title, const FText& Meta, const TSharedRef<SWidget>& HeaderRight,
        const TSharedRef<SWidget>& Body, const TSharedRef<SWidget>& Footer, float PanelWidth) const;
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
    // Result line echoed under the cleanup row after a discard.
    FText InventoryStatus;
    // Skill trees: which tree the left selector has focused, and the last
    // purchase/respec message echoed under the node grid.
    int32 SelectedTreeIndex = 0;
    FText SkillTreeStatus;
    // Skill matrix board tab: 0 = Class (the path board), 1 = Core (the
    // constellation map). One tab pair, not a mode toggle — the header and
    // the detail rail persist across the swap.
    int32 SkillBoardTab = 0;
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
