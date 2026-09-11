// THE RIFT DEBRIEF — SBreakerMenu::ShowRiftDebrief and BuildRiftDebriefScreen,
// in their own translation unit (ui.md: BreakerMenu.cpp is over 11,000 lines;
// a new screen goes in its own TU).
//
// Owner-asked: "i really like the entering rift screen so maybe when a rift is
// closed we can add something very similar that shows the items we gained from
// completion/on completion kinda like a loot highlight", and then, on seeing
// the first draft: "that completion screen is not the same as the loading in
// one". So this is the BRIEFING'S TWIN and is DRAWN FROM THE BRIEFING'S OWN
// PIECES (UI/BreakerLoadingScreen.h): the same card frame, the same headline
// block, the same gold rail, the same stat row, the same lattice breathing
// under the stage line. Where the deployment card puts the area level, this
// puts the haul. It decides nothing: every string and every number arrives in
// BreakerRiftDebrief::FModel, composed from the game mode's run ledger.
//
// ONE VERB. The death screen has two because both are level travels and the
// player must choose; here the run is already over and the only question left
// is when they have finished looking. CONTINUE takes the player back to where
// they entered the rift from — the game mode's ReturnFromRift — so the screen
// is the whole of the closing beat, not a curtain in front of one.
//
// COLOUR BY VERB (O179). Rarity colours the item's own rail, because rarity is
// a noun the player already reads that way everywhere else; GOLD is the reward
// accent and carries the haul's rail. Nothing here is cyan (movement) or teal
// (a rift object) — the rift is closed, and this is what came out of it.

#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Data/BreakerStrings.h"
#include "Game/BreakerGameMode.h"
#include "UI/BreakerLoadingScreen.h"
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
    // The haul sits where the deployment card's level block sits, and its
    // width is the one number that is this screen's own: the card's headline
    // wraps at the content width minus 320, so anything wider than that
    // narrows the name column under RIFT CLOSED.
    constexpr float BreakerDebriefHaulWidth = 360.0f;
    constexpr float BreakerDebriefRowHeight = 34.0f;
    constexpr float BreakerDebriefRailWidth = 4.0f;
    constexpr float BreakerDebriefLevelColumn = 56.0f;
    constexpr float BreakerDebriefVerbWidth = 260.0f;
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
    // which is where most of a haul lives. The name ends in an ellipsis rather
    // than spilling over the level column: the column is fixed, the name is
    // not, and a name that crossed the card's edge would read as a defect.
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
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
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

    // ---- The haul ----------------------------------------------------------
    // Where the deployment card prints the area level, this prints what the
    // run paid, on the same gold rail.
    TSharedRef<SVerticalBox> Haul = SNew(SVerticalBox);
    if (M.IsEmptyHanded())
    {
        // SAID PLAINLY. An empty box reads as a broken screen; a sentence reads
        // as a run that paid nothing, which is what happened.
        Haul->AddSlot().AutoHeight()
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("NOTHING CAME BACK WITH YOU")))
                .ColorAndOpacity(BreakerUI::TextMuted)
                .Font(BreakerBodyFont(BreakerUI::TypeBody, true))
        ];
    }
    for (const BreakerRiftDebrief::FLine& Line : M.Highlights)
    {
        Haul->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, BreakerUI::Space4)
        [
            BreakerDebriefRow(Line)
        ];
    }
    if (M.MoreCount > 0)
    {
        Haul->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space4, 0.0f, 0.0f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("AND %d MORE IN YOUR PACK"), M.MoreCount)))
                .ColorAndOpacity(BreakerUI::TextMuted)
                .Font(BreakerBodyFont(BreakerUI::TypeCaption, true))
        ];
    }
    const TSharedRef<SWidget> HaulBlock = SBreakerLoadingScreen::MakeRailedBlock(
        SNew(SBox).WidthOverride(BreakerDebriefHaulWidth)[Haul]);

    // ---- The stat row ------------------------------------------------------
    // The two totals, in the deployment card's own readout shape.
    TArray<TPair<FString, FString>> Stats;
    Stats.Emplace(FString(TEXT("RIFTGLASS")), FString::FromInt(M.Riftglass));
    Stats.Emplace(FString(TEXT("EXPERIENCE")), FString::FromInt(M.Experience));
    const TSharedRef<SWidget> StatRow = SBreakerLoadingScreen::MakeStatRow(Stats);

    // ---- The lattice -------------------------------------------------------
    // The same pulse the deployment card breathes under OPENING THE RIFT; the
    // stage words are set here the way UBreakerGameInstance sets its own.
    const TSharedRef<SBreakerRiftLattice> Lattice = SNew(SBreakerRiftLattice);
    Lattice->SetStage(FText::FromString(BreakerStrings::Get(EBreakerStringKey::LoadingStageClosing)));

    // ---- The content block -------------------------------------------------
    // Kicker, headline over area name beside the haul, the stat row, the
    // lattice — the deployment card's order, slot for slot — then the verb.
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()
    [
        BreakerMonoText(FText::FromString(M.TierKicker), 11, BreakerUI::TextMuted, 0.22f)
    ];
    Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
        [
            SBreakerLoadingScreen::MakeHeadline(M.Headline, M.AreaName)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(BreakerUI::Space40, 0.0f, 0.0f, 0.0f)
        [
            HaulBlock
        ]
    ];
    Content->AddSlot().AutoHeight().Padding(0.0f, 56.0f, 0.0f, 0.0f)[StatRow];
    Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space40, 0.0f, 0.0f)[Lattice];
    Content->AddSlot().AutoHeight().Padding(0.0f, BreakerDebriefVerbGap, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(BreakerDebriefVerbWidth)
        [
            MakeButton(FText::FromString(TEXT("CONTINUE")),
                FOnClicked::CreateSP(this, &SBreakerMenu::CloseRiftDebrief), true)
        ]
    ];

    // THE CARD, not a scrim. The first draft laid a column over the live world
    // and the owner read it as a different screen from the one he liked; the
    // frame is the deployment card's own — opaque, ruled, badged, crawling.
    return SBreakerLoadingScreen::MakeCardFrame(Content, Lattice->GetCrawl());
}

FReply SBreakerMenu::CloseRiftDebrief()
{
    // BACK TO WHERE YOU ENTERED FROM. The pawn under this screen is standing in
    // a rift that is now closed, and CONTINUE is the whole of the way out: the
    // game mode's ReturnFromRift is the one travel path for a closed rift, the
    // same shape the local map's travel rows use. Travel FIRST, resume second —
    // travel is legal while the menu holds the pause, and resuming first would
    // unpause a world that is about to be torn down by the level change.
    if (!Character.IsValid()) return FReply::Handled();
    UWorld* World = Character->GetWorld();
    ABreakerGameMode* Mode = World ? World->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    if (Mode) Mode->ReturnFromRift(Character.Get());
    if (Character.IsValid()) Character->ResumeFromMenu();
    return FReply::Handled();
}
