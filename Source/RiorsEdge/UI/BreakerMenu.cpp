#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor Background(0.008f, 0.012f, 0.022f, 1.0f);
    const FLinearColor Panel(0.025f, 0.04f, 0.065f, 0.98f);
    const FLinearColor PanelRaised(0.045f, 0.07f, 0.105f, 1.0f);
    const FLinearColor Cyan(0.12f, 0.78f, 1.0f, 1.0f);
    const FLinearColor SoftText(0.62f, 0.72f, 0.82f, 1.0f);

    TSharedRef<STextBlock> MenuText(const FText& Text, int32 Size, const FLinearColor& Color = FLinearColor::White, bool bBold = false)
    {
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Color)
            .Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size));
    }
}

void SBreakerMenu::Construct(const FArguments& InArgs)
{
    Character = InArgs._Character;
    ChildSlot
    [
        SAssignNew(ContentHost, SBox)
    ];
    ShowMainMenu();
}

void SBreakerMenu::ShowMainMenu()
{
    RootScreen = EBreakerMenuScreen::Main;
    Rebuild(EBreakerMenuScreen::Main);
}

void SBreakerMenu::ShowPauseMenu()
{
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Pause);
}

void SBreakerMenu::HandleEscape()
{
    if (CurrentScreen == EBreakerMenuScreen::Settings || CurrentScreen == EBreakerMenuScreen::Loadout)
    {
        Rebuild(RootScreen);
    }
    else if (CurrentScreen == EBreakerMenuScreen::Pause && Character.IsValid())
    {
        Character->ResumeFromMenu();
    }
}

void SBreakerMenu::Rebuild(EBreakerMenuScreen NewScreen)
{
    CurrentScreen = NewScreen;
    if (!ContentHost.IsValid()) return;
    switch (CurrentScreen)
    {
        case EBreakerMenuScreen::Pause: ContentHost->SetContent(BuildPauseScreen()); break;
        case EBreakerMenuScreen::Settings: ContentHost->SetContent(BuildSettingsScreen()); break;
        case EBreakerMenuScreen::Loadout: ContentHost->SetContent(BuildLoadoutScreen()); break;
        default: ContentHost->SetContent(BuildMainScreen()); break;
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildFrame(const FText& Title, const FText& Subtitle, const TSharedRef<SWidget>& Body) const
{
    TSharedRef<SVerticalBox> PanelContent = SNew(SVerticalBox);
    PanelContent->AddSlot().AutoHeight().Padding(42.0f, 34.0f, 42.0f, 0.0f)
    [
        MenuText(Title, 38, FLinearColor::White, true)
    ];
    PanelContent->AddSlot().AutoHeight().Padding(44.0f, 5.0f, 42.0f, 22.0f)
    [
        MenuText(Subtitle, 12, Cyan, true)
    ];
    PanelContent->AddSlot().FillHeight(1.0f).Padding(42.0f, 0.0f, 42.0f, 34.0f)
    [
        Body
    ];

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Background)
        ]
        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top)
        [
            SNew(SBox).HeightOverride(5.0f)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(Cyan)
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(28.0f)
        [
            SNew(SBox).WidthOverride(720.0f).MaxDesiredHeight(760.0f)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(Panel)
                [
                    PanelContent
                ]
            ]
        ];
}

TSharedRef<SWidget> SBreakerMenu::MakeButton(const FText& Label, const FOnClicked& OnClicked, bool bPrimary) const
{
    return SNew(SBox).HeightOverride(52.0f)
    [
        SNew(SButton)
        .ButtonColorAndOpacity(bPrimary ? Cyan : PanelRaised)
        .ContentPadding(FMargin(18.0f, 10.0f))
        .HAlign(HAlign_Left)
        .OnClicked(OnClicked)
        [
            MenuText(Label, 14, FLinearColor::White, true)
        ]
    ];
}

TSharedRef<SWidget> SBreakerMenu::BuildMainScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
    [
        MenuText(FText::FromString(TEXT("MOVEMENT-DRIVEN COMBAT PROTOTYPE")), 11, SoftText)
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        MakeButton(FText::FromString(TEXT("ENTER PLAYTEST GYM")), FOnClicked::CreateLambda([this]()
        {
            if (Character.IsValid()) Character->ResumeFromMenu();
            return FReply::Handled();
        }), true)
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        MakeButton(FText::FromString(TEXT("LOADOUT")), FOnClicked::CreateLambda([this]()
        {
            Rebuild(EBreakerMenuScreen::Loadout);
            return FReply::Handled();
        }))
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        MakeButton(FText::FromString(TEXT("SETTINGS")), FOnClicked::CreateLambda([this]()
        {
            Rebuild(EBreakerMenuScreen::Settings);
            return FReply::Handled();
        }))
    ];
    Body->AddSlot().AutoHeight()
    [
        MakeButton(FText::FromString(TEXT("QUIT TO DESKTOP")), FOnClicked::CreateLambda([this]()
        {
            if (Character.IsValid()) Character->QuitFromMenu();
            return FReply::Handled();
        }))
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 26.0f, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("BUILD 0.1  |  WIN64 DEVELOPMENT")), 9, SoftText)
    ];
    return BuildFrame(FText::FromString(TEXT("RIOR'S EDGE")), FText::FromString(TEXT("BREAK THE LINE. KEEP THE MOMENTUM.")), Body);
}

TSharedRef<SWidget> SBreakerMenu::BuildPauseScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    auto AddButton = [&Body](const TSharedRef<SWidget>& Button)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)[Button];
    };
    AddButton(MakeButton(FText::FromString(TEXT("RESUME")), FOnClicked::CreateLambda([this]()
    {
        if (Character.IsValid()) Character->ResumeFromMenu();
        return FReply::Handled();
    }), true));
    AddButton(MakeButton(FText::FromString(TEXT("LOADOUT")), FOnClicked::CreateLambda([this]()
    {
        Rebuild(EBreakerMenuScreen::Loadout);
        return FReply::Handled();
    })));
    AddButton(MakeButton(FText::FromString(TEXT("SETTINGS")), FOnClicked::CreateLambda([this]()
    {
        Rebuild(EBreakerMenuScreen::Settings);
        return FReply::Handled();
    })));
    AddButton(MakeButton(FText::FromString(TEXT("RETURN TO TITLE")), FOnClicked::CreateLambda([this]()
    {
        if (Character.IsValid()) Character->ReturnToTitleMenu();
        return FReply::Handled();
    })));
    AddButton(MakeButton(FText::FromString(TEXT("QUIT TO DESKTOP")), FOnClicked::CreateLambda([this]()
    {
        if (Character.IsValid()) Character->QuitFromMenu();
        return FReply::Handled();
    })));
    Body->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("ESC  RESUME")), 10, SoftText)
    ];
    return BuildFrame(FText::FromString(TEXT("PAUSED")), FText::FromString(TEXT("PLAYTEST GYM / SESSION ACTIVE")), Body);
}

TSharedRef<SWidget> SBreakerMenu::BuildSettingsScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("LOOK SENSITIVITY     %.2f"), Character.IsValid() ? Character->GetLookSensitivity() : 1.0f)); })
        .ColorAndOpacity(FLinearColor::White)
        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 20.0f)
    [
        SNew(SSlider)
        .Value(Character.IsValid() ? (Character->GetLookSensitivity() - 0.2f) / 1.8f : 0.44f)
        .OnValueChanged_Lambda([this](float Value)
        {
            if (Character.IsValid()) Character->ApplyMenuSettings(0.2f + Value * 1.8f, Character->GetCurrentFOV(), Character->IsLookInverted());
        })
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [
        SNew(STextBlock)
        .Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("FIELD OF VIEW     %.0f"), Character.IsValid() ? Character->GetCurrentFOV() : 90.0f)); })
        .ColorAndOpacity(FLinearColor::White)
        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 20.0f)
    [
        SNew(SSlider)
        .Value(Character.IsValid() ? (Character->GetCurrentFOV() - 70.0f) / 50.0f : 0.4f)
        .OnValueChanged_Lambda([this](float Value)
        {
            if (Character.IsValid()) Character->ApplyMenuSettings(Character->GetLookSensitivity(), 70.0f + Value * 50.0f, Character->IsLookInverted());
        })
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 24.0f)
    [
        SNew(SCheckBox)
        .IsChecked(Character.IsValid() && Character->IsLookInverted() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
        {
            if (Character.IsValid()) Character->ApplyMenuSettings(Character->GetLookSensitivity(), Character->GetCurrentFOV(), State == ECheckBoxState::Checked);
        })
        [
            MenuText(FText::FromString(TEXT("INVERT VERTICAL LOOK")), 13, FLinearColor::White, true)
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("KEYBINDS")), 12, Cyan, true)];
    Body->AddSlot().AutoHeight().Padding(14.0f, 0.0f, 0.0f, 22.0f)
    [
        MenuText(FText::FromString(TEXT("WASD  Move       SHIFT  Sprint toggle       SPACE  Jump\nQ  Dash           C / CTRL  Slide             R  Reload\nLMB  Fire         RMB  Aim                   1 / 2  Weapon slots\nF1  Reset         F2  Copy report            F3  Diagnostics")), 11, SoftText)
    ];
    Body->AddSlot().AutoHeight()[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[MenuText(FText::FromString(TEXT("Changes save immediately  |  ESC Back")), 9, SoftText)];
    return BuildFrame(FText::FromString(TEXT("SETTINGS")), FText::FromString(TEXT("CONTROLS / CAMERA")), Body);
}

TSharedRef<SWidget> SBreakerMenu::MakeGearCard(const FText& Slot, const FText& Name, const FText& Details, const FLinearColor& Accent) const
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(PanelRaised)
        .Padding(FMargin(18.0f, 15.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(Slot, 10, Accent, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 5.0f)[MenuText(Name, 20, FLinearColor::White, true)]
            + SVerticalBox::Slot().AutoHeight()[MenuText(Details, 10, SoftText)]
        ];
}

TSharedRef<SWidget> SBreakerMenu::BuildLoadoutScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        MakeGearCard(FText::FromString(TEXT("EQUIPPED / SLOT 1")), FText::FromString(TEXT("BREAKER RIFLE")), FText::FromString(TEXT("AUTOMATIC  |  30 ROUNDS  |  MID-RANGE")), Cyan)
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 18.0f)
    [
        MakeGearCard(FText::FromString(TEXT("EQUIPPED / SLOT 2")), FText::FromString(TEXT("SCATTERGUN")), FText::FromString(TEXT("SEMI-AUTOMATIC  |  8 SHELLS  |  CLOSE-RANGE")), FLinearColor(1.0f, 0.5f, 0.15f))
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("ARMORY")), 12, SoftText, true)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 20.0f)
    [
        MakeGearCard(FText::FromString(TEXT("AVAILABLE / NOT EQUIPPED")), FText::FromString(TEXT("MARKSMAN")), FText::FromString(TEXT("SEMI-AUTOMATIC  |  8 ROUNDS  |  LONG-RANGE")), FLinearColor(0.7f, 0.5f, 1.0f))
    ];
    Body->AddSlot().AutoHeight()[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[MenuText(FText::FromString(TEXT("Two equipped weapons maximum  |  ESC Back")), 9, SoftText)];
    return BuildFrame(FText::FromString(TEXT("LOADOUT")), FText::FromString(TEXT("ACTIVE GEAR / PROTOTYPE ARMORY")), Body);
}

FReply SBreakerMenu::GoBack()
{
    Rebuild(RootScreen);
    return FReply::Handled();
}
