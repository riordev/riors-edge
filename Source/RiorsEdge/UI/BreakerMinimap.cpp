#include "UI/BreakerPlaytestHUD.h"
#include "Characters/BreakerCharacter.h"
#include "Game/BreakerLocalMapComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "UI/BreakerUIStyle.h"

void ABreakerPlaytestHUD::DrawMinimap(const ABreakerCharacter* Character)
{
    if (!Character || !Canvas || !Character->GetLocalMap()) return;
    const auto* Map = Character->GetLocalMap();
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now >= NextMinimapGroundRefresh)
    {
        MinimapGround = Map->GetGround();
        NextMinimapGroundRefresh = Now + 2.0; // O2 PLACEHOLDER; topology refresh, not a per-frame actor scan.
    }
    const float Size = S(200), X = Canvas->ClipX - S(240), Y = S(146); // O2 PLACEHOLDER
    const FVector2D Center(X + Size * .5, Y + Size * .5);
    const FVector2D Here(Character->GetActorLocation());
    const double Scale = Size / 6000.0; // O2 PLACEHOLDER: 60 m local survey, north stays up.
    auto Point = [&](FVector2D P) { P = (P - Here) * Scale; return Center + FVector2D(P.Y, -P.X); };
    DrawRect(FLinearColor(.018f,.024f,.025f,.9f), X, Y, Size, Size);
    for (const auto& Box : MinimapGround)
    {
        const FVector2D A = Point(Box.Min), B = Point(Box.Max);
        const float Left = FMath::Clamp(float(FMath::Min(A.X,B.X)), X, X+Size);
        const float Top = FMath::Clamp(float(FMath::Min(A.Y,B.Y)), Y, Y+Size);
        const float Right = FMath::Clamp(float(FMath::Max(A.X,B.X)), X, X+Size);
        const float Bottom = FMath::Clamp(float(FMath::Max(A.Y,B.Y)), Y, Y+Size);
        if (Right > Left && Bottom > Top) DrawRect(FLinearColor(.16f,.19f,.18f,.9f), Left,Top,Right-Left,Bottom-Top);
    }
    auto Line = [&](FVector2D A,FVector2D B,FLinearColor Color,float Width=1.f)
        { DrawLine(A.X,A.Y,B.X,B.Y,Color,S(Width)); };
    const auto& Markers = MarkersThisFrame(Character);
    const bool HasTrackedSite = Markers.ContainsByPredicate([&](const auto& Marker)
        { return Marker.Id == Map->GetTracked() && Map->IsVisible(Marker); });
    for (const auto& Marker : Markers)
    {
        if (!Map->IsVisible(Marker)) continue;
        const bool Tracked = HasTrackedSite ? Marker.Id == Map->GetTracked() : Marker.bObjective;
        FVector2D P = Point(FVector2D(Marker.Location));
        const FVector2D Delta = P-Center;
        const double Limit = Size*.5-S(10);
        const double Longest = FMath::Max(FMath::Abs(Delta.X),FMath::Abs(Delta.Y));
        if (Longest > Limit)
        {
            if (!Tracked) continue;
            P = Center + Delta * (Limit / Longest);
        }
        const FLinearColor Color = Tracked ? BreakerUI::Gold : Marker.bRift ? BreakerUI::TealName : BreakerUI::TextSecondary;
        const float R = S(Tracked ? 5.f : 3.f);
        Line(P+FVector2D(0,-R),P+FVector2D(R,0),Color,2);
        Line(P+FVector2D(R,0),P+FVector2D(0,R),Color,2);
        Line(P+FVector2D(0,R),P+FVector2D(-R,0),Color,2);
        Line(P+FVector2D(-R,0),P+FVector2D(0,-R),Color,2);
    }
    const FVector Forward = Character->GetActorForwardVector();
    const FVector2D D(Forward.Y,-Forward.X), Side(-D.Y,D.X);
    const FVector2D Tip=Center+D*S(8), L=Center-D*S(5)+Side*S(5), R=Center-D*S(5)-Side*S(5);
    Line(Tip,L,BreakerUI::TextPrimary,2); Line(L,R,BreakerUI::TextPrimary,2); Line(R,Tip,BreakerUI::TextPrimary,2);
    DrawSpecTextCentered(TEXT("N"),Center.X,Y+S(3),BreakerUI::TextPrimary,10);
    DrawSpecTextCentered(TEXT("LOCAL MAP"),Center.X,Y+Size+S(6),BreakerUI::TextMuted,10);
}
