#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerClassDefinition.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionNode.h"
#include "Progression/BreakerProgressionTree.h"
#include "Interaction/BreakerNPC.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SCanvas.h"
#include "UI/BreakerUIStyle.h"
#include "Algo/Reverse.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    // ---------------------------------------------------------------------
    // FIELDPLATE. Every colour on this screen comes from BreakerUIStyle.h —
    // see Docs/Design/UI-Style-Guide-Fieldplate.md. The old local names are
    // kept as aliases so the whole file moves onto the system in one place
    // instead of a thousand call sites.
    // ---------------------------------------------------------------------
    const FLinearColor Background = BreakerUI::BgVoid;      // screen field
    const FLinearColor Panel = BreakerUI::Panel00;          // plate face
    const FLinearColor PanelRaised = BreakerUI::Panel10;    // cards, rows, slots
    const FLinearColor PanelHover = BreakerUI::Panel20;     // headers, selected
    const FLinearColor Cyan = BreakerUI::Cyan;              // player / system
    const FLinearColor Primary = BreakerUI::TextPrimary;
    const FLinearColor SoftText = BreakerUI::TextSecondary;
    const FLinearColor Muted = BreakerUI::TextMuted;
    const FLinearColor Disabled = BreakerUI::TextDisabled;
    const FLinearColor BorderRest = BreakerUI::BorderRest;
    const FLinearColor BorderEmphasis = BreakerUI::BorderEmphasis;
    const FLinearColor Harm = BreakerUI::Harm;
    const FLinearColor HarmDeep = BreakerUI::HarmDeep;
    // Reward / purchase-confirm gold. Gold is the only colour that means
    // "spend now", which is what makes scanning a tree work.
    const FLinearColor Amber = BreakerUI::Gold;
    const FLinearColor Transparent(0.0f, 0.0f, 0.0f, 0.0f);

    TSharedRef<STextBlock> MenuText(const FText& Text, int32 Size, const FLinearColor& Color = BreakerUI::TextPrimary, bool bBold = false)
    {
        return SNew(STextBlock)
            .Text(Text)
            .ColorAndOpacity(Color)
            .Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size));
    }

    TSharedRef<SWidget> SolidBlock(const FLinearColor& Color)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Color)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    // 1px ring around a control. Buttons in this system are a fill plus a
    // border; Slate's button brush has no border, so it gets one here.
    TSharedRef<SWidget> BorderWrap(const TSharedRef<SWidget>& Inner, const FLinearColor& BorderColor, float Thickness = BreakerUI::BorderThin)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BorderColor)
            .Padding(FMargin(Thickness))
            [
                Inner
            ];
    }

    // A plate: flat face, 1px border, one 3px rail full-bleed to the edge.
    // FIELDPLATE 03 — the rail is the signature, and one plate never carries
    // two of them. RailEdge Left is identity, Top is transient status.
    TSharedRef<SWidget> MakePlate(const TSharedRef<SWidget>& Content, const FLinearColor& Face, const FLinearColor& Rail,
        const FMargin& ContentPadding = FMargin(16.0f, 12.0f), bool bTopRail = false,
        const FLinearColor& BorderColor = BreakerUI::BorderRest)
    {
        TSharedRef<SWidget> Face2 = SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Face)
            .Padding(ContentPadding)
            [
                Content
            ];

        TSharedRef<SWidget> Railed = bTopRail
            ? StaticCastSharedRef<SWidget>(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(BreakerUI::RailThickness)[SolidBlock(Rail)]]
                + SVerticalBox::Slot().FillHeight(1.0f)[Face2])
            : StaticCastSharedRef<SWidget>(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(BreakerUI::RailThickness)[SolidBlock(Rail)]]
                + SHorizontalBox::Slot().FillWidth(1.0f)[Face2]);

        // The 1px border is the outermost ring: borders carry depth in this
        // system, gradients do not exist.
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(BorderColor)
            .Padding(FMargin(BreakerUI::BorderThin))
            [
                Railed
            ];
    }

    // ---------------------------------------------------------------------
    // Path-board primitives.
    //
    // The skill matrix board is drawn on an SCanvas at fixed pixel positions
    // (FIELDPLATE authors at 1920x1080). Nothing on the board measures itself
    // against its allotted size, so there is no layout feedback loop of the
    // kind SWrapBox/UseAllottedSize produced inside a scroll box.
    // ---------------------------------------------------------------------

    // A dashed hairline. Slate has no dash pattern, so it is a fixed run of
    // blocks — the count comes from the caller's pixel width, never from an
    // allotted size.
    TSharedRef<SWidget> DashedLine(float Width, const FLinearColor& Color, float Dash = 6.0f, float Gap = 6.0f)
    {
        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        const int32 Count = FMath::Clamp(FMath::CeilToInt(Width / (Dash + Gap)), 1, 240);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Gap, 0.0f)
            [
                SNew(SBox).WidthOverride(Dash)[SolidBlock(Color)]
            ];
        }
        return Row;
    }

    // A straight 2px segment between two board points, drawn as a bar rotated
    // about its own centre. Trunks pass A/B on the same X; diagonals do not.
    void AddCanvasSegment(const TSharedRef<SCanvas>& Canvas, const FVector2D& A, const FVector2D& B,
        const FLinearColor& Color, float Thickness = 2.0f)
    {
        const FVector2D Delta = B - A;
        const float Length = FMath::Max(1.0f, static_cast<float>(Delta.Size()));
        const float Angle = FMath::Atan2(static_cast<float>(Delta.Y), static_cast<float>(Delta.X));
        const FVector2D Mid = (A + B) * 0.5;
        Canvas->AddSlot()
            .Position(FVector2D(Mid.X - Length * 0.5f, Mid.Y - Thickness * 0.5f))
            .Size(FVector2D(Length, Thickness))
            [
                SNew(SBox)
                .RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(Angle))))
                .RenderTransformPivot(FVector2D(0.5, 0.5))
                [
                    SolidBlock(Color)
                ]
            ];
    }

    // Diamond markers are square markers turned 45 degrees. The rotation is a
    // render transform, so the layout box stays axis-aligned and the board
    // geometry stays trivially predictable.
    TSharedRef<SWidget> RotateFortyFive(const TSharedRef<SWidget>& Inner)
    {
        return SNew(SBox)
            .RenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(45.0f)))))
            .RenderTransformPivot(FVector2D(0.5, 0.5))
            [
                Inner
            ];
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

void SBreakerMenu::ShowInventory()
{
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Inventory);
}

void SBreakerMenu::ShowDialogue(ABreakerNPC* NPC)
{
    DialogueNPC = NPC;
    DialogueNodeId = NPC ? NPC->GetStartNodeId() : NAME_None;
    RootScreen = EBreakerMenuScreen::Pause;
    Rebuild(EBreakerMenuScreen::Dialogue);
}

void SBreakerMenu::HandleEscape()
{
    if (CurrentScreen == EBreakerMenuScreen::Dialogue)
    {
        if (Character.IsValid()) Character->ResumeFromMenu();
        return;
    }
    if (CurrentScreen == EBreakerMenuScreen::Settings || CurrentScreen == EBreakerMenuScreen::Loadout || CurrentScreen == EBreakerMenuScreen::Inventory || CurrentScreen == EBreakerMenuScreen::ClassSelect || CurrentScreen == EBreakerMenuScreen::SkillTrees)
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
    // Diagnostic for the reported screen flip-flop: every transition is
    // logged with a timestamp so a repro session shows exactly what drives
    // the loop. Cheap enough to leave in during playtests.
    UE_LOG(LogTemp, Log, TEXT("[MenuRebuild] %d -> %d at %.3f"),
        static_cast<int32>(CurrentScreen), static_cast<int32>(NewScreen),
        FPlatformTime::Seconds());
    // Flip-flop diagnosis: when the transition is between Inventory and
    // SkillTrees, dump the callstack so the log names the caller.
    if ((CurrentScreen == EBreakerMenuScreen::Inventory && NewScreen == EBreakerMenuScreen::SkillTrees) ||
        (CurrentScreen == EBreakerMenuScreen::SkillTrees && NewScreen == EBreakerMenuScreen::Inventory))
    {
        ANSICHAR StackTrace[4096];
        StackTrace[0] = 0;
        FPlatformStackWalk::StackWalkAndDump(StackTrace, UE_ARRAY_COUNT(StackTrace), 1);
        UE_LOG(LogTemp, Log, TEXT("[MenuRebuild] caller:\n%hs"), StackTrace);
    }

    // Deferred: swapping the content synchronously destroys the button whose
    // OnClicked is still on the callstack — a Slate re-entrancy footgun and
    // the prime suspect for the screen flip-flop. Coalesce all requests made
    // this frame and apply once on the next Slate tick.
    PendingScreen = NewScreen;
    if (!bRebuildScheduled)
    {
        bRebuildScheduled = true;
        RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda(
            [this](double, float) -> EActiveTimerReturnType
            {
                bRebuildScheduled = false;
                ApplyScreen(PendingScreen);
                return EActiveTimerReturnType::Stop;
            }));
    }
}

void SBreakerMenu::ApplyScreen(EBreakerMenuScreen NewScreen)
{
    CurrentScreen = NewScreen;
    // Consume the one-shot cleanup arm: only the rebuild triggered by the
    // arming click sees it, everything else disarms.
    CleanupArmedIndex = PendingCleanupArm;
    PendingCleanupArm = -1;
    // A confirmation modal belongs to the screen that raised it; leaving the
    // screen answers it with "no".
    if (CurrentScreen != EBreakerMenuScreen::Inventory) DiscardModalIndex = -1;
    if (!ContentHost.IsValid()) return;
    switch (CurrentScreen)
    {
        case EBreakerMenuScreen::Pause: ContentHost->SetContent(BuildPauseScreen()); break;
        case EBreakerMenuScreen::Settings: ContentHost->SetContent(BuildSettingsScreen()); break;
        case EBreakerMenuScreen::Loadout: ContentHost->SetContent(BuildLoadoutScreen()); break;
        case EBreakerMenuScreen::Inventory: ContentHost->SetContent(BuildInventoryScreen()); break;
        case EBreakerMenuScreen::ClassSelect: ContentHost->SetContent(BuildClassSelectScreen()); break;
        case EBreakerMenuScreen::SkillTrees: ContentHost->SetContent(BuildSkillTreesScreen()); break;
        case EBreakerMenuScreen::Dialogue: ContentHost->SetContent(BuildDialogueScreen()); break;
        default: ContentHost->SetContent(BuildMainScreen()); break;
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildFrame(const FText& Title, const FText& Subtitle, const TSharedRef<SWidget>& Body, float PanelWidth) const
{
    // Header zone: h1 title top-left with the caption directly beneath it,
    // separated from the body by a 1px divider rather than by whitespace —
    // the system reads structure off borders, not off gaps.
    TSharedRef<SVerticalBox> PanelContent = SNew(SVerticalBox);
    PanelContent->AddSlot().AutoHeight()
    [
        MenuText(Title, BreakerUI::TypeH1, Primary, true)
    ];
    PanelContent->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space16)
    [
        MenuText(Subtitle, BreakerUI::TypeCaption, Muted, true)
    ];
    PanelContent->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(BorderRest)]
    ];
    PanelContent->AddSlot().FillHeight(1.0f)
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
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(BreakerUI::Space40)
        [
            SNew(SBox).WidthOverride(PanelWidth).MaxDesiredHeight(880.0f)
            [
                // The screen plate carries the cyan identity rail: the front
                // end belongs to the player/system family.
                MakePlate(PanelContent, Panel, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space24))
            ]
        ];
}

TSharedRef<SWidget> SBreakerMenu::BuildZonedFrame(const FText& Title, const FText& Meta, const TSharedRef<SWidget>& HeaderRight,
    const TSharedRef<SWidget>& Body, const TSharedRef<SWidget>& Footer, float PanelWidth) const
{
    // Header band, 88 tall at bg/raised on the cyan identity rail: h1 title
    // with the meta caption beneath it, the screen's own controls pinned to
    // the right of the same band. Zones are separated by the band, never by
    // whitespace.
    TSharedRef<SVerticalBox> Root = SNew(SVerticalBox);
    Root->AddSlot().AutoHeight()
    [
        SNew(SBox).HeightOverride(88.0f)
        [
            MakePlate(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[MenuText(Title, BreakerUI::TypeH1, Primary, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                    [
                        MenuText(Meta, BreakerUI::TypeCaption, Muted, true)
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(BreakerUI::Space40, 0.0f, 0.0f, 0.0f)
                [
                    HeaderRight
                ],
                BreakerUI::BgRaised, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space8))
        ]
    ];
    Root->AddSlot().FillHeight(1.0f).Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)[Body];
    Root->AddSlot().AutoHeight()[Footer];

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Background)
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(BreakerUI::Space40)
        [
            SNew(SBox).WidthOverride(PanelWidth).MaxDesiredHeight(1000.0f)
            [
                Root
            ]
        ];
}

TSharedRef<SWidget> SBreakerMenu::BuildScreenTabs(EBreakerMenuScreen ActiveScreen)
{
    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
    auto AddTab = [this, &Tabs, ActiveScreen](const FString& Label, EBreakerMenuScreen Target)
    {
        const bool bActive = ActiveScreen == Target;
        // Selected carries the 2px accent border; unselected keeps the same
        // geometry on a neutral 1px ring. Never a teal underline — teal is a
        // noun in this system, and a tab is not a rift object.
        Tabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bActive ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, Target, bActive]()
                {
                    if (!bActive)
                    {
                        if (Target == EBreakerMenuScreen::SkillTrees) SkillTreeStatus = FText::GetEmpty();
                        Rebuild(Target);
                    }
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bActive ? Primary : Muted, true)
                ],
                bActive ? Cyan : BorderEmphasis,
                bActive ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    };
    AddTab(TEXT("EQUIPMENT"), EBreakerMenuScreen::Inventory);
    AddTab(TEXT("SKILL TREES"), EBreakerMenuScreen::SkillTrees);
    return Tabs;
}

// FIELDPLATE 01, interaction states. Primary: panel/20 fill inside a 1px cyan
// ring, text/primary. Secondary: no fill inside a 1px #2A3E58 ring,
// text/secondary. Neither ever changes opacity — that would show the plate
// seams behind it.
TSharedRef<SWidget> SBreakerMenu::MakeButton(const FText& Label, const FOnClicked& OnClicked, bool bPrimary) const
{
    return SNew(SBox).HeightOverride(BreakerUI::MinHitTarget + BreakerUI::Space8)
    [
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bPrimary ? PanelHover : Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Center)
            .OnClicked(OnClicked)
            [
                MenuText(Label, BreakerUI::TypeBody, bPrimary ? Primary : SoftText, true)
            ],
            bPrimary ? Cyan : BorderEmphasis)
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
        MakeButton(FText::FromString(TEXT("INVENTORY")), FOnClicked::CreateLambda([this]()
        {
            Rebuild(EBreakerMenuScreen::Inventory);
            return FReply::Handled();
        }))
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        MakeButton(FText::FromString(TEXT("BREAKER CLASS")), FOnClicked::CreateLambda([this]()
        {
            Rebuild(EBreakerMenuScreen::ClassSelect);
            return FReply::Handled();
        }))
    ];
    // Skill trees are reached through the INVENTORY tab strip; no separate
    // top-level entry point.
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
    AddButton(MakeButton(FText::FromString(TEXT("INVENTORY")), FOnClicked::CreateLambda([this]()
    {
        Rebuild(EBreakerMenuScreen::Inventory);
        return FReply::Handled();
    })));
    // SKILL TREES intentionally absent: the INVENTORY screen's tab strip owns
    // that route now.
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
        .ColorAndOpacity(Primary)
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
        .ColorAndOpacity(Primary)
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
            MenuText(FText::FromString(TEXT("INVERT VERTICAL LOOK")), 13, Primary, true)
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("KEYBINDS")), 12, Cyan, true)];
    Body->AddSlot().AutoHeight().Padding(14.0f, 0.0f, 0.0f, 22.0f)
    [
        MenuText(FText::FromString(TEXT("WASD  Move       SHIFT  Sprint toggle       SPACE  Jump\nQ  Dash           C / CTRL  Slide             R  Reload\nLMB  Fire         RMB  Aim                   1 / 2  Weapon slots\nI  Inventory      F  Talk to NPC             F4  Start wave\nF1  Reset         F2  Copy report            F3  Diagnostics")), 11, SoftText)
    ];
    Body->AddSlot().AutoHeight()[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[MenuText(FText::FromString(TEXT("Changes save immediately  |  ESC Back")), 9, SoftText)];
    return BuildFrame(FText::FromString(TEXT("SETTINGS")), FText::FromString(TEXT("CONTROLS / CAMERA")), Body);
}

TSharedRef<SWidget> SBreakerMenu::MakeGearCard(const FText& Slot, const FText& Name, const FText& Details, const FLinearColor& Accent) const
{
    // Card face stays panel/10 at every rarity so a wall of loot does not
    // become a wall of colour; the accent lives on the rail and the name.
    return MakePlate(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[MenuText(Slot, BreakerUI::TypeCaption, Muted, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space4)[MenuText(Name, BreakerUI::TypeH2, Accent, true)]
        + SVerticalBox::Slot().AutoHeight()[MenuText(Details, BreakerUI::TypeCaption, SoftText)],
        PanelRaised, Accent, FMargin(BreakerUI::Space16, BreakerUI::Space16));
}

TSharedRef<SWidget> SBreakerMenu::BuildLoadoutScreen()
{
    UBreakerWeaponComponent* Weapon = Character.IsValid() ? Character->GetWeapon() : nullptr;

    struct FArchetypeEntry { EBreakerWeaponArchetype Archetype; const TCHAR* Name; const TCHAR* Details; };
    static const FArchetypeEntry Archetypes[] =
    {
        { EBreakerWeaponArchetype::Rifle,   TEXT("RIFLE"),   TEXT("AUTOMATIC  |  30 ROUNDS  |  MID-RANGE") },
        { EBreakerWeaponArchetype::SMG,     TEXT("SMG"),     TEXT("AUTOMATIC  |  35 ROUNDS  |  CLOSE-MID, HIGH CADENCE") },
        { EBreakerWeaponArchetype::Sniper,  TEXT("SNIPER"),  TEXT("SEMI-AUTOMATIC  |  8 ROUNDS  |  LONG-RANGE") },
        { EBreakerWeaponArchetype::Shotgun, TEXT("SHOTGUN"), TEXT("SEMI-AUTOMATIC  |  8 SHELLS  |  CLOSE-RANGE") },
        { EBreakerWeaponArchetype::Rocket,  TEXT("ROCKET"),  TEXT("PROJECTILE  |  4 ROCKETS  |  AREA DAMAGE") },
    };

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    for (int32 SlotNumber = 1; SlotNumber <= 2; ++SlotNumber)
    {
        const EBreakerWeaponArchetype Assigned = Weapon ? Weapon->GetSlotArchetype(SlotNumber) : EBreakerWeaponArchetype::Rifle;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("SLOT %d — click an archetype to assign"), SlotNumber)), 11, Cyan, true)
        ];
        TSharedRef<SHorizontalBox> RowBox = SNew(SHorizontalBox);
        for (const FArchetypeEntry& Entry : Archetypes)
        {
            const bool bAssigned = Entry.Archetype == Assigned;
            const EBreakerWeaponArchetype CapturedArchetype = Entry.Archetype;
            const int32 CapturedSlot = SlotNumber;
            RowBox->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(SBox).HeightOverride(64.0f)
                [
                    // Assigned carries the accent ring, not an accent fill:
                    // a solid cyan tile would outrank the screen title.
                    BorderWrap(
                        SNew(SButton)
                        .ButtonColorAndOpacity(bAssigned ? PanelHover : Panel)
                        .HAlign(HAlign_Center).VAlign(VAlign_Center)
                        .OnClicked(FOnClicked::CreateLambda([this, CapturedSlot, CapturedArchetype]()
                        {
                            if (Character.IsValid() && Character->GetWeapon()) Character->GetWeapon()->SetSlotArchetype(CapturedSlot, CapturedArchetype);
                            Rebuild(EBreakerMenuScreen::Loadout);
                            return FReply::Handled();
                        }))
                        [
                            MenuText(FText::FromString(Entry.Name), BreakerUI::TypeH2, bAssigned ? Primary : SoftText, true)
                        ],
                        // Weapons are the orange family; the assigned slot says so.
                        bAssigned ? BreakerUI::Orange : BorderEmphasis,
                        bAssigned ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
                ]
            ];
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)[RowBox];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("ARMORY REFERENCE")), 12, SoftText, true)];
    {
        FString Reference;
        for (const FArchetypeEntry& Entry : Archetypes)
        {
            Reference += FString::Printf(TEXT("%-8s  %s\n"), Entry.Name, Entry.Details);
        }
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[MenuText(FText::FromString(Reference), 10, SoftText)];
    }
    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[MenuText(FText::FromString(TEXT("Two equipped weapons maximum  |  ESC Back")), 9, SoftText)];
    return BuildFrame(FText::FromString(TEXT("LOADOUT")), FText::FromString(TEXT("WEAPON SLOTS / ARMORY")), Body, 880.0f);
}

namespace
{
    // One rarity ramp for the whole game: the same values the HUD draws a
    // ground drop's rail and beam with.
    FLinearColor RarityColor(EBreakerItemRarity Rarity)
    {
        return BreakerUI::RarityColor(Rarity);
    }

    // A card whose rarity reads from its 3px left rail. Anomalous also takes
    // a full 1px border, because it is the only tier that is simultaneously a
    // world object class.
    TSharedRef<SWidget> MakeRarityCard(const TSharedRef<SWidget>& Inner, EBreakerItemRarity Rarity, bool bHasItem)
    {
        const FLinearColor Rail = bHasItem ? BreakerUI::RarityColor(Rarity) : BreakerUI::BorderEmphasis;
        const FLinearColor Ring = bHasItem && BreakerUI::RarityGetsFullBorder(Rarity)
            ? BreakerUI::RarityColor(Rarity) : BreakerUI::BorderRest;
        return BorderWrap(
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(BreakerUI::RailThickness)[SolidBlock(Rail)]]
            + SHorizontalBox::Slot().FillWidth(1.0f)[Inner],
            Ring);
    }

    FString RarityName(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon: return TEXT("UNCOMMON");
            case EBreakerItemRarity::Exceptional: return TEXT("EXCEPTIONAL");
            case EBreakerItemRarity::Aberrant: return TEXT("ABERRANT");
            case EBreakerItemRarity::Anomalous: return TEXT("ANOMALOUS");
            default: return TEXT("STANDARD");
        }
    }

    FString SlotName(EBreakerEquipSlot Slot)
    {
        switch (Slot)
        {
            case EBreakerEquipSlot::Helmet: return TEXT("HELMET");
            case EBreakerEquipSlot::BodyArmour: return TEXT("BODY ARMOUR");
            case EBreakerEquipSlot::Gloves: return TEXT("GLOVES");
            case EBreakerEquipSlot::Boots: return TEXT("BOOTS");
            case EBreakerEquipSlot::Necklace: return TEXT("NECKLACE");
            case EBreakerEquipSlot::Waist: return TEXT("WAIST");
            case EBreakerEquipSlot::Primary: return TEXT("PRIMARY");
            case EBreakerEquipSlot::Secondary: return TEXT("SECONDARY");
            default: return TEXT("SLOT");
        }
    }

    FString ClassDisplayName(EBreakerClassId ClassId)
    {
        switch (ClassId)
        {
            case EBreakerClassId::Caster:   return TEXT("CASTER");
            case EBreakerClassId::Swift:    return TEXT("SWIFT");
            case EBreakerClassId::Gunsmith: return TEXT("GUNSMITH");
            case EBreakerClassId::Tank:     return TEXT("TANK");
            case EBreakerClassId::Support:  return TEXT("SUPPORT");
            default:                        return TEXT("UNCLASSED");
        }
    }

    // The two bulk-discard thresholds the header offers, indexed by arm. One
    // function so the chip, the modal's count and the modal's Destroy button
    // cannot disagree about what "below" means.
    EBreakerItemRarity CleanupThresholdForArm(int32 ArmIndex)
    {
        return ArmIndex == 1 ? EBreakerItemRarity::Exceptional : EBreakerItemRarity::Uncommon;
    }

    FString TierLabel(int32 Tier)
    {
        return Tier < 0 ? TEXT("T-1") : FString::Printf(TEXT("T%d"), Tier);
    }

    // One affix as the player reads it: "Movement Speed  +5.0%  T4".
    FString DescribeAffix(const FBreakerRolledAffix& Affix)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Affix.AffixId);
        const FString Name = Definition ? Definition->DisplayName.ToString() : Affix.AffixId.ToString();
        const bool bPercent = Definition && Definition->StatBucket != EBreakerStatBucket::Flat;
        // Critical Chance and Critical Damage roll as flat numbers but are
        // printed as percentages, because that is what they mean.
        const bool bPercentStyleFlat = Definition &&
            (Definition->StatTarget == EBreakerStatTarget::CriticalChance || Definition->StatTarget == EBreakerStatTarget::CriticalDamage);
        return FString::Printf(TEXT("%s  +%.1f%s  %s"), *Name, Affix.Value,
            bPercent || bPercentStyleFlat ? TEXT("%") : TEXT(""), *TierLabel(Affix.Tier));
    }

    FString DescribeItem(const FBreakerItemInstance& Item)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("ITEM LEVEL %d"), Item.ItemLevel));
        for (const FBreakerRolledAffix& Affix : Item.Affixes)
        {
            Lines.Add(DescribeAffix(Affix));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    // The affix list with per-affix deltas (UI-Inventory-Spec "Card anatomy"
    // line 3): the glyph sits in a fixed column so the affix names keep a
    // straight left edge whether or not a card is being compared.
    //
    // Deltas is UBreakerEquipmentComponent's answer, one row per affix in the
    // same order as Item.Affixes — this function decides nothing about better
    // or worse, it only picks a glyph and a colour. Pass an empty array for a
    // card with nothing to compare against (an equipped piece).
    TSharedRef<SWidget> MakeAffixLines(const FBreakerItemInstance& Item, const TArray<FBreakerAffixComparison>& Deltas)
    {
        TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
        Lines->AddSlot().AutoHeight()
        [
            MenuText(FText::FromString(FString::Printf(TEXT("ITEM LEVEL %d"), Item.ItemLevel)), BreakerUI::TypeCaption, SoftText)
        ];
        for (int32 Index = 0; Index < Item.Affixes.Num(); ++Index)
        {
            FString Glyph;
            FLinearColor GlyphColor = Muted;
            if (Deltas.IsValidIndex(Index))
            {
                switch (Deltas[Index].Delta)
                {
                    case EBreakerAffixDelta::Better: Glyph = BreakerUI::DeltaBetterGlyph; GlyphColor = Cyan; break;
                    case EBreakerAffixDelta::Worse:  Glyph = BreakerUI::DeltaWorseGlyph;  GlyphColor = Harm; break;
                    default:                         Glyph = BreakerUI::DeltaParityGlyph; GlyphColor = Muted; break;
                }
            }
            Lines->AddSlot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(BreakerUI::DeltaGlyphColumn)
                    [
                        MenuText(FText::FromString(Glyph), BreakerUI::TypeCaption, GlyphColor, true)
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    MenuText(FText::FromString(DescribeAffix(Item.Affixes[Index])), BreakerUI::TypeCaption, SoftText)
                ]
            ];
        }
        return Lines;
    }

    // The five rarity beams, as the empty backpack draws them: one vertical
    // bar per tier in the same ramp the ground drops use, so the screen and
    // the world teach the same lesson.
    TSharedRef<SWidget> MakeRarityBeams()
    {
        static const EBreakerItemRarity Ramp[] =
        {
            EBreakerItemRarity::Standard,
            EBreakerItemRarity::Uncommon,
            EBreakerItemRarity::Exceptional,
            EBreakerItemRarity::Aberrant,
            EBreakerItemRarity::Anomalous,
        };
        TSharedRef<SHorizontalBox> Beams = SNew(SHorizontalBox);
        for (const EBreakerItemRarity Rarity : Ramp)
        {
            Beams->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space24, 0.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    // The beam itself: a 6px column of the rarity's own colour,
                    // the same value the HUD draws a ground drop's beam with.
                    SNew(SBox).WidthOverride(6.0f).HeightOverride(180.0f)
                    [
                        SolidBlock(BreakerUI::RarityColor(Rarity))
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(RarityName(Rarity)), BreakerUI::TypeCaption, BreakerUI::RarityColor(Rarity), true)
                ]
            ];
        }
        return Beams;
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildInventoryScreen()
{
    UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;

    // The outline handles belong to the widget tree being replaced right now.
    EquipSlotOutlines.Reset();

    // One-click cards: an equipped slot unequips on click, a backpack item
    // equips on click.
    auto MakeSlotCard = [this, Equipment](EBreakerEquipSlot Slot) -> TSharedRef<SWidget>
    {
        FBreakerItemInstance Item;
        const bool bHasItem = Equipment && Equipment->GetEquippedItem(Slot, Item);
        const FLinearColor Accent = bHasItem ? RarityColor(Item.Rarity) : Disabled;
        // An empty slot keeps its full geometry and its name: the doll never
        // looks broken, only unfinished.
        const FString Name = bHasItem ? RarityName(Item.Rarity) : TEXT("EMPTY");
        const FString Details = bHasItem ? DescribeItem(Item) : TEXT("—");

        // The doomed-piece outline. It sits OUTSIDE the card's own ring so the
        // rarity ring is never overwritten, and it rests on the screen field
        // colour, which reads as nothing until a hovered backpack card names
        // this slot.
        TSharedRef<SBorder> Outline = SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Background)
            .Padding(FMargin(BreakerUI::BorderSelected))
            [
                MakeRarityCard(
                SNew(SButton)
                .ButtonColorAndOpacity(bHasItem ? PanelRaised : Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, Slot]()
                {
                    if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->UnequipSlot(Slot);
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(SlotName(Slot)), BreakerUI::TypeCaption, Muted, true)]
                        + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(Name), BreakerUI::TypeCaption, Accent, true)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(Details), BreakerUI::TypeCaption, bHasItem ? SoftText : Disabled)
                    ]
                ],
                bHasItem ? Item.Rarity : EBreakerItemRarity::Standard, bHasItem)
            ];

        EquipSlotOutlines.Add(Slot, Outline);
        return SNew(SBox).MinDesiredHeight(72.0f).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[Outline];
    };

    // ---- Character column, 560 wide (UI-Inventory-Spec "Zones") -----------
    // Render slot on top, gear totals pinned beneath it so the numbers are
    // always on screen with the doll. The old single printf blob is gone:
    // the spec wants aligned label/value rows with the value coloured by its
    // function family.
    TSharedRef<SVerticalBox> CharacterColumn = SNew(SVerticalBox);
    CharacterColumn->AddSlot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space16)
    [
        SNew(SBox).MinDesiredHeight(300.0f)
        [
            // The render slot keeps full geometry while empty: the doll never
            // looks broken, only unfinished.
            MakePlate(
                SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(TEXT("FULL-BODY RENDER SLOT\n\nSILHOUETTE PLACEHOLDER")), BreakerUI::TypeCaption, Muted)
                ],
                BreakerUI::BgRaised, BorderEmphasis, FMargin(BreakerUI::Space16))
        ]
    ];
    {
        TSharedRef<SVerticalBox> Totals = SNew(SVerticalBox);
        Totals->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(TEXT("GEAR TOTALS")), BreakerUI::TypeCaption, Muted, true)
        ];
        auto AddTotalRow = [&Totals](const FString& Label, const FString& Value, const FLinearColor& ValueColor)
        {
            Totals->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    // Fixed value column so the numbers form a straight edge
                    // and never reflow as they tick.
                    SNew(SBox).WidthOverride(104.0f).HAlign(HAlign_Right)
                    [
                        MenuText(FText::FromString(Value), BreakerUI::TypeCaption, ValueColor, true)
                    ]
                ]
            ];
        };
        if (Equipment)
        {
            const FBreakerEquipmentStats& Stats = Equipment->GetStats();
            // Value colour is the FIELDPLATE function family: player/system
            // survivability and movement cyan, weapon stats orange, reward
            // gold. (There is no gear-granted shield stat yet — health is the
            // cyan survivability row until one exists.)
            AddTotalRow(TEXT("HEALTH"), FString::Printf(TEXT("+%.0f"), Stats.BonusHealth), Cyan);
            AddTotalRow(TEXT("MAX RESOURCE"), FString::Printf(TEXT("+%.0f"), Stats.BonusMaxResource), Cyan);
            AddTotalRow(TEXT("RESOURCE REGEN"), FString::Printf(TEXT("+%.1f/s"), Stats.ResourceRegenPerSecond), Cyan);
            AddTotalRow(TEXT("PHYS DR"), FString::Printf(TEXT("%.1f%%"), Stats.PhysicalDamageReductionPercent), Cyan);
            AddTotalRow(TEXT("MOVE SPEED"), FString::Printf(TEXT("x%.2f"), Stats.MoveSpeedMultiplier), Cyan);
            AddTotalRow(TEXT("SLIDE SPEED"), FString::Printf(TEXT("x%.2f"), Stats.SlideSpeedMultiplier), Cyan);
            AddTotalRow(TEXT("AIR CONTROL"), FString::Printf(TEXT("x%.2f"), Stats.AirControlMultiplier), Cyan);
            AddTotalRow(TEXT("DASH COOLDOWN"), FString::Printf(TEXT("x%.2f"), Stats.DashCooldownMultiplier), Cyan);
            // Read the composed attribute, not the gear-only figure. Gear,
            // skill nodes and the point-spend baseline all land in one additive
            // Increased bucket on DamageMultiplier now, and this row printing
            // only the gear half is precisely how "I spend points and damage
            // never changes" would still look true after it stopped being true.
            {
                const UBreakerAttributeSet* Attributes = Character.IsValid() ? Character->GetAttributes() : nullptr;
                const float ComposedDamage = Attributes ? Attributes->GetDamageMultiplier() : Stats.WeaponDamageMultiplier;
                AddTotalRow(TEXT("DAMAGE"), FString::Printf(TEXT("x%.2f"), ComposedDamage), BreakerUI::Orange);
            }
            AddTotalRow(TEXT("CRIT CHANCE"), FString::Printf(TEXT("+%.1f%%"), Stats.CriticalChanceBonus * 100.0f), BreakerUI::Orange);
            AddTotalRow(TEXT("CRIT DAMAGE"), FString::Printf(TEXT("+%.1f%%"), Stats.CriticalMultiplierBonus * 100.0f), BreakerUI::Orange);
            AddTotalRow(TEXT("DROP CHANCE"), FString::Printf(TEXT("+%.1f%%"), Stats.DropChancePercent), Amber);
        }
        else
        {
            Totals->AddSlot().AutoHeight()
            [
                MenuText(FText::FromString(TEXT("NO EQUIPMENT COMPONENT")), BreakerUI::TypeCaption, Disabled, true)
            ];
        }
        CharacterColumn->AddSlot().AutoHeight()
        [
            MakePlate(Totals, PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space16))
        ];
    }

    // ---- Equipment column, 400 wide ---------------------------------------
    // Eight slots as full-width rows in wear order — head to foot, then
    // trinkets, then weapons — rather than the old two-column split.
    static const EBreakerEquipSlot WearOrder[] =
    {
        EBreakerEquipSlot::Helmet,
        EBreakerEquipSlot::BodyArmour,
        EBreakerEquipSlot::Gloves,
        EBreakerEquipSlot::Waist,
        EBreakerEquipSlot::Boots,
        EBreakerEquipSlot::Necklace,
        EBreakerEquipSlot::Primary,
        EBreakerEquipSlot::Secondary,
    };
    TSharedRef<SVerticalBox> EquipRows = SNew(SVerticalBox);
    for (const EBreakerEquipSlot Slot : WearOrder)
    {
        EquipRows->AddSlot().AutoHeight()[MakeSlotCard(Slot)];
    }
    TSharedRef<SVerticalBox> EquipmentColumn = SNew(SVerticalBox);
    EquipmentColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        MenuText(FText::FromString(TEXT("EQUIPPED — CLICK TO UNEQUIP")), BreakerUI::TypeCaption, Muted, true)
    ];
    EquipmentColumn->AddSlot().FillHeight(1.0f)
    [
        SNew(SScrollBox) + SScrollBox::Slot()[EquipRows]
    ];

    // Bottom: backpack grid — best rarity first, optional slot filter,
    // cards grow to fit their affix list so nothing truncates. Fixed rows of
    // three, never a wrap box: SWrapBox measured by allotted width inside a
    // scroll box oscillates between two layouts every frame.
    TSharedRef<SVerticalBox> BackpackGrid = SNew(SVerticalBox);
    TSharedPtr<SHorizontalBox> BackpackRow;
    int32 BackpackCardIndex = 0;
    TArray<FBreakerItemInstance> BackpackItems = Equipment ? Equipment->GetBackpack() : TArray<FBreakerItemInstance>();
    Algo::Reverse(BackpackItems);
    BackpackItems.StableSort([](const FBreakerItemInstance& A, const FBreakerItemInstance& B)
    {
        return static_cast<uint8>(A.Rarity) > static_cast<uint8>(B.Rarity);
    });
    const int32 TotalBackpackCount = BackpackItems.Num();
    if (BackpackSlotFilter >= 0)
    {
        BackpackItems.RemoveAll([this](const FBreakerItemInstance& Item)
        {
            return static_cast<int32>(Item.Slot) != BackpackSlotFilter;
        });
    }
    for (const FBreakerItemInstance& Item : BackpackItems)
    {
        const FGuid ItemId = Item.ItemId;

        // Every consequence of clicking this card, answered by the equipment
        // component before the click. The screen states them; it works none of
        // them out itself.
        const FBreakerEquipPreview Preview = Equipment
            ? Equipment->PreviewEquip(Item)
            : UBreakerEquipmentComponent::PreviewEquipAgainst(TArray<FBreakerItemInstance>(), Item);

        // Footer line one: the ordinary slot swap. Gold means "this costs you
        // something", cyan means the action is free.
        const FString DeltaLine = Preview.bSlotOccupied
            ? FString::Printf(TEXT("EQUIP · REPLACES %s i%d"), *RarityName(Preview.SlotDisplaced.Rarity), Preview.SlotDisplaced.ItemLevel)
            : FString(TEXT("EQUIP · SLOT EMPTY"));
        const FLinearColor DeltaColor = Preview.bSlotOccupied ? Amber : Cyan;

        // Footer line two, only when the rarity cap is already met: a SECOND
        // consequence, so it gets a second line. The action is never blocked —
        // it is disclosed (UI-Inventory-Spec "Limit tells"). Items carry no
        // display name yet, so the ejected piece is named by rarity and slot,
        // which is exactly how its own card is titled.
        const bool bLimitTell = Preview.bExceedsRarityLimit && Preview.LimitDisplaced.IsValid();
        const FString LimitLine = bLimitTell
            ? FString::Printf(TEXT("LIMIT FULL %d/%d · EJECTS %s %s i%d"),
                Preview.RarityCount, Preview.RarityLimit,
                *RarityName(Preview.LimitDisplaced.Rarity), *SlotName(Preview.LimitDisplaced.Slot), Preview.LimitDisplaced.ItemLevel)
            : FString();
        const EBreakerEquipSlot DoomedSlot = Preview.LimitDisplaced.Slot;

        const FOnClicked DiscardOne = FOnClicked::CreateLambda([this, ItemId]()
        {
            if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->DiscardFromBackpack(ItemId);
            InventoryStatus = FText::FromString(TEXT("Discarded 1 item."));
            Rebuild(EBreakerMenuScreen::Inventory);
            return FReply::Handled();
        });

        // Fixed rows of two, never a wrap box: SWrapBox measured by allotted
        // width inside a scroll box oscillates between two layouts every
        // frame. Two because the backpack zone is what is left of the panel
        // after the 560 character column and the 400 equipment column.
        if (BackpackCardIndex % 2 == 0)
        {
            BackpackRow = SNew(SHorizontalBox);
            BackpackGrid->AddSlot().AutoHeight()[BackpackRow.ToSharedRef()];
        }
        ++BackpackCardIndex;
        BackpackRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SBox).WidthOverride(300.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    // The wrapper only exists to catch right-click: SButton
                    // leaves non-left buttons unhandled, so they bubble here.
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder")))
                    .Padding(0.0f)
                    .OnMouseButtonDown(FPointerEventHandler::CreateLambda([DiscardOne](const FGeometry&, const FPointerEvent& MouseEvent)
                    {
                        if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton) return DiscardOne.Execute();
                        return FReply::Unhandled();
                    }))
                    [
                        // Card anatomy (UI-Inventory-Spec): line 1 name plus
                        // item level, line 2 rarity and slot, then the affix
                        // list, then a footer stating what clicking costs you.
                        MakeRarityCard(
                            SNew(SButton)
                            .ButtonColorAndOpacity(PanelRaised)
                            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                            .OnClicked(FOnClicked::CreateLambda([this, ItemId]()
                            {
                                if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->EquipFromBackpack(ItemId);
                                Rebuild(EBreakerMenuScreen::Inventory);
                                return FReply::Handled();
                            }))
                            // The hover half of the limit tell. Event-driven on
                            // purpose: this paints one border on enter and
                            // clears it on leave, and never runs on a tick.
                            .OnHovered(FSimpleDelegate::CreateLambda([this, bLimitTell, DoomedSlot]()
                            {
                                if (bLimitTell) SetEquipSlotOutline(DoomedSlot, true);
                            }))
                            .OnUnhovered(FSimpleDelegate::CreateLambda([this, bLimitTell, DoomedSlot]()
                            {
                                if (bLimitTell) SetEquipSlotOutline(DoomedSlot, false);
                            }))
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(RarityName(Item.Rarity)), BreakerUI::TypeH2, RarityColor(Item.Rarity), true)]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(BreakerUI::Space8, 0.0f, 22.0f, 0.0f)[MenuText(FText::FromString(FString::Printf(TEXT("i%d"), Item.ItemLevel)), BreakerUI::TypeCaption, Primary, true)]
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                                [
                                    MenuText(FText::FromString(SlotName(Item.Slot)), BreakerUI::TypeCaption, Muted, true)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                                [
                                    // Line 3 of the card anatomy: every affix
                                    // carrying its delta against the equipped
                                    // piece in this slot.
                                    MakeAffixLines(Item, Preview.AffixDeltas)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                                [
                                    MenuText(FText::FromString(DeltaLine.ToUpper()), BreakerUI::TypeCaption, DeltaColor, true)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                                [
                                    // Harm red, and only present when it is
                                    // true: an always-visible limit line would
                                    // stop meaning anything.
                                    bLimitTell
                                        ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(LimitLine), BreakerUI::TypeCaption, Harm, true))
                                        : SNullWidget::NullWidget
                                ]
                            ],
                            Item.Rarity, true)
                    ]
                ]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(BreakerUI::Space4, BreakerUI::Space4, BreakerUI::Space4, 0.0f)
                [
                    // Discard state: no fill, deep-red ring, harm-red glyph.
                    BorderWrap(
                        SNew(SButton)
                        .ButtonColorAndOpacity(Panel)
                        .ContentPadding(FMargin(BreakerUI::Space8, 1.0f))
                        .ToolTipText(FText::FromString(TEXT("Discard this item (or right-click the card)")))
                        .OnClicked(DiscardOne)
                        [
                            MenuText(FText::FromString(TEXT("X")), BreakerUI::TypeCaption, Harm, true)
                        ],
                        HarmDeep)
                ]
            ]
        ];
    }

    // Slot filter row: ALL plus one chip per equipment slot.
    TSharedRef<SHorizontalBox> FilterRow = SNew(SHorizontalBox);
    auto AddFilterChip = [this, &FilterRow](const FString& Label, int32 FilterValue)
    {
        const bool bSelectedChip = BackpackSlotFilter == FilterValue;
        FilterRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space4, 0.0f)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bSelectedChip ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
                .OnClicked(FOnClicked::CreateLambda([this, FilterValue]()
                {
                    BackpackSlotFilter = FilterValue;
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bSelectedChip ? Primary : Muted, true)
                ],
                bSelectedChip ? Cyan : BorderEmphasis,
                bSelectedChip ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    };
    AddFilterChip(TEXT("ALL"), -1);
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBreakerEquipSlot::Count); ++SlotIndex)
    {
        AddFilterChip(SlotName(static_cast<EBreakerEquipSlot>(SlotIndex)), SlotIndex);
    }

    // Clean-up chips. First click arms (gold, "CONFIRM"), second click opens
    // the confirmation modal — it never destroys anything directly. Any other
    // interaction rebuilds and disarms.
    TSharedRef<SHorizontalBox> CleanupRow = SNew(SHorizontalBox);
    auto AddCleanupChip = [this, &CleanupRow](const FString& Label, int32 ArmIndex)
    {
        const bool bArmed = CleanupArmedIndex == ArmIndex;
        // Two-step arm: the button turns gold and reads CONFIRM. Armed carries
        // the 2px gold ring, disarmed reads as a destructive control (deep-red
        // ring, harm text).
        CleanupRow->AddSlot().AutoWidth().Padding(BreakerUI::Space4, 0.0f, 0.0f, 0.0f)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bArmed ? PanelHover : Panel)
            .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
            .OnClicked(FOnClicked::CreateLambda([this, ArmIndex, bArmed]()
            {
                if (bArmed)
                {
                    // The modal is the only thing that can destroy: it states
                    // the count and the exclusions first.
                    DiscardModalIndex = ArmIndex;
                    PendingCleanupArm = ArmIndex;
                }
                else
                {
                    PendingCleanupArm = ArmIndex;
                }
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(bArmed ? FString(TEXT("CONFIRM")) : Label), BreakerUI::TypeCaption, bArmed ? Amber : Harm, true)
            ],
            bArmed ? Amber : HarmDeep,
            bArmed ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    };
    AddCleanupChip(TEXT("DISCARD < UNCOMMON"), 0);
    AddCleanupChip(TEXT("DISCARD < EXCEPTIONAL"), 1);

    // Dev gear grants ride the same playtest flag as dev class swap.
    bool bDevTools = false;
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), bDevTools, GGameUserSettingsIni);
    TSharedRef<SHorizontalBox> DevRow = SNew(SHorizontalBox);
    if (bDevTools)
    {
        DevRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("DEV:")), 9, Amber, true)
        ];
        auto AddDevChip = [this, &DevRow](int32 ItemLevel)
        {
            DevRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(PanelRaised)
                .ContentPadding(FMargin(9.0f, 4.0f))
                .OnClicked(FOnClicked::CreateLambda([this, ItemLevel]()
                {
                    if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->DevGrantTestGear(ItemLevel);
                    InventoryStatus = FText::FromString(FString::Printf(TEXT("Granted a full Exceptional set at ilvl %d."), ItemLevel));
                    Rebuild(EBreakerMenuScreen::Inventory);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("GRANT TEST GEAR ilvl %d"), ItemLevel)), 9, Primary, true)
                ]
            ];
        };
        AddDevChip(30);
        AddDevChip(50);
    }

    // ---- Backpack zone -----------------------------------------------------
    // Filter bar 64 tall carrying the slot chips and the input hint, then the
    // card grid. The spec puts the input hints here, which is why the screen
    // has no footer.
    TSharedRef<SVerticalBox> BackpackColumn = SNew(SVerticalBox);
    BackpackColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        SNew(SBox).HeightOverride(64.0f)
        [
            MakePlate(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
                    [
                        MenuText(FText::FromString(FString::Printf(TEXT("BACKPACK %d/%d"), BackpackItems.Num(), TotalBackpackCount)), BreakerUI::TypeCaption, Primary, true)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[FilterRow]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        MenuText(FText::FromString(TEXT("RMB / X DISCARD · LMB EQUIP")), BreakerUI::TypeCaption, Muted, true)
                    ]
                ],
                Panel, BorderEmphasis, FMargin(BreakerUI::Space16, BreakerUI::Space8))
        ]
    ];
    if (bDevTools)
    {
        BackpackColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[DevRow];
    }
    if (!InventoryStatus.IsEmpty())
    {
        BackpackColumn->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(InventoryStatus, BreakerUI::TypeCaption, Amber, true)
        ];
    }
    // The empty backpack is the one place the screen teaches the world: the
    // five rarity beams as vertical bars, and the single line that ties them
    // to what the player sees on the ground.
    TSharedRef<SWidget> EmptyBackpack =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space40, 0.0f, 0.0f)
        [
            MakeRarityBeams()
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("LOOT IS FOUND BY COLOUR")), BreakerUI::TypeH2, Primary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
        [
            MenuText(FText::FromString(TEXT("EMPTY · ENEMY KILLS DROP ROLLED ITEMS")), BreakerUI::TypeCaption, Muted, true)
        ];

    BackpackColumn->AddSlot().FillHeight(1.0f)
    [
        BackpackItems.IsEmpty()
            ? EmptyBackpack
            : StaticCastSharedRef<SWidget>(SNew(SScrollBox) + SScrollBox::Slot()[BackpackGrid])
    ];

    // ---- Header band -------------------------------------------------------
    // The two equip-limit counters live here permanently, so the constraint is
    // never a surprise at click time. Both the counts and the caps come from
    // the equipment component: the screen must never hold a second opinion
    // about a rule that decides which of the player's items gets ejected.
    const int32 AberrantEquipped = Equipment ? Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant) : 0;
    const int32 AnomalousEquipped = Equipment ? Equipment->CountEquippedOfRarity(EBreakerItemRarity::Anomalous) : 0;
    const int32 AberrantLimit = UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant);
    const int32 AnomalousLimit = UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Anomalous);

    auto MakeLimitChip = [](const FString& Label, int32 Count, int32 Limit, const FLinearColor& Rail, bool bFullBorder) -> TSharedRef<SWidget>
    {
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(Label), BreakerUI::TypeCaption, Muted, true)]
            + SVerticalBox::Slot().AutoHeight()
            [
                MenuText(FText::FromString(FString::Printf(TEXT("%d/%d"), Count, Limit)), BreakerUI::TypeH2,
                    Count >= Limit ? Rail : Primary, true)
            ],
            PanelRaised, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space4), false,
            bFullBorder ? Rail : BreakerUI::BorderRest);
    };

    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[BuildScreenTabs(EBreakerMenuScreen::Inventory)];
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    // O11: up to three Aberrant equipped, one Anomalous. Aberrant takes the
    // harm rail (it shares that hue by design); Anomalous is the one rarity
    // that is also a world object class, so it takes the teal rail AND the
    // full teal border — the single legal teal on this screen.
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
    [
        MakeLimitChip(TEXT("ABERRANT"), AberrantEquipped, AberrantLimit, BreakerUI::RarityAberrant, false)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
    [
        MakeLimitChip(TEXT("ANOMALOUS"), AnomalousEquipped, AnomalousLimit, BreakerUI::RarityAnomalous, true)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[CleanupRow];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    // Meta line. Gear score is the sum of equipped item levels — O2
    // PLACEHOLDER, the shipping formula is not authored yet.
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    int32 GearScore = 0;
    if (Equipment)
    {
        for (const FBreakerItemInstance& EquippedItem : Equipment->GetEquipped())
        {
            if (EquippedItem.IsValid()) GearScore += EquippedItem.ItemLevel;
        }
    }
    const FString MetaLine = FString::Printf(TEXT("BREAKER · %s · LV %d · GEAR SCORE %s"),
        *ClassDisplayName(Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None),
        Progression ? Progression->GetProgressionState().CharacterLevel : 1,
        *BreakerUI::FormatTicker(static_cast<float>(GearScore)));

    // ---- Zones -------------------------------------------------------------
    TSharedRef<SHorizontalBox> Body = SNew(SHorizontalBox);
    Body->AddSlot().AutoWidth()
    [
        SNew(SBox).WidthOverride(560.0f)[CharacterColumn]
    ];
    Body->AddSlot().AutoWidth().Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(400.0f)[EquipmentColumn]
    ];
    Body->AddSlot().FillWidth(1.0f).Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
    [
        BackpackColumn
    ];

    // No footer by design (UI-Inventory-Spec "Zones"): the input hints live in
    // the backpack filter bar and BACK sits in the header band.
    TSharedRef<SWidget> Screen = BuildZonedFrame(
        FText::FromString(TEXT("LOADOUT")),
        FText::FromString(MetaLine),
        HeaderRight,
        Body,
        SNullWidget::NullWidget,
        1760.0f);

    if (DiscardModalIndex < 0) return Screen;

    // The confirmation modal sits above the whole screen, not inside a zone:
    // it is the last thing between the player and an irreversible action.
    const EBreakerItemRarity MinimumKept = CleanupThresholdForArm(DiscardModalIndex);
    const int32 DoomedCount = Equipment ? Equipment->CountBackpackBelowRarity(MinimumKept) : 0;
    return SNew(SOverlay)
        + SOverlay::Slot()[Screen]
        + SOverlay::Slot()[BuildDiscardModal(DiscardModalIndex, MinimumKept, DoomedCount)];
}

TSharedRef<SWidget> SBreakerMenu::BuildDiscardModal(int32 ArmIndex, EBreakerItemRarity MinimumKept, int32 Count)
{
    const FString Threshold = RarityName(MinimumKept);
    // The count is the equipment component's own answer, produced by the same
    // predicate the discard uses — the modal cannot promise a different number
    // from the one it destroys.
    TSharedRef<SVerticalBox> Plate = SNew(SVerticalBox);
    Plate->AddSlot().AutoHeight()
    [
        MenuText(FText::FromString(TEXT("DESTROY BACKPACK ITEMS")), BreakerUI::TypeH1, Primary, true)
    ];
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(FString::Printf(TEXT("%d backpack item%s below %s will be destroyed. This cannot be undone."),
            Count, Count == 1 ? TEXT("") : TEXT("s"), *Threshold)), BreakerUI::TypeBody, SoftText)
    ];
    // The exclusions, stated rather than assumed. Both are properties of the
    // component: equipped gear is a separate container, and Aberrant and
    // Anomalous sit above every threshold this screen offers.
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("NEVER INCLUDED\n· EQUIPPED GEAR\n· ABERRANT\n· ANOMALOUS")), BreakerUI::TypeCaption, Muted, true)
    ];

    TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
    Actions->AddSlot().AutoWidth()
    [
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Panel)
            .ContentPadding(FMargin(BreakerUI::Space24, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this]()
            {
                DiscardModalIndex = -1;
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(TEXT("CANCEL")), BreakerUI::TypeCaption, Primary, true)
            ],
            BorderEmphasis)
    ];
    Actions->AddSlot().AutoWidth().Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        // The destructive control: the count in the label, harm-red text on
        // the destructive face, harm-red ring. Nothing else on the screen
        // looks like this.
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(BreakerUI::DestructiveFace)
            .ContentPadding(FMargin(BreakerUI::Space24, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this, ArmIndex]()
            {
                const int32 Removed = Character.IsValid() && Character->GetEquipment()
                    ? Character->GetEquipment()->DiscardBackpackBelowRarity(CleanupThresholdForArm(ArmIndex))
                    : 0;
                InventoryStatus = FText::FromString(FString::Printf(TEXT("Destroyed %d item%s."), Removed, Removed == 1 ? TEXT("") : TEXT("s")));
                DiscardModalIndex = -1;
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(FString::Printf(TEXT("DESTROY %d"), Count)), BreakerUI::TypeCaption, Harm, true)
            ],
            Harm, BreakerUI::BorderSelected)
    ];
    Plate->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f).HAlign(HAlign_Right)[Actions];

    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            // The scrim both dims the screen and swallows clicks, so the
            // controls behind a modal cannot be operated through it.
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
            SNew(SBox).WidthOverride(560.0f)
            [
                // The destructive face on a harm-red rail: the only plate in
                // the system that is not part of the panel ramp.
                MakePlate(Plate, BreakerUI::DestructiveFace, Harm,
                    FMargin(BreakerUI::Space24, BreakerUI::Space24), false, Harm)
            ]
        ];
}

void SBreakerMenu::SetEquipSlotOutline(EBreakerEquipSlot Slot, bool bDoomed)
{
    if (const TWeakPtr<SBorder>* Found = EquipSlotOutlines.Find(Slot))
    {
        if (const TSharedPtr<SBorder> Outline = Found->Pin())
        {
            // Harm red while a card that would eject this piece is hovered,
            // the screen field otherwise — which reads as no outline at all.
            Outline->SetBorderBackgroundColor(FSlateColor(bDoomed ? Harm : Background));
        }
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildClassSelectScreen()
{
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const EBreakerClassId CurrentClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;

    struct FClassEntry { EBreakerClassId ClassId; const TCHAR* Name; const TCHAR* Resource; const TCHAR* Branches; const TCHAR* Pitch; };
    static const FClassEntry Classes[] =
    {
        { EBreakerClassId::Swift,    TEXT("SWIFT"),    TEXT("MOMENTUM"), TEXT("Frenzy / Kinetic / Marksman"),          TEXT("Speed is the build. Movement generates power.") },
        { EBreakerClassId::Caster,   TEXT("CASTER"),   TEXT("MANA"),     TEXT("Spellblade / Void Whisperer / Multispell"), TEXT("Statuses, reactions, and ability-driven combat.") },
        { EBreakerClassId::Gunsmith, TEXT("GUNSMITH"), TEXT("SCRAP"),    TEXT("Armory / Field Tech / Tinkerer"),       TEXT("Deployables and weapon mastery.") },
        { EBreakerClassId::Tank,     TEXT("TANK"),     TEXT("GRIT"),     TEXT("Leech / Bastion / Demolitionist"),      TEXT("Mitigation becomes fuel. Hold the line.") },
        { EBreakerClassId::Support,  TEXT("SUPPORT"),  TEXT("CHARGE"),   TEXT("Medic / Conductor / Warden"),           TEXT("Amplify, sustain, control — solo viable.") },
    };

    bool bDevClassSwap = false;
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), bDevClassSwap, GGameUserSettingsIni);

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    if (CurrentClass != EBreakerClassId::None)
    {
        const FClassEntry* Locked = nullptr;
        for (const FClassEntry& Entry : Classes) if (Entry.ClassId == CurrentClass) Locked = &Entry;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("CLASS LOCKED: %s — class selection is permanent per character."), Locked ? Locked->Name : TEXT("UNKNOWN"))), 13, Cyan, true)
        ];
    }
    else
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            MenuText(FText::FromString(TEXT("Selection is PERMANENT for this character. Branches and abilities arrive with the class kits.")), 11, SoftText)
        ];
    }

    for (const FClassEntry& Entry : Classes)
    {
        const bool bIsCurrent = Entry.ClassId == CurrentClass;
        const bool bSelectable = CurrentClass == EBreakerClassId::None || bDevClassSwap;
        const EBreakerClassId CapturedClass = Entry.ClassId;
        const bool bCapturedDevSwap = bDevClassSwap;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bSelectable ? PanelRaised : BreakerUI::BgRaised)
            .IsEnabled(bSelectable)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space16))
            .OnClicked(FOnClicked::CreateLambda([this, CapturedClass, bCapturedDevSwap]()
            {
                if (Character.IsValid() && Character->GetProgression())
                {
                    UBreakerProgressionComponent* Progression = Character->GetProgression();
                    if (bCapturedDevSwap) Progression->DevForceClass(CapturedClass);
                    if (bCapturedDevSwap || Progression->ChoosePermanentClassById(CapturedClass)) Character->SaveGameState();
                }
                Rebuild(EBreakerMenuScreen::ClassSelect);
                return FReply::Handled();
            }))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(Entry.Name), BreakerUI::TypeH2, bSelectable ? Primary : Disabled, true)]
                    + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(Entry.Resource), BreakerUI::TypeCaption, bIsCurrent ? Cyan : Muted, true)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)[MenuText(FText::FromString(Entry.Branches), BreakerUI::TypeCaption, Muted, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)[MenuText(FText::FromString(Entry.Pitch), BreakerUI::TypeCaption, SoftText)]
            ],
            // Locked-in class carries the accent ring; everything else keeps
            // the same geometry on the neutral rest border.
            bIsCurrent ? Cyan : BorderRest,
            bIsCurrent ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 10.0f)
    [
        SNew(SCheckBox)
        .IsChecked(bDevClassSwap ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
        {
            GConfig->SetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), State == ECheckBoxState::Checked, GGameUserSettingsIni);
            GConfig->Flush(false, GGameUserSettingsIni);
            Rebuild(EBreakerMenuScreen::ClassSelect);
        })
        [
            MenuText(FText::FromString(TEXT("DEV MODE — allow class swap (playtest only)")), 11, SoftText, true)
        ]
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 0.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    return BuildFrame(FText::FromString(TEXT("BREAKER CLASS")), FText::FromString(TEXT("PERMANENT SELECTION / FIVE DISCIPLINES")), Body, 860.0f);
}

namespace
{
    // ---------------------------------------------------------------------
    // Progression adapter.
    //
    // Every call this screen makes into UBreakerProgressionComponent goes
    // through one of these one-line shims. The progression API is being
    // extended concurrently (fallback tree content, purchase/enumeration
    // helpers); when a signature lands or changes, the fix is one line in
    // this block rather than a sweep through the Slate tree below.
    // ---------------------------------------------------------------------

    TArray<const UBreakerProgressionTree*> ProgressionGatherTrees(UBreakerProgressionComponent* Progression)
    {
        TArray<const UBreakerProgressionTree*> Trees;
        if (!Progression) return Trees;
        // INTEGRATION: expected enumerator is
        //   TArray<const UBreakerProgressionTree*> UBreakerProgressionComponent::GetAvailableTrees() const
        // (with a static fallback-content variant supplying the class tree and
        // the six core constellations). No such method exists on the header
        // this file was written against, so we walk the only reachable source
        // today: the class definition's branch trees. When the enumerator
        // lands, replace this whole body with the single call.
        if (const UBreakerClassDefinition* ClassDef = Progression->ClassDefinition)
        {
            for (const UBreakerProgressionTree* Tree : ClassDef->BranchTrees)
            {
                if (Tree) Trees.Add(Tree);
            }
        }
        return Trees;
    }

    int32 ProgressionGetNodeRank(UBreakerProgressionComponent* Progression, FName NodeId, EBreakerPointCurrency Currency)
    {
        return Progression ? Progression->GetNodeRank(NodeId, Currency) : 0;
    }

    int32 ProgressionGetUnspent(UBreakerProgressionComponent* Progression, EBreakerPointCurrency Currency)
    {
        if (!Progression) return 0;
        const FBreakerProgressionState& ProgState = Progression->GetProgressionState();
        return Currency == EBreakerPointCurrency::ClassPoints ? ProgState.UnspentClassPoints : ProgState.UnspentCorePoints;
    }

    bool ProgressionPurchaseNode(UBreakerProgressionComponent* Progression, const UBreakerProgressionTree* Tree, FName NodeId, FText& OutFailureReason)
    {
        if (!Progression || !Tree)
        {
            OutFailureReason = FText::FromString(TEXT("No progression component."));
            return false;
        }
        return Progression->PurchaseNode(Tree, NodeId, OutFailureReason);
    }

    bool ProgressionRespec(UBreakerProgressionComponent* Progression, EBreakerPointCurrency Currency, FText& OutFailureReason)
    {
        if (!Progression)
        {
            OutFailureReason = FText::FromString(TEXT("No progression component."));
            return false;
        }
        // bIsAtForge is passed true unconditionally: Forge-proximity gating
        // arrives with the hub. Until then respec is always available from
        // the menu so the flow is testable. UI-UX-Spec 6.6 wants the button
        // visible-but-disabled away from a Forge — wire that here when the
        // hub exists.
        return Progression->RespecAtForge(Currency, /*bIsAtForge=*/true, OutFailureReason);
    }

    // Points already committed to a tree, and the tree's full cost if every
    // node were maxed. Derived locally from node ranks so it needs no new
    // progression API.
    void ProgressionTreeInvestment(UBreakerProgressionComponent* Progression, const UBreakerProgressionTree* Tree, int32& OutSpent, int32& OutTotal)
    {
        OutSpent = 0;
        OutTotal = 0;
        if (!Tree) return;
        for (const UBreakerProgressionNode* Node : Tree->Nodes)
        {
            if (!Node) continue;
            OutSpent += ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) * Node->CostPerRank;
            OutTotal += Node->MaxRank * Node->CostPerRank;
        }
    }

    // Card body is one short generic line: the first sentence, clipped.
    FString ShortSummary(const FString& Description, int32 MaxLength = 60)
    {
        FString Line = Description;
        int32 SentenceEnd = INDEX_NONE;
        if (Line.FindChar(TEXT('.'), SentenceEnd)) Line = Line.Left(SentenceEnd + 1);
        Line.ReplaceInline(TEXT("\n"), TEXT(" "));
        Line.ReplaceInline(TEXT("\r"), TEXT(""));
        Line.TrimStartAndEndInline();
        if (Line.Len() > MaxLength) Line = Line.Left(MaxLength - 1).TrimEnd() + TEXT("…");
        return Line;
    }

    // Short, player-facing stat names. UEnum display names read like code
    // ("DamageOverTime"); the card has room for two words, not a symbol.
    FString StatTargetLabel(EBreakerNodeStatTarget Target)
    {
        switch (Target)
        {
        case EBreakerNodeStatTarget::CriticalChance: return TEXT("CRIT CHANCE");
        case EBreakerNodeStatTarget::CriticalDamage: return TEXT("CRIT DAMAGE");
        case EBreakerNodeStatTarget::MoveSpeed:      return TEXT("MOVE SPEED");
        case EBreakerNodeStatTarget::SlideSpeed:     return TEXT("SLIDE SPEED");
        case EBreakerNodeStatTarget::AirControl:     return TEXT("AIR CONTROL");
        case EBreakerNodeStatTarget::DodgeChance:    return TEXT("DODGE CHANCE");
        case EBreakerNodeStatTarget::BlockChance:    return TEXT("BLOCK CHANCE");
        case EBreakerNodeStatTarget::Health:         return TEXT("HEALTH");
        case EBreakerNodeStatTarget::DamageOverTime: return TEXT("DOT DAMAGE");
        default:                                     return TEXT("STAT");
        }
    }

    // "+12% MOVE SPEED" / "+7 CRIT CHANCE". Flat crit/dodge/block values are
    // authored in whole points, so they carry no percent sign here even
    // though they land as chance fractions.
    FString FormatNodeEffect(const FBreakerNodeEffect& Effect)
    {
        const bool bPercent = Effect.StatBucket != EBreakerNodeStatBucket::Flat;
        const FString Number = FString::Printf(TEXT("%+g"), Effect.ValuePerRank);
        return FString::Printf(TEXT("%s%s %s"), *Number, bPercent ? TEXT("%") : TEXT(""), *StatTargetLabel(Effect.StatTarget));
    }

    // The one line that tells a player what they are buying. Stat nodes show
    // their strongest-reading first effect; rule and verb nodes have no
    // number to show, so they name what they grant instead.
    FString PrimaryEffectLine(const UBreakerProgressionNode* Node)
    {
        if (!Node) return FString();
        if (Node->Effects.Num() > 0)
        {
            FString Line = FormatNodeEffect(Node->Effects[0]) + TEXT(" / RANK");
            if (Node->Effects.Num() > 1) Line += FString::Printf(TEXT("  (+%d MORE)"), Node->Effects.Num() - 1);
            return Line;
        }
        if (Node->GrantedAbilityIds.Num() > 0)
        {
            return FString::Printf(TEXT("GRANTS %s"), *Node->GrantedAbilityIds[0].ToString().ToUpper());
        }
        return TEXT("CHANGES A RULE");
    }

    // Post-purchase status line: name, every effect at its per-rank value,
    // and the rank the player just reached.
    FString PurchaseFeedback(const UBreakerProgressionNode* Node, int32 NewRank)
    {
        if (!Node) return TEXT("ALLOCATED");
        const FString Name = Node->DisplayName.IsEmpty() ? Node->NodeId.ToString().ToUpper() : Node->DisplayName.ToString().ToUpper();
        TArray<FString> Parts;
        for (const FBreakerNodeEffect& Effect : Node->Effects) Parts.Add(FormatNodeEffect(Effect));
        for (const FName AbilityId : Node->GrantedAbilityIds) Parts.Add(FString::Printf(TEXT("GRANTS %s"), *AbilityId.ToString().ToUpper()));
        if (Parts.Num() == 0) Parts.Add(TEXT("RULE CHANGE ACTIVE"));
        return FString::Printf(TEXT("%s  %s  (RANK %d/%d)"), *Name, *FString::Join(Parts, TEXT("  ")), NewRank, Node->MaxRank);
    }

    // Owned rank reads as a word, not a row of small circles the owner had
    // to squint at.
    FString RankLabel(int32 Rank, int32 MaxRank)
    {
        return FString::Printf(TEXT("RANK %d/%d"), Rank, MaxRank);
    }

    // Selector buttons are 240px wide; long authored tree names clip there
    // ("CORE CONSTELLATIONS (S..."). Short display aliases live here rather
    // than in the content library so authored names stay descriptive.
    FString TreeSelectorLabel(const UBreakerProgressionTree* Tree)
    {
        if (!Tree) return FString();
        if (Tree->TreeId == FName(TEXT("Core.Slice"))) return TEXT("CORE SLICE");
        const FString Name = Tree->DisplayName.IsEmpty() ? Tree->TreeId.ToString() : Tree->DisplayName.ToString();
        return Name.ToUpper();
    }

    FString CurrencyLabel(EBreakerPointCurrency Currency)
    {
        return Currency == EBreakerPointCurrency::ClassPoints ? TEXT("CLASS") : TEXT("CORE");
    }

    // INTEGRATION: the progression API is expected to grow
    //   bool UBreakerProgressionComponent::CanPurchase(const UBreakerProgressionTree*, FName, FText& OutReason) const
    // Until it exists this mirrors the purchase rules the component enforces
    // (max rank, unspent points, investment gate, prerequisite ranks) so the
    // cards can render a lock reason without attempting a purchase. Delete
    // this function and forward to CanPurchase when it lands — the returned
    // reason text is already shaped for direct display.
    bool SkillNodeIsPurchasable(UBreakerProgressionComponent* Progression, const UBreakerProgressionTree* Tree, const UBreakerProgressionNode* Node, int32 TreeSpent, FString& OutLockReason)
    {
        OutLockReason.Reset();
        if (!Progression || !Tree || !Node)
        {
            OutLockReason = TEXT("NO DATA");
            return false;
        }
        if (ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) >= Node->MaxRank)
        {
            OutLockReason = TEXT("MAX RANK");
            return false;
        }
        if (Node->RequiredTreeInvestment > TreeSpent)
        {
            OutLockReason = FString::Printf(TEXT("REQUIRES %d INVESTED (%d)"), Node->RequiredTreeInvestment, TreeSpent);
            return false;
        }
        for (const FBreakerNodePrerequisite& Prereq : Node->Prerequisites)
        {
            const int32 HeldRank = ProgressionGetNodeRank(Progression, Prereq.NodeId, Node->Currency);
            if (HeldRank < Prereq.RequiredRank)
            {
                const UBreakerProgressionNode* PrereqNode = nullptr;
                for (const UBreakerProgressionNode* Candidate : Tree->Nodes)
                {
                    if (Candidate && Candidate->NodeId == Prereq.NodeId) PrereqNode = Candidate;
                }
                const FString PrereqName = PrereqNode ? PrereqNode->DisplayName.ToString() : Prereq.NodeId.ToString();
                OutLockReason = FString::Printf(TEXT("NEEDS %s RANK %d"), *PrereqName.ToUpper(), Prereq.RequiredRank);
                return false;
            }
        }
        for (const FName ExclusiveId : Node->MutuallyExclusiveNodeIds)
        {
            if (ProgressionGetNodeRank(Progression, ExclusiveId, Node->Currency) > 0)
            {
                OutLockReason = FString::Printf(TEXT("LOCKED OUT BY %s"), *ExclusiveId.ToString().ToUpper());
                return false;
            }
        }
        if (ProgressionGetUnspent(Progression, Node->Currency) < Node->CostPerRank)
        {
            OutLockReason = FString::Printf(TEXT("NEEDS %d %s POINTS"), Node->CostPerRank, *CurrencyLabel(Node->Currency));
            return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Path-board node kinds (UI-Skill-Tree-Spec "Class <-> Core").
    //
    // INTEGRATION: UBreakerProgressionNode carries no node-kind field, so the
    // kind is derived from rules the content already states. When a Kind enum
    // lands on the node asset, delete this and read it directly.
    //   Keystone    — the authored cornerstone flag
    //   Minor       — anything multi-rank (the rank prints inside the marker)
    //   Convergence — single-rank nodes costing 3+ points (the O21 promotion
    //                 tier: Fixate/Necrosis/Reflex all sit here)
    //   Notable     — everything else
    // -----------------------------------------------------------------------
    enum class ESkillMarkerKind : uint8 { Minor, Notable, Convergence, Keystone };

    ESkillMarkerKind ClassifyNode(const UBreakerProgressionNode* Node)
    {
        if (!Node) return ESkillMarkerKind::Minor;
        if (Node->bCornerstone) return ESkillMarkerKind::Keystone;
        if (Node->MaxRank > 1) return ESkillMarkerKind::Minor;
        if (Node->CostPerRank >= 3) return ESkillMarkerKind::Convergence;
        return ESkillMarkerKind::Notable;
    }

    float MarkerSize(ESkillMarkerKind Kind)
    {
        switch (Kind)
        {
            case ESkillMarkerKind::Notable:     return 44.0f;
            case ESkillMarkerKind::Convergence: return 64.0f;
            case ESkillMarkerKind::Keystone:    return 60.0f;
            default:                            return 48.0f;
        }
    }

    bool MarkerIsDiamond(ESkillMarkerKind Kind)
    {
        return Kind == ESkillMarkerKind::Notable || Kind == ESkillMarkerKind::Keystone;
    }

    // Convergence and Keystone label to the RIGHT of the marker so the trunk
    // never runs through their text.
    bool MarkerLabelsRight(ESkillMarkerKind Kind)
    {
        return Kind == ESkillMarkerKind::Convergence || Kind == ESkillMarkerKind::Keystone;
    }

    FString MarkerKindLabel(ESkillMarkerKind Kind)
    {
        switch (Kind)
        {
            case ESkillMarkerKind::Notable:     return TEXT("NOTABLE");
            case ESkillMarkerKind::Convergence: return TEXT("CONVERGENCE");
            case ESkillMarkerKind::Keystone:    return TEXT("KEYSTONE");
            default:                            return TEXT("MINOR");
        }
    }

    // Everything the hover rail prints, captured by value at build time. The
    // hover handler therefore touches no live widget tree and no attribute —
    // the rail is event-driven, never polled.
    struct FSkillNodeView
    {
        FString Name;
        FString Kind;
        FString RankLine;
        FString CostLine;
        FString Description;
        FString ActionLine;
        FString GateLine;
        TArray<FString> EffectLines;
        TArray<FString> PrereqLines;
        bool bOwned = false;
        bool bPurchasable = false;
        bool bMaxed = false;
    };

    FSkillNodeView MakeSkillNodeView(const UBreakerProgressionNode* Node, int32 Rank, bool bPurchasable,
        const FString& LockReason, int32 TreeSpent)
    {
        FSkillNodeView View;
        if (!Node) return View;
        const ESkillMarkerKind Kind = ClassifyNode(Node);
        View.Name = Node->DisplayName.IsEmpty() ? Node->NodeId.ToString().ToUpper() : Node->DisplayName.ToString().ToUpper();
        View.Kind = MarkerKindLabel(Kind) + (Node->bCornerstone ? TEXT("  ·  CORNERSTONE") : TEXT(""));
        View.bOwned = Rank > 0;
        View.bMaxed = Rank >= Node->MaxRank;
        View.bPurchasable = bPurchasable;
        View.RankLine = View.bMaxed ? FString(TEXT("MAXED")) : RankLabel(Rank, Node->MaxRank);
        View.CostLine = FString::Printf(TEXT("%d %s PER RANK"), Node->CostPerRank, *CurrencyLabel(Node->Currency));
        View.Description = Node->Description.IsEmpty() ? TEXT("—") : Node->Description.ToString();
        View.ActionLine = View.bMaxed
            ? FString(TEXT("MAXED"))
            : (bPurchasable ? FString::Printf(TEXT("%d PT -> RANK %d"), Node->CostPerRank, Rank + 1) : LockReason);
        for (const FBreakerNodeEffect& Effect : Node->Effects) View.EffectLines.Add(FormatNodeEffect(Effect));
        for (const FName AbilityId : Node->GrantedAbilityIds) View.EffectLines.Add(FString::Printf(TEXT("GRANTS %s"), *AbilityId.ToString().ToUpper()));
        for (const FBreakerNodePrerequisite& Prereq : Node->Prerequisites)
        {
            View.PrereqLines.Add(FString::Printf(TEXT("%s RANK %d"), *Prereq.NodeId.ToString().ToUpper(), Prereq.RequiredRank));
        }
        if (Node->RequiredTreeInvestment > 0)
        {
            View.GateLine = FString::Printf(TEXT("TIER GATE %d / %d"), TreeSpent, Node->RequiredTreeInvestment);
        }
        return View;
    }

    // The 420px rail's card. Fixed width comes from the host box, so this can
    // grow vertically without ever moving the board.
    TSharedRef<SWidget> MakeSkillDetailCard(const FSkillNodeView& View)
    {
        const FLinearColor Rail = View.bOwned ? BreakerUI::Cyan
            : (View.bPurchasable ? BreakerUI::Gold : BreakerUI::BorderEmphasis);

        TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
        Column->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(View.Name))
            .ColorAndOpacity(BreakerUI::TextPrimary)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeH2))
            .AutoWrapText(true)
        ];
        Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, BreakerUI::Space8)
        [
            MenuText(FText::FromString(View.Kind), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
        ];
        Column->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(View.RankLine), BreakerUI::TypeCaption, View.bOwned ? BreakerUI::Cyan : BreakerUI::TextMuted, true)]
            + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(View.CostLine), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)]
        ];
        Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, BreakerUI::Space8)
        [
            SNew(SBox).HeightOverride(BreakerUI::BorderThin)[SolidBlock(BreakerUI::BorderRest)]
        ];
        Column->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(View.Description))
            .ColorAndOpacity(BreakerUI::TextSecondary)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeBody))
            .AutoWrapText(true)
        ];
        if (View.EffectLines.Num() > 0)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
            [
                MenuText(FText::FromString(TEXT("EFFECTS PER RANK")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
            for (const FString& Line : View.EffectLines)
            {
                Column->AddSlot().AutoHeight()[MenuText(FText::FromString(Line), BreakerUI::TypeCaption, BreakerUI::TextPrimary, true)];
            }
        }
        if (View.PrereqLines.Num() > 0)
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, BreakerUI::Space4)
            [
                MenuText(FText::FromString(TEXT("PREREQUISITES")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
            for (const FString& Line : View.PrereqLines)
            {
                Column->AddSlot().AutoHeight()[MenuText(FText::FromString(Line), BreakerUI::TypeCaption, BreakerUI::TextSecondary, true)];
            }
        }
        if (!View.GateLine.IsEmpty())
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
            [
                MenuText(FText::FromString(View.GateLine), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)
            ];
        }
        if (!View.ActionLine.IsEmpty())
        {
            Column->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
            [
                MenuText(FText::FromString(View.ActionLine), BreakerUI::TypeCaption,
                    View.bMaxed ? BreakerUI::Cyan : (View.bPurchasable ? BreakerUI::Gold : BreakerUI::Harm), true)
            ];
        }
        return MakePlate(Column, BreakerUI::Panel10, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space16));
    }

    // Rest state of the rail. It is the same plate geometry as a populated
    // card, so the column never changes shape when a node is hovered.
    TSharedRef<SWidget> MakeSkillDetailPlaceholder()
    {
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(TEXT("NODE DETAIL")), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Hover a marker to read its full detail here. This column never changes width, so the board does not move when it fills.")))
                .ColorAndOpacity(BreakerUI::TextSecondary)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeBody))
                .AutoWrapText(true)
            ],
            BreakerUI::Panel00, BreakerUI::BorderEmphasis, FMargin(BreakerUI::Space16, BreakerUI::Space16));
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildSkillTreesScreen()
{
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const TArray<const UBreakerProgressionTree*> Trees = ProgressionGatherTrees(Progression);

    // One tab pair, not a mode toggle: the board swaps, the header and the
    // detail rail persist.
    TArray<const UBreakerProgressionTree*> ClassTrees;
    TArray<const UBreakerProgressionTree*> CoreTrees;
    for (const UBreakerProgressionTree* Tree : Trees)
    {
        if (!Tree) continue;
        if (Tree->Currency == EBreakerPointCurrency::ClassPoints) ClassTrees.Add(Tree);
        else CoreTrees.Add(Tree);
    }
    if (SkillBoardTab == 0 && ClassTrees.IsEmpty() && !CoreTrees.IsEmpty()) SkillBoardTab = 1;
    if (SkillBoardTab == 1 && CoreTrees.IsEmpty() && !ClassTrees.IsEmpty()) SkillBoardTab = 0;
    const bool bCoreBoard = SkillBoardTab == 1;

    const int32 UnspentClass = ProgressionGetUnspent(Progression, EBreakerPointCurrency::ClassPoints);
    const int32 UnspentCore = ProgressionGetUnspent(Progression, EBreakerPointCurrency::CorePoints);

    int32 ClassSpent = 0;
    int32 CoreSpent = 0;
    for (const UBreakerProgressionTree* Tree : ClassTrees)
    {
        int32 Spent = 0;
        int32 Total = 0;
        ProgressionTreeInvestment(Progression, Tree, Spent, Total);
        ClassSpent += Spent;
    }
    for (const UBreakerProgressionTree* Tree : CoreTrees)
    {
        int32 Spent = 0;
        int32 Total = 0;
        ProgressionTreeInvestment(Progression, Tree, Spent, Total);
        CoreSpent += Spent;
    }

    // The fixed 420px detail rail, built before the board so hover handlers
    // have a target. It is filled through SetContent on hover and never from
    // a per-frame attribute, and its width never changes, so populating it
    // cannot reflow the board.
    SAssignNew(SkillDetailHost, SBox).WidthOverride(420.0f)
    [
        MakeSkillDetailPlaceholder()
    ];

    // Live purchasable count for the footer, over the visible board only.
    int32 PurchasableCount = 0;
    {
        const TArray<const UBreakerProgressionTree*>& VisibleTrees = bCoreBoard ? CoreTrees : ClassTrees;
        for (const UBreakerProgressionTree* Tree : VisibleTrees)
        {
            int32 Spent = 0;
            int32 Total = 0;
            ProgressionTreeInvestment(Progression, Tree, Spent, Total);
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                FString Ignored;
                if (SkillNodeIsPurchasable(Progression, Tree, Node, Spent, Ignored)) ++PurchasableCount;
            }
        }
    }

    // Wires one marker. Markers are never disabled: a locked node still has to
    // explain itself on hover, and a disabled SButton fires no hover events.
    auto WireMarker = [this](const UBreakerProgressionTree* Tree, const UBreakerProgressionNode* Node,
        const FSkillNodeView& View, bool bPurchasable, const FString& LockReason,
        const FLinearColor& Fill, const FLinearColor& Ring, float RingThickness,
        const TSharedRef<SWidget>& Inner) -> TSharedRef<SWidget>
    {
        return BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Fill)
            .ContentPadding(FMargin(0.0f))
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .OnHovered(FSimpleDelegate::CreateLambda([this, View]()
            {
                if (SkillDetailHost.IsValid()) SkillDetailHost->SetContent(MakeSkillDetailCard(View));
            }))
            .OnClicked(FOnClicked::CreateLambda([this, Tree, Node, bPurchasable, LockReason]()
            {
                if (!Node) return FReply::Handled();
                if (!bPurchasable)
                {
                    // The action is never silently swallowed: it is disclosed.
                    SkillTreeStatus = FText::FromString(LockReason.IsEmpty() ? FString(TEXT("LOCKED")) : LockReason);
                    Rebuild(EBreakerMenuScreen::SkillTrees);
                    return FReply::Handled();
                }
                UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                // SHIFT buys to max. The modifier state is read once, here, on
                // the click itself — never polled from a per-frame attribute,
                // which is the pattern that made this screen jitter before.
                const bool bToMax = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
                FText FailureReason;
                int32 Bought = 0;
                while (ProgressionPurchaseNode(Prog, Tree, Node->NodeId, FailureReason))
                {
                    ++Bought;
                    if (!bToMax || Bought >= Node->MaxRank) break;
                }
                if (Bought > 0)
                {
                    const int32 NewRank = ProgressionGetNodeRank(Prog, Node->NodeId, Node->Currency);
                    SkillTreeStatus = FText::FromString(PurchaseFeedback(Node, NewRank));
                    if (Character.IsValid()) Character->SaveGameState();
                }
                else
                {
                    SkillTreeStatus = FailureReason.IsEmpty() ? FText::FromString(TEXT("PURCHASE FAILED")) : FailureReason;
                }
                Rebuild(EBreakerMenuScreen::SkillTrees);
                return FReply::Handled();
            }))
            [
                Inner
            ],
            Ring, RingThickness);
    };

    // Shared empty-board plate. Every one of these paths exists today and is
    // reachable: no character, no class, no registered content.
    auto MakeEmptyBoard = [](const FString& Message) -> TSharedRef<SWidget>
    {
        return MakePlate(
            SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(Message), BreakerUI::TypeBody, BreakerUI::TextSecondary)
            ],
            BreakerUI::Panel00, BreakerUI::BorderEmphasis, FMargin(BreakerUI::Space24, BreakerUI::Space24));
    };

    // ---- Class board: PATHS, not a card grid -------------------------------
    auto BuildClassBoard = [&]() -> TSharedRef<SWidget>
    {
        if (ClassTrees.IsEmpty())
        {
            return MakeEmptyBoard(TEXT("[ NO CLASS BRANCHES ]\n\nLock a Breaker class, or register class branch trees,\nand the path board draws here."));
        }

        const float GutterWidth = 76.0f;    // the dedicated tier-gate gutter
        const float TierHeight = 190.0f;
        const float TopPad = 28.0f;
        const float NodeSpacing = 176.0f;
        const float LabelWidth = 168.0f;

        TArray<int32> Tiers;
        for (const UBreakerProgressionTree* Tree : ClassTrees)
        {
            for (const UBreakerProgressionNode* Node : Tree->Nodes)
            {
                if (Node) Tiers.AddUnique(Node->Tier);
            }
        }
        Tiers.Sort();
        if (Tiers.Num() == 0)
        {
            return MakeEmptyBoard(TEXT("[ NO NODES AUTHORED ]\n\nThe class branches carry no nodes yet."));
        }

        TArray<int32> BranchSpent;
        TArray<int32> BranchTotal;
        TArray<float> ColumnWidth;
        for (const UBreakerProgressionTree* Tree : ClassTrees)
        {
            int32 Spent = 0;
            int32 Total = 0;
            ProgressionTreeInvestment(Progression, Tree, Spent, Total);
            BranchSpent.Add(Spent);
            BranchTotal.Add(Total);

            int32 Widest = 1;
            for (const int32 Tier : Tiers)
            {
                int32 Count = 0;
                for (const UBreakerProgressionNode* Node : Tree->Nodes)
                {
                    if (Node && Node->Tier == Tier) ++Count;
                }
                Widest = FMath::Max(Widest, Count);
            }
            // Column is sized to the widest tier row it must hold, so the
            // labels of neighbouring nodes cannot collide.
            ColumnWidth.Add(FMath::Max(360.0f, Widest * NodeSpacing));
        }

        float FieldWidth = GutterWidth;
        for (const float Width : ColumnWidth) FieldWidth += Width;
        // Convergence/Keystone labels sit to the right of their marker, so the
        // board is a label wider than the field.
        const float BoardWidth = FieldWidth + LabelWidth;
        const float BoardHeight = TopPad + Tiers.Num() * TierHeight + 40.0f;

        TSharedRef<SCanvas> Canvas = SNew(SCanvas);

        // Tier gates: one dashed hairline across the field, labelled ONCE in
        // the 76px gutter, so a gate label can never land on node copy.
        for (int32 TierIndex = 0; TierIndex < Tiers.Num(); ++TierIndex)
        {
            const float TierTop = TopPad + TierIndex * TierHeight;
            int32 Gate = 0;
            for (const UBreakerProgressionTree* Tree : ClassTrees)
            {
                for (const UBreakerProgressionNode* Node : Tree->Nodes)
                {
                    if (Node && Node->Tier == Tiers[TierIndex]) Gate = FMath::Max(Gate, Node->RequiredTreeInvestment);
                }
            }

            Canvas->AddSlot()
                .Position(FVector2D(GutterWidth, TierTop))
                .Size(FVector2D(FieldWidth - GutterWidth, 1.0f))
                [
                    DashedLine(FieldWidth - GutterWidth, BorderEmphasis)
                ];
            Canvas->AddSlot()
                .Position(FVector2D(0.0f, TierTop + BreakerUI::Space8))
                .Size(FVector2D(GutterWidth - BreakerUI::Space8, 40.0f))
                [
                    // Two short lines: tier, then gate cost.
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        MenuText(FText::FromString(FString::Printf(TEXT("TIER %d"), Tiers[TierIndex])), BreakerUI::TypeCaption, Muted, true)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        MenuText(FText::FromString(FString::Printf(TEXT("GATE %d"), Gate)), BreakerUI::TypeCaption, Gate > 0 ? Amber : Muted, true)
                    ]
                ];
        }

        float ColumnX = GutterWidth;
        for (int32 BranchIndex = 0; BranchIndex < ClassTrees.Num(); ++BranchIndex)
        {
            const UBreakerProgressionTree* Tree = ClassTrees[BranchIndex];
            const float TrunkX = ColumnX + ColumnWidth[BranchIndex] * 0.5f;
            const int32 Spent = BranchSpent[BranchIndex];

            for (int32 TierIndex = 0; TierIndex < Tiers.Num(); ++TierIndex)
            {
                const float TierTop = TopPad + TierIndex * TierHeight;

                TArray<const UBreakerProgressionNode*> TierNodes;
                for (const UBreakerProgressionNode* Node : Tree->Nodes)
                {
                    if (Node && Node->Tier == Tiers[TierIndex]) TierNodes.Add(Node);
                }

                bool bRouteOwned = false;
                for (const UBreakerProgressionNode* Node : TierNodes)
                {
                    if (ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) > 0) bRouteOwned = true;
                }

                // The trunk: one 2px line per branch running the full height,
                // cyan where the route is owned and panel/20 where it is not.
                AddCanvasSegment(Canvas, FVector2D(TrunkX, TierTop), FVector2D(TrunkX, TierTop + TierHeight),
                    bRouteOwned ? Cyan : PanelHover);

                for (int32 NodeIndex = 0; NodeIndex < TierNodes.Num(); ++NodeIndex)
                {
                    const UBreakerProgressionNode* Node = TierNodes[NodeIndex];
                    const ESkillMarkerKind Kind = ClassifyNode(Node);
                    const float Size = MarkerSize(Kind);
                    const float NodeX = TrunkX + (NodeIndex - (TierNodes.Num() - 1) * 0.5f) * NodeSpacing;
                    const float NodeY = TierTop + 76.0f;

                    const int32 Rank = ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency);
                    FString LockReason;
                    const bool bPurchasable = SkillNodeIsPurchasable(Progression, Tree, Node, Spent, LockReason);
                    const bool bOwned = Rank > 0;
                    const bool bMaxed = Rank >= Node->MaxRank;

                    // 2px diagonal dropping from the trunk to the marker.
                    AddCanvasSegment(Canvas, FVector2D(TrunkX, TierTop + BreakerUI::Space8),
                        FVector2D(NodeX, NodeY - Size * 0.5f), bOwned ? Cyan : PanelHover);

                    const FLinearColor Fill = (bOwned || bPurchasable) ? PanelRaised : Panel;
                    // Gold is the only border colour that means "spend now".
                    const FLinearColor Ring = bOwned ? Cyan : (bPurchasable ? Amber : BorderRest);
                    const float RingThickness = (bOwned || bPurchasable) ? BreakerUI::BorderSelected : BreakerUI::BorderThin;

                    // Multi-rank Minors carry their rank inside the marker;
                    // every other kind is a bare shape with text beneath.
                    TSharedRef<SWidget> Inner = (Kind == ESkillMarkerKind::Minor)
                        ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(FString::Printf(TEXT("%d/%d"), Rank, Node->MaxRank)),
                            BreakerUI::TypeCaption, bOwned ? Cyan : Muted, true))
                        : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(1.0f, 1.0f)));

                    const FSkillNodeView View = MakeSkillNodeView(Node, Rank, bPurchasable, LockReason, Spent);
                    TSharedRef<SWidget> Marker = WireMarker(Tree, Node, View, bPurchasable, LockReason, Fill, Ring, RingThickness, Inner);
                    if (MarkerIsDiamond(Kind)) Marker = RotateFortyFive(Marker);

                    Canvas->AddSlot()
                        .Position(FVector2D(NodeX - Size * 0.5f, NodeY - Size * 0.5f))
                        .Size(FVector2D(Size, Size))
                        [
                            Marker
                        ];

                    // Name, number and state sit as plain text near the
                    // marker — not inside a card.
                    const FString NodeName = Node->DisplayName.IsEmpty() ? Node->NodeId.ToString().ToUpper() : Node->DisplayName.ToString().ToUpper();
                    const FString StateLine = bMaxed
                        ? FString(TEXT("MAXED"))
                        : (bPurchasable
                            ? FString::Printf(TEXT("%d PT -> RANK %d"), Node->CostPerRank, Rank + 1)
                            : (bOwned ? RankLabel(Rank, Node->MaxRank) : LockReason));
                    const FLinearColor StateColor = bMaxed ? Cyan : (bPurchasable ? Amber : (bOwned ? Cyan : Muted));

                    TSharedRef<SVerticalBox> Label = SNew(SVerticalBox);
                    Label->AddSlot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(NodeName))
                        .ColorAndOpacity((bOwned || bPurchasable) ? Primary : Disabled)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
                        .AutoWrapText(true)
                    ];
                    Label->AddSlot().AutoHeight()
                    [
                        SNew(STextBlock)
                        // The effect as a number, so the board is readable
                        // without hovering anything.
                        .Text(FText::FromString(PrimaryEffectLine(Node)))
                        .ColorAndOpacity((bOwned || bPurchasable) ? SoftText : Disabled)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
                        .AutoWrapText(true)
                    ];
                    if (!StateLine.IsEmpty())
                    {
                        Label->AddSlot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(StateLine))
                            .ColorAndOpacity(StateColor)
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeCaption))
                            .AutoWrapText(true)
                        ];
                    }

                    // Convergence and Keystone label to the RIGHT, so the
                    // trunk never runs through their text.
                    const bool bLabelRight = MarkerLabelsRight(Kind);
                    Canvas->AddSlot()
                        .Position(bLabelRight
                            ? FVector2D(NodeX + Size * 0.5f + BreakerUI::Space16, NodeY - 34.0f)
                            : FVector2D(NodeX - LabelWidth * 0.5f, NodeY + Size * 0.5f + BreakerUI::Space8))
                        .Size(FVector2D(LabelWidth, 86.0f))
                        [
                            Label
                        ];
                }
            }
            ColumnX += ColumnWidth[BranchIndex];
        }

        // 60px branch header strip above the path field.
        TSharedRef<SHorizontalBox> BranchStrip = SNew(SHorizontalBox);
        BranchStrip->AddSlot().AutoWidth()
        [
            SNew(SBox).WidthOverride(GutterWidth)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))]
        ];
        for (int32 BranchIndex = 0; BranchIndex < ClassTrees.Num(); ++BranchIndex)
        {
            const UBreakerProgressionTree* Tree = ClassTrees[BranchIndex];
            const FString BranchName = TreeSelectorLabel(Tree);
            BranchStrip->AddSlot().AutoWidth()
            [
                SNew(SBox).WidthOverride(ColumnWidth[BranchIndex]).Padding(FMargin(0.0f, 0.0f, BreakerUI::Space8, 0.0f))
                [
                    MakePlate(
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(BranchName), BreakerUI::TypeH2, BranchSpent[BranchIndex] > 0 ? Primary : SoftText, true)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(FString::Printf(TEXT("%d / %d INVESTED"), BranchSpent[BranchIndex], BranchTotal[BranchIndex])),
                                BreakerUI::TypeCaption, Muted, true)
                        ],
                        BreakerUI::BgRaised, BranchSpent[BranchIndex] > 0 ? Cyan : BorderEmphasis,
                        FMargin(BreakerUI::Space16, BreakerUI::Space8))
                ]
            ];
        }

        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                SNew(SBox).HeightOverride(60.0f)[BranchStrip]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SNew(SBox).WidthOverride(BoardWidth).HeightOverride(BoardHeight)[Canvas]
                ]
            ];
    };

    // ---- Core board: the spatial constellation map -------------------------
    auto BuildCoreBoard = [&]() -> TSharedRef<SWidget>
    {
        if (CoreTrees.IsEmpty())
        {
            return MakeEmptyBoard(TEXT("[ NO CORE CONSTELLATIONS ]\n\nNo core-currency tree is registered yet."));
        }

        const UBreakerProgressionTree* CoreTree = CoreTrees[0];
        int32 TreeSpent = 0;
        int32 TreeTotal = 0;
        ProgressionTreeInvestment(Progression, CoreTree, TreeSpent, TreeTotal);

        struct FConstellationCluster
        {
            FString Name;
            FString Prefix;
            FVector2D Centre = FVector2D::ZeroVector;
            bool bHub = false;
            bool bSealed = false;
            TArray<const UBreakerProgressionNode*> Nodes;
        };

        TArray<FConstellationCluster> Clusters;
        auto AddCluster = [&Clusters](const TCHAR* Name, const TCHAR* Prefix, float X, float Y, bool bHub, bool bSealed)
        {
            FConstellationCluster Cluster;
            Cluster.Name = FString(Name).ToUpper();
            Cluster.Prefix = Prefix;
            Cluster.Centre = FVector2D(X, Y);
            Cluster.bHub = bHub;
            Cluster.bSealed = bSealed;
            Clusters.Add(MoveTemp(Cluster));
        };
        // Kinesis is the hub; the other clusters sit around it. Elements is
        // sealed below centre.
        AddCluster(TEXT("Kinesis"), TEXT("Core.Kinesis."), 520.0f, 350.0f, true, false);
        AddCluster(TEXT("Precision"), TEXT("Core.Precision."), 190.0f, 130.0f, false, false);
        AddCluster(TEXT("Volley"), TEXT("Core.Volley."), 850.0f, 130.0f, false, false);
        AddCluster(TEXT("Affliction"), TEXT("Core.Affliction."), 190.0f, 570.0f, false, false);
        AddCluster(TEXT("Bulwark"), TEXT("Core.Bulwark."), 850.0f, 570.0f, false, false);
        AddCluster(TEXT("Elements"), TEXT("Core.Elements."), 520.0f, 660.0f, false, true);

        // Constellation membership rides the NodeId prefix
        // (Core.<Constellation>.<Node>) — the content library has no
        // constellation field yet. When one lands, read it here instead.
        TSet<FName> Claimed;
        for (FConstellationCluster& Cluster : Clusters)
        {
            for (const UBreakerProgressionNode* Node : CoreTree->Nodes)
            {
                if (!Node || !Node->NodeId.ToString().StartsWith(Cluster.Prefix)) continue;
                Cluster.Nodes.Add(Node);
                Claimed.Add(Node->NodeId);
            }
            Cluster.Nodes.Sort([](const UBreakerProgressionNode& A, const UBreakerProgressionNode& B) { return A.Tier < B.Tier; });
        }
        {
            // A node authored outside the known prefixes must never silently
            // vanish off the map.
            FConstellationCluster Other;
            Other.Name = TEXT("UNMAPPED");
            Other.Centre = FVector2D(520.0f, 130.0f);
            for (const UBreakerProgressionNode* Node : CoreTree->Nodes)
            {
                if (Node && !Claimed.Contains(Node->NodeId)) Other.Nodes.Add(Node);
            }
            if (Other.Nodes.Num() > 0) Clusters.Add(MoveTemp(Other));
        }

        const float BoardWidth = 1060.0f;
        const float BoardHeight = 800.0f;
        const float PlateWidth = 300.0f;
        const float PlateHeight = 156.0f;

        TSharedRef<SCanvas> Canvas = SNew(SCanvas);

        auto ClusterHasOwned = [Progression](const FConstellationCluster& Cluster)
        {
            for (const UBreakerProgressionNode* Node : Cluster.Nodes)
            {
                if (ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency) > 0) return true;
            }
            return false;
        };

        // Convergence lines radiate from the hub. Drawn first so the plates
        // paint over them.
        const bool bHubOwned = ClusterHasOwned(Clusters[0]);
        for (int32 Index = 1; Index < Clusters.Num(); ++Index)
        {
            const bool bLinked = bHubOwned && ClusterHasOwned(Clusters[Index]);
            AddCanvasSegment(Canvas, Clusters[0].Centre, Clusters[Index].Centre, bLinked ? Cyan : PanelHover);
        }

        for (const FConstellationCluster& Cluster : Clusters)
        {
            int32 ClusterPurchasable = 0;
            TSharedRef<SHorizontalBox> Grid = SNew(SHorizontalBox);
            for (const UBreakerProgressionNode* Node : Cluster.Nodes)
            {
                const int32 Rank = ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency);
                FString LockReason;
                const bool bPurchasable = SkillNodeIsPurchasable(Progression, CoreTree, Node, TreeSpent, LockReason);
                if (bPurchasable) ++ClusterPurchasable;
                const bool bOwned = Rank > 0;
                const ESkillMarkerKind Kind = ClassifyNode(Node);

                const FLinearColor Fill = (bOwned || bPurchasable) ? PanelRaised : Panel;
                const FLinearColor Ring = bOwned ? Cyan : (bPurchasable ? Amber : BorderRest);
                const float RingThickness = (bOwned || bPurchasable) ? BreakerUI::BorderSelected : BreakerUI::BorderThin;
                const FSkillNodeView View = MakeSkillNodeView(Node, Rank, bPurchasable, LockReason, TreeSpent);

                // The cluster grid is a glance, not the path board: every kind
                // draws at one compact size here, keeping its silhouette.
                TSharedRef<SWidget> Chip = WireMarker(CoreTree, Node, View, bPurchasable, LockReason, Fill, Ring, RingThickness,
                    Node->MaxRank > 1
                        ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(FString::FromInt(Rank)), BreakerUI::TypeCaption, bOwned ? Cyan : Muted, true))
                        : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))));
                if (MarkerIsDiamond(Kind)) Chip = RotateFortyFive(Chip);

                Grid->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
                [
                    SNew(SBox).WidthOverride(30.0f).HeightOverride(30.0f)[Chip]
                ];
            }

            // Elements is sealed, and reads in suppression teal: the one place
            // teal is legal on this screen, because a rift is a world object,
            // not chrome.
            const FLinearColor Rail = Cluster.bSealed ? BreakerUI::TealHardware : (Cluster.bHub ? Cyan : BorderEmphasis);
            const FLinearColor Border = Cluster.bSealed ? BreakerUI::TealHardware : BreakerUI::BorderRest;

            TSharedRef<SVerticalBox> Inner = SNew(SVerticalBox);
            Inner->AddSlot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Cluster.Name), BreakerUI::TypeH2,
                        Cluster.bSealed ? BreakerUI::TealHardware : Primary, true)
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    MenuText(FText::FromString(Cluster.bHub ? TEXT("HUB") : TEXT("")), BreakerUI::TypeCaption, Cyan, true)
                ]
            ];
            if (Cluster.Nodes.Num() == 0)
            {
                Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(Cluster.bSealed ? TEXT("SEALED") : TEXT("NO NODES AUTHORED")),
                        BreakerUI::TypeCaption, Cluster.bSealed ? BreakerUI::TealHardware : Muted, true)
                ];
                if (Cluster.bSealed)
                {
                    Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(TEXT("RIFT / ENTROPY / VOID")), BreakerUI::TypeCaption, Muted, true)
                    ];
                }
            }
            else
            {
                Inner->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, BreakerUI::Space8)[Grid];
                Inner->AddSlot().AutoHeight()
                [
                    MenuText(FText::FromString(FString::Printf(TEXT("%d NODES · %d PURCHASABLE"), Cluster.Nodes.Num(), ClusterPurchasable)),
                        BreakerUI::TypeCaption, ClusterPurchasable > 0 ? Amber : Muted, true)
                ];
            }

            Canvas->AddSlot()
                .Position(FVector2D(Cluster.Centre.X - PlateWidth * 0.5f, Cluster.Centre.Y - PlateHeight * 0.5f))
                .Size(FVector2D(PlateWidth, PlateHeight))
                [
                    MakePlate(Inner, PanelRaised, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space8), false, Border)
                ];
        }

        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                SNew(SBox).HeightOverride(60.0f)
                [
                    MakePlate(
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(TEXT("CORE CONSTELLATIONS — KINESIS AT THE HUB")), BreakerUI::TypeH2, Primary, true)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            MenuText(FText::FromString(FString::Printf(TEXT("%d / %d INVESTED"), TreeSpent, TreeTotal)), BreakerUI::TypeCaption, Muted, true)
                        ],
                        BreakerUI::BgRaised, Amber, FMargin(BreakerUI::Space16, BreakerUI::Space8))
                ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SNew(SBox).WidthOverride(BoardWidth).HeightOverride(BoardHeight)[Canvas]
                ]
            ];
    };

    TSharedRef<SWidget> Board = Trees.IsEmpty()
        ? MakeEmptyBoard(TEXT("[ NO TREE CONTENT ]\n\nThe progression component is not serving any trees yet.\nThe class branches and the core constellations appear\nhere once tree content is registered."))
        : (bCoreBoard ? BuildCoreBoard() : BuildClassBoard());

    // ---- Header zone -------------------------------------------------------
    const EBreakerClassId PermanentClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    const FString MetaLine = FString::Printf(TEXT("BREAKER · %s · LV %d"),
        *ClassDisplayName(PermanentClass),
        Progression ? Progression->GetProgressionState().CharacterLevel : 1);

    TSharedRef<SHorizontalBox> BoardTabs = SNew(SHorizontalBox);
    auto AddBoardTab = [this, &BoardTabs, bCoreBoard](const FString& Label, int32 TabIndex)
    {
        const bool bActive = (bCoreBoard ? 1 : 0) == TabIndex;
        BoardTabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(bActive ? PanelHover : Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, TabIndex]()
                {
                    SkillBoardTab = TabIndex;
                    SkillTreeStatus = FText::GetEmpty();
                    Rebuild(EBreakerMenuScreen::SkillTrees);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(Label), BreakerUI::TypeCaption, bActive ? Primary : Muted, true)
                ],
                bActive ? Cyan : BorderEmphasis,
                bActive ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
        ];
    };
    AddBoardTab(FString::Printf(TEXT("CLASS · %s"), *ClassDisplayName(PermanentClass)), 0);
    AddBoardTab(TEXT("CORE"), 1);

    auto MakePointChip = [](const FString& Label, int32 Unspent, int32 Spent, const FLinearColor& Rail) -> TSharedRef<SWidget>
    {
        return MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(Label), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)]
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(FString::FromInt(Unspent)), BreakerUI::TypeH2, Rail, true)]
            + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(FString::Printf(TEXT("/ %d SPENT"), Spent)), BreakerUI::TypeCaption, BreakerUI::TextMuted, true)],
            BreakerUI::Panel10, Rail, FMargin(BreakerUI::Space16, BreakerUI::Space4));
    };

    // Dev-only recovery row: saves made before the slice seeding rule relaxed
    // can land here with a class chosen and zero points. Same
    // RiorsEdge.Playtest/DevClassSwap gate the class screen uses.
    bool bDevTools = false;
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), bDevTools, GGameUserSettingsIni);

    const EBreakerPointCurrency BoardCurrency = bCoreBoard ? EBreakerPointCurrency::CorePoints : EBreakerPointCurrency::ClassPoints;

    TSharedRef<SHorizontalBox> HeaderRight = SNew(SHorizontalBox);
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)[BuildScreenTabs(EBreakerMenuScreen::SkillTrees)];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)[BoardTabs];
    HeaderRight->AddSlot().FillWidth(1.0f)[SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))];
    // Two counters as separate railed chips — class cyan, core gold — so the
    // two currencies are never read as one pool.
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
    [
        MakePointChip(TEXT("CLASS POINTS"), UnspentClass, ClassSpent, Cyan)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space16, 0.0f)
    [
        MakePointChip(TEXT("CORE POINTS"), UnspentCore, CoreSpent, Amber)
    ];
    if (bDevTools)
    {
        HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this]()
                {
                    UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                    if (Prog)
                    {
                        // O2 PLACEHOLDER: same 10 Class / 12 Core slice budget
                        // ApplySliceDefaultsIfFresh seeds.
                        Prog->GrantPlaytestPoints(10, 12);
                        SkillTreeStatus = FText::FromString(TEXT("DEV: GRANTED 10 CLASS / 12 CORE"));
                        if (Character.IsValid()) Character->SaveGameState();
                    }
                    else
                    {
                        SkillTreeStatus = FText::FromString(TEXT("DEV: NO PROGRESSION COMPONENT"));
                    }
                    Rebuild(EBreakerMenuScreen::SkillTrees);
                    return FReply::Handled();
                }))
                [
                    MenuText(FText::FromString(TEXT("DEV: GRANT POINTS")), BreakerUI::TypeCaption, Amber, true)
                ],
                Amber)
        ];
    }
    // Respec is per-tree and destructive: discard styling, and the label
    // states which tree it will clear.
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center)
    [
        BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this, BoardCurrency]()
            {
                UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                FText FailureReason;
                if (ProgressionRespec(Prog, BoardCurrency, FailureReason))
                {
                    SkillTreeStatus = FText::FromString(FString::Printf(TEXT("%s POINTS REFUNDED"), *CurrencyLabel(BoardCurrency)));
                    if (Character.IsValid()) Character->SaveGameState();
                }
                else
                {
                    SkillTreeStatus = FailureReason.IsEmpty() ? FText::FromString(TEXT("RESPEC FAILED")) : FailureReason;
                }
                Rebuild(EBreakerMenuScreen::SkillTrees);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(FString::Printf(TEXT("RESPEC %s"), *CurrencyLabel(BoardCurrency))), BreakerUI::TypeCaption, Harm, true)
            ],
            HarmDeep)
    ];
    HeaderRight->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(120.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
    ];

    // ---- Body: board plus the fixed 420px detail rail ----------------------
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    if (!SkillTreeStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            MenuText(SkillTreeStatus, BreakerUI::TypeCaption, Cyan, true)
        ];
    }
    Body->AddSlot().FillHeight(1.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f)[Board]
        + SHorizontalBox::Slot().AutoWidth().Padding(BreakerUI::Space24, 0.0f, 0.0f, 0.0f)
        [
            SkillDetailHost.ToSharedRef()
        ]
    ];

    // ---- Footer: input legend plus the live purchasable count --------------
    TSharedRef<SBox> Footer = SNew(SBox).HeightOverride(56.0f)
    [
        MakePlate(
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                // O2: node numbers are not balanced yet.
                MenuText(FText::FromString(TEXT("LMB BUY 1 RANK · HOVER FULL DETAIL · SHIFT+LMB BUY TO MAX · CLICK CLASS / CORE TO SWITCH BOARD · [O2] VALUES ARE PLACEHOLDER · ESC BACK")),
                    BreakerUI::TypeCaption, Muted, true)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(FString::Printf(TEXT("%d PURCHASABLE"), PurchasableCount)), BreakerUI::TypeH2, Amber, true)
            ],
            BreakerUI::BgRaised, Amber, FMargin(BreakerUI::Space24, BreakerUI::Space8))
    ];

    return BuildZonedFrame(
        FText::FromString(TEXT("SKILL MATRIX")),
        FText::FromString(MetaLine),
        HeaderRight,
        Body,
        Footer,
        1760.0f);
}

TSharedRef<SWidget> SBreakerMenu::BuildDialogueScreen()
{
    ABreakerNPC* NPC = DialogueNPC.Get();
    FBreakerDialogueNode Node;
    if (!NPC || !NPC->FindDialogueNode(DialogueNodeId, Node))
    {
        if (Character.IsValid()) Character->ResumeFromMenu();
        return SNew(SBox);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space24)
    [
        MakePlate(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)[MenuText(NPC->GetDisplayName(), BreakerUI::TypeCaption, Muted, true)]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(Node.SpeakerLine))
                .ColorAndOpacity(SoftText)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeBody))
                .AutoWrapText(true)
            ],
            PanelRaised, Cyan, FMargin(BreakerUI::Space24, BreakerUI::Space16))
    ];

    int32 ChoiceNumber = 0;
    for (const FBreakerDialogueChoice& Choice : Node.Choices)
    {
        ++ChoiceNumber;
        const FName NextNodeId = Choice.NextNodeId;
        const FName QuestFlag = Choice.SetsQuestFlag;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(Panel)
            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
            .OnClicked(FOnClicked::CreateLambda([this, NextNodeId, QuestFlag]()
            {
                if (Character.IsValid())
                {
                    Character->AddQuestFlag(QuestFlag);
                    if (NextNodeId == NAME_None)
                    {
                        Character->ResumeFromMenu();
                        return FReply::Handled();
                    }
                }
                DialogueNodeId = NextNodeId;
                Rebuild(EBreakerMenuScreen::Dialogue);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(FString::Printf(TEXT("%d.  %s"), ChoiceNumber, *Choice.Text)), BreakerUI::TypeBody, SoftText, true)
            ],
            BorderEmphasis)
        ];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
    [
        MenuText(FText::FromString(TEXT("Choices marked [Leave] end the conversation  |  ESC to walk away")), 9, SoftText)
    ];
    return BuildFrame(FText::FromString(TEXT("CONVERSATION")), NPC->GetDisplayName(), Body, 780.0f);
}

FReply SBreakerMenu::GoBack()
{
    Rebuild(RootScreen);
    return FReply::Handled();
}
