// THE RIFT DEBRIEF — SBreakerMenu::ShowRiftDebrief and BuildRiftDebriefScreen,
// in their own translation unit (ui.md: BreakerMenu.cpp is over 11,000 lines;
// a new screen goes in its own TU).
//
// Owner-asked: "i really like the entering rift screen so maybe when a rift is
// closed we can add something very similar that shows the items we gained from
// completion/on completion kinda like a loot highlight". So this is the
// BRIEFING'S TWIN and reads like one — a kicker, a headline, then the run's
// haul — and it decides nothing: every string and every number arrives in
// BreakerRiftDebrief::FModel, composed from the game mode's run ledger.
//
// ONE VERB. The death screen has two because both are level travels and the
// player must choose; here the run is already over and the only question left
// is when they have finished looking. The rift's exit offer is its own beat and
// this screen does not pre-empt it.
//
// COLOUR BY VERB (O179). Rarity colours the item's own rail, because rarity is
// a noun the player already reads that way everywhere else; GOLD is the reward
// accent and carries the two totals. Nothing here is cyan (movement) or teal (a
// rift object) — the rift is closed, and this is what came out of it.

#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "UI/BreakerTypeRoles.h"
#include "UI/BreakerUIStyle.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    // Geometry, all O2 PLACEHOLDER until the owner has closed a rift on it.
    // The column matches the death screen's so the two beats sit in the same
    // place on screen: a reward and a loss that jump about read as two
    // different games.
    constexpr int32 BreakerDebriefHeadlinePixels = 40;
    constexpr float BreakerDebriefColumnWidth = 720.0f;
    constexpr float BreakerDebriefRowHeight = 34.0f;
    constexpr float BreakerDebriefRailWidth = 4.0f;
    constexpr float BreakerDebriefLevelColumn = 56.0f;
    constexpr float BreakerDebriefVerbWidth = 260.0f;
    constexpr float BreakerDebriefGroupGap = 24.0f;
    constexpr float BreakerDebriefVerbGap = 32.0f;

    TSharedRef<SWidget> BreakerDebriefSolid(const FLinearColor& Colour)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    // One line of the haul: a rarity rail, the item's own name, its level.
    // THE RAIL CARRIES THE RARITY AND THE TEXT DOES NOT, deliberately — a name
    // printed in its own rarity is unreadable at the bottom of the ladder,
    // which is where most of a haul lives.
    TSharedRef<SWidget> BreakerDebriefRow(const BreakerRiftDebrief::FLine& Line)
    {
        return SNew(SBox).HeightOverride(BreakerDebriefRowHeight)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).WidthOverride(BreakerDebriefRailWidth)
                [
                    BreakerDebriefSolid(BreakerUI::RarityColor(Line.Rarity))
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
              .Padding(BreakerUI::Space12, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(Line.Name))
                    .ColorAndOpacity(BreakerUI::TextPrimary)
                    .Font(BreakerBodyFont(BreakerUI::TypeBody, false))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(BreakerDebriefLevelColumn)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("i%d"), Line.ItemLevel)))
                        .Justification(ETextJustify::Right)
                        .ColorAndOpacity(BreakerUI::TextMuted)
                        .Font(BreakerBodyFont(BreakerUI::TypeCaption, true))
                ]
            ]
        ];
    }

    // A total, named and valued on one row. Gold on the value alone: the label
    // is what it is, and the number is the reward.
    TSharedRef<SWidget> BreakerDebriefTotal(const TCHAR* Label, int32 Value)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .ColorAndOpacity(BreakerUI::TextMuted)
                    .Font(BreakerBodyFont(BreakerUI::TypeCaption, true))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
              .Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::FromInt(Value)))
                    .ColorAndOpacity(BreakerUI::Gold)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
            ];
    }
}

void SBreakerMenu::ShowRiftDebrief(const BreakerRiftDebrief::FModel& Model)
{
    RiftDebriefModel = Model;
    // Pause, for the same reason the death screen uses it: raised from
    // gameplay, and nothing backs out of it into another screen.
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::RiftDebrief);
}

TSharedRef<SWidget> SBreakerMenu::BuildRiftDebriefScreen()
{
    const BreakerRiftDebrief::FModel& M = RiftDebriefModel;

    TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight()
    [
        SNew(STextBlock)
            .Text(FText::FromString(M.TierKicker))
            .ColorAndOpacity(BreakerUI::TextMuted)
            .Font(BreakerBodyFont(BreakerUI::TypeCaption, true))
    ];
    Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        SNew(STextBlock)
            .Text(FText::FromString(M.Headline))
            .ColorAndOpacity(BreakerUI::TextPrimary)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerDebriefHeadlinePixels))
    ];
    Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
    [
        SNew(STextBlock)
            .Text(M.AreaName)
            .ColorAndOpacity(BreakerUI::TextSecondary)
            .Font(BreakerBodyFont(BreakerUI::TypeBody, false))
    ];

    Column->AddSlot().AutoHeight().Padding(0.0f, BreakerDebriefGroupGap, 0.0f, BreakerUI::Space8)
    [
        SNew(SBox).HeightOverride(1.0f)[ BreakerDebriefSolid(BreakerUI::BorderRest) ]
    ];

    if (M.IsEmptyHanded())
    {
        // SAID PLAINLY. An empty box reads as a broken screen; a sentence reads
        // as a run that paid nothing, which is what happened.
        Column->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("NOTHING CAME BACK WITH YOU")))
                .ColorAndOpacity(BreakerUI::TextMuted)
                .Font(BreakerBodyFont(BreakerUI::TypeBody, true))
        ];
    }
    for (const BreakerRiftDebrief::FLine& Line : M.Highlights)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            BreakerDebriefRow(Line)
        ];
    }
    if (M.MoreCount > 0)
    {
        Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("AND %d MORE IN YOUR PACK"), M.MoreCount)))
                .ColorAndOpacity(BreakerUI::TextMuted)
                .Font(BreakerBodyFont(BreakerUI::TypeCaption, true))
        ];
    }

    Column->AddSlot().AutoHeight().Padding(0.0f, BreakerDebriefGroupGap, 0.0f, BreakerUI::Space8)
    [
        SNew(SBox).HeightOverride(1.0f)[ BreakerDebriefSolid(BreakerUI::BorderRest) ]
    ];
    Column->AddSlot().AutoHeight()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()[ BreakerDebriefTotal(TEXT("RIFTGLASS"), M.Riftglass) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(BreakerDebriefGroupGap, 0.0f, 0.0f, 0.0f)
          [ BreakerDebriefTotal(TEXT("EXPERIENCE"), M.Experience) ]
    ];

    Column->AddSlot().AutoHeight().Padding(0.0f, BreakerDebriefVerbGap, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(BreakerDebriefVerbWidth)
        [
            MakeButton(FText::FromString(TEXT("CONTINUE")),
                FOnClicked::CreateSP(this, &SBreakerMenu::CloseRiftDebrief), true)
        ]
    ];

    // THE SCRIM AND THE CENTRED COLUMN, exactly as the death beat wraps its
    // own. The first capture returned the bare column and the host stretched
    // it: the kicker started off the left edge and CONTINUE ran the full
    // width of a 1920 screen. A screen is not a column; it is a column placed.
    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            BreakerDebriefSolid(BreakerUI::Alpha(BreakerUI::BgVoid, 0.72f))   // O2 PLACEHOLDER
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(BreakerDebriefVerbGap)
        [
            SNew(SBox).WidthOverride(BreakerDebriefColumnWidth)
            [
                Column
            ]
        ];
}

FReply SBreakerMenu::CloseRiftDebrief()
{
    // BACK TO THE WORLD, not to a menu. The pawn under this screen is alive and
    // standing in a rift that is now closed; the exit offer is its own beat and
    // this screen must not stand in front of it.
    if (Character.IsValid()) Character->ResumeFromMenu();
    return FReply::Handled();
}
