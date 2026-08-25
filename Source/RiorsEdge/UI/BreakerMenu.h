#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Items/BreakerItemTypes.h"
// The FIELDPLATE tokens: the layout namespace below picks its delta glyphs
// from the same one place the builder draws them from.
#include "UI/BreakerUIStyle.h"
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
    // RETIRED (owner ruling 2026-08-17). The archetype-picker screen is gone —
    // equipment IS the loadout now. The value stays so nothing below it
    // renumbers (these are logged and mapped from capture strings); no screen
    // builds for it and no button reaches it.
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
    // O100. NOT a peer of the four above, and deliberately absent from
    // BuildScreenTabs: unlocking is an ANCHOR interaction, so its only entry is
    // the quartermaster's dialogue. Adding it to the tab strip is exactly how
    // the Forge became reachable from the pause menu, which content-and-modes
    // rules out and which Game.BootFlow.ShippedConfiguration now asserts
    // against for this screen.
    Quartermaster,
    // The front door. Main is the title root (PLAY / SETTINGS / QUIT) and is
    // gated behind a press-any-key reveal; these two are what PLAY leads to.
    // Peers rather than modes of Main, for the same reason SkillTrees is its
    // own value: each has its own header, its own back target and its own
    // rebuild triggers.
    CharacterSelect,
    CharacterCreate,
    // The Anchor's navigation screen. Owner: "the anchor as the hub and a
    // navigation place to click into and select where youd like to go". A peer
    // of Dialogue rather than a mode of it: both are opened by the F key on a
    // world interactable and both leave by resuming the game, but a travel
    // point is not an NPC and its screen reads a destination list, not a
    // dialogue graph.
    //
    // APPENDED at the end of the enum on purpose. The values are logged by
    // Rebuild and mapped from a capture string in Characters/, so inserting in
    // the middle would renumber every screen above it for no gain.
    Travel,
    // The breakpoint sandbox, and it is DEV TOOLING, said in its own name.
    // Owner: "a way for me as a player/dev to test different breakpoints and
    // strength throughout the progression of the game". It is a peer screen
    // rather than a mode of Settings because everything on it acts on the
    // CHARACTER and the WORLD (level, area level, class, gear), not on the
    // player's preferences — and because it is plumbing to existing dev calls
    // (AwardExperience, DevForceClass, GrantPlaytestPoints, seeded RollItem),
    // never a place where new game rules live. Appended last, same rule as
    // Travel above.
    DevSandbox,
    // The character sheet, opened with C from the field. Owner: "a lot of
    // information is bloated or overwhelming" -- so the numbers a build is
    // actually made of move OFF the HUD and onto a screen the player opens
    // deliberately. A peer rather than a mode of Inventory: it reads the
    // COMPOSED character where Inventory reads the items, and the two answer
    // different questions. Appended last, same rule as Travel and DevSandbox.
    CharacterSheet
};

// ---------------------------------------------------------------------------
// INVENTORY LAYOUT — the loadout screen's arithmetic and its text rules, split
// out of the builder so they can be exercised with no widget, no world and no
// Slate application.
//
// Everything here is a PRESENTATION rule from Assets/ui-reference/
// Inventory.dc.html. Nothing here decides a game rule: which piece a swap
// ejects, whether an affix is better or worse and what a rarity cap is are all
// UBreakerEquipmentComponent's answers, arriving as FBreakerEquipPreview. This
// namespace only chooses a width, a glyph and a sentence.
// ---------------------------------------------------------------------------
namespace BreakerInventoryLayout
{
    // The reference's ZONES section, authored at 1920x1080.
    inline constexpr float SpecPanelWidth = 1920.0f;
    inline constexpr float HeaderHeight = 88.0f;
    inline constexpr float SpecCharacterColumn = 560.0f;
    inline constexpr float SpecEquipmentColumn = 400.0f;
    inline constexpr float SpecBackpackColumn = 960.0f;
    // The render slot's authored box. Height is a MINIMUM here: the column
    // stretches with the plate, and the doll is centred in whatever it gets.
    inline constexpr float RenderSlotHeight = 660.0f;
    inline constexpr float FilterBarHeight = 64.0f;
    // "a 3-across card grid at 16px gaps". THREE IS THE TARGET, NOT THE RULE.
    //
    // The reference was drawn with its own type metrics; ours are wider, and a
    // 300px card could not hold "Physical Damage Reduction" — the affix names
    // truncated mid-word ("Physical Damag+"), which is the sixth report of
    // clipped text in this file. The requirement the owner actually stated is
    // LEGIBILITY; the grid count is a means to it. So the count is solved from
    // the zone against a minimum readable card width, capping at three.
    //
    // It is still arithmetic done BEFORE layout starts, on a zone width that
    // came from the viewport — the same rule that keeps SWrapBox off this
    // screen. Nothing here reads an allotted size.
    inline constexpr int32 MaxBackpackCardsPerRow = 3;
    inline constexpr float BackpackCardGap = 16.0f;
    // Floors. Below these the copy stops fitting rather than merely looking
    // tight, and the backpack is the last zone that should give ground because
    // it is where the cards are.
    inline constexpr float MinBackpackColumn = 640.0f;
    inline constexpr float MinCharacterColumn = 320.0f;
    inline constexpr float MinEquipmentColumn = 280.0f;
    // One equipment row: a 44px icon square (the system's minimum hit target)
    // plus its two stacked text lines and 12px of interior padding.
    inline constexpr float EquipRowHeight = 68.0f;

    struct FColumns
    {
        float Character = SpecCharacterColumn;
        float Equipment = SpecEquipmentColumn;
        float Backpack = SpecBackpackColumn;
    };

    // Spec widths at the authored panel; scaled down TOGETHER once the panel
    // cannot hold them plus a usable backpack, each floored independently.
    inline FColumns SolveColumns(float PanelWidth, float Gutters)
    {
        FColumns Columns;
        const float FixedRoom = PanelWidth - Gutters - MinBackpackColumn;
        const float SpecFixed = SpecCharacterColumn + SpecEquipmentColumn;
        if (FixedRoom < SpecFixed)
        {
            // FLOORED to whole pixels, and that is not cosmetic. Scaling both
            // columns by a ratio makes their sum land a fraction of a pixel
            // ABOVE the room they were given (560+400 at 760/960 sums to
            // 760.00001), so the backpack came out at 639.99999 against its own
            // 640 floor — a real test failure from a rounding error rather than
            // from a layout mistake. Rounding each column down guarantees the
            // sum never exceeds FixedRoom, so the floor below is exact.
            // Whole-pixel column widths are also what Slate wants: a fractional
            // width is where measure-versus-rasterise disagreements start, and
            // this file has six reports of clipped text.
            const float Scale = FMath::Max(0.55f, FixedRoom / SpecFixed);
            Columns.Character = FMath::Max(MinCharacterColumn, FMath::FloorToFloat(SpecCharacterColumn * Scale));
            Columns.Equipment = FMath::Max(MinEquipmentColumn, FMath::FloorToFloat(SpecEquipmentColumn * Scale));
        }
        Columns.Backpack = FMath::Max(320.0f, PanelWidth - Columns.Character - Columns.Equipment - Gutters);
        return Columns;
    }

    // ---- THE READABILITY BUDGET -------------------------------------------
    // Every number below exists so a card can print its affix lines. They are
    // written as a BUDGET rather than as a card width, because that is the
    // direction the requirement runs: the longest stat name decides how wide a
    // card has to be, and the card width decides how many fit.
    //
    // Approximate advance of one glyph at TypeCaption in the engine's Slate
    // face (Roboto Regular, 11px, ~0.63em). An ESTIMATE and deliberately so:
    // the font measure service needs a renderer, and this has to be usable from
    // a headless test. Getting it slightly wrong is safe in both directions —
    // every text block that consumes it also wraps at a known pixel width, so
    // an underestimate costs a second wrapped line, never a truncated word.
    inline constexpr float CaptionAdvance = 6.9f;
    // The longest affix DisplayName in the slice pool, in characters.
    // "Physical Damage Reduction" is 25 today; one spare. Tests/
    // BreakerInventoryLayoutTests.cpp asserts the pool against this, so an
    // affix with a longer name fails a test instead of silently truncating on
    // the most-read line of the screen.
    inline constexpr int32 LongestAffixNameChars = 26;
    inline float EstimateCaptionWidth(int32 Characters)
    {
        return static_cast<float>(Characters) * CaptionAdvance;
    }
    // THE SKILL SCREEN'S CLUSTER CHROME, extracted so the teal object law can be
    // asserted over every input rather than read off one call site. Both used to
    // return suppression teal for the sealed Elements cluster, and a rail and a
    // border are both on the law's forbidden list whatever the panel is about.
    // The cluster states SEALED in words, so nothing was lost by neutralising
    // them. RiorsEdge.UI.Teal.SealedCluster walks the whole 2x2 domain of THIS
    // widget pair, and is named for it. It is NOT the whole-UI law:
    // UI.Teal.ObjectLaw asserts teal appears on no interface element anywhere,
    // and remains unimplemented -- a test covering one pair must not be filed
    // under a name that claims the screen.
    inline FLinearColor SkillClusterRailColour(bool bSealed, bool bHub)
    {
        if (bSealed) return BreakerUI::BorderEmphasis;
        return bHub ? BreakerUI::Cyan : BreakerUI::BorderEmphasis;
    }

    inline FLinearColor SkillClusterBorderColour(bool bSealed)
    {
        return bSealed ? BreakerUI::BorderEmphasis : BreakerUI::BorderRest;
    }

    // THE SEALED LABEL. Teal is a NOUN, never an adjective, and SEALED is an
    // adjective: it describes the panel's state rather than naming a thing that
    // exists in the world. The constellation's NAME is the other case and keeps
    // its colour -- name text is on the permitted list, and ELEMENTS names a
    // class of world object. The two text runs sat one line apart and are not
    // one finding.
    inline FLinearColor SkillClusterSealedLabelColour()
    {
        return BreakerUI::TextSecondary;
    }

    // THE LOADOUT SUBTITLE, and it is a function so that its ABSENCE of an item
    // score can be asserted. It read "BREAKER · SWIFT · LV 1 · GEAR SCORE 1",
    // and art-and-ui says no screen prints an aggregate item score — the rule
    // exists because a single number telling the player which item is better
    // deletes the decision the whole endgame is made of, and because a scalar
    // cannot even be honest across O54's partitioned damage model.
    //
    // Extracted rather than left inline because a string built inside a Slate
    // tree cannot be reached by a test, and a rule with nothing holding it is
    // how this one came to be violated on the most-used screen in the game.
    // RiorsEdge.UI.NoItemScore asserts the shape: the class, the level, and no
    // second number for anything to be an aggregate of.
    inline FString LoadoutMetaLine(const FString& ClassName, int32 CharacterLevel)
    {
        return FString::Printf(TEXT("BREAKER · %s · LV %d"), *ClassName, CharacterLevel);
    }

    // The affix row's right-hand columns: the delta glyph and its magnitude.
    // 48 holds "1234.5", which is the widest value O29's affix ladder rolls.
    inline constexpr float DeltaValueColumn = 48.0f;
    inline constexpr float AffixTailWidth =
        BreakerUI::DeltaGlyphColumn + DeltaValueColumn + BreakerUI::Space4;
    // Everything a card spends before its content starts: the 3px rarity rail,
    // the 1px ring on both sides, and 16px of interior pad on both sides.
    inline constexpr float CardChrome =
        BreakerUI::RailThickness + 2.0f * BreakerUI::BorderThin + 2.0f * BreakerUI::Space16;
    // Line one's right-hand furniture: the item-level column, its gap, and the
    // clearance the discard X needs in the overlay above the card.
    inline constexpr float ItemLevelColumn = 56.0f;
    inline constexpr float DiscardClearance = 22.0f;

    // THE RULE THIS SCREEN IS NOW HELD TO: a card is only worth drawing at a
    // width where the longest stat name fits on one line beside its delta.
    inline constexpr float MinAffixColumnWidth = LongestAffixNameChars * CaptionAdvance;
    inline constexpr float MinReadableCardWidth = MinAffixColumnWidth + AffixTailWidth + CardChrome;

    // How many cards a zone can hold READABLY. Three is the target and the cap;
    // below the readable width it drops to two, then to one. One card that can
    // be read beats three that cannot.
    inline int32 SolveCardsPerRow(float ZoneWidth)
    {
        for (int32 Count = MaxBackpackCardsPerRow; Count > 1; --Count)
        {
            const float Gaps = BackpackCardGap * (Count - 1);
            if ((ZoneWidth - Gaps) / static_cast<float>(Count) >= MinReadableCardWidth) return Count;
        }
        return 1;
    }

    // Width of one backpack card once the count is known. Floored so a card is
    // never negative on an absurd window.
    inline float BackpackCardWidth(float ZoneWidth, int32 CardsPerRow)
    {
        const int32 Count = FMath::Max(1, CardsPerRow);
        const float Gaps = BackpackCardGap * (Count - 1);
        return FMath::Max(140.0f, (ZoneWidth - Gaps) / static_cast<float>(Count));
    }

    // What is left of a card for text.
    inline float CardContentWidth(float CardWidth)
    {
        return FMath::Max(80.0f, CardWidth - CardChrome);
    }

    // Where an affix line wraps. A pixel figure derived from a card width that
    // was itself settled before layout — never an allotted size.
    inline float AffixNameWrapWidth(float CardWidth)
    {
        return FMath::Max(80.0f, CardContentWidth(CardWidth) - AffixTailWidth);
    }

    // Where the item name on line one wraps: clear of the item level AND of the
    // discard X floating over the card's top-right corner. The name collided
    // with both before this existed.
    inline float CardTitleWrapWidth(float CardWidth)
    {
        return FMath::Max(64.0f,
            CardContentWidth(CardWidth) - ItemLevelColumn - BreakerUI::Space8 - DiscardClearance);
    }

    // WEAR ORDER, and it is the reference's PROSE order: "head to foot, then
    // trinkets, then weapons". That deliberately differs from the mockup's own
    // markup, which draws boots before necklace before waist — i.e. it simply
    // walks EBreakerEquipSlot's declaration order and puts a trinket in the
    // middle of the armour run. The prose is the rule; the mockup is one
    // hand-built sample of it. Flagged rather than silently reconciled.
    inline const TArray<EBreakerEquipSlot>& WearOrder()
    {
        static const TArray<EBreakerEquipSlot> Order = {
            EBreakerEquipSlot::Helmet,
            EBreakerEquipSlot::BodyArmour,
            EBreakerEquipSlot::Gloves,
            EBreakerEquipSlot::Waist,
            EBreakerEquipSlot::Boots,
            EBreakerEquipSlot::Necklace,
            EBreakerEquipSlot::Primary,
            EBreakerEquipSlot::Secondary,
        };
        return Order;
    }

    // The filter bar's four chips: ALL / ARMOUR / WEAPONS / TRINKETS. Four
    // CATEGORIES, not nine slots — the reference names these four, and nine
    // slot chips are what forced the bar to wrap into rows in the first place.
    enum class EBackpackFilter : int32
    {
        All = -1,
        Armour = 0,
        Weapons = 1,
        Trinkets = 2,
    };

    inline bool PassesFilter(EBreakerEquipSlot Slot, EBackpackFilter Filter)
    {
        switch (Filter)
        {
            case EBackpackFilter::Armour:
                return Slot == EBreakerEquipSlot::Helmet || Slot == EBreakerEquipSlot::BodyArmour
                    || Slot == EBreakerEquipSlot::Gloves || Slot == EBreakerEquipSlot::Waist
                    || Slot == EBreakerEquipSlot::Boots;
            case EBackpackFilter::Weapons:
                return FBreakerItemInstance::IsWeaponSlot(Slot);
            case EBackpackFilter::Trinkets:
                return Slot == EBreakerEquipSlot::Necklace;
            default:
                return true;
        }
    }

    // The glyph column. The COMPONENT decides better/worse/parity; this only
    // picks the mark, which is why it takes the decision rather than the two
    // values. See BreakerUIStyle.h for why these are ASCII today.
    inline const TCHAR* DeltaGlyph(EBreakerAffixDelta Delta)
    {
        switch (Delta)
        {
            case EBreakerAffixDelta::Better: return BreakerUI::DeltaBetterGlyph;
            case EBreakerAffixDelta::Worse:  return BreakerUI::DeltaWorseGlyph;
            default:                         return BreakerUI::DeltaParityGlyph;
        }
    }

    // "each carrying its delta against the equipped piece". The magnitude is
    // printed unsigned — the glyph beside it already carries the sign, and a
    // "- -40" would read as a typo. Parity prints nothing but the glyph.
    inline FString FormatDelta(const FBreakerAffixComparison& Comparison)
    {
        if (Comparison.Delta == EBreakerAffixDelta::Parity) return FString();
        return FString::Printf(TEXT("%.1f"), FMath::Abs(Comparison.GetDifference()));
    }

    // LIMIT TELLS. A card discloses a cap ejection only when the component
    // says the cap is exceeded AND names the piece that leaves — a tell with
    // no named victim is worse than no tell, because the player cannot check
    // it against the equipment column.
    inline bool ShouldShowLimitTell(const FBreakerEquipPreview& Preview)
    {
        return Preview.bExceedsRarityLimit && Preview.LimitDisplaced.IsValid();
    }

    // The footer's left half: "LIMIT FULL 3/3" when a cap is being spent,
    // otherwise what the ordinary slot swap costs.
    inline FString MakeFooterLead(const FBreakerEquipPreview& Preview)
    {
        if (ShouldShowLimitTell(Preview))
        {
            return FString::Printf(TEXT("LIMIT FULL %d/%d"), Preview.RarityCount, Preview.RarityLimit);
        }
        return Preview.bSlotOccupied ? TEXT("EQUIP · REPLACES") : TEXT("EQUIP · SLOT EMPTY");
    }
}

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
    void ShowCharacterSheet();
    void ShowDialogue(class ABreakerNPC* NPC);
    // The travel picker's front door, in the same shape as ShowDialogue: the
    // caller has already opened the menu (OpenMenu(false)) and hands us the
    // interactable the player walked into. Sets RootScreen to Pause for the
    // same reason ShowDialogue does — this screen is entered from GAMEPLAY, so
    // its "root" is the pause menu, and Escape resumes the game rather than
    // dropping the player onto the title.
    //
    // Safe with any number of destinations, including one and including zero:
    // the screen says what it has. The caller is free to keep travelling
    // directly when there is exactly one — see the note in BuildTravelScreen.
    void ShowTravel(class ABreakerTravelPoint* InTravelPoint);
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
    // The Anchor's navigation screen: one card per available destination.
    TSharedRef<SWidget> BuildTravelScreen();
    // The breakpoint sandbox (see the enum note on DevSandbox). Reached from
    // the pause menu behind an explicit DEV label; also the reachable home of
    // the DEV MODE class-swap checkbox that used to live only on the
    // unreachable class-select screen.
    TSharedRef<SWidget> BuildDevSandboxScreen();
    // Reach: Items/BreakerForgeLibrary.h's three crafting verbs plus salvage,
    // wired to a wallet readout. See the FORGE tab note on EBreakerMenuScreen.
    TSharedRef<SWidget> BuildForgeScreen();
    TSharedRef<SWidget> BuildQuartermasterScreen();
    // The quartermaster's last refusal or confirmation, shown on the screen
    // and cleared by the next rebuild that has something new to say.
    FText QuartermasterStatus;
    // True only while the Forge screen was entered through Kess. The respec
    // gate reads it, so the "only available at a Forge" refusal is reachable
    // instead of being a branch nothing could ever take.
    bool bAtForge = false;
    // Reach: a picker over UBreakerAbilityComponent's selection API
    // (GetSelectableAbilityIds / PreviewSelection / TryEquipAbility), which
    // shipped with zero callers. See the ABILITIES tab note on EBreakerMenuScreen.
    TSharedRef<SWidget> BuildAbilitiesScreen();
    // Shared EQUIPMENT | SKILL TREES | FORGE | ABILITIES tab strip; all four
    // character screens live behind it so the I-key flow reaches any of them
    // in one click.
    TSharedRef<SWidget> BuildScreenTabs(EBreakerMenuScreen ActiveScreen);
    TSharedRef<SWidget> BuildCharacterSheetScreen();
    // Which of the four panels is open. A member rather than a screen value:
    // switching panels must not change the back target.
    int32 CharacterSheetTab = 0;
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
    // Which settings pane the sidebar has open: 0 INPUT, 1 KEYBINDS, 2 VIDEO,
    // 3 AUDIO. A member rather than a screen value for the same reason
    // CharacterSheetTab is: switching panes must not change the back target.
    int32 SettingsPane = 0;
    // One keybind row: label, THE KEY CONTROL, conflict badge, DEFAULT.
    // Four columns, not five. The separate BIND button is gone — owner: "this
    // should show the current bind you click and replace it this is ugly" — so
    // the key display IS the control and the row has one thing to click.
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
    // cleanup and O37's COMMIT control already use. Discharging a character is
    // the most destructive button in the game and it is not getting a bare
    // single click.
    FGuid PendingDeleteCharacterId;
    // The roster legend's verbs, factored so the DISCHARGE button, the DEL
    // key and ENTER/second-click DEPLOY all run the same code. Deploy is a
    // no-op without a valid selection; discharge arms on the first call for a
    // character and commits on the second.
    void DeploySelectedCharacter();
    void ArmOrConfirmDischarge(const FGuid& CharacterId);
    // Create-screen state.
    EBreakerClassId PendingCreateClass = EBreakerClassId::None;
    FText PendingCreateName;
    FText CharacterScreenStatus;
    TSharedPtr<SBox> ContentHost;
    EBreakerMenuScreen CurrentScreen = EBreakerMenuScreen::Main;
    // ALL / ARMOUR / WEAPONS / TRINKETS, the reference's four filter chips.
    BreakerInventoryLayout::EBackpackFilter BackpackFilter =
        BreakerInventoryLayout::EBackpackFilter::All;
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
    // ---- Breakpoint sandbox state ----------------------------------------
    // The level the slider is parked on. 0 means "not moved yet", which reads
    // as the character's current level — so opening the screen and pressing
    // SET LEVEL is a no-op rather than a surprise jump to 1.
    int32 DevTargetLevel = 0;
    // The seed field's text, stored WITHOUT rebuilding on each keystroke for
    // the same reason the create screen's name field does it: a rebuild would
    // destroy the text box mid-word and take focus with it. Empty means "roll
    // me a fresh seed".
    FString DevSeedString;
    // Rarity and slot the next GRANT uses. Slot -1 means "draw the slot from
    // the seed through RollDropSlot", i.e. exactly what a real kill does.
    EBreakerItemRarity DevGrantRarity = EBreakerItemRarity::Exceptional;
    int32 DevGrantSlot = -1;
    // Item level the next GRANT rolls at (O48: the sandbox is THE testing
    // route for chase items now that the rarity gates are frozen, and affix
    // tiers run past any reachable area level — T1 opens at ilvl 120). 0 means
    // "not moved yet", which reads as the old behaviour: the gym's area level,
    // or the character's level outside a gym world.
    int32 DevGrantItemLevel = 0;
    // Result line echoed under the sandbox controls; cleared on leaving the
    // screen, same rule as TravelStatus.
    FText DevSandboxStatus;
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
    // Travel screen state. Weak for the same reason DialogueNPC is: the actor
    // is owned by the world and can be destroyed while the menu is up, and a
    // screen holding a dangling interactable is worse than a screen that
    // notices it is gone and leaves.
    TWeakObjectPtr<class ABreakerTravelPoint> TravelPoint;
    // Which destination card draws the selected ring. Preset by ShowTravel to
    // the first available destination so the screen never opens with nothing
    // marked, and moved by a click before the travel attempt — so a refused
    // travel (a destination that went away between the build and the click)
    // leaves the player looking at the row they actually chose.
    FName SelectedTravelDestinationId = NAME_None;
    // Echoed under the destination list. Only ever populated by a REFUSED
    // travel; a successful one closes the menu.
    FText TravelStatus;
    EBreakerMenuScreen RootScreen = EBreakerMenuScreen::Main;
};
