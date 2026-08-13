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
#include "Algo/Reverse.h"
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
    // Destructive-confirm accent; only ever used for an armed cleanup button.
    const FLinearColor Amber(1.0f, 0.5f, 0.08f, 1.0f);

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
            SNew(SBox).WidthOverride(PanelWidth).MaxDesiredHeight(820.0f)
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

TSharedRef<SWidget> SBreakerMenu::BuildScreenTabs(EBreakerMenuScreen ActiveScreen)
{
    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
    auto AddTab = [this, &Tabs, ActiveScreen](const FString& Label, EBreakerMenuScreen Target)
    {
        const bool bActive = ActiveScreen == Target;
        Tabs->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(bActive ? Cyan : PanelRaised)
            .ContentPadding(FMargin(16.0f, 6.0f))
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
                MenuText(FText::FromString(Label), 11, FLinearColor::White, true)
            ]
        ];
    };
    AddTab(TEXT("EQUIPMENT"), EBreakerMenuScreen::Inventory);
    AddTab(TEXT("SKILL TREES"), EBreakerMenuScreen::SkillTrees);
    return Tabs;
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
        MenuText(FText::FromString(TEXT("WASD  Move       SHIFT  Sprint toggle       SPACE  Jump\nQ  Dash           C / CTRL  Slide             R  Reload\nLMB  Fire         RMB  Aim                   1 / 2  Weapon slots\nI  Inventory      F  Talk to NPC             F4  Start wave\nF1  Reset         F2  Copy report            F3  Diagnostics")), 11, SoftText)
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
                    SNew(SButton)
                    .ButtonColorAndOpacity(bAssigned ? Cyan : PanelRaised)
                    .HAlign(HAlign_Center).VAlign(VAlign_Center)
                    .OnClicked(FOnClicked::CreateLambda([this, CapturedSlot, CapturedArchetype]()
                    {
                        if (Character.IsValid() && Character->GetWeapon()) Character->GetWeapon()->SetSlotArchetype(CapturedSlot, CapturedArchetype);
                        Rebuild(EBreakerMenuScreen::Loadout);
                        return FReply::Handled();
                    }))
                    [
                        MenuText(FText::FromString(Entry.Name), 12, FLinearColor::White, true)
                    ]
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
    FLinearColor RarityColor(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon: return FLinearColor(0.25f, 0.55f, 1.0f);
            case EBreakerItemRarity::Exceptional: return FLinearColor(0.72f, 0.4f, 1.0f);
            case EBreakerItemRarity::Aberrant: return FLinearColor(1.0f, 0.25f, 0.25f);
            case EBreakerItemRarity::Anomalous: return FLinearColor(0.15f, 0.95f, 0.85f);
            default: return FLinearColor(0.85f, 0.85f, 0.85f);
        }
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
        const FLinearColor Accent = bHasItem ? RarityColor(Item.Rarity) : SoftText * 0.6f;
        const FString Name = bHasItem ? RarityName(Item.Rarity) : TEXT("EMPTY");
        const FString Details = bHasItem ? DescribeItem(Item) : TEXT("—");

        return SNew(SBox).MinDesiredHeight(72.0f).Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(PanelRaised)
            .ContentPadding(FMargin(12.0f, 8.0f))
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
                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(SlotName(Slot)), 10, Cyan, true)]
                    + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(Name), 10, Accent, true)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    MenuText(FText::FromString(Details), 10, bHasItem ? FLinearColor::White : SoftText)
                ]
            ]
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
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(PanelRaised)
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                MenuText(FText::FromString(TEXT("[ CHARACTER ]\n\nmodel render arrives\nwith the presentation\npass")), 10, SoftText)
            ]
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
        CharacterPanel->AddSlot().AutoHeight()[MenuText(FText::FromString(StatText), 10, FLinearColor::White)];
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
        const FString DeltaLine = bSlotOccupied
            ? FString::Printf(TEXT("replaces: %s ilvl %d"), *RarityName(CurrentlyEquipped.Rarity), CurrentlyEquipped.ItemLevel)
            : FString(TEXT("slot empty"));
        const FLinearColor DeltaColor = bSlotOccupied ? RarityColor(CurrentlyEquipped.Rarity) : SoftText;

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
                        SNew(SButton)
                        .ButtonColorAndOpacity(PanelRaised)
                        .ContentPadding(FMargin(12.0f, 9.0f))
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
                                + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(SlotName(Item.Slot)), 11, Cyan, true)]
                                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 22.0f, 0.0f)[MenuText(FText::FromString(RarityName(Item.Rarity)), 11, RarityColor(Item.Rarity), true)]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                            [
                                MenuText(FText::FromString(DeltaLine), 9, DeltaColor, true)
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
                            [
                                MenuText(FText::FromString(DescribeItem(Item)), 10, FLinearColor::White)
                            ]
                        ]
                    ]
                ]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(4.0f, 4.0f, 4.0f, 0.0f)
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(Panel)
                    .ContentPadding(FMargin(6.0f, 1.0f))
                    .ToolTipText(FText::FromString(TEXT("Discard this item (or right-click the card)")))
                    .OnClicked(DiscardOne)
                    [
                        MenuText(FText::FromString(TEXT("X")), 9, SoftText, true)
                    ]
                ]
            ]
        ];
    }

    // Slot filter row: ALL plus one chip per equipment slot.
    TSharedRef<SHorizontalBox> FilterRow = SNew(SHorizontalBox);
    auto AddFilterChip = [this, &FilterRow](const FString& Label, int32 FilterValue)
    {
        FilterRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(BackpackSlotFilter == FilterValue ? Cyan : PanelRaised)
            .ContentPadding(FMargin(9.0f, 4.0f))
            .OnClicked(FOnClicked::CreateLambda([this, FilterValue]()
            {
                BackpackSlotFilter = FilterValue;
                Rebuild(EBreakerMenuScreen::Inventory);
                return FReply::Handled();
            }))
            [
                MenuText(FText::FromString(Label), 9, FLinearColor::White, true)
            ]
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
        CleanupRow->AddSlot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(bArmed ? Amber : PanelRaised)
            .ContentPadding(FMargin(9.0f, 4.0f))
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
                MenuText(FText::FromString(bArmed ? FString(TEXT("CONFIRM?")) : Label), 9, FLinearColor::White, true)
            ]
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
                    MenuText(FText::FromString(FString::Printf(TEXT("GRANT TEST GEAR ilvl %d"), ItemLevel)), 9, FLinearColor::White, true)
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
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(bIsCurrent ? Cyan : PanelRaised)
            .IsEnabled(bSelectable)
            .ContentPadding(FMargin(16.0f, 11.0f))
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
                    + SHorizontalBox::Slot().FillWidth(1.0f)[MenuText(FText::FromString(Entry.Name), 16, FLinearColor::White, true)]
                    + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(Entry.Resource), 11, Cyan, true)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[MenuText(FText::FromString(Entry.Branches), 10, SoftText, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)[MenuText(FText::FromString(Entry.Pitch), 10, SoftText)]
            ]
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

    // Rank readout as pips when the node is cheap enough to show them,
    // otherwise the plain x/y count.
    FString RankPips(int32 Rank, int32 MaxRank)
    {
        if (MaxRank <= 0 || MaxRank > 6) return FString::Printf(TEXT("%d/%d"), Rank, MaxRank);
        FString Pips;
        for (int32 Index = 0; Index < MaxRank; ++Index) Pips += Index < Rank ? TEXT("●") : TEXT("○");
        return Pips;
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

    // Unspent-points banner — the one number the player is spending against.
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(PanelRaised)
        .Padding(FMargin(16.0f, 10.0f))
        [
            MenuText(FText::FromString(FString::Printf(TEXT("CLASS %d  |  CORE %d  UNSPENT"), UnspentClass, UnspentCore)), 14, Cyan, true)
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
                    MenuText(FText::FromString(TEXT("DEV: GRANT SLICE POINTS")), 10, FLinearColor::Black, true)
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

            Selector->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(bSelected ? Cyan : PanelRaised)
                .ContentPadding(FMargin(12.0f, 9.0f))
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
                    + SVerticalBox::Slot().AutoHeight()[MenuText(FText::FromString(TreeName), 12, FLinearColor::White, true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(FString::Printf(TEXT("%d / %d SPENT   %d %s UNSPENT"), Spent, Total, Unspent, *CurrencyLabel(Tree->Currency))), 9, bSelected ? FLinearColor::White : SoftText)
                    ]
                ]
            ];
        }

        // Right side: the selected tree's nodes, one row per investment gate.
        const UBreakerProgressionTree* Selected = Trees[SelectedTreeIndex];
        int32 SelectedSpent = 0;
        int32 SelectedTotal = 0;
        ProgressionTreeInvestment(Progression, Selected, SelectedSpent, SelectedTotal);

        TArray<int32> TierGates;
        for (const UBreakerProgressionNode* Node : Selected->Nodes)
        {
            if (Node) TierGates.AddUnique(Node->RequiredTreeInvestment);
        }
        TierGates.Sort();

        TSharedRef<SVerticalBox> NodeColumn = SNew(SVerticalBox);
        int32 TierNumber = 0;
        for (const int32 Gate : TierGates)
        {
            ++TierNumber;
            const FString TierHeader = Gate > 0
                ? FString::Printf(TEXT("TIER %d — GATE %d INVESTED  (%d)"), TierNumber, Gate, SelectedSpent)
                : FString::Printf(TEXT("TIER %d"), TierNumber);
            NodeColumn->AddSlot().AutoHeight().Padding(0.0f, TierNumber == 1 ? 0.0f : 10.0f, 0.0f, 6.0f)
            [
                MenuText(FText::FromString(TierHeader), 10, Gate > SelectedSpent ? SoftText : Cyan, true)
            ];

            // Fixed rows of three, never a wrap box: SWrapBox sized by its
            // allotted width inside a scroll box re-measures to a different
            // answer every frame — the layout oscillation the owner saw as
            // the screen "bouncing between two sizes".
            TSharedRef<SVerticalBox> TierRow = SNew(SVerticalBox);
            TSharedPtr<SHorizontalBox> CurrentRow;
            int32 CardIndex = 0;
            for (const UBreakerProgressionNode* Node : Selected->Nodes)
            {
                if (!Node || Node->RequiredTreeInvestment != Gate) continue;

                const int32 Rank = ProgressionGetNodeRank(Progression, Node->NodeId, Node->Currency);
                FString LockReason;
                const bool bPurchasable = SkillNodeIsPurchasable(Progression, Selected, Node, SelectedSpent, LockReason);
                const bool bOwned = Rank > 0;
                const bool bMaxed = Rank >= Node->MaxRank;

                // Owned reads cyan, purchasable reads as a raised panel,
                // locked reads dimmed with its reason spelled out.
                const FLinearColor CardColor = bOwned ? Cyan : (bPurchasable ? PanelRaised : Panel);
                const FLinearColor NameColor = (bPurchasable || bOwned) ? FLinearColor::White : SoftText * 0.8f;

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
                            const bool bPercent = Effect.StatBucket != EBreakerNodeStatBucket::Flat;
                            TipLines.Add(FString::Printf(TEXT("  %s  %+.1f%s"),
                                *UEnum::GetDisplayValueAsText(Effect.StatTarget).ToString().ToUpper(),
                                Effect.ValuePerRank, bPercent ? TEXT("%") : TEXT("")));
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

                TSharedRef<SVerticalBox> Card = SNew(SVerticalBox);
                Card->AddSlot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[MenuText(FText::FromString(NodeName), 12, NameColor, true)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 6.0f, 0.0f)
                    [
                        MenuText(FText::FromString(RankPips(Rank, Node->MaxRank)), 10, bOwned ? FLinearColor::White : SoftText, true)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(Background)
                        .Padding(FMargin(6.0f, 2.0f))
                        [
                            MenuText(FText::FromString(CostChip), 9, bMaxed ? SoftText : Cyan, true)
                        ]
                    ]
                ];
                Card->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    // PERF: this line used to be a Text_Lambda that polled
                    // FSlateApplication::GetModifierKeys() for Alt-expansion —
                    // once per card (~46) per frame, which made the screen
                    // jitter. The summary is now baked at build time and the
                    // hover tooltip is the only path to full detail.
                    .Text(FText::FromString(ShortSummary(Description)))
                    .ColorAndOpacity(bOwned ? FLinearColor::White : SoftText)
                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
                    .AutoWrapText(true)
                ];

                if (CardIndex % 3 == 0)
                {
                    CurrentRow = SNew(SHorizontalBox);
                    TierRow->AddSlot().AutoHeight()[CurrentRow.ToSharedRef()];
                }
                ++CardIndex;
                CurrentRow->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 8.0f)
                [
                    // Tooltip lives on the wrapper too so locked (disabled)
                    // cards still explain themselves on hover.
                    SNew(SBox).WidthOverride(250.0f)
                    .ToolTipText(TooltipTextValue)
                    [
                        SNew(SButton)
                        .ButtonColorAndOpacity(CardColor)
                        .IsEnabled(bPurchasable)
                        .ToolTipText(TooltipTextValue)
                        .ContentPadding(FMargin(12.0f, 8.0f))
                        .OnClicked(FOnClicked::CreateLambda([this, CapturedTree, CapturedNodeId]()
                        {
                            UBreakerProgressionComponent* Prog = Character.IsValid() ? Character->GetProgression() : nullptr;
                            FText FailureReason;
                            if (ProgressionPurchaseNode(Prog, CapturedTree, CapturedNodeId, FailureReason))
                            {
                                SkillTreeStatus = FText::FromString(FString::Printf(TEXT("ALLOCATED %s"), *CapturedNodeId.ToString().ToUpper()));
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
                        ]
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
                MenuText(FText::FromString(FString::Printf(TEXT("%s — %d / %d POINTS"), *SelectedName, SelectedSpent, SelectedTotal)), 13, Cyan, true)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .ButtonColorAndOpacity(PanelRaised)
                .ContentPadding(FMargin(14.0f, 7.0f))
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
                    MenuText(FText::FromString(TEXT("RESPEC")), 10, FLinearColor::White, true)
                ]
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
    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 18.0f)
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(PanelRaised)
        .Padding(FMargin(20.0f, 16.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MenuText(NPC->GetDisplayName(), 12, Cyan, true)]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(Node.SpeakerLine))
                .ColorAndOpacity(FLinearColor::White)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
                .AutoWrapText(true)
            ]
        ]
    ];

    int32 ChoiceNumber = 0;
    for (const FBreakerDialogueChoice& Choice : Node.Choices)
    {
        ++ChoiceNumber;
        const FName NextNodeId = Choice.NextNodeId;
        const FName QuestFlag = Choice.SetsQuestFlag;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(PanelRaised)
            .ContentPadding(FMargin(16.0f, 10.0f))
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
                MenuText(FText::FromString(FString::Printf(TEXT("%d.  %s"), ChoiceNumber, *Choice.Text)), 12, FLinearColor::White, true)
            ]
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
