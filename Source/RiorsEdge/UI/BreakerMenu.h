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
    TSharedRef<SWidget> BuildMainScreen();
    TSharedRef<SWidget> BuildPauseScreen();
    TSharedRef<SWidget> BuildSettingsScreen();
    TSharedRef<SWidget> BuildLoadoutScreen();
    TSharedRef<SWidget> BuildInventoryScreen();
    TSharedRef<SWidget> BuildClassSelectScreen();
    TSharedRef<SWidget> BuildSkillTreesScreen();
    TSharedRef<SWidget> BuildDialogueScreen();
    TSharedRef<SWidget> BuildFrame(const FText& Title, const FText& Subtitle, const TSharedRef<SWidget>& Body, float PanelWidth = 720.0f) const;
    TSharedRef<SWidget> MakeButton(const FText& Label, const FOnClicked& OnClicked, bool bPrimary = false) const;
    TSharedRef<SWidget> MakeGearCard(const FText& Slot, const FText& Name, const FText& Details, const FLinearColor& Accent) const;
    FReply GoBack();

    TWeakObjectPtr<ABreakerCharacter> Character;
    TSharedPtr<SBox> ContentHost;
    EBreakerMenuScreen CurrentScreen = EBreakerMenuScreen::Main;
    // -1 shows every slot; otherwise an EBreakerEquipSlot index.
    int32 BackpackSlotFilter = -1;
    // Skill trees: which tree the left selector has focused, and the last
    // purchase/respec message echoed under the node grid.
    int32 SelectedTreeIndex = 0;
    FText SkillTreeStatus;
    TWeakObjectPtr<class ABreakerNPC> DialogueNPC;
    FName DialogueNodeId = NAME_None;
    EBreakerMenuScreen RootScreen = EBreakerMenuScreen::Main;
};
