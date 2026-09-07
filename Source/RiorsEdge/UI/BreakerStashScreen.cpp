// THE STASH SCREEN — SBreakerMenu::ShowStash and BuildStashScreen, in their
// own translation unit (ui.md: BreakerMenu.cpp is over 11,000 lines; a new
// screen goes in its own TU).
//
// Two grids over two containers. The left grid is the account's stash
// (UBreakerAccountSave::StashItems, 70 cells, 10 x 7); the right is the
// character's backpack (UBreakerEquipmentComponent::GetBackpack, 25 cells,
// 5 x 5). A click on a cell selects; MOVE TO STASH and TAKE TO BACKPACK act
// on the selection through DepositToStash / WithdrawFromStash, which own
// every rule — the Anchor gate, the two caps, the O182 level gate, the
// claim mark. This file never decides whether a move is legal; it asks, and
// echoes the refusal.
//
// Every number is BreakerStashLayout (UI/BreakerStashLayout.h) and every
// colour is BreakerUI (UI/BreakerUIStyle.h). BreakerMenu.cpp's drawing
// helpers are file-local to that TU, so the few this screen needs are
// re-declared below under a BreakerStash prefix (unity builds merge
// anonymous namespaces). The members it can reach — MakeButton and
// BuildZonedFrame — it reaches.
//
// The world at 40 % behind the plate is the FRAME's job (Cycle 15), not this
// screen's: BuildZonedFrame paints the same BgVoid field every zoned screen
// does today.

#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Game/BreakerGameInstance.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Save/BreakerAccountSave.h"
#include "UI/BreakerStashLayout.h"
#include "UI/BreakerTypeRoles.h"
#include "UI/BreakerUIStyle.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    // Role by size, the same seam BreakerMenu.cpp's MenuText uses: H2 and up
    // takes the display face, body and caption the body face.
    TSharedRef<STextBlock> BreakerStashText(const FText& Text, int32 Size, const FLinearColor& Colour, bool bBold = false)
    {
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Colour)
            .Font(Size >= BreakerUI::TypeH2 ? BreakerDisplayFont(Size, bBold) : BreakerBodyFont(Size, bBold));
    }

    TSharedRef<SWidget> BreakerStashSolid(const FLinearColor& Colour)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    // A 1px ring (2px when it carries the accent) around a widget. Borders
    // carry depth in this system; there are no gradients and no shadows.
    TSharedRef<SWidget> BreakerStashRing(const TSharedRef<SWidget>& Inner, const FLinearColor& Colour, float Thickness = BreakerUI::BorderThin)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            .Padding(FMargin(Thickness))
            [
                Inner
            ];
    }

    // The viewport, sampled once per rebuild — never an allotted size. The
    // plate ceiling is the design's own body plus the frame's padding; below
    // that the plate follows the window and the grids, which are fixed
    // geometry, scroll rather than shrink.
    FVector2D BreakerStashMeasurePlate()
    {
        FVector2D Viewport(1920.0f, 1080.0f);
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->GetViewportSize(Viewport);
        }
        if (Viewport.X < 640.0f || Viewport.Y < 360.0f) Viewport = FVector2D(1920.0f, 1080.0f);
        const float Ceiling = BreakerStashLayout::DesignBodyWidth + 2.0f * BreakerUI::Space24 + 2.0f * BreakerUI::BorderThin;
        return FVector2D(
            FMath::Clamp(static_cast<float>(Viewport.X) - 2.0f * BreakerUI::Space40, 720.0f, Ceiling),
            FMath::Clamp(static_cast<float>(Viewport.Y) - 2.0f * BreakerUI::Space40, 420.0f, 1000.0f));
    }

    // A DISABLED control, PAINTED. Same geometry as MakeButton — the
    // MinHitTarget + Space8 box, the ring, the body-bold label — with the
    // rest ring and disabled text, and no SButton underneath: a control that
    // cannot act should not take a click. Never faded (ui.md): fading shows
    // the plate seams through the control.
    TSharedRef<SWidget> BreakerStashPaintedButton(const FText& Label)
    {
        return SNew(SBox).HeightOverride(BreakerUI::MinHitTarget + BreakerUI::Space8)
        [
            BreakerStashRing(
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(BreakerUI::Panel00)
                .Padding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(Label)
                        .Justification(ETextJustify::Left)
                        .ColorAndOpacity(BreakerUI::TextDisabled)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
                ],
                BreakerUI::BorderRest)
        ];
    }

    // One tab chip, in BuildScreenTabs' vocabulary: the active chip carries
    // the 2px cyan ring, the rest a neutral 1px one. Never a teal underline.
    TSharedRef<SWidget> BreakerStashTabChip(const FString& Label, bool bActive, const FOnClicked& OnClicked)
    {
        return BreakerStashRing(
            SNew(SButton)
            .ButtonColorAndOpacity(bActive ? BreakerUI::Panel20 : BreakerUI::Panel00)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .OnClicked(OnClicked)
            [
                BreakerStashText(FText::FromString(Label), BreakerUI::TypeCaption,
                    bActive ? BreakerUI::TextPrimary : BreakerUI::TextMuted, true)
            ],
            bActive ? BreakerUI::System : BreakerUI::BorderEmphasis,
            bActive ? BreakerUI::BorderSelected : BreakerUI::BorderThin);
    }

    // The five-cell rarity tally: filled cells in the rarity's colour, the
    // rest in the rest border. A COUNT as well as a colour, so the tier reads
    // without the hue (never state by colour alone).
    TSharedRef<SWidget> BreakerStashRarityTally(EBreakerItemRarity Rarity)
    {
        const int32 Filled = BreakerStashLayout::RarityTallyFilled(Rarity);
        const FLinearColor Colour = BreakerUI::RarityColor(Rarity);
        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        for (int32 Cell = 0; Cell < BreakerStashLayout::RarityTallyCells; ++Cell)
        {
            Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 2.0f, 0.0f)
            [
                SNew(SBox).WidthOverride(BreakerStashLayout::RarityTallyCellW).HeightOverride(BreakerStashLayout::RarityTallyCellH)
                [
                    BreakerStashSolid(Cell < Filled ? Colour : BreakerUI::BorderRest)
                ]
            ];
        }
        return Row;
    }

    // The legendary mark: a Gold square turned 45 degrees. Drawn geometry,
    // not a glyph — the engine face has no Geometric Shapes block, and every
    // non-alphanumeric mark in this system is drawn.
    TSharedRef<SWidget> BreakerStashLegendaryDiamond()
    {
        const float Side = BreakerStashLayout::LegendaryDiamond;
        return SNew(SBox).WidthOverride(Side * 1.5f).HeightOverride(Side * 1.5f).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(Side).HeightOverride(Side)
            .RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.0f)))))
            .RenderTransformPivot(FVector2D(0.5f, 0.5f))
            [
                BreakerStashSolid(BreakerUI::Gold)
            ]
        ];
    }

    // One occupied cell. The ring is the selection: BorderEmphasis at 2px
    // when focused, BorderRest at 1px otherwise. Inside, top to bottom: the
    // icon square beside the legendary mark, the rarity tally, the slot word.
    TSharedRef<SWidget> BreakerStashItemCell(const FBreakerItemInstance& Item, float Width, float Height, bool bFocused,
        const FOnClicked& OnClicked)
    {
        TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
        Content->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                // THE ICON SLOT, EMPTY. No icon atlas exists in this project —
                // no item definition carries an icon and no texture set has
                // been imported — so the square is reserved at the design's
                // 48px and draws the cell's own face. When an atlas lands,
                // an SImage goes here and nothing else on the cell moves.
                SNew(SBox).WidthOverride(BreakerStashLayout::IconSquare).HeightOverride(BreakerStashLayout::IconSquare)
                [
                    BreakerStashSolid(BreakerUI::Panel00)
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(SSpacer)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
            [
                Item.IsLegendary()
                    ? BreakerStashLegendaryDiamond()
                    : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(1.0f, 1.0f)))
            ]
        ];
        Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
        [
            BreakerStashRarityTally(Item.Rarity)
        ];
        Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
        [
            BreakerStashText(FText::FromString(BreakerStashLayout::SlotWord(Item.Slot)), BreakerUI::TypeCaption,
                bFocused ? BreakerUI::TextPrimary : BreakerUI::TextSecondary, true)
        ];

        return SNew(SBox).WidthOverride(Width).HeightOverride(Height)
        [
            BreakerStashRing(
                SNew(SButton)
                .ButtonColorAndOpacity(bFocused ? BreakerUI::Panel20 : BreakerUI::Panel10)
                .ContentPadding(FMargin(BreakerUI::Space8))
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Top)
                .OnClicked(OnClicked)
                [
                    Content
                ],
                bFocused ? BreakerUI::BorderEmphasis : BreakerUI::BorderRest,
                bFocused ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    }

    // An empty cell: the same footprint on the darker face, so the grid's
    // shape is the cap and an empty slot is visibly a slot.
    TSharedRef<SWidget> BreakerStashEmptyCell(float Width, float Height)
    {
        return SNew(SBox).WidthOverride(Width).HeightOverride(Height)
        [
            BreakerStashRing(BreakerStashSolid(BreakerUI::Panel00), BreakerUI::BorderRest)
        ];
    }

    // A fixed grid: Columns x Rows cells, Items filling row-major from the
    // top-left, the rest empty. The cell count is the cap, never the item
    // count, so the grid cannot change shape between rebuilds.
    TSharedRef<SWidget> BreakerStashGrid(const TArray<FBreakerItemInstance>& Items, int32 Columns, int32 Rows,
        float CellW, float CellH, const FGuid& FocusedId, TFunction<FReply(FGuid)> OnCellClicked)
    {
        TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
            for (int32 Column = 0; Column < Columns; ++Column)
            {
                const int32 Index = Row * Columns + Column;
                TSharedRef<SWidget> Cell = BreakerStashEmptyCell(CellW, CellH);
                if (Items.IsValidIndex(Index))
                {
                    const FGuid ItemId = Items[Index].ItemId;
                    Cell = BreakerStashItemCell(Items[Index], CellW, CellH, ItemId == FocusedId,
                        FOnClicked::CreateLambda([OnCellClicked, ItemId]() { return OnCellClicked(ItemId); }));
                }
                Line->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Column + 1 < Columns ? BreakerStashLayout::StashGap : 0.0f, 0.0f)
                [
                    Cell
                ];
            }
            Grid->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, Row + 1 < Rows ? BreakerStashLayout::StashGap : 0.0f)
            [
                Line
            ];
        }
        return Grid;
    }
}

void SBreakerMenu::ShowStash()
{
    StashStatus = FText::GetEmpty();
    StashSelectedItemId.Invalidate();
    // Pause, not Main: entered from gameplay by walking to a thing and
    // pressing F, so "back" is the paused game — the same reason ShowDialogue
    // and ShowTravel set it.
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Stash);
}

TSharedRef<SWidget> SBreakerMenu::BuildStashScreen()
{
    using namespace BreakerStashLayout;

    // THE GATE, read from the map and not from the actor: the component
    // refuses on bAtAnchor, and the map is what "at the Anchor" means
    // (Game/BreakerGameInstance.h). A capture run in the gym gets a screen
    // whose two buttons are both painted, which is the truth of that state.
    const bool bAtAnchor = Character.IsValid() && UBreakerGameInstance::IsAnchorMap(Character.Get());

    UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;
    const UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const int32 CharacterLevel = Progression ? Progression->GetProgressionState().CharacterLevel : 1;
    UBreakerAccountSave* Account = UBreakerAccountSave::LoadOrCreate();

    static const TArray<FBreakerItemInstance> NoItems;
    const TArray<FBreakerItemInstance>& Backpack = Equipment ? Equipment->GetBackpack() : NoItems;
    // The stash MINUS claim-marked copies (VisibleStashItems), THEN the tab.
    // The counter reads the account's real count — a claimed item still
    // occupies a stash slot until its restore releases it.
    const TArray<FBreakerItemInstance> StashVisible = Account ? VisibleStashItems(*Account) : NoItems;
    const int32 StashCount = Account ? Account->StashItems.Num() : 0;
    const EStashTab Tab = TabFromIndex(StashTab);
    TArray<FBreakerItemInstance> StashShown;
    for (const FBreakerItemInstance& Item : StashVisible)
    {
        if (PassesTab(Item.Slot, Tab)) StashShown.Add(Item);
    }

    // Resolve the selection in EITHER container on every rebuild — no cache.
    // An id that just moved is found on the other side; an id that left both
    // resolves to nothing selected.
    bool bSelectedInBackpack = false;
    bool bSelectedInStash = false;
    FBreakerItemInstance Selected;
    if (StashSelectedItemId.IsValid())
    {
        for (const FBreakerItemInstance& Item : Backpack)
        {
            if (Item.ItemId == StashSelectedItemId) { Selected = Item; bSelectedInBackpack = true; break; }
        }
        if (!bSelectedInBackpack)
        {
            for (const FBreakerItemInstance& Item : StashVisible)
            {
                if (Item.ItemId == StashSelectedItemId) { Selected = Item; bSelectedInStash = true; break; }
            }
        }
    }
    if (!bSelectedInBackpack && !bSelectedInStash) StashSelectedItemId.Invalidate();

    // ---- Header: STASH, the two counters, the three tab chips --------------
    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
    const TArray<FString>& Labels = TabLabels();
    for (int32 Index = 0; Index < Labels.Num(); ++Index)
    {
        const bool bActive = Index == StashTab;
        Tabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            BreakerStashTabChip(Labels[Index], bActive, FOnClicked::CreateLambda([this, Index, bActive]()
            {
                if (!bActive)
                {
                    StashTab = Index;
                    Rebuild(EBreakerMenuScreen::Stash);
                }
                return FReply::Handled();
            }))
        ];
    }

    // ---- Body: the two grids -------------------------------------------------
    auto SelectCell = [this](FGuid ItemId) -> FReply
    {
        StashSelectedItemId = ItemId;
        StashStatus = FText::GetEmpty();
        Rebuild(EBreakerMenuScreen::Stash);
        return FReply::Handled();
    };

    // THE BUDGET, at the 1920x1080 canvas: the zoned plate is 1840 wide with
    // a 1px ring and 24px of padding a side, so 1790 is what the body gets,
    // and the two grids are 1156 + 616 = 1772 of it. The design's 44px gutter
    // and 40px origins do not survive that — the frame's own margin and
    // padding already spent them — so the gutter is whatever the plate
    // leaves (18 at 1920) and the grids sit flush to the padding. The grids
    // never shrink: a cell is the design's size or it is not drawn, so a
    // window under the design width scrolls. Where the plate lands once the
    // world sits behind it at 40 % is Cycle 15's frame work, not this file's.
    // Vertically the 808px stash grid fits the 1000px plate's 820px body
    // with 12px to spare, which is why no caption sits over either grid: the
    // counters live in the header's meta line and nowhere else.
    TSharedRef<SWidget> Body = SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                BreakerStashGrid(StashShown, StashColumns, StashRows, StashCell, StashCell, StashSelectedItemId, SelectCell)
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(SSpacer).Size(FVector2D(BreakerUI::Space8, 1.0f))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                BreakerStashGrid(Backpack, BackpackColumns, BackpackRows, BackpackCellW, BackpackCellH, StashSelectedItemId, SelectCell)
            ]
        ];

    // ---- Footer: the status line and the two verbs ---------------------------
    // Both PAINTED — drawn, not hidden — when the move they name cannot
    // happen: outside the Anchor (both), with nothing selected on their side
    // (each), at the cap they would cross (each). A hidden button teaches
    // nothing; a painted one says what the screen can do from here.
    const bool bMoveLive = bAtAnchor && bSelectedInBackpack && CanMoveToStash(StashCount);
    const bool bTakeLive = bAtAnchor && bSelectedInStash && CanTakeToBackpack(Backpack.Num());

    const FText MoveLabel = FText::FromString(TEXT("MOVE TO STASH"));
    const FText TakeLabel = FText::FromString(TEXT("TAKE TO BACKPACK"));

    TSharedRef<SWidget> MoveButton = bMoveLive
        ? MakeButton(MoveLabel, FOnClicked::CreateLambda([this, bAtAnchor, StashCount]()
        {
            UBreakerEquipmentComponent* Live = Character.IsValid() ? Character->GetEquipment() : nullptr;
            const FGuid ItemId = StashSelectedItemId;
            if (Live && Live->DepositToStash(ItemId, bAtAnchor))
            {
                StashStatus = FText::GetEmpty();
            }
            else
            {
                // The component logged WHY; the screen re-reads the same
                // predicates to say it. See DepositRefusal for the gap.
                const FString Reason = DepositRefusal(bAtAnchor, StashCount);
                StashStatus = FText::FromString(Reason.IsEmpty() ? FString(GenericRefusal()) : Reason);
            }
            Rebuild(EBreakerMenuScreen::Stash);
            return FReply::Handled();
        }), true)
        : BreakerStashPaintedButton(MoveLabel);

    const int32 SelectedItemLevel = Selected.ItemLevel;
    const bool bSelectedClaimed = Account && Account->PendingWithdrawals.Contains(StashSelectedItemId);
    const int32 BackpackCount = Backpack.Num();
    TSharedRef<SWidget> TakeButton = bTakeLive
        ? MakeButton(TakeLabel, FOnClicked::CreateLambda(
            [this, bAtAnchor, bSelectedClaimed, SelectedItemLevel, CharacterLevel, BackpackCount]()
        {
            UBreakerEquipmentComponent* Live = Character.IsValid() ? Character->GetEquipment() : nullptr;
            const FGuid ItemId = StashSelectedItemId;
            if (Live && Live->WithdrawFromStash(ItemId, bAtAnchor))
            {
                StashStatus = FText::GetEmpty();
            }
            else
            {
                const FString Reason = WithdrawRefusal(bAtAnchor, bSelectedClaimed, SelectedItemLevel, CharacterLevel, BackpackCount);
                StashStatus = FText::FromString(Reason.IsEmpty() ? FString(GenericRefusal()) : Reason);
            }
            Rebuild(EBreakerMenuScreen::Stash);
            return FReply::Handled();
        }), true)
        : BreakerStashPaintedButton(TakeLabel);

    // One row: the status line on the left, the two verbs on the right. No
    // hint line beneath — the vertical budget above is spent — and the
    // status box is a fixed height so a refusal landing cannot move the
    // buttons.
    TSharedRef<SWidget> Footer = SNew(SBox).Padding(FMargin(0.0f, BreakerUI::Space16, 0.0f, 0.0f))
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(SBox).HeightOverride(20.0f)
            [
                BreakerStashText(StashStatus, BreakerUI::TypeCaption, BreakerUI::Harm, true)
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            SNew(SBox).WidthOverride(220.0f)[MoveButton]
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(220.0f)[TakeButton]
        ]
    ];

    const FVector2D Plate = BreakerStashMeasurePlate();
    return BuildZonedFrame(
        FText::FromString(TEXT("STASH")),
        FText::FromString(MetaLine(StashCount, Backpack.Num())),
        Tabs, Body, Footer, Plate.X, Plate.Y, /*bFillHeight=*/true);
}
