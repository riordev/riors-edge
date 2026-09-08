#include "UI/BreakerMenu.h"
#include "Characters/BreakerCharacter.h"
#include "Game/BreakerLocalMapComponent.h"
#include "UI/BreakerTypeRoles.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Input/SButton.h"
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
    int32 Index = 0;
    for (const auto& Marker : Map->GetMarkers())
    {
        if (!Map->IsVisible(Marker)) continue;
        const FString Detail = Marker.Detail.IsEmpty() ? TEXT("") : Marker.Detail.ToString() + TEXT("   ·   ");
        const FString Caption = FString::Printf(TEXT("%d   %s%s\n%s%dm"), ++Index, Marker.bObjective ? TEXT("OBJECTIVE · ") : TEXT(""), *Marker.Label.ToString(), *Detail,
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
    Body->AddSlot().AutoHeight()
    [SNew(SBox).HeightOverride(620)
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
