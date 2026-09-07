// THE DEATH SCREEN — SBreakerMenu::ShowDeath and BuildDeathScreen, in their
// own translation unit (ui.md: BreakerMenu.cpp is over 11,000 lines; a new
// screen goes in its own TU).
//
// Two lines and two verbs (O82; Assets/design/04-death-banners). Line 1 is
// display 40, line 2 body 14. RETRY THE RIFT carries the focus ring and
// RETURN TO ANCHOR sits beside it on the panel; a spent budget leaves RETURN
// alone. The tally — DEATHS REMAINING n OF m, with m drawn cells — is drawn
// only for a budgeted tier. This file decides none of that: every string,
// the tally and the terminal variant arrive in FBreakerDeathScreenModel,
// built by BreakerDeathBudget::Model (Game/BreakerDeathBudgetMath.h), and
// the two verbs call the game mode's RetryRift and ReturnToAnchor, which are
// level travels. Nothing here resumes the game: the pawn under this screen
// is dead, and ResumeFromMenu would unpause it.
//
// BreakerMenu.cpp's drawing helpers are file-local to that TU, so the few
// this screen needs are re-declared below under a BreakerDeath prefix (unity
// builds merge anonymous namespaces). MakeButton paints its primary ring
// cyan, which is the movement verb, so the focus ring here is a local gold
// wrapper rather than MakeButton's flag.

#include "UI/BreakerMenu.h"
#include "UI/BreakerDeathInput.h"
#include "Game/BreakerGameInstance.h"

#include "Characters/BreakerCharacter.h"
#include "Game/BreakerGameMode.h"
#include "UI/BreakerTypeRoles.h"
#include "UI/BreakerUIStyle.h"

#include "Engine/World.h"
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
    // The sheet's headline size: display 700 at 40, above the type scale's
    // H1. The rest of the geometry is O2 PLACEHOLDER until the owner has
    // died on it.
    constexpr int32 BreakerDeathHeadlinePixels = 40;      // 04-death-banners
    constexpr float BreakerDeathColumnWidth = 720.0f;     // O2 PLACEHOLDER
    constexpr float BreakerDeathButtonWidth = 260.0f;     // O2 PLACEHOLDER
    constexpr float BreakerDeathTallyCellW = 24.0f;       // O2 PLACEHOLDER
    constexpr float BreakerDeathTallyCellH = 8.0f;        // O2 PLACEHOLDER
    constexpr float BreakerDeathTallyGap = 4.0f;          // O2 PLACEHOLDER

    TSharedRef<SWidget> BreakerDeathSolid(const FLinearColor& Colour)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    // A ring around a widget. Borders carry depth in this system; there are
    // no gradients and no shadows.
    TSharedRef<SWidget> BreakerDeathRing(const TSharedRef<SWidget>& Inner, const FLinearColor& Colour, float Thickness)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            .Padding(FMargin(Thickness))
            [
                Inner
            ];
    }

    // The focused verb: MakeButton's geometry — the MinHitTarget + Space8
    // box, the panel/20 face, the body-bold label — under a 2px Gold ring.
    // Gold is the reward and weak-point accent; here it is the FOCUS ring
    // and nothing else on the screen carries it, so the ring cannot be
    // mistaken for a state. Never cyan (movement) and never teal (a noun).
    TSharedRef<SWidget> BreakerDeathFocusedButton(const FText& Label, const FOnClicked& OnClicked)
    {
        return SNew(SBox).HeightOverride(BreakerUI::MinHitTarget + BreakerUI::Space8)
        [
            BreakerDeathRing(
                SNew(SButton)
                .ButtonColorAndOpacity(BreakerUI::Panel20)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                .OnClicked(OnClicked)
                [
                    SNew(STextBlock)
                        .Text(Label)
                        .Justification(ETextJustify::Left)
                        .ColorAndOpacity(BreakerUI::TextPrimary)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
                ],
                BreakerUI::Gold, BreakerUI::BorderSelected)
        ];
    }

    // DEATHS REMAINING n OF m over m cells, n of them filled in the system
    // bone and the rest on the rest border. The numerals carry the count;
    // the cells repeat it, so the tally never reads by colour alone.
    TSharedRef<SWidget> BreakerDeathTally(int32 Remaining, int32 Budget)
    {
        TSharedRef<SHorizontalBox> Cells = SNew(SHorizontalBox);
        for (int32 Cell = 0; Cell < Budget; ++Cell)
        {
            Cells->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Cell + 1 < Budget ? BreakerDeathTallyGap : 0.0f, 0.0f)
            [
                SNew(SBox).WidthOverride(BreakerDeathTallyCellW).HeightOverride(BreakerDeathTallyCellH)
                [
                    BreakerDeathSolid(Cell < Remaining ? BreakerUI::System : BreakerUI::BorderRest)
                ]
            ];
        }
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("DEATHS REMAINING %d OF %d"), Remaining, Budget)))
                    .ColorAndOpacity(BreakerUI::TextMuted)
                    .Font(BreakerBodyFont(BreakerUI::TypeCaption, true))
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
            [
                Cells
            ];
    }
}

void SBreakerMenu::ShowDeath(const FBreakerDeathScreenModel& Model)
{
    DeathModel = Model;
    bDeathActionPending = false;
    // Pause, not Main: raised from gameplay under the death beat's black. No
    // screen backs out of this one — both verbs leave the world — so the
    // root is never rebuilt from here; it is set so the menu's own state is
    // the same shape every gameplay-entered screen leaves it in.
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Death);
}

FReply SBreakerMenu::ExecuteDeathAction(bool bRetry)
{
    if (CurrentScreen != EBreakerMenuScreen::Death || bDeathActionPending || (bRetry && !DeathModel.bRetry))
        return FReply::Handled();
    ABreakerGameMode* Mode = Character.IsValid() && Character->GetWorld()
        ? Character->GetWorld()->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    if (!Mode) return FReply::Handled();
    const UBreakerGameInstance* Session = Mode->GetGameInstance<UBreakerGameInstance>();
    if (bRetry && (!Session || !Session->PendingRift.IsSet())) return FReply::Handled();
    bDeathActionPending = true;
    if (bRetry) Mode->RetryRift(Character.Get());
    else Mode->ReturnToAnchor(Character.Get());
    return FReply::Handled();
}

FReply SBreakerMenu::HandleDeathConfirmKey(const FKeyEvent& KeyEvent)
{
    if (CurrentScreen != EBreakerMenuScreen::Death) return FReply::Unhandled();
    const FKey Key = KeyEvent.GetKey();
    const bool bConfirm = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
    // Tab/arrows continue into Slate. An explicitly focused Return button
    // owns its confirmation; only the root's default chooses Retry.
    if (!bConfirm || !HasKeyboardFocus()) return FReply::Unhandled();
    const EBreakerDeathAction Action = BreakerDeathInput::Confirm(KeyEvent.GetKey(), KeyEvent.IsRepeat(), DeathModel.bRetry, bDeathActionPending);
    if (Action != EBreakerDeathAction::None) ExecuteDeathAction(Action == EBreakerDeathAction::Retry);
    // The dead pawn cannot resume, and a held confirmation must not activate
    // the same button again while travel is pending.
    return FReply::Handled();
}
TSharedRef<SWidget> SBreakerMenu::BuildDeathScreen()
{
    const FBreakerDeathScreenModel& M = DeathModel;

    auto Retry = [this]() { return ExecuteDeathAction(true); };
    auto Return = [this]() { return ExecuteDeathAction(false); };
    // The verbs: RETRY focused and first while the budget holds; RETURN
    // alone, and therefore focused, when it is spent (the sheet's terminal
    // frame).
    TSharedRef<SHorizontalBox> Verbs = SNew(SHorizontalBox);
    if (M.bRetry)
    {
        Verbs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            SNew(SBox).WidthOverride(BreakerDeathButtonWidth)
            [
                BreakerDeathFocusedButton(FText::FromString(TEXT("RETRY THE RIFT")), FOnClicked::CreateLambda(Retry))
            ]
        ];
        Verbs->AddSlot().AutoWidth()
        [
            SNew(SBox).WidthOverride(BreakerDeathButtonWidth)
            [
                MakeButton(FText::FromString(TEXT("RETURN TO ANCHOR")), FOnClicked::CreateLambda(Return), false)
            ]
        ];
    }
    else
    {
        Verbs->AddSlot().AutoWidth()
        [
            SNew(SBox).WidthOverride(BreakerDeathButtonWidth)
            [
                BreakerDeathFocusedButton(FText::FromString(TEXT("RETURN TO ANCHOR")), FOnClicked::CreateLambda(Return))
            ]
        ];
    }

    TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
    Column->AddSlot().AutoHeight().HAlign(HAlign_Center)
    [
        SNew(STextBlock)
            .Text(FText::FromString(M.Headline))
            .Justification(ETextJustify::Center)
            .ColorAndOpacity(BreakerUI::TextPrimary)
            .Font(BreakerDisplayFont(BreakerDeathHeadlinePixels, true))
    ];
    Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
    [
        SNew(STextBlock)
            .Text(FText::FromString(M.Line2))
            .Justification(ETextJustify::Center)
            .ColorAndOpacity(BreakerUI::TextSecondary)
            .Font(BreakerBodyFont(BreakerUI::TypeBody))
    ];
    if (M.bShowTally)
    {
        Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
        [
            BreakerDeathTally(M.DeathsRemaining, M.DeathBudget)
        ];
    }
    Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space40, 0.0f, 0.0f)
    [
        Verbs
    ];

    // The field under the lines is the zoned frame's own scrim: the world at
    // 40 % behind it is the frame's job, and under the death beat's black
    // there is no world to show yet.
    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            BreakerDeathSolid(BreakerUI::Alpha(BreakerUI::BgVoid, 0.6f))   // O2 PLACEHOLDER
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(BreakerUI::Space40)
        [
            SNew(SBox).WidthOverride(BreakerDeathColumnWidth)
            [
                Column
            ]
        ];
}
