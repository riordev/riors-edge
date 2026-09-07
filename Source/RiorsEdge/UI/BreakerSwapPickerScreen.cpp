// THE EQUIP-LIMIT SWAP PICKER — SBreakerMenu::BuildSwapPickerModal, in its
// own translation unit (ui.md: BreakerMenu.cpp is over 11,000 lines; a new
// screen goes in its own TU).
//
// O205: at the equip limit the equipment component supplies the swap
// candidates and the pre-focused one, and validates the choice; the picker
// offers and never computes the list. So this file reads
// UBreakerEquipmentComponent::SwapCandidates for its rows, takes its focus
// from the id the card click parked in SwapPickerFocusId (the component's
// LimitDisplaced), and confirms through EquipFromBackpackDisplacing, which
// refuses anything off the list. Nothing here decides which piece leaves.
//
// The modal is the discard modal's pattern (BuildDiscardModal): a scrim that
// dims and swallows clicks, a centred plate, one rail — here the top status
// rail in the rarity colour, as the sheet draws it. Every number and string
// is BreakerSwapPickerLayout (UI/BreakerSwapPickerLayout.h) and every colour
// is BreakerUI (UI/BreakerUIStyle.h). BreakerMenu.cpp's drawing helpers are
// file-local to that TU, so the few this modal needs are re-declared below
// under a BreakerSwap prefix (unity builds merge anonymous namespaces). The
// members it can reach — MakeButton, Rebuild, Character — it reaches.

#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemRules.h"
#include "UI/BreakerSwapPickerLayout.h"
#include "UI/BreakerTypeRoles.h"
#include "UI/BreakerUIStyle.h"
#include "Weapons/BreakerWeaponArchetype.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    // Role by size, the same seam BreakerMenu.cpp's MenuText uses: H2 and up
    // takes the display face, body and caption the body face.
    TSharedRef<STextBlock> BreakerSwapText(const FText& Text, int32 Size, const FLinearColor& Colour, bool bBold = false)
    {
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Colour)
            .Font(Size >= BreakerUI::TypeH2 ? BreakerDisplayFont(Size, bBold) : BreakerBodyFont(Size, bBold));
    }

    TSharedRef<SWidget> BreakerSwapSolid(const FLinearColor& Colour)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    // A ring around a widget: 1px at rest, 2px when it carries the focus.
    // Borders carry depth in this system; there are no gradients and no
    // shadows.
    TSharedRef<SWidget> BreakerSwapRing(const TSharedRef<SWidget>& Inner, const FLinearColor& Colour, float Thickness = BreakerUI::BorderThin)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            .Padding(FMargin(Thickness))
            [
                Inner
            ];
    }

    // The name a row prints, in the loadout card's own order: a legendary's
    // display name, the starter's ISSUE RIFLE, a weapon's archetype, an
    // armour piece's slot word. The loadout's ItemDisplayName is file-local
    // to BreakerMenu.cpp; the same four answers are given here.
    FString BreakerSwapItemName(const FBreakerItemInstance& Item)
    {
        if (Item.IsLegendary())
        {
            const FBreakerLegendaryDefinition Legendary = UBreakerItemRuleLibrary::FindLegendary(Item.LegendaryId);
            if (Legendary.IsValid() && !Legendary.DisplayName.IsEmpty())
            {
                return Legendary.DisplayName.ToString().ToUpper();
            }
        }
        if (Item.DefinitionId == UBreakerEquipmentComponent::StarterRifleDefinitionId)
        {
            return TEXT("ISSUE RIFLE");
        }
        if (Item.IsWeapon())
        {
            return BreakerWeaponArchetypeNames::Short(Item.WeaponArchetype);
        }
        return BreakerStashLayout::SlotWord(Item.Slot);
    }

    // The row's comparison column: one drawn mark per affix line of the
    // incoming piece, compared against THIS row's piece by the component
    // (CompareAffixes). The glyph is BreakerInventoryLayout::DeltaGlyph, the
    // same one the card prints; the component decided better/worse/parity.
    TSharedRef<SWidget> BreakerSwapDeltaMarks(const TArray<FBreakerAffixComparison>& Deltas)
    {
        TSharedRef<SHorizontalBox> Marks = SNew(SHorizontalBox);
        for (const FBreakerAffixComparison& Delta : Deltas)
        {
            const FLinearColor Colour = Delta.Delta == EBreakerAffixDelta::Better ? BreakerUI::System
                : Delta.Delta == EBreakerAffixDelta::Worse ? BreakerUI::Harm
                : BreakerUI::TextMuted;
            Marks->AddSlot().AutoWidth()
            [
                SNew(SBox).WidthOverride(BreakerUI::DeltaGlyphColumn).HAlign(HAlign_Center)
                [
                    BreakerMonoText(FText::FromString(BreakerInventoryLayout::DeltaGlyph(Delta.Delta)), BreakerSwapPickerLayout::LevelSize, Colour)
                ]
            ];
        }
        return Marks;
    }

    // The icon square: RESERVED and empty. No icon atlas exists in this
    // project, so the square draws the row's own face at the sheet's 40px;
    // when an atlas lands an SImage goes here and nothing else moves.
    TSharedRef<SWidget> BreakerSwapIconSquare()
    {
        return SNew(SBox).WidthOverride(BreakerSwapPickerLayout::IconSquare).HeightOverride(BreakerSwapPickerLayout::IconSquare)
        [
            BreakerSwapSolid(BreakerUI::Panel20)
        ];
    }

    // One 64px row on the rarity's identity rail: icon, kind over name, the
    // comparison marks beside the item level, then the row's verb. The row
    // body takes the focus; the verb confirms. The focused row carries the
    // hover face and the 2px high ring; the rest sit on the base face at 1px.
    TSharedRef<SWidget> BreakerSwapRow(const FBreakerItemInstance& Row, const FBreakerItemInstance& Incoming, bool bFocused,
        const FLinearColor& Rail, const FOnClicked& OnFocus, const FOnClicked& OnConfirm)
    {
        const TArray<FBreakerAffixComparison> Deltas = UBreakerEquipmentComponent::CompareAffixes(Incoming, Row);

        TSharedRef<SHorizontalBox> Body = SNew(SHorizontalBox);
        Body->AddSlot().AutoWidth().VAlign(VAlign_Center)[BreakerSwapIconSquare()];
        Body->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerSwapPickerLayout::RowPaddingX, 0.0f, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                BreakerMonoText(FText::FromString(BreakerStashLayout::SlotWord(Row.Slot)), BreakerSwapPickerLayout::KindSize, BreakerUI::TextSecondary, 0.08f)
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                BreakerSwapText(FText::FromString(BreakerSwapItemName(Row)), BreakerSwapPickerLayout::NameSize,
                    bFocused ? BreakerUI::TextPrimary : BreakerUI::TextSecondary)
            ]
        ];
        Body->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerSwapPickerLayout::RowPaddingX, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BreakerSwapDeltaMarks(Deltas)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space4, 0.0f, 0.0f, 0.0f)
            [
                BreakerMonoText(FText::FromString(FString::Printf(TEXT("i%d"), Row.ItemLevel)), BreakerSwapPickerLayout::LevelSize, BreakerUI::TextSecondary)
            ]
        ];
        Body->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).HeightOverride(BreakerSwapPickerLayout::RowButtonHeight)
            [
                BreakerSwapRing(
                    SNew(SButton)
                    .ButtonColorAndOpacity(bFocused ? BreakerUI::Gold : BreakerUI::Panel00)
                    .ContentPadding(FMargin(BreakerUI::Space16, 0.0f))
                    .VAlign(VAlign_Center)
                    .OnClicked(OnConfirm)
                    [
                        BreakerSwapText(FText::FromString(BreakerSwapPickerLayout::RowButtonLabel(Row, Incoming)),
                            BreakerSwapPickerLayout::RowButtonSize, bFocused ? BreakerUI::BgVoid : BreakerUI::TextPrimary, true)
                    ],
                    bFocused ? BreakerUI::Gold : BreakerUI::BorderEmphasis)
            ]
        ];

        return SNew(SBox).HeightOverride(BreakerSwapPickerLayout::RowHeight)
        [
            BreakerSwapRing(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(BreakerSwapPickerLayout::RowRail)[BreakerSwapSolid(Rail)]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(bFocused ? BreakerUI::Panel20 : BreakerUI::BgBase)
                    .ContentPadding(FMargin(BreakerSwapPickerLayout::RowPaddingX, 0.0f))
                    .HAlign(HAlign_Fill)
                    .VAlign(VAlign_Center)
                    .OnClicked(OnFocus)
                    [
                        Body
                    ]
                ],
                bFocused ? BreakerUI::BorderHigh : BreakerUI::BorderRest,
                bFocused ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    }

    // The INCOMING line: the same row anatomy without a verb, so the piece
    // the player is holding reads in the same shape as the pieces it would
    // take off.
    TSharedRef<SWidget> BreakerSwapIncomingRow(const FBreakerItemInstance& Incoming, const FLinearColor& Rail)
    {
        TSharedRef<SHorizontalBox> Body = SNew(SHorizontalBox);
        Body->AddSlot().AutoWidth().VAlign(VAlign_Center)[BreakerSwapIconSquare()];
        Body->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerSwapPickerLayout::RowPaddingX, 0.0f, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                BreakerMonoText(FText::FromString(BreakerSwapPickerLayout::IncomingLine(Incoming.Slot)), BreakerSwapPickerLayout::KindSize, BreakerUI::TextSecondary, 0.08f)
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                BreakerSwapText(FText::FromString(BreakerSwapItemName(Incoming)), BreakerSwapPickerLayout::NameSize, BreakerUI::TextPrimary)
            ]
        ];
        Body->AddSlot().AutoWidth().VAlign(VAlign_Center)
        [
            BreakerMonoText(FText::FromString(FString::Printf(TEXT("i%d"), Incoming.ItemLevel)), BreakerSwapPickerLayout::LevelSize, BreakerUI::TextSecondary)
        ];

        return SNew(SBox).HeightOverride(BreakerSwapPickerLayout::RowHeight)
        [
            BreakerSwapRing(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(BreakerSwapPickerLayout::RowRail)[BreakerSwapSolid(Rail)]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(BreakerUI::BgBase)
                    .Padding(FMargin(BreakerSwapPickerLayout::RowPaddingX, 0.0f))
                    .VAlign(VAlign_Center)
                    [
                        Body
                    ]
                ],
                BreakerUI::BorderRest)
        ];
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildSwapPickerModal()
{
    UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;
    const FBreakerItemInstance* Found = Equipment
        ? Equipment->GetBackpack().FindByPredicate([this](const FBreakerItemInstance& Item) { return Item.ItemId == SwapPickerItemId; })
        : nullptr;
    if (!Found)
    {
        // The incoming piece left the backpack under the picker (a discard,
        // a save load). The state is stale, not the player's: clear it and
        // draw nothing, so the next rebuild is the plain screen.
        SwapPickerItemId.Invalidate();
        SwapPickerFocusId.Invalidate();
        return SNullWidget::NullWidget;
    }
    const FBreakerItemInstance Incoming = *Found;
    const FBreakerEquipPreview Preview = Equipment->PreviewEquip(Incoming);
    const TArray<FBreakerItemInstance> Rows = Equipment->SwapCandidates(Incoming);
    // The rows carry the rarity on their identity rail — an item's rarity
    // carrier, legal at every tier. The PLATE's top rail is chrome, and teal
    // never touches a control's chrome (ui.md; the sheet gives the Unwritten
    // counter a border-low rail for the same reason), so an Unwritten incoming
    // takes the emphasis border on the plate and keeps its teal on the rows.
    const FLinearColor Rail = BreakerUI::RarityColor(Incoming.Rarity);
    const FLinearColor PlateRail = BreakerUI::IsReservedTeal(Rail) ? BreakerUI::BorderEmphasis : Rail;

    // The pre-focus is the component's: the card click parked LimitDisplaced
    // here, and that is Rows[0] by construction. If the parked id is not in
    // the list any more (the loadout changed under the picker), the focus
    // falls back to the head of the list rather than to nothing.
    const bool bFocusInList = Rows.ContainsByPredicate([this](const FBreakerItemInstance& Row) { return Row.ItemId == SwapPickerFocusId; });
    if (!bFocusInList) SwapPickerFocusId = Rows.Num() > 0 ? Rows[0].ItemId : FGuid();

    TSharedRef<SVerticalBox> Plate = SNew(SVerticalBox);
    Plate->AddSlot().AutoHeight()
    [
        BreakerSwapText(FText::FromString(BreakerSwapPickerLayout::Title(Incoming.Rarity, Preview.RarityCount, Preview.RarityLimit)),
            BreakerSwapPickerLayout::TitleSize, BreakerUI::TextPrimary, true)
    ];
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerSwapPickerLayout::RowGap, 0.0f, 0.0f)
    [
        BreakerSwapIncomingRow(Incoming, Rail)
    ];
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerSwapPickerLayout::RowGap, 0.0f, 0.0f)
    [
        SNew(SBox).HeightOverride(BreakerUI::BorderThin)[BreakerSwapSolid(BreakerUI::BorderRest)]
    ];

    for (const FBreakerItemInstance& Row : Rows)
    {
        const FGuid RowId = Row.ItemId;
        const FOnClicked OnFocus = FOnClicked::CreateLambda([this, RowId]()
        {
            SwapPickerFocusId = RowId;
            Rebuild(EBreakerMenuScreen::Inventory);
            return FReply::Handled();
        });
        const FOnClicked OnConfirm = FOnClicked::CreateLambda([this, RowId]()
        {
            // Confirm hands the component exactly the focused id; the
            // component refuses anything off its own list, so a stale row
            // cannot equip past the cap. Snaps on frame one (the sheet's
            // 100 ms leave is recorded in the layout header, not faked).
            SwapPickerFocusId = RowId;
            if (UBreakerEquipmentComponent* Live = Character.IsValid() ? Character->GetEquipment() : nullptr)
            {
                Live->EquipFromBackpackDisplacing(SwapPickerItemId, SwapPickerFocusId);
            }
            SwapPickerItemId.Invalidate();
            SwapPickerFocusId.Invalidate();
            Rebuild(EBreakerMenuScreen::Inventory);
            return FReply::Handled();
        });
        Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerSwapPickerLayout::RowGap, 0.0f, 0.0f)
        [
            BreakerSwapRow(Row, Incoming, Row.ItemId == SwapPickerFocusId, Rail, OnFocus, OnConfirm)
        ];
    }

    // KEEP CURRENT: nothing moves, the picker closes.
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerSwapPickerLayout::RowGap + BreakerUI::Space8, 0.0f, 0.0f).HAlign(HAlign_Right)
    [
        MakeButton(FText::FromString(BreakerSwapPickerLayout::KeepCurrentLabel()), FOnClicked::CreateLambda([this]()
        {
            SwapPickerItemId.Invalidate();
            SwapPickerFocusId.Invalidate();
            Rebuild(EBreakerMenuScreen::Inventory);
            return FReply::Handled();
        }))
    ];

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            // The scrim both dims the screen and swallows clicks, so the
            // controls behind the picker cannot be operated through it.
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BreakerUI::Alpha(BreakerUI::BgVoid, 0.85f))
            .OnMouseButtonDown(FPointerEventHandler::CreateLambda([](const FGeometry&, const FPointerEvent&)
            {
                return FReply::Handled();
            }))
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(BreakerSwapPickerLayout::ModalWidth)
            [
                // One plate, one rail: the top status rail in the rarity
                // colour, over the raised face, inside the rest border.
                BreakerSwapRing(
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SBox).HeightOverride(BreakerSwapPickerLayout::TopRail)[BreakerSwapSolid(PlateRail)]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(BreakerUI::BgRaised)
                        .Padding(FMargin(BreakerSwapPickerLayout::PlatePadding))
                        [
                            Plate
                        ]
                    ],
                    BreakerUI::BorderRest)
            ]
        ];
}
