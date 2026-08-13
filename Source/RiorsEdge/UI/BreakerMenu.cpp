#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
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

void SBreakerMenu::HandleEscape()
{
    if (CurrentScreen == EBreakerMenuScreen::Settings || CurrentScreen == EBreakerMenuScreen::Loadout || CurrentScreen == EBreakerMenuScreen::Inventory || CurrentScreen == EBreakerMenuScreen::ClassSelect)
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
        case EBreakerMenuScreen::Inventory: ContentHost->SetContent(BuildInventoryScreen()); break;
        case EBreakerMenuScreen::ClassSelect: ContentHost->SetContent(BuildClassSelectScreen()); break;
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
        MenuText(FText::FromString(TEXT("WASD  Move       SHIFT  Sprint toggle       SPACE  Jump\nQ  Dash           C / CTRL  Slide             R  Reload\nLMB  Fire         RMB  Aim                   1 / 2  Weapon slots\nI  Inventory\nF1  Reset         F2  Copy report            F3  Diagnostics")), 11, SoftText)
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
    // cards grow to fit their affix list so nothing truncates.
    TSharedRef<SWrapBox> BackpackGrid = SNew(SWrapBox).UseAllottedSize(true);
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
        BackpackGrid->AddSlot().Padding(0.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SBox).WidthOverride(300.0f)
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
                        + SHorizontalBox::Slot().AutoWidth()[MenuText(FText::FromString(RarityName(Item.Rarity)), 11, RarityColor(Item.Rarity), true)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
                    [
                        MenuText(FText::FromString(DescribeItem(Item)), 10, FLinearColor::White)
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

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
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
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            MenuText(FText::FromString(FString::Printf(TEXT("BACKPACK (%d/%d) — click to equip"), BackpackItems.Num(), TotalBackpackCount)), 10, SoftText, true)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f)[FilterRow]
    ];
    Body->AddSlot().FillHeight(1.0f)
    [
        BackpackItems.IsEmpty()
            ? StaticCastSharedRef<SWidget>(MenuText(FText::FromString(TEXT("Empty. Enemy kills drop rolled items.")), 11, SoftText))
            : StaticCastSharedRef<SWidget>(SNew(SScrollBox) + SScrollBox::Slot()[BackpackGrid])
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
        const bool bSelectable = CurrentClass == EBreakerClassId::None;
        const EBreakerClassId CapturedClass = Entry.ClassId;
        Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(bIsCurrent ? Cyan : PanelRaised)
            .IsEnabled(bSelectable)
            .ContentPadding(FMargin(16.0f, 11.0f))
            .OnClicked(FOnClicked::CreateLambda([this, CapturedClass]()
            {
                if (Character.IsValid() && Character->GetProgression() && Character->GetProgression()->ChoosePermanentClassById(CapturedClass))
                {
                    Character->SaveGameState();
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

    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateSP(this, &SBreakerMenu::GoBack), true)];
    return BuildFrame(FText::FromString(TEXT("BREAKER CLASS")), FText::FromString(TEXT("PERMANENT SELECTION / FIVE DISCIPLINES")), Body, 860.0f);
}

FReply SBreakerMenu::GoBack()
{
    Rebuild(RootScreen);
    return FReply::Handled();
}
