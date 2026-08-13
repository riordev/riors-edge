#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
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

    FString TierLabel(int32 Tier)
    {
        return Tier < 0 ? TEXT("T-1") : FString::Printf(TEXT("T%d"), Tier);
    }

    FString DescribeItem(const FBreakerItemInstance& Item)
    {
        const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("ITEM LEVEL %d"), Item.ItemLevel));
        for (const FBreakerRolledAffix& Affix : Item.Affixes)
        {
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Affix.AffixId);
            const FString Name = Definition ? Definition->DisplayName.ToString() : Affix.AffixId.ToString();
            const bool bPercent = Definition && Definition->StatBucket != EBreakerStatBucket::Flat;
            const bool bPercentStyleFlat = Definition &&
                (Definition->StatTarget == EBreakerStatTarget::CriticalChance || Definition->StatTarget == EBreakerStatTarget::CriticalDamage);
            Lines.Add(FString::Printf(TEXT("%s  +%.1f%s  %s"), *Name, Affix.Value, bPercent || bPercentStyleFlat ? TEXT("%") : TEXT(""), *TierLabel(Affix.Tier)));
        }
        return FString::Join(Lines, TEXT("\n"));
    }
}

TSharedRef<SWidget> SBreakerMenu::BuildInventoryScreen()
{
    UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;

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

        return SNew(SBox).MinDesiredHeight(72.0f).Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
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
    };

    // Left column: the Breaker panel — silhouette placeholder plus the live
    // totals the equipped gear currently grants.
    TSharedRef<SVerticalBox> CharacterPanel = SNew(SVerticalBox);
    CharacterPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("BREAKER")), 12, Cyan, true)];
    CharacterPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
    [
        SNew(SBox).HeightOverride(240.0f)
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
    if (Equipment)
    {
        const FBreakerEquipmentStats& Stats = Equipment->GetStats();
        const FString StatText = FString::Printf(
            TEXT("HEALTH BONUS     +%.0f\nCRIT CHANCE      +%.1f%%\nCRIT DAMAGE      +%.1f%%\nMOVE SPEED       x%.2f\nSLIDE SPEED      x%.2f\nAIR CONTROL      x%.2f\nDASH COOLDOWN    x%.2f\nPHYS DR          %.1f%%\nDROP CHANCE      +%.1f%%\nMAX RESOURCE     +%.0f\nRESOURCE REGEN   +%.1f/s"),
            Stats.BonusHealth, Stats.CriticalChanceBonus * 100.0f, Stats.CriticalMultiplierBonus * 100.0f,
            Stats.MoveSpeedMultiplier, Stats.SlideSpeedMultiplier, Stats.AirControlMultiplier, Stats.DashCooldownMultiplier,
            Stats.PhysicalDamageReductionPercent, Stats.DropChancePercent, Stats.BonusMaxResource, Stats.ResourceRegenPerSecond);
        CharacterPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("GEAR TOTALS")), 10, SoftText, true)];
        CharacterPanel->AddSlot().AutoHeight()[MenuText(FText::FromString(StatText), 10, Primary)];
    }

    // Right side: gear slots arranged top-down like the body — head to
    // boots, weapons last.
    TSharedRef<SVerticalBox> LeftSlots = SNew(SVerticalBox);
    TSharedRef<SVerticalBox> RightSlots = SNew(SVerticalBox);
    const EBreakerEquipSlot LeftColumn[] = { EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour, EBreakerEquipSlot::Waist, EBreakerEquipSlot::Primary };
    const EBreakerEquipSlot RightColumn[] = { EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Secondary };
    for (const EBreakerEquipSlot Slot : LeftColumn) LeftSlots->AddSlot().AutoHeight()[MakeSlotCard(Slot)];
    for (const EBreakerEquipSlot Slot : RightColumn) RightSlots->AddSlot().AutoHeight()[MakeSlotCard(Slot)];

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

        // Readability aid: what clicking this card would displace. One line,
        // tinted with the rarity being replaced so a downgrade reads at a
        // glance.
        FBreakerItemInstance CurrentlyEquipped;
        const bool bSlotOccupied = Equipment && Equipment->GetEquippedItem(Item.Slot, CurrentlyEquipped);
        // Gold means "this costs you something", cyan means the action is
        // free. The footer states the consequence of clicking, never hides it.
        const FString DeltaLine = bSlotOccupied
            ? FString::Printf(TEXT("EQUIP · REPLACES %s i%d"), *RarityName(CurrentlyEquipped.Rarity), CurrentlyEquipped.ItemLevel)
            : FString(TEXT("EQUIP · SLOT EMPTY"));
        const FLinearColor DeltaColor = bSlotOccupied ? Amber : Cyan;

        const FOnClicked DiscardOne = FOnClicked::CreateLambda([this, ItemId]()
        {
            if (Character.IsValid() && Character->GetEquipment()) Character->GetEquipment()->DiscardFromBackpack(ItemId);
            InventoryStatus = FText::FromString(TEXT("Discarded 1 item."));
            Rebuild(EBreakerMenuScreen::Inventory);
            return FReply::Handled();
        });

        if (BackpackCardIndex % 3 == 0)
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
                                    MenuText(FText::FromString(DescribeItem(Item)), BreakerUI::TypeCaption, SoftText)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                                [
                                    MenuText(FText::FromString(DeltaLine.ToUpper()), BreakerUI::TypeCaption, DeltaColor, true)
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

    // Clean-up chips. First click arms (amber, "CONFIRM?"), second click
    // executes; any other interaction rebuilds and disarms.
    TSharedRef<SHorizontalBox> CleanupRow = SNew(SHorizontalBox);
    auto AddCleanupChip = [this, &CleanupRow](const FString& Label, int32 ArmIndex, EBreakerItemRarity MinimumKept)
    {
        const bool bArmed = CleanupArmedIndex == ArmIndex;
        // Two-step arm: the button turns gold and reads CONFIRM before it
        // destroys anything. Armed carries the 2px gold ring, disarmed reads
        // as a destructive control (deep-red ring, harm text).
        CleanupRow->AddSlot().AutoWidth().Padding(BreakerUI::Space4, 0.0f, 0.0f, 0.0f)
        [
            BorderWrap(
            SNew(SButton)
            .ButtonColorAndOpacity(bArmed ? PanelHover : Panel)
            .ContentPadding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
            .OnClicked(FOnClicked::CreateLambda([this, ArmIndex, MinimumKept, bArmed]()
            {
                if (bArmed)
                {
                    const int32 Removed = Character.IsValid() && Character->GetEquipment()
                        ? Character->GetEquipment()->DiscardBackpackBelowRarity(MinimumKept)
                        : 0;
                    InventoryStatus = FText::FromString(FString::Printf(TEXT("Discarded %d item%s."), Removed, Removed == 1 ? TEXT("") : TEXT("s")));
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
    AddCleanupChip(TEXT("DISCARD < UNCOMMON"), 0, EBreakerItemRarity::Uncommon);
    AddCleanupChip(TEXT("DISCARD < EXCEPTIONAL"), 1, EBreakerItemRarity::Exceptional);

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

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[BuildScreenTabs(EBreakerMenuScreen::Inventory)];
    Body->AddSlot().AutoHeight()
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(250.0f)[CharacterPanel]
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(16.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(FText::FromString(TEXT("EQUIPPED — click to unequip")), 10, SoftText, true)]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)[LeftSlots]
                + SHorizontalBox::Slot().FillWidth(1.0f)[RightSlots]
            ]
        ]
    ];
    // Header band: one fixed row carrying label + filters + cleanup so it
    // stays put above the scrolling grid.
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("BACKPACK (%d/%d) — click equip / right-click or X discard"), BackpackItems.Num(), TotalBackpackCount)), 10, SoftText, true)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[FilterRow]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[CleanupRow]
    ];
    if (bDevTools)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[DevRow];
    }
    if (!InventoryStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[MenuText(InventoryStatus, 9, Amber, true)];
    }
    Body->AddSlot().FillHeight(1.0f)
    [
        SNew(SBox).MinDesiredHeight(280.0f)
        [
            BackpackItems.IsEmpty()
                ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(TEXT("Empty. Enemy kills drop rolled items.")), 11, SoftText))
                : StaticCastSharedRef<SWidget>(SNew(SScrollBox) + SScrollBox::Slot()[BackpackGrid])
        ]
    ];
    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(180.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(14.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
        [
            MenuText(FText::FromString(TEXT("Stats apply immediately  |  I or ESC to close")), 9, SoftText)
        ]
    ];
    return BuildFrame(FText::FromString(TEXT("INVENTORY")), FText::FromString(TEXT("BREAKER / EQUIPMENT / BACKPACK")), Body, 1120.0f);
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
}

TSharedRef<SWidget> SBreakerMenu::BuildSkillTreesScreen()
{
    UBreakerProgressionComponent* Progression = Character.IsValid() ? Character->GetProgression() : nullptr;
    const TArray<const UBreakerProgressionTree*> Trees = ProgressionGatherTrees(Progression);

    const int32 UnspentClass = ProgressionGetUnspent(Progression, EBreakerPointCurrency::ClassPoints);
    const int32 UnspentCore = ProgressionGetUnspent(Progression, EBreakerPointCurrency::CorePoints);

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[BuildScreenTabs(EBreakerMenuScreen::SkillTrees)];

    // Two point counters as separate railed chips — class points cyan, core
    // points gold — so the two currencies are never read as one pool.
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, BreakerUI::Space8, 0.0f)
        [
            MakePlate(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(TEXT("CLASS POINTS")), BreakerUI::TypeCaption, Muted, true)]
                + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(FString::Printf(TEXT("%d UNSPENT"), UnspentClass)), BreakerUI::TypeH2, Cyan, true)],
                PanelRaised, Cyan, FMargin(BreakerUI::Space16, BreakerUI::Space8))
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            MakePlate(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(TEXT("CORE POINTS")), BreakerUI::TypeCaption, Muted, true)]
                + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(FString::Printf(TEXT("%d UNSPENT"), UnspentCore)), BreakerUI::TypeH2, Amber, true)],
                PanelRaised, Amber, FMargin(BreakerUI::Space16, BreakerUI::Space8))
        ]
    ];

    // Dev-only recovery row: saves made before the slice seeding rule relaxed
    // can land here with a class chosen and zero points. Same
    // RiorsEdge.Playtest/DevClassSwap gate the class screen uses.
    bool bDevTools = false;
    GConfig->GetBool(TEXT("RiorsEdge.Playtest"), TEXT("DevClassSwap"), bDevTools, GGameUserSettingsIni);
    if (bDevTools)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .ButtonColorAndOpacity(Amber)
                .ContentPadding(FMargin(14.0f, 7.0f))
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
                    MenuText(FText::FromString(TEXT("DEV: GRANT SLICE POINTS")), 10, Background, true)
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(12.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(TEXT("Dev tools are on (Class screen toggle). Adds 10 CLASS / 12 CORE.")), 9, SoftText)
            ]
        ];
    }

    if (Trees.IsEmpty())
    {
        // INTEGRATION: no tree content is reachable through the progression
        // API available to this file. When GetAvailableTrees() (or the static
        // fallback-content provider) lands, ProgressionGatherTrees() starts
        // returning trees and this placeholder stops rendering — no other
        // change is needed here.
        Body->AddSlot().FillHeight(1.0f).Padding(0.0f, 20.0f, 0.0f, 20.0f)
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(PanelRaised)
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(TEXT("[ NO TREE CONTENT ]\n\nThe progression component is not serving any\ntrees yet. Class tree and the six core\nconstellations appear here once tree content\nis registered.")), 12, SoftText)
            ]
        ];
    }
    else
    {
        SelectedTreeIndex = FMath::Clamp(SelectedTreeIndex, 0, Trees.Num() - 1);

        // Left column: one selector button per tree, each showing its own
        // spent/total plus the unspent pool that tree draws from.
        TSharedRef<SVerticalBox> Selector = SNew(SVerticalBox);
        Selector->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            MenuText(FText::FromString(TEXT("TREES")), 11, SoftText, true)
        ];
        for (int32 TreeIndex = 0; TreeIndex < Trees.Num(); ++TreeIndex)
        {
            const UBreakerProgressionTree* Tree = Trees[TreeIndex];
            int32 Spent = 0;
            int32 Total = 0;
            ProgressionTreeInvestment(Progression, Tree, Spent, Total);
            const bool bSelected = TreeIndex == SelectedTreeIndex;
            const int32 Unspent = ProgressionGetUnspent(Progression, Tree->Currency);
            const FString TreeName = TreeSelectorLabel(Tree);
            const int32 CapturedIndex = TreeIndex;

            // Branch header strip: the selected branch carries the identity
            // rail, the others sit on the neutral border.
            Selector->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space8)
            [
                MakePlate(
                    SNew(SButton)
                    .ButtonColorAndOpacity(bSelected ? PanelHover : Panel)
                    .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                    .HAlign(HAlign_Left)
                    .OnClicked(FOnClicked::CreateLambda([this, CapturedIndex]()
                    {
                        SelectedTreeIndex = CapturedIndex;
                        SkillTreeStatus = FText::GetEmpty();
                        Rebuild(EBreakerMenuScreen::SkillTrees);
                        return FReply::Handled();
                    }))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(TreeName), BreakerUI::TypeH2, bSelected ? Primary : SoftText, true)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                        [
                            MenuText(FText::FromString(FString::Printf(TEXT("%d INVESTED · %d %s UNSPENT"), Spent, Unspent, *CurrencyLabel(Tree->Currency))), BreakerUI::TypeCaption, Muted, true)
                        ]
                    ],
                    bSelected ? PanelHover : Panel,
                    bSelected ? Cyan : BorderEmphasis,
                    FMargin(0.0f))
            ];
        }

        // Right side: the selected tree's nodes, one row per investment gate.
        const UBreakerProgressionTree* Selected = Trees[SelectedTreeIndex];
        int32 SelectedSpent = 0;
        int32 SelectedTotal = 0;
        ProgressionTreeInvestment(Progression, Selected, SelectedSpent, SelectedTotal);

        // One section per header bar. Class trees section by investment tier;
        // the Core slice sections by constellation instead, because a flat
        // tier list mixes five unrelated constellations into one row and the
        // structure the content is actually built around disappears.
        struct FTreeSection
        {
            FString Header;
            bool bGated = false;
            TArray<const UBreakerProgressionNode*> Nodes;
        };
        TArray<FTreeSection> Sections;

        const bool bCoreSlice = Selected->TreeId == FName(TEXT("Core.Slice"));
        if (bCoreSlice)
        {
            // Constellation membership is carried by the NodeId prefix
            // (Core.<Constellation>.<Node>) — the content library has no
            // constellation field yet. When one lands, read it here instead.
            static const TCHAR* ConstellationOrder[] = {TEXT("Precision"), TEXT("Volley"), TEXT("Affliction"), TEXT("Bulwark"), TEXT("Kinesis")};
            TSet<FName> Claimed;
            for (const TCHAR* Constellation : ConstellationOrder)
            {
                FTreeSection Section;
                Section.Header = FString(Constellation).ToUpper();
                const FString Prefix = FString::Printf(TEXT("Core.%s."), Constellation);
                for (const UBreakerProgressionNode* Node : Selected->Nodes)
                {
                    if (!Node || !Node->NodeId.ToString().StartsWith(Prefix)) continue;
                    Section.Nodes.Add(Node);
                    Claimed.Add(Node->NodeId);
                }
                Section.Nodes.Sort([](const UBreakerProgressionNode& A, const UBreakerProgressionNode& B) { return A.Tier < B.Tier; });
                if (Section.Nodes.Num() > 0) Sections.Add(MoveTemp(Section));
            }
            // Anything authored outside the five known prefixes still has to
            // render — an unlisted node must never silently vanish.
            FTreeSection Other;
            Other.Header = TEXT("OTHER");
            for (const UBreakerProgressionNode* Node : Selected->Nodes)
            {
                if (Node && !Claimed.Contains(Node->NodeId)) Other.Nodes.Add(Node);
            }
            if (Other.Nodes.Num() > 0) Sections.Add(MoveTemp(Other));
        }
        else
        {
            TArray<int32> TierGates;
            for (const UBreakerProgressionNode* Node : Selected->Nodes)
            {
                if (Node) TierGates.AddUnique(Node->RequiredTreeInvestment);
            }
            TierGates.Sort();

            int32 TierNumber = 0;
            for (const int32 Gate : TierGates)
            {
                ++TierNumber;
                FTreeSection Section;
                Section.bGated = Gate > SelectedSpent;
                Section.Header = Gate > 0
                    ? FString::Printf(TEXT("TIER %d — UNLOCKS AT %d POINTS IN TREE  (%d SPENT)"), TierNumber, Gate, SelectedSpent)
                    : FString::Printf(TEXT("TIER %d — OPEN"), TierNumber);
                for (const UBreakerProgressionNode* Node : Selected->Nodes)
                {
                    if (Node && Node->RequiredTreeInvestment == Gate) Section.Nodes.Add(Node);
                }
                if (Section.Nodes.Num() > 0) Sections.Add(MoveTemp(Section));
            }
        }

        TSharedRef<SVerticalBox> NodeColumn = SNew(SVerticalBox);
        int32 SectionNumber = 0;
        for (const FTreeSection& Section : Sections)
        {
            ++SectionNumber;
            // Section bar, not a caption: a filled strip so the eye can find
            // where one group ends and the next begins while scrolling.
            const FString HeaderText = Section.bGated
                ? FString::Printf(TEXT("▮ %s"), *Section.Header)
                : Section.Header;
            NodeColumn->AddSlot().AutoHeight().Padding(0.0f, SectionNumber == 1 ? 0.0f : BreakerUI::Space24, 0.0f, BreakerUI::Space8)
            [
                MakePlate(
                    MenuText(FText::FromString(HeaderText), BreakerUI::TypeCaption, Section.bGated ? Muted : Primary, true),
                    Section.bGated ? Panel : BreakerUI::BgRaised,
                    Section.bGated ? BorderEmphasis : Cyan,
                    FMargin(BreakerUI::Space16, BreakerUI::Space8))
            ];

            // Fixed rows of two, never a wrap box: SWrapBox sized by its
            // allotted width inside a scroll box re-measures to a different
            // answer every frame — the layout oscillation the owner saw as
            // the screen "bouncing between two sizes". Two per row because
            // the cards are now 300px wide.
            TSharedRef<SVerticalBox> TierRow = SNew(SVerticalBox);
            TSharedPtr<SHorizontalBox> CurrentRow;
            int32 CardIndex = 0;
            for (const UBreakerProgressionNode* Node : Section.Nodes)
            {
                if (!Node) continue;

                const int32 Rank = ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency);
                FString LockReason;
                const bool bPurchasable = SkillNodeIsPurchasable(Progression, Selected, Node, SelectedSpent, LockReason);
                const bool bOwned = Rank > 0;
                const bool bMaxed = Rank >= Node->MaxRank;

                // UI-Skill-Tree-Spec node states. Owned: cyan rail. Purchasable:
                // gold 2px border and a gold action line — gold is the only
                // border colour that means "spend now", which is what makes
                // scanning the tree work. Available: neutral 1px. Locked:
                // muted, with the reason stated literally.
                const FLinearColor CardColor = (bOwned || bPurchasable) ? PanelRaised : Panel;
                const FLinearColor NameColor = (bPurchasable || bOwned) ? Primary : Disabled;
                // "1 PT -> RANK 4" when it can be bought, the lock reason when
                // it cannot. Never both, never neither.
                const FString ActionLine = bMaxed
                    ? FString(TEXT("MAXED"))
                    : (bPurchasable
                        ? FString::Printf(TEXT("%d PT -> RANK %d"), Node->CostPerRank, Rank + 1)
                        : LockReason);
                const FLinearColor ActionColor = bMaxed ? Cyan : (bPurchasable ? Amber : Harm);

                const FName CapturedNodeId = Node->NodeId;
                const UBreakerProgressionTree* CapturedTree = Selected;
                const FString NodeName = Node->DisplayName.IsEmpty() ? Node->NodeId.ToString().ToUpper() : Node->DisplayName.ToString().ToUpper();
                const FString Description = Node->Description.IsEmpty() ? TEXT("—") : Node->Description.ToString();
                const FString CostChip = bMaxed
                    ? FString(TEXT("MAXED"))
                    : FString::Printf(TEXT("%d %s"), Node->CostPerRank, *CurrencyLabel(Node->Currency));

                // Everything that used to crowd the card now lives in the
                // hover tooltip; the card keeps name, rank, cost, one line.
                FString TooltipText;
                // Built once per Rebuild and shared by the wrapper box and the
                // button below — never rebuilt from an attribute lambda.
                {
                    TArray<FString> TipLines;
                    TipLines.Add(NodeName + (Node->bCornerstone ? TEXT("   [CORNERSTONE]") : TEXT("")));
                    TipLines.Add(FString::Printf(TEXT("RANK %d / %d   COST %d %s PER RANK"), Rank, Node->MaxRank, Node->CostPerRank, *CurrencyLabel(Node->Currency)));
                    TipLines.Add(TEXT(""));
                    TipLines.Add(Description);
                    if (Node->Effects.Num() > 0)
                    {
                        TipLines.Add(TEXT(""));
                        TipLines.Add(TEXT("EFFECTS PER RANK"));
                        for (const FBreakerNodeEffect& Effect : Node->Effects)
                        {
                            TipLines.Add(FString::Printf(TEXT("  %s"), *FormatNodeEffect(Effect)));
                        }
                    }
                    if (Node->Prerequisites.Num() > 0)
                    {
                        TipLines.Add(TEXT(""));
                        TipLines.Add(TEXT("PREREQUISITES"));
                        for (const FBreakerNodePrerequisite& Prereq : Node->Prerequisites)
                        {
                            TipLines.Add(FString::Printf(TEXT("  %s RANK %d"), *Prereq.NodeId.ToString().ToUpper(), Prereq.RequiredRank));
                        }
                    }
                    if (Node->RequiredTreeInvestment > 0)
                    {
                        TipLines.Add(TEXT(""));
                        TipLines.Add(FString::Printf(TEXT("REQUIRES %d INVESTED IN THIS TREE (%d)"), Node->RequiredTreeInvestment, SelectedSpent));
                    }
                    if (!LockReason.IsEmpty())
                    {
                        TipLines.Add(TEXT(""));
                        TipLines.Add(FString::Printf(TEXT("LOCKED: %s"), *LockReason));
                    }
                    TooltipText = FString::Join(TipLines, TEXT("\n"));
                }
                const FText TooltipTextValue = FText::FromString(TooltipText);

                // Line 2 is the number the player is buying. The generic
                // description drops to line 3 so a card never reads as flavour
                // text with no stated effect.
                const FString EffectLine = PrimaryEffectLine(Node);

                TSharedRef<SVerticalBox> CardBody = SNew(SVerticalBox);
                CardBody->AddSlot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[MenuText(FText::FromString(NodeName), BreakerUI::TypeH2, NameColor, true)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(BreakerUI::Space8, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(Background)
                        .Padding(FMargin(BreakerUI::Space8, BreakerUI::Space4))
                        [
                            MenuText(FText::FromString(CostChip), BreakerUI::TypeCaption, bMaxed ? Cyan : Muted, true)
                        ]
                    ]
                ];
                // Line two is the number the player is buying, in the numeric
                // face, so the tree is readable without hovering anything.
                CardBody->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(EffectLine), BreakerUI::TypeCaption, bOwned || bPurchasable ? Primary : Muted, true)
                ];
                // Owned rank reads as a word next to the filled rail; the old
                // pip row was too small to parse at a glance. Maxed reads
                // MAXED rather than 5/5 alone.
                if (bOwned)
                {
                    CardBody->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(bMaxed ? FString(TEXT("MAXED")) : RankLabel(Rank, Node->MaxRank)), BreakerUI::TypeCaption, Cyan, true)
                    ];
                }
                CardBody->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    // PERF: this line used to be a Text_Lambda that polled
                    // FSlateApplication::GetModifierKeys() for Alt-expansion —
                    // once per card (~46) per frame, which made the screen
                    // jitter. The summary is now baked at build time and the
                    // hover tooltip is the only path to full detail.
                    .Text(FText::FromString(ShortSummary(Description)))
                    .ColorAndOpacity(bOwned || bPurchasable ? SoftText : Disabled)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), BreakerUI::TypeCaption))
                    .AutoWrapText(true)
                ];
                // The state line, stated literally: what it costs to advance,
                // or exactly why it is locked.
                if (!ActionLine.IsEmpty())
                {
                    CardBody->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space8, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(ActionLine), BreakerUI::TypeCaption, ActionColor, true)
                    ];
                }

                // 3px owned rail: state readable from the edge of the card
                // without reading any text at all.
                TSharedRef<SHorizontalBox> Card = SNew(SHorizontalBox);
                Card->AddSlot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(BreakerUI::RailThickness)
                    [
                        SolidBlock(bOwned ? Cyan : Transparent)
                    ]
                ];
                Card->AddSlot().FillWidth(1.0f).Padding(BreakerUI::Space16, 0.0f, 0.0f, 0.0f)[CardBody];

                if (CardIndex % 2 == 0)
                {
                    CurrentRow = SNew(SHorizontalBox);
                    TierRow->AddSlot().AutoHeight()[CurrentRow.ToSharedRef()];
                }
                ++CardIndex;
                const UBreakerProgressionNode* CapturedNode = Node;
                CurrentRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 10.0f)
                [
                    // Tooltip lives on the wrapper too so locked (disabled)
                    // cards still explain themselves on hover.
                    SNew(SBox).WidthOverride(300.0f)
                    .ToolTipText(TooltipTextValue)
                    [
                        // Purchasable-right-now cards carry the gold 2px ring,
                        // so "what can I buy" is a scan and not a read.
                        BorderWrap(
                            SNew(SButton)
                            .ButtonColorAndOpacity(CardColor)
                            .IsEnabled(bPurchasable)
                            .ToolTipText(TooltipTextValue)
                            .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space16))
                            .OnClicked(FOnClicked::CreateLambda([this, CapturedTree, CapturedNodeId, CapturedNode]()
                            {
                                UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                                FText FailureReason;
                                if (ProgressionPurchaseNode(Prog, CapturedTree, CapturedNodeId, FailureReason))
                                {
                                    // Say what changed, not just that something did.
                                    const int32 NewRank = CapturedNode
                                        ? ProgressionGetNodeRank(Prog, CapturedNodeId, CapturedNode->Currency)
                                        : 0;
                                    SkillTreeStatus = FText::FromString(PurchaseFeedback(CapturedNode, NewRank));
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
                                Card
                            ],
                            bPurchasable ? Amber : BorderRest,
                            bPurchasable ? BreakerUI::BorderSelected : BreakerUI::BorderThin)
                    ]
                ];
            }
            NodeColumn->AddSlot().AutoHeight()[TierRow];
        }

        const FString SelectedName = Selected->DisplayName.IsEmpty() ? Selected->TreeId.ToString().ToUpper() : Selected->DisplayName.ToString().ToUpper();
        const EBreakerPointCurrency SelectedCurrency = Selected->Currency;

        TSharedRef<SVerticalBox> RightPane = SNew(SVerticalBox);
        RightPane->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(FString::Printf(TEXT("%s — %d / %d POINTS"), *SelectedName, SelectedSpent, SelectedTotal)), BreakerUI::TypeH2, Primary, true)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                // Respec is per-tree and destructive: discard styling, and the
                // label states which tree it will clear.
                BorderWrap(
                SNew(SButton)
                .ButtonColorAndOpacity(Panel)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .OnClicked(FOnClicked::CreateLambda([this, SelectedCurrency]()
                {
                    UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                    FText FailureReason;
                    if (ProgressionRespec(Prog, SelectedCurrency, FailureReason))
                    {
                        SkillTreeStatus = FText::FromString(FString::Printf(TEXT("%s POINTS REFUNDED"), *CurrencyLabel(SelectedCurrency)));
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
                    MenuText(FText::FromString(FString::Printf(TEXT("RESPEC %s"), *CurrencyLabel(SelectedCurrency))), BreakerUI::TypeCaption, Harm, true)
                ],
                HarmDeep)
            ]
        ];
        RightPane->AddSlot().FillHeight(1.0f)
        [
            SNew(SScrollBox) + SScrollBox::Slot()[NodeColumn]
        ];

        Body->AddSlot().FillHeight(1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).WidthOverride(240.0f)[Selector]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(16.0f, 0.0f, 0.0f, 0.0f)
            [
                RightPane
            ]
        ];
    }

    if (!SkillTreeStatus.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            MenuText(SkillTreeStatus, 10, Cyan, true)
        ];
    }
    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).WidthOverride(180.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)]
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(14.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
        [
            // O2: node numbers are not balanced yet.
            MenuText(FText::FromString(TEXT("Hover a node for full detail  |  [O2] values are placeholder until TTK re-anchoring  |  ESC Back")), 9, SoftText)
        ]
    ];
    return BuildFrame(FText::FromString(TEXT("SKILL TREES")), FText::FromString(TEXT("CLASS TREE / CORE CONSTELLATIONS")), Body, 1120.0f);
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
