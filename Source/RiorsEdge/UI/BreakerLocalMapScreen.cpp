#include "UI/BreakerMenu.h"
#include "Data/BreakerStrings.h"
#include "Characters/BreakerCharacter.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Save/BreakerQuestContent.h"
#include "Save/BreakerMissionContent.h"
#include "UI/BreakerTypeRoles.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Input/SButton.h"
#include "UI/BreakerMapTravelRules.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Game/BreakerGameInstance.h"
#include "Game/BreakerGameMode.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Rendering/DrawElements.h"

class SBreakerLocalMapCanvas : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SBreakerLocalMapCanvas) {} SLATE_ARGUMENT(ABreakerCharacter*, Player) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Player = Args._Player;
        if (Player.IsValid())
        {
            Ground = Player->GetLocalMap()->GetGround();
            Markers = Player->GetLocalMap()->GetMarkers();
            Bounds = FBox2D(ForceInit);
            for (const auto& Box : Ground) { Bounds += Box.Min; Bounds += Box.Max; }
            for (const auto& Marker : Markers) Bounds += FVector2D(Marker.Location);
            Bounds += FVector2D(Player->GetActorLocation());
            Bounds = Bounds.ExpandBy(800); // O2 map margin, world cm.
        }
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640, 400); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        if (!Player.IsValid() || !Bounds.bIsValid) return Layer;
        const FVector2D Size = Geometry.GetLocalSize();
        // X is north on this local survey; rotate extents with the projection.
        const double RotatedScale = FMath::Min((Size.X - 48) / FMath::Max(1.0, Bounds.GetSize().Y), (Size.Y - 48) / FMath::Max(1.0, Bounds.GetSize().X));
        auto Point = [&](FVector2D P) { P = (P - Bounds.GetCenter()) * RotatedScale; return Size * .5 + FVector2D(P.Y, -P.X); };
        auto Line = [&](TArray<FVector2D> Points, FLinearColor Colour, float Width = 1)
        { FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour, true, Width); };
        for (const auto& Box : Ground)
            Line({Point(Box.Min), Point(FVector2D(Box.Max.X, Box.Min.Y)), Point(Box.Max), Point(FVector2D(Box.Min.X, Box.Max.Y)), Point(Box.Min)}, BreakerUI::BorderEmphasis);
        const FVector2D Here = Point(FVector2D(Player->GetActorLocation()));
        const FVector Forward = Player->GetActorForwardVector();
        const FVector2D Direction(Forward.Y, -Forward.X), Side(-Direction.Y, Direction.X);
        Line({Here + Direction * 10, Here - Direction * 7 + Side * 6, Here - Direction * 7 - Side * 6, Here + Direction * 10}, BreakerUI::TextPrimary, 2);
        int32 Index = 0;
        for (const auto& Marker : Markers)
        {
            if (!Player->GetLocalMap()->IsVisible(Marker)) continue;
            ++Index;
            const FVector2D P = Point(FVector2D(Marker.Location));
            const bool Tracked = Player->GetLocalMap()->GetTracked() == Marker.Id;
            const auto Colour = Marker.bObjective ? BreakerUI::Gold : Marker.bRift ? BreakerUI::TealName : BreakerUI::TextSecondary;
            if (Tracked) Line({Here, P}, Colour.CopyWithNewOpacity(.6f), 1);
            const double Radius = Tracked ? 10 : 7;
            Line({P + FVector2D(0,-Radius), P + FVector2D(Radius,0), P + FVector2D(0,Radius), P + FVector2D(-Radius,0), P + FVector2D(0,-Radius)}, Colour, Tracked ? 2 : 1);
            if (Marker.bObjective) Line({P + FVector2D(-12,-12), P + FVector2D(12,-12), P + FVector2D(12,12), P + FVector2D(-12,12), P + FVector2D(-12,-12)}, Colour, 1);
            FSlateDrawElement::MakeText(Elements, Layer + 2, Geometry.ToPaintGeometry(FVector2D(30,20), FSlateLayoutTransform(P + FVector2D(12,-10))),
                FString::FromInt(Index), BreakerBodyFont(12), ESlateDrawEffect::None, Colour);
        }
        return Layer + 2;
    }
private:
    TWeakObjectPtr<ABreakerCharacter> Player;
    TArray<FBreakerLocalMapMarker> Markers;
    TArray<FBox2D> Ground;
    FBox2D Bounds;
};

TSharedRef<SWidget> SBreakerMenu::BuildLocalMapScreen()
{
    auto* Player = Character.Get();
    if (!Player) return SNew(SBox);
    auto* Map = Player->GetLocalMap();
    if (Player->HasAuthority() && Map->DiscoverNearby(Player->GetActorLocation())) Player->SaveGameState();
    TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
    // The journal can hold concurrent quests; show every accepted one and
    // every unfinished objective rather than hiding them in the HUD sentence.
    if (const auto* Journal = Player->GetQuestJournal())
    {
        List->AddSlot().AutoHeight().Padding(0,8,0,8)
            [SNew(STextBlock).Text(FText::FromString(TEXT("ACTIVE QUESTS"))).Font(BreakerBodyFont(16,true)).ColorAndOpacity(BreakerUI::TextPrimary)];
        int32 Count = 0;
        for (const auto& Quest : UBreakerQuestLibrary::GetFallbackQuests())
        {
            const auto State = UBreakerQuestLibrary::ComputeQuestState(Quest, Journal->GetState());
            if (State != EBreakerQuestState::Active && State != EBreakerQuestState::ReadyToTurnIn) continue;
            ++Count;
            FString Detail = Quest.Title;
            if (State == EBreakerQuestState::ReadyToTurnIn) Detail += TEXT("  ·  RETURN TO ") + Quest.Giver;
            for (const auto& Objective : Quest.Objectives)
            {
                const bool Complete = Journal->HasFlag(Objective.CompletionFlag);
                Detail += FString::Printf(TEXT("\n%s %s"), Complete ? TEXT("DONE:") : TEXT("TODO:"), *Objective.Text);
                if (Objective.RequiredCount > 0) Detail += FString::Printf(TEXT("  %d/%d"),
                    FMath::Min(Journal->GetCounter(Objective.ProgressCounter),Objective.RequiredCount),Objective.RequiredCount);
            }
            List->AddSlot().AutoHeight().Padding(0,0,0,12)
                [SNew(STextBlock).Text(FText::FromString(Detail)).Font(BreakerBodyFont(14)).WrapTextAt(236)
                    .ColorAndOpacity(State == EBreakerQuestState::ReadyToTurnIn ? BreakerUI::Gold : BreakerUI::TextSecondary)];
        }
        if (!Count) List->AddSlot().AutoHeight().Padding(0,0,0,12)
            [SNew(STextBlock).Text(FText::FromString(TEXT("No side quests accepted. Talk to the quartermaster and salvager.")))
                .Font(BreakerBodyFont(14)).WrapTextAt(236).ColorAndOpacity(BreakerUI::TextMuted)];
    }
    List->AddSlot().AutoHeight().Padding(0,8)
        [SNew(STextBlock).Text(FText::FromString(TEXT("DISCOVERED SITES"))).Font(BreakerBodyFont(16,true)).ColorAndOpacity(BreakerUI::TextPrimary)];
    int32 Index = 0;
    for (const auto& Marker : Map->GetMarkers())
    {
        if (!Map->IsVisible(Marker)) continue;
        const FString Detail = Marker.Detail.IsEmpty() ? TEXT("") : Marker.Detail.ToString() + TEXT("   ·   ");
        const FString Caption = FString::Printf(TEXT("%d   %s%s\n%s%dm"), ++Index, Marker.bObjective ? *(BreakerStrings::Get(EBreakerStringKey::HudObjective) + TEXT(" · ")) : TEXT(""), *Marker.Label.ToString(), *Detail,
            FMath::RoundToInt(FVector::Dist2D(Player->GetActorLocation(), Marker.Location) / 100));
        List->AddSlot().AutoHeight().Padding(0,0,0,8)
        [
            SNew(SButton).ContentPadding(FMargin(12)).OnClicked_Lambda([this, Id = Marker.Id]()
            { if (Character.IsValid()) Character->GetLocalMap()->Track(Id); Rebuild(EBreakerMenuScreen::LocalMap); return FReply::Handled(); })
            [SNew(STextBlock).Text(FText::FromString(Caption)).Font(BreakerBodyFont(14)).AutoWrapText(true)
                .ColorAndOpacity(Map->GetTracked() == Marker.Id ? BreakerUI::Gold : BreakerUI::TextPrimary)]
        ];
    }
    List->AddSlot().AutoHeight().Padding(0,8)
    [MakeButton(FText::FromString(TEXT("CLEAR TRACKING")), FOnClicked::CreateLambda([this]()
        { if (Character.IsValid()) Character->GetLocalMap()->Track(NAME_None); Rebuild(EBreakerMenuScreen::LocalMap); return FReply::Handled(); }))];
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    if (!Map->GetCampaignObjective().IsEmpty())
        Body->AddSlot().AutoHeight().Padding(0,0,0,12)
        [SNew(STextBlock).Text(Map->GetCampaignObjective()).Font(BreakerBodyFont(14)).AutoWrapText(true).ColorAndOpacity(BreakerUI::Gold)];
    // ---- Fast travel (O265) ------------------------------------------------
    // The map IS the travel screen now. From any instance it offers exactly
    // one way out — home — and from the Anchor it offers the ordinary
    // registry. The rule itself is pure and lives in BreakerMapTravelRules.h;
    // everything here is presentation and the two live reads it needs.
    const bool bInHub = UBreakerGameInstance::IsAnchorMap(Player->GetWorld());
    const bool bInCombat = Player->IsInResourceCombat();
    TArray<FName> RegistryIds;
    for (const FBreakerTravelDestination& Destination : ABreakerTravelPoint::GetFallbackRegistry())
    {
        if (!Destination.bEnabled || Destination.bDoorOnly) continue;
        if (Destination.Id == ABreakerTravelPoint::ErasedEarthDestinationId
            && !ABreakerTravelPoint::CanEnterErasedEarth(Player)) continue;
        if ((Destination.Id == ABreakerTravelPoint::StrippedEarthDestinationId
            || Destination.Id == ABreakerTravelPoint::WinningEarthDestinationId)
            && !ABreakerTravelPoint::CanEnterFinaleEarth(Destination.Id, Player)) continue;
        RegistryIds.Add(Destination.Id);
    }
    const TArray<FName> Offered = BreakerMapTravel::OfferedDestinations(
        bInHub, RegistryIds, ABreakerTravelPoint::HubDestinationId);
    if (!Offered.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [SNew(STextBlock).Text(FText::FromString(bInHub ? TEXT("TRAVEL") : TEXT("TRAVEL  ·  ONE WAY OUT OF AN INSTANCE, AND IT LEADS HOME")))
            .Font(BreakerBodyFont(12, true)).ColorAndOpacity(BreakerUI::TextSecondary)];
        for (const FName DestinationId : Offered)
        {
            FBreakerTravelDestination Destination;
            if (!ABreakerTravelPoint::FindDestination(DestinationId, Destination)) continue;
            // ui.md: a disabled control is PAINTED, never faded. In combat the
            // row says what it is waiting for rather than going grey.
            const bool bRefused = BreakerMapTravel::TravelRefusedInCombat(bInCombat);
            const FString Label = bRefused
                ? Destination.DisplayName.ToString().ToUpper() + FString(TEXT("   —   NOT WHILE IN COMBAT"))
                : Destination.DisplayName.ToString().ToUpper();
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(bRefused ? BreakerUI::BgRaised : BreakerUI::Panel20)
                .ContentPadding(FMargin(16, 10))
                .HAlign(HAlign_Fill)
                .OnClicked(FOnClicked::CreateLambda([this, DestinationId, bRefused]()
                {
                    if (bRefused || !Character.IsValid()) return FReply::Handled();
                    UWorld* World = Character->GetWorld();
                    ABreakerGameMode* Mode = World ? World->GetAuthGameMode<ABreakerGameMode>() : nullptr;
                    if (!Mode) return FReply::Handled();
                    // The SAME verb the death screen already uses, and the
                    // same one a travel point calls: no second travel path.
                    // Travel FIRST, resume second — the travel screen's own
                    // order. Travel is legal while the menu holds the pause,
                    // and resuming first would unpause a world that is about
                    // to be torn down by the level change.
                    Mode->HandleHubTravelSelected(DestinationId, Character.Get());
                    if (Character.IsValid()) Character->ResumeFromMenu();
                    return FReply::Handled();
                }))
                [SNew(STextBlock).Text(FText::FromString(Label)).Font(BreakerBodyFont(14, true))
                    .ColorAndOpacity(bRefused ? BreakerUI::TextMuted : BreakerUI::TextPrimary)]
            ];
        }
    }
    Body->AddSlot().AutoHeight()
    // The canvas yields to the travel list. In an instance there is ONE row and
    // the map keeps its full height; in the Anchor there are seven, and a fixed
    // canvas pushed the map's own footer and BACK off the plate — which nobody
    // saw until the capture harness could finally photograph the hub. The floor
    // keeps the map legible rather than letting it collapse to a strip.
    [SNew(SBox).HeightOverride(FMath::Clamp(520.0f - Offered.Num() * 62.0f, 240.0f, 520.0f))
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,20,0)[SNew(SBreakerLocalMapCanvas).Player(Player)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(260)[SNew(SScrollBox) + SScrollBox::Slot()[List]]]]];
    Body->AddSlot().AutoHeight().Padding(0,12)
    [SNew(STextBlock).Text(FText::FromString(TEXT("NORTH ↑   ·   TRIANGLE: YOU   ·   GOLD SQUARE: CAMPAIGN OBJECTIVE\nSelect an objective or discovered site to track. Survey lines show floor footprints; the tracking line shows direction.")))
        .Font(BreakerBodyFont(12)).AutoWrapText(true).ColorAndOpacity(BreakerUI::TextSecondary)];
    Body->AddSlot().AutoHeight()
    [MakeButton(FText::FromString(TEXT("BACK")), FOnClicked::CreateLambda([this]()
        { Rebuild(EBreakerMenuScreen::Pause); return FReply::Handled(); }))];
    return BuildFrame(Map->GetRegionName(), FText::FromString(TEXT("LOCAL MAP")), Body, 1100);
}
