#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Items/BreakerItemTypes.h"
#include "Progression/BreakerProgressionTypes.h"
#include "UObject/StrongObjectPtr.h"
// Complete type: the roster is held by value in a TStrongObjectPtr below.
#include "Save/BreakerCharacterRoster.h"
// Complete type: the settings model is held the same way, for the same reason.
#include "Settings/BreakerGameSettings.h"

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
    Dialogue,
    // Reach items (Docs/Design/Decisions.md O37/O39/O40c): built systems that
    // had no UI path. Both are peers of Inventory/SkillTrees in the shared
    // EQUIPMENT | SKILL TREES | FORGE | ABILITIES tab strip (BuildScreenTabs),
    // not sub-modes of one screen — same reason SkillTrees is its own value
    // rather than a tab flag on Inventory.
    Forge,
    Abilities,
    // The front door. Main is the title root (PLAY / SETTINGS / QUIT) and is
    // gated behind a press-any-key reveal; these two are what PLAY leads to.
    // Peers rather than modes of Main, for the same reason SkillTrees is its
    // own value: each has its own header, its own back target and its own
    // rebuild triggers.
    CharacterSelect,
    CharacterCreate
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
    // Enter/Space, routed from the input component rather than from Slate
    // focus - see ABreakerCharacter::ConfirmMenuKey for why.
    void HandleConfirmKey();
    // THE REBIND CAPTURE SEAM, and it is deliberately public and unused inside
    // this class.
    //
    // The settings screen's "press a key" flow captures through Slate
    // (OnPreviewKeyDown / OnPreviewMouseButtonDown below), which is sound for
    // the case that actually occurs — the player CLICKED a row one event
    // earlier, so Slate focus is on a descendant of this widget and preview
    // runs on the way down. But this project has already been bitten twice by
    // assuming Slate focus: under FInputModeGameAndUI
    // (Characters/BreakerCharacter.cpp:1050-1054) the menu does not reliably
    // hold it, which is why the title gate had to move onto a player-input
    // BindKey with bExecuteWhenPaused (BreakerCharacter.cpp:344-352).
    //
    // If capture turns out not to fire, this is the entry point that fixes it
    // without touching this file: one line next to the Enter/Space binds,
    //   PlayerInputComponent->BindKey(EKeys::AnyKey, IE_Pressed, this,
    //       &ThisClass::MenuRebindKey).bExecuteWhenPaused = true;
    // forwarding the pressed key here. Harmless to call at any time: it does
    // nothing unless a row is actively listening.
    void HandleRebindKey(const FKey& Key);
    // Dev capture only: jump straight to a screen so a screenshot run can see
    // it. Every menu in this project has been authored, reworked and shipped
    // without anyone looking at it, which is how the skill tree reached the
    // owner clipping its own numbers.
    // A screen is not one picture. The skill matrix alone has a Class board,
    // a Core board and a COMPARE ALL view, and only the first was ever
    // reachable from the harness — so the other two shipped unlooked-at for
    // the same reason everything else did. `-BreakerCaptureBoard=CORE` or
    // `=COMPARE` selects the sub-view; absent, nothing changes.
    void ShowScreenForCapture(EBreakerMenuScreen Screen);

private:
    void Rebuild(EBreakerMenuScreen NewScreen);
    void ApplyScreen(EBreakerMenuScreen NewScreen);
    TSharedRef<SWidget> BuildMainScreen();
    TSharedRef<SWidget> BuildPauseScreen();
    TSharedRef<SWidget> BuildSettingsScreen();
    TSharedRef<SWidget> BuildLoadoutScreen();
    TSharedRef<SWidget> BuildInventoryScreen();
    TSharedRef<SWidget> BuildClassSelectScreen();
    TSharedRef<SWidget> BuildCharacterSelectScreen();
    TSharedRef<SWidget> BuildCharacterCreateScreen();
    // One row of the roster, and one class tile of the create carousel. Split
    // out because both are loops whose bodies would otherwise bury the screen
    // they belong to.
    TSharedRef<SWidget> MakeCharacterRow(const struct FBreakerCharacterSummary& Summary, bool bSelected);
    TSharedRef<SWidget> MakeClassTile(EBreakerClassId ClassId, bool bSelected);
    // The greyed silhouette a class tile draws instead of a model. Owner's
    // call: unimplemented classes are shown as silhouettes rather than hidden,
    // so the roster of what the game intends to be is legible from the start
    // while O39 still refuses to let anyone lock into one.
    TSharedRef<SWidget> MakeClassSilhouette(EBreakerClassId ClassId, bool bImplemented, float Scale = 1.0f, bool bShowCaption = true) const;
    // The narrow selector on the LEFT of the create screen. Named a banner
    // rather than a tile because it carries identity, not detail — the detail
    // panel on the right is what reads.
    TSharedRef<SWidget> MakeClassBanner(EBreakerClassId ClassId, bool bSelected);
    void EnsureRosterLoaded();
    TSharedRef<SWidget> BuildSkillTreesScreen();
    TSharedRef<SWidget> BuildDialogueScreen();
    // Reach: Items/BreakerForgeLibrary.h's three crafting verbs plus salvage,
    // wired to a wallet readout. See the FORGE tab note on EBreakerMenuScreen.
    TSharedRef<SWidget> BuildForgeScreen();
    // Reach: a picker over UBreakerAbilityComponent's selection API
    // (GetSelectableAbilityIds / PreviewSelection / TryEquipAbility), which
    // shipped with zero callers. See the ABILITIES tab note on EBreakerMenuScreen.
    TSharedRef<SWidget> BuildAbilitiesScreen();
    // Shared EQUIPMENT | SKILL TREES | FORGE | ABILITIES tab strip; all four
    // character screens live behind it so the I-key flow reaches any of them
    // in one click.
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
        const TSharedRef<SWidget>& Body, const TSharedRef<SWidget>& Footer, float PanelWidth, float PanelHeight = 1000.0f,
        bool bFillHeight = false) const;
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

    // The title gate. The owner asked for the game to open on the main menu
    // "until you hit enter" — so the root screen opens as an attract plate and
    // the PLAY / SETTINGS / QUIT column only appears once a key lands. Slate
    // keyboard focus is already set on this widget by ABreakerCharacter's
    // FInputModeUIOnly (BreakerCharacter.cpp:1009), so OnKeyDown genuinely
    // reaches us rather than being swallowed by the game viewport.
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    // Mouse capture for the rebind flow. "Rebindable to whatever" includes the
    // two keys the game is most played with — Fire is LMB and Aim is RMB — and
    // a mouse button never arrives as a key event, so a keyboard-only listener
    // could not rebind either of them. Preview, so the click is consumed before
    // whatever button is under the cursor treats it as a press.
    virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent) override;

    // ---- Settings screen -------------------------------------------------
    // The screen's section builders. Split out because the four sections have
    // nothing to do with each other and one function carrying all of them
    // would be the longest in the file.
    TSharedRef<SWidget> BuildSettingsInputSection();
    TSharedRef<SWidget> BuildSettingsKeybindSection();
    TSharedRef<SWidget> BuildSettingsVideoSection();
    TSharedRef<SWidget> BuildSettingsAudioSection();
    // One keybind row: label, resolved key, conflict badge, BIND, DEFAULT.
    TSharedRef<SWidget> MakeKeybindRow(FName Action, const TMap<FName, TArray<FKey>>& DefaultKeys,
        const TMap<FName, FKey>& FlatDefaults);
    // Loads the model on first use and caches the project's default keybinds
    // alongside it. Both are needed by every settings rebuild, and the
    // defaults involve a synchronous asset load that must not happen per row.
    void EnsureSettingsLoaded();
    void BeginKeybindListen(FName Action);
    void CancelKeybindListen();
    // The whole rebind decision: conflict check, the arm/confirm step, the
    // write, the save. Shared by the keyboard and mouse capture paths and by
    // HandleRebindKey, so all three behave identically.
    void CommitKeybind(const FKey& Key);

    TWeakObjectPtr<ABreakerCharacter> Character;
    // False until the title is dismissed. Deliberately NOT persisted: the
    // attract plate is the front door of a session, not a one-time tutorial.
    bool bTitleRevealed = false;
    // The roster is a UObject held by a Slate widget, so it needs an explicit
    // strong reference — a raw pointer here would be collected out from under
    // the character-select screen between rebuilds.
    TStrongObjectPtr<UBreakerCharacterRoster> Roster;
    // The settings MODEL. A UObject held by a Slate widget, so it needs the
    // same explicit strong reference the roster does. Loaded lazily on the
    // first settings rebuild rather than in Construct: most sessions never
    // open the screen, and LoadOrDefaults touches GConfig.
    TStrongObjectPtr<UBreakerGameSettings> GameSettings;
    // The project's default keybinds, resolved once per settings-screen entry
    // from DA_PlayerInputConfig's mapping context. Every row and every
    // conflict check reads this; re-resolving it per row would mean a
    // synchronous asset load per row.
    TMap<FName, TArray<FKey>> DefaultKeybinds;
    // NAME_None unless a row is in its "PRESS A KEY…" state. Exactly one row
    // can listen at a time, which is why this is a name and not a set.
    FName ListeningKeybindAction;
    // The arm half of the conflict arm/confirm, the same two-click shape the
    // inventory cleanup and O37's COMMIT already use. A rebind that clashes
    // does not silently steal the key and does not silently refuse: the first
    // press names the clash and parks it here, and only a second, deliberate
    // click on the same row commits it.
    FName PendingKeybindAction;
    FKey PendingKeybindKey;
    // The line echoed under the keybind list. Harm-red when it is reporting a
    // clash, muted otherwise.
    FText KeybindStatus;
    bool bKeybindStatusIsClash = false;
    FGuid SelectedCharacterId;
    // Two-step delete, the same arm/confirm shape the inventory's destructive
    // cleanup and O37's COMMIT control already use. Deleting a character is
    // the most destructive button in the game and it is not getting a bare
    // single click.
    FGuid PendingDeleteCharacterId;
    // Create-screen state.
    EBreakerClassId PendingCreateClass = EBreakerClassId::None;
    FText PendingCreateName;
    FText CharacterScreenStatus;
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
    // O37 commit control: first click arms, second confirms. Cleared on any
    // other strip interaction so a stray click can never commit permanently.
    FName PendingCommitBranch;
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
    // The skill board's view transform, held here so a purchase — which
    // rebuilds the whole screen — does not throw the player back to the
    // top-left of the board they were reading. Written from the board
    // viewport's OnViewChanged (an input event, never a tick), and reset
    // deliberately when the board itself changes, because a pan that made
    // sense on one branch means nothing on another.
    // Zero means "not chosen yet" — the board opens on the zoom that fits it,
    // which is not 1:1 for COMPARE ALL.
    float SkillBoardZoom = 0.0f;
    FVector2D SkillBoardPan = FVector2D::ZeroVector;
    void HandleBoardViewChanged(float NewZoom, FVector2D NewPan);
    void ResetBoardView();
    // The fixed 420px hover-detail rail. Node hover handlers swap its content
    // through SetContent; it is never driven by a per-frame attribute, and it
    // never changes width, so the board cannot reflow when it populates.
    TSharedPtr<SBox> SkillDetailHost;
    // Which node the detail rail is showing. Survives the screen rebuild that a
    // purchase triggers; the card itself is rebuilt from live data, never
    // restored as a stale widget.
    FName SkillDetailNodeId = NAME_None;
    // Which Core constellation is opened to its node list. NAME_None is the
    // seven-plate map. Survives a purchase rebuild, so buying inside an
    // expanded constellation does not throw the player back to the map.
    FName SkillExpandedConstellation = NAME_None;
    EBreakerMenuScreen PendingScreen = EBreakerMenuScreen::Main;
    bool bRebuildScheduled = false;
    // Forge tab: which held item (equipped or backpack, found by id in either
    // container on every rebuild) the three verbs and salvage act on, and the
    // result line echoed under them. Invalid/consumed ids just resolve to "no
    // selection" on the next rebuild rather than needing an explicit clear.
    FGuid ForgeSelectedItemId;
    FText ForgeStatus;
    // Abilities tab: result line echoed under the slot that was last clicked,
    // so a refusal (e.g. a Caster's "That ability has not been unlocked.")
    // stays readable after the rebuild it triggers.
    FText AbilityStatus;
    TWeakObjectPtr<class ABreakerNPC> DialogueNPC;
    FName DialogueNodeId = NAME_None;
    EBreakerMenuScreen RootScreen = EBreakerMenuScreen::Main;
};
