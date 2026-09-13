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
// THE OFFER (O270): THREE OFFERED, ONE CHOSEN. A closed rift offers three
// items and the player takes one home. The three sit side by side where the
// haul sat, each a square wearing the inventory's rarity card (rail, tally,
// name, level); hovering one paints its full stats into the host beneath,
// against the piece the player is wearing, exactly as the inventory's rail
// does. THE SQUARE SELECTS, CONTINUE CLAIMS. A one-click irrevocable choice
// under a firing hand — the player has just come out of a fight and is still
// clicking — is the mis-click the confirm prevents: the square only moves
// the ring, and the verb is what puts the item in the pack. Until a square
// is chosen the verb reads CHOOSE ONE and refuses. The kill haul keeps its
// gold rail beneath the squares, because the two are different things: the
// haul is already in the pack; the offer is not, until the claim.
//
// ONE VERB. The death screen has two because both are level travels and the
// player must choose; here the run is already over and the only question left
// is when they have finished looking. CONTINUE claims the chosen offer and
// takes the player back to where they entered the rift from — the game
// mode's ReturnFromRift — so the screen is the whole of the closing beat,
// not a curtain in front of one.
//
// COLOUR BY VERB (O179). Rarity colours the item's own rail, because rarity is
// a noun the player already reads that way everywhere else; GOLD is the reward
// accent and carries the haul's rail. The chosen ring is bone (the player's
// own readouts), with a raised face and a fixed 2px footprint so selection
// never changes the card geometry. Nothing here is cyan (movement) or teal (a rift object)
// except the top rarity's own frame — the rift is closed, and this is what
// came out of it.

#include "UI/BreakerMenu.h"

#include "Characters/BreakerCharacter.h"
#include "Data/BreakerStrings.h"
#include "Game/BreakerGameMode.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "UI/BreakerLoadingScreen.h"
#include "UI/BreakerTypeRoles.h"
#include "UI/BreakerUIStyle.h"

#include "Styling/CoreStyle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
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
    // The offer squares (O270). Three across at the inventory's 16px card
    // gap; the stats host beneath takes the row's full width so the affix
    // wrap is computed from a known figure, never from allotted space.
    constexpr float BreakerDebriefSquareSize = 200.0f;      // O2 PLACEHOLDER
    constexpr float BreakerDebriefSquareGap = 16.0f;        // O2 PLACEHOLDER
    constexpr int32 BreakerDebriefTallyBoxes = 5;
    constexpr float BreakerDebriefTallyBox = 8.0f;          // O2 PLACEHOLDER
    constexpr float BreakerDebriefTallyGap = 2.0f;          // O2 PLACEHOLDER
    constexpr float BreakerDebriefOfferBlockWidth =
        BreakerDebriefSquareSize * 3.0f + BreakerDebriefSquareGap * 2.0f;

    TSharedRef<SWidget> BreakerDebriefSolid(const FLinearColor& Colour)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            [
                SNew(SSpacer).Size(FVector2D(1.0f, 1.0f))
            ];
    }

    // A ring of drawn geometry: a solid border of the given thickness around
    // Inner. The same shape BreakerMenu.cpp's file-local BorderWrap draws.
    TSharedRef<SWidget> BreakerDebriefRing(const TSharedRef<SWidget>& Inner, const FLinearColor& Colour, float Thickness)
    {
        return SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(Colour)
            .Padding(FMargin(Thickness))
            [
                Inner
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

    // The rarity tally as the inventory card draws it: five 8x8 boxes at a
    // 2px gap, the first rank+1 filled in the rarity's colour and the rest in
    // the rest ring. The rank is the enum's own order (Standard 0 ..
    // Unwritten 4), which is append-only. Drawn here rather than borrowed
    // because BreakerMenu.cpp's tally is file-local to that TU.
    TSharedRef<SWidget> BreakerDebriefRarityTally(EBreakerItemRarity Rarity)
    {
        const int32 Filled = static_cast<int32>(Rarity) + 1;
        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        for (int32 Index = 0; Index < BreakerDebriefTallyBoxes; ++Index)
        {
            Row->AddSlot().AutoWidth()
                .Padding(0.0f, 0.0f, Index < BreakerDebriefTallyBoxes - 1 ? BreakerDebriefTallyGap : 0.0f, 0.0f)
            [
                SNew(SBox).WidthOverride(BreakerDebriefTallyBox).HeightOverride(BreakerDebriefTallyBox)
                [
                    BreakerDebriefSolid(Index < Filled ? BreakerUI::RarityColor(Rarity) : BreakerUI::BorderRest)
                ]
            ];
        }
        return Row;
    }

    // ONE OFFERED SQUARE (O270). The inventory's rarity card folded into a
    // fixed square: the 3px rarity rail on the left edge, the tally, the
    // name, the level. The rest ring follows the inventory card's rule — a
    // 1px BorderRest, or the rarity's own colour for the tiers that take a
    // full border (BreakerUI::RarityGetsFullBorder). The chosen square draws
    // a bone ring and a raised panel face, with the same geometry as rest.
    //
    // The click SELECTS: it moves the ring and nothing else. The hover paints
    // the stats detail into the host beneath. Neither is the claim.
    TSharedRef<SWidget> BreakerDebriefOfferSquare(const BreakerRiftDebrief::FOfferRow& Row, int32 Index, bool bChosen,
        const FOnClicked& OnClicked, const FSimpleDelegate& OnHovered)
    {
        const FLinearColor Rail = BreakerUI::RarityColor(Row.Line.Rarity);
        const FLinearColor RestRing = BreakerUI::RarityGetsFullBorder(Row.Line.Rarity) ? Rail : BreakerUI::BorderRest;
        const FLinearColor Ring = bChosen ? BreakerUI::TextPrimary : RestRing;
        const float RingThickness = BreakerUI::BorderSelected; // Selection changes paint, never content geometry.

        return SNew(SBox).WidthOverride(BreakerDebriefSquareSize).HeightOverride(BreakerDebriefSquareSize)
        [
            BreakerDebriefRing(
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(BreakerUI::RailThickness)[BreakerDebriefSolid(Rail)]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity(bChosen ? BreakerUI::Panel20 : BreakerUI::Panel10)
                    .ContentPadding(FMargin(BreakerUI::Space12, BreakerUI::Space12))
                    .HAlign(HAlign_Fill)
                    .VAlign(VAlign_Fill)
                    .OnClicked(OnClicked)
                    .OnHovered(OnHovered)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            BreakerDebriefRarityTally(Row.Line.Rarity)
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center)
                          .Padding(0.0f, BreakerUI::Space8, 0.0f, BreakerUI::Space8)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Row.Line.Name))
                                .ColorAndOpacity(Rail)
                                // Wrapped to the width the slot actually
                                // gives it: a figure computed from the
                                // square's parts came out wider than the
                                // slot and "of Zenith" left the frame.
                                .WrapTextAt(BreakerDebriefSquareSize - 40.0f)
                                .Font(BreakerBodyFont(BreakerUI::TypeBody, true))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(FString::Printf(TEXT("i%d"), Row.Line.ItemLevel)))
                                .ColorAndOpacity(BreakerUI::TextMuted)
                                .Font(BreakerBodyFont(BreakerUI::TypeCaption, true))
                        ]
                    ]
                ],
                Ring, RingThickness)
        ];
    }

    // THE REFUSING VERB, painted as disabled (ui.md: painted, never faded):
    // the button's own geometry and height, the rest ring, the plate face,
    // the label in the disabled text colour. The click still lands on
    // CloseRiftDebrief, which refuses it while nothing is chosen — so a
    // click here does what the label says, which is nothing yet.
    TSharedRef<SWidget> BreakerDebriefDisabledVerb(const FText& Label, const FOnClicked& OnClicked)
    {
        return SNew(SBox).HeightOverride(BreakerUI::MinHitTarget + BreakerUI::Space8)
        [
            BreakerDebriefRing(
                SNew(SButton)
                .ButtonColorAndOpacity(BreakerUI::Panel00)
                .ContentPadding(FMargin(BreakerUI::Space16, BreakerUI::Space8))
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Center)
                .OnClicked(OnClicked)
                [
                    SNew(STextBlock)
                        .Text(Label)
                        .Justification(ETextJustify::Left)
                        .ColorAndOpacity(BreakerUI::TextDisabled)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BreakerUI::TypeBody))
                ],
                BreakerUI::BorderRest, BreakerUI::BorderThin)
        ];
    }
}

void SBreakerMenu::ShowRiftDebrief(const BreakerRiftDebrief::FModel& Model)
{
    RiftDebriefModel = Model;
#if !UE_BUILD_SHIPPING
    // Photograph a selected card as well as the unselected state. This selects
    // an existing offer only; it cannot grant or claim an item.
    FString CaptureScreen;
    int32 CaptureIndex = INDEX_NONE;
    if (FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureMenu="),CaptureScreen)
        && CaptureScreen.Equals(TEXT("RIFTDEBRIEF"),ESearchCase::IgnoreCase)
        && FParse::Value(FCommandLine::Get(),TEXT("BreakerCaptureRewardIndex="),CaptureIndex)
        && RiftDebriefModel.Offer.IsValidIndex(CaptureIndex))
        RiftDebriefModel.ChosenIndex = CaptureIndex;
#endif
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

    // ---- The offer (O270) --------------------------------------------------
    // Three squares side by side, the stats host beneath them, and the haul
    // beneath that on its own rail. When nothing is offered the haul stands
    // where it always stood and no host is made.
    RiftDebriefDetailHost.Reset();
    TSharedRef<SWidget> SideBlock = HaulBlock;
    if (!M.Offer.IsEmpty())
    {
        UBreakerEquipmentComponent* Equipment = Character.IsValid() ? Character->GetEquipment() : nullptr;
        TSharedRef<SHorizontalBox> Squares = SNew(SHorizontalBox);
        for (int32 Index = 0; Index < M.Offer.Num(); ++Index)
        {
            const BreakerRiftDebrief::FOfferRow& Row = M.Offer[Index];
            // The deltas against the equipped piece, as the inventory card
            // computes them; captured by value with the item so the hover
            // builds the detail lazily and reads no widget state.
            const TArray<FBreakerAffixComparison> HoverDeltas = Equipment
                ? Equipment->PreviewEquip(Row.Item).AffixDeltas : TArray<FBreakerAffixComparison>();
            const FBreakerItemInstance HoverItem = Row.Item;
            const bool bLast = Index == M.Offer.Num() - 1;
            Squares->AddSlot().AutoWidth().Padding(0.0f, 0.0f, bLast ? 0.0f : BreakerDebriefSquareGap, 0.0f)
            [
                BreakerDebriefOfferSquare(Row, Index, Index == M.ChosenIndex,
                    // THE SELECT. One rebuild per click, because the ring is
                    // drawn from the model; nothing rebuilds on hover.
                    FOnClicked::CreateLambda([this, Index]()
                    {
                        RiftDebriefModel.ChosenIndex = Index;
                        Rebuild(EBreakerMenuScreen::RiftDebrief);
                        return FReply::Handled();
                    }),
                    // THE HOVER. SetContent, never Rebuild: a rebuild per
                    // hover is the historical jitter pattern, and it would
                    // also destroy the widget whose hover is being handled.
                    FSimpleDelegate::CreateLambda([this, HoverItem, HoverDeltas]()
                    {
                        if (RiftDebriefDetailHost.IsValid())
                        {
                            RiftDebriefDetailHost->SetContent(
                                SBreakerMenu::MakeItemDetail(HoverItem, HoverDeltas, BreakerDebriefOfferBlockWidth - 20.0f, Character.Get()));
                        }
                    }))
            ];
        }
        // The host's seed: the chosen square if there is one, else the first,
        // so the host has content before anything is pointed at and a capture
        // (which cannot hover) is never of a blank rectangle. Sticky after
        // that: leaving a square leaves its detail up.
        const int32 SeedIndex = M.Offer.IsValidIndex(M.ChosenIndex) ? M.ChosenIndex : 0;
        const TArray<FBreakerAffixComparison> SeedDeltas = Equipment
            ? Equipment->PreviewEquip(M.Offer[SeedIndex].Item).AffixDeltas : TArray<FBreakerAffixComparison>();
        SideBlock = SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                Squares
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
            [
                SNew(SBox).WidthOverride(BreakerDebriefOfferBlockWidth).HeightOverride(240.0f) // O2 PLACEHOLDER: invariant selection footprint.
                [SNew(SScrollBox) + SScrollBox::Slot()
                [SAssignNew(RiftDebriefDetailHost, SBox).WidthOverride(BreakerDebriefOfferBlockWidth - 20.0f)
                [
                    SBreakerMenu::MakeItemDetail(M.Offer[SeedIndex].Item, SeedDeltas, BreakerDebriefOfferBlockWidth - 20.0f, Character.Get())
                ]]]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
            [
                HaulBlock
            ];
    }

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

    // ---- The verb ----------------------------------------------------------
    // CHOOSE ONE, painted disabled, until a square is chosen; CONTINUE after.
    // Both land on CloseRiftDebrief, which is the one place the gate is
    // enforced — the label describes the refusal, it does not implement it.
    const FOnClicked OnContinue = FOnClicked::CreateSP(this, &SBreakerMenu::CloseRiftDebrief);
    const TSharedRef<SWidget> Verb = M.CanContinue()
        ? MakeButton(FText::FromString(BreakerStrings::Get(EBreakerStringKey::DebriefContinue)), OnContinue, true)
        : BreakerDebriefDisabledVerb(FText::FromString(BreakerStrings::Get(EBreakerStringKey::DebriefChooseOne)), OnContinue);

    // ---- The content block -------------------------------------------------
    // Kicker, headline over area name beside the haul (or the offer), the
    // stat row, the lattice — the deployment card's order, slot for slot —
    // then the verb.
    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
    Content->AddSlot().AutoHeight()
    [
        BreakerMonoText(FText::FromString(M.TierKicker), 11, BreakerUI::TextMuted, 0.22f)
    ];
    // The headline needs its whole column: at 104 px "RIFT CLOSED" is wider
    // than what is left beside three squares, and it clipped to RIFT CLOSE.
    // With an offer up, the offer block sits UNDER the headline at full
    // width; with only a kill haul, the haul sits beside it, as the level
    // block does on the deployment card.
    if (M.Offer.IsEmpty())
    {
        Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Top)
            [
                SBreakerLoadingScreen::MakeHeadline(M.Headline, M.AreaName)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(BreakerUI::Space40, 0.0f, 0.0f, 0.0f)
            [
                SideBlock
            ]
        ];
    }
    else
    {
        Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space16, 0.0f, 0.0f)
        [
            SBreakerLoadingScreen::MakeHeadline(M.Headline, M.AreaName)
        ];
        Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space24, 0.0f, 0.0f)
        [
            SideBlock
        ];
    }
    Content->AddSlot().AutoHeight().Padding(0.0f, 56.0f, 0.0f, 0.0f)[StatRow];
    Content->AddSlot().AutoHeight().Padding(0.0f, BreakerUI::Space40, 0.0f, 0.0f)[Lattice];
    Content->AddSlot().AutoHeight().Padding(0.0f, BreakerDebriefVerbGap, 0.0f, 0.0f)
    [
        SNew(SBox).WidthOverride(BreakerDebriefVerbWidth)
        [
            Verb
        ]
    ];

    // THE CARD, not a scrim. The first draft laid a column over the live world
    // and the owner read it as a different screen from the one he liked; the
    // frame is the deployment card's own — opaque, ruled, badged, crawling.
    return SBreakerLoadingScreen::MakeCardFrame(Content, Lattice->GetCrawl());
}

FReply SBreakerMenu::CloseRiftDebrief()
{
    // THE GATE. An offer left unchosen is refused here, whatever the button
    // said — the label is a description of this line, not a second copy of
    // it. With nothing offered there is nothing to gate.
    if (!RiftDebriefModel.CanContinue()) return FReply::Handled();
    if (!Character.IsValid()) return FReply::Handled();
    // THE CLAIM, FIRST. The chosen item goes into the pack through the
    // progression component (O270) while the run ledger is still open and
    // the pawn is still in the rift, so the claim lands in the ledger the
    // way a pickup does. A refusal here is the component's to log; the
    // screen has nothing to add to it and the travel out still happens —
    // the rift is closed either way.
    if (!RiftDebriefModel.Offer.IsEmpty())
    {
        if (UBreakerProgressionComponent* Progression = Character->GetProgression())
        {
            Progression->ClaimRiftCompletionOffer(RiftDebriefModel.ChosenIndex);
        }
    }
    // BACK TO WHERE YOU ENTERED FROM. The pawn under this screen is standing in
    // a rift that is now closed, and CONTINUE is the whole of the way out: the
    // game mode's ReturnFromRift is the one travel path for a closed rift, the
    // same shape the local map's travel rows use. Travel FIRST, resume second —
    // travel is legal while the menu holds the pause, and resuming first would
    // unpause a world that is about to be torn down by the level change.
    UWorld* World = Character->GetWorld();
    ABreakerGameMode* Mode = World ? World->GetAuthGameMode<ABreakerGameMode>() : nullptr;
    if (Mode) Mode->ReturnFromRift(Character.Get());
    if (Character.IsValid()) Character->ResumeFromMenu();
    return FReply::Handled();
}
