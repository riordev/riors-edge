#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerMonsterChassis.h"

// ---------------------------------------------------------------------------
// THE NAMEPLATE'S ARITHMETIC, with no HUD under it.
//
// Every number the nameplate sheet (Assets/design/03-nameplates) states as a
// rule — how a plate shrinks with range, how wide a rank's bar is, where the
// elite's ellipse sits, which shape announces which modifier — is a pure
// function here so a test can walk it. Combat/BreakerEnemyHealthBars.cpp is
// the thin caller: it projects, it asks, it draws.
//
// A mark is DRAWN GEOMETRY, never a glyph (ui.md): ShapeFor hands back the
// rectangles, lines, diamonds and rings that make one up, in a 16-unit cell,
// and the caller scales the cell. Nothing here knows a pixel.
//
// Named BreakerEnemyBarMath, not BreakerEnemyBar: unity builds merge
// translation units and the bar TU already carries that namespace.
// ---------------------------------------------------------------------------
namespace BreakerEnemyBarMath
{
    // --- Range --------------------------------------------------------------
    // Full size to NearCm, shrinking linearly to the floor at FarCm, holding
    // the floor to DrawCm and gone past it. The chip and the shield line are
    // detail: they go before the plate does.
    inline constexpr float NearCm = 1200.0f;              // 03-nameplates
    inline constexpr float FarCm = 3500.0f;               // 03-nameplates
    inline constexpr float DrawCm = 6000.0f;              // 03-nameplates
    inline constexpr float ChipAndShieldHideCm = 2500.0f; // 03-nameplates
    inline constexpr float FloorScale = 0.5f;             // 03-nameplates

    inline float ScaleFor(float DistanceCm)
    {
        const float Span = FarCm - NearCm;
        const float T = Span > 0.0f ? (DistanceCm - NearCm) / Span : 1.0f;
        return FMath::Clamp(1.0f - (1.0f - FloorScale) * T, FloorScale, 1.0f);
    }

    // --- The bar --------------------------------------------------------------
    // Two widths: the standard body and the champion. Boss takes the champion's
    // until its own pass rules otherwise. The border never scales; the plate
    // never drops below MinPlateH so the fill inside it never drops below
    // MinFillH — a one-pixel fill is not a health read.
    inline constexpr float StandardBarW = 96.0f;  // 03-nameplates
    inline constexpr float StandardBarH = 6.0f;   // 03-nameplates
    inline constexpr float ChampionBarW = 160.0f; // 03-nameplates
    inline constexpr float ChampionBarH = 8.0f;   // 03-nameplates
    inline constexpr float BorderPx = 1.0f;       // 03-nameplates
    inline constexpr float MinPlateH = 5.0f;      // 03-nameplates
    inline constexpr float MinFillH = 3.0f;       // 03-nameplates
    inline constexpr float ShieldLinePx = 2.0f;   // 03-nameplates

    struct FBarSize
    {
        float W = 0.0f;
        float H = 0.0f;
        // The fill's height inside the border, already floored.
        float FillH = 0.0f;
    };

    inline FBarSize BarSizeFor(EBreakerMonsterRank Rank, float Scale)
    {
        const bool bWide = Rank == EBreakerMonsterRank::ModifierBearing || Rank == EBreakerMonsterRank::Boss;
        FBarSize Size;
        Size.W = (bWide ? ChampionBarW : StandardBarW) * Scale;
        Size.H = FMath::Max((bWide ? ChampionBarH : StandardBarH) * Scale, MinPlateH);
        Size.FillH = FMath::Max(Size.H - 2.0f * BorderPx, MinFillH);
        return Size;
    }

    // --- The elite's ellipse ---------------------------------------------------
    // At the feet, wider than the body by the ratio, HaloHeightPx tall at the
    // near end and half that at the floor. Stroke never scales.
    inline constexpr float HaloWidthRatio = 1.6f; // 03-nameplates
    inline constexpr float HaloHeightPx = 28.0f;  // 03-nameplates
    inline constexpr float HaloStrokePx = 2.0f;   // 03-nameplates

    inline float HaloHeightFor(float Scale) { return HaloHeightPx * Scale; }

    // --- The champion's diamonds ----------------------------------------------
    // One filled diamond off each end of the bar. The gap OPENS as the plate
    // shrinks so the two never fuse with the bar at range.
    inline constexpr float ChampionDiamondPx = 10.0f;    // 03-nameplates
    inline constexpr float ChampionDiamondMinPx = 5.0f;  // 03-nameplates
    inline constexpr float ChampionDiamondGapPx = 4.0f;  // 03-nameplates

    inline float ChampionDiamondFor(float Scale) { return FMath::Max(ChampionDiamondPx * Scale, ChampionDiamondMinPx); }
    inline float ChampionDiamondGapFor(float Scale) { return ChampionDiamondGapPx + (1.0f - Scale) * 4.0f; }  // 03-nameplates: 4 px at s=1, 6 px at the floor

    // --- The column -------------------------------------------------------------
    // Marks row, name, bar, bottom-up from PlateAboveHeadPx above the head.
    inline constexpr float PlateAboveHeadPx = 12.0f; // 03-nameplates
    inline constexpr float ColumnGapPx = 6.0f;       // 03-nameplates
    inline constexpr float MarkCellPx = 16.0f;       // 03-nameplates
    inline constexpr float MarkGapPx = 8.0f;         // 03-nameplates
    inline constexpr float MarkGapMinPx = 4.0f;      // 03-nameplates
    // Encounter-Design caps a body at three modifiers; this is that cap, not a
    // truncation.
    inline constexpr int32 MaximumMarks = 3;         // 03-nameplates

    inline float MarkGapFor(float Scale) { return FMath::Max(MarkGapPx * Scale, MarkGapMinPx); }

    // --- The marks ---------------------------------------------------------------
    // Plain and unserialized: a mark is a screen shape resolved every frame from
    // the modifier, never stored. Reordering is safe.
    enum class EBreakerEnemyMark : uint8
    {
        None,
        HollowSquare,
        FilledDiamond,
        RightTriangle,
        InvertedT,
        BarsHorizontal,
        Ring,
        Chevron,
        BarsVertical,
        Steps,
        SquareDot,
        Count
    };

    inline EBreakerEnemyMark MarkFor(EBreakerEnemyModifier Modifier)
    {
        switch (Modifier)
        {
        case EBreakerEnemyModifier::Warded:      return EBreakerEnemyMark::HollowSquare;
        case EBreakerEnemyModifier::Volatile:    return EBreakerEnemyMark::FilledDiamond;
        case EBreakerEnemyModifier::Fleetfoot:   return EBreakerEnemyMark::RightTriangle;
        case EBreakerEnemyModifier::Anchored:    return EBreakerEnemyMark::InvertedT;
        case EBreakerEnemyModifier::Splitting:   return EBreakerEnemyMark::BarsHorizontal;
        case EBreakerEnemyModifier::WardingAura: return EBreakerEnemyMark::Ring;
        case EBreakerEnemyModifier::Reflective:  return EBreakerEnemyMark::Chevron;
        case EBreakerEnemyModifier::Phasing:     return EBreakerEnemyMark::BarsVertical;
        case EBreakerEnemyModifier::Cascading:   return EBreakerEnemyMark::Steps;
        case EBreakerEnemyModifier::Wakeful:     return EBreakerEnemyMark::SquareDot;
        default:                                 return EBreakerEnemyMark::None;
        }
    }

    // The primitives a mark is built from, in cell units (0..MarkCellPx).
    //  Rect                : A = top-left, B = size.
    //  Line                : A, B = ends; Thickness.
    //  FilledDiamond       : A = centre, B.X = half-extent.
    //  FilledTriangleRight : A, B, C = vertices, B the apex.
    //  Ring                : A = centre, B.X = radius; Thickness.
    enum class EBreakerMarkPrimitive : uint8
    {
        Rect,
        Line,
        FilledDiamond,
        FilledTriangleRight,
        Ring
    };

    struct FBreakerMarkPrimitive
    {
        EBreakerMarkPrimitive Kind = EBreakerMarkPrimitive::Rect;
        FVector2D A = FVector2D::ZeroVector;
        FVector2D B = FVector2D::ZeroVector;
        FVector2D C = FVector2D::ZeroVector;
        float Thickness = 0.0f;

        static FBreakerMarkPrimitive Rect(float X, float Y, float W, float H)
        {
            FBreakerMarkPrimitive P;
            P.Kind = EBreakerMarkPrimitive::Rect;
            P.A = FVector2D(X, Y);
            P.B = FVector2D(W, H);
            return P;
        }
        static FBreakerMarkPrimitive Line(float X0, float Y0, float X1, float Y1, float T)
        {
            FBreakerMarkPrimitive P;
            P.Kind = EBreakerMarkPrimitive::Line;
            P.A = FVector2D(X0, Y0);
            P.B = FVector2D(X1, Y1);
            P.Thickness = T;
            return P;
        }
        static FBreakerMarkPrimitive Diamond(float CX, float CY, float Half)
        {
            FBreakerMarkPrimitive P;
            P.Kind = EBreakerMarkPrimitive::FilledDiamond;
            P.A = FVector2D(CX, CY);
            P.B = FVector2D(Half, Half);
            return P;
        }
        static FBreakerMarkPrimitive TriangleRight(float X0, float Y0, float ApexX, float ApexY, float X2, float Y2)
        {
            FBreakerMarkPrimitive P;
            P.Kind = EBreakerMarkPrimitive::FilledTriangleRight;
            P.A = FVector2D(X0, Y0);
            P.B = FVector2D(ApexX, ApexY);
            P.C = FVector2D(X2, Y2);
            return P;
        }
        static FBreakerMarkPrimitive Ring(float CX, float CY, float R, float T)
        {
            FBreakerMarkPrimitive P;
            P.Kind = EBreakerMarkPrimitive::Ring;
            P.A = FVector2D(CX, CY);
            P.B = FVector2D(R, R);
            P.Thickness = T;
            return P;
        }
    };

    // The extent a primitive paints, including its stroke, so a test can prove
    // every mark stays inside its cell.
    inline FBox2D MarkPrimitiveBounds(const FBreakerMarkPrimitive& P)
    {
        switch (P.Kind)
        {
        case EBreakerMarkPrimitive::Rect:
            return FBox2D(P.A, P.A + P.B);
        case EBreakerMarkPrimitive::Line:
        {
            const FVector2D Pad(P.Thickness * 0.5f, P.Thickness * 0.5f);
            FBox2D Box(P.A - Pad, P.A + Pad);
            Box += FBox2D(P.B - Pad, P.B + Pad);
            return Box;
        }
        case EBreakerMarkPrimitive::FilledDiamond:
            return FBox2D(P.A - FVector2D(P.B.X, P.B.X), P.A + FVector2D(P.B.X, P.B.X));
        case EBreakerMarkPrimitive::FilledTriangleRight:
        {
            FBox2D Box(P.A, P.A);
            Box += P.B;
            Box += P.C;
            return Box;
        }
        case EBreakerMarkPrimitive::Ring:
        {
            const float Reach = P.B.X + P.Thickness * 0.5f;
            return FBox2D(P.A - FVector2D(Reach, Reach), P.A + FVector2D(Reach, Reach));
        }
        }
        return FBox2D(P.A, P.A);
    }

    // The shapes, in a MarkCellPx cell. Every coordinate here is a design
    // number from the nameplate sheet. The hollow square is four rects because
    // the canvas has no stroked rectangle; the dot variant is the same four
    // plus a centre.
    inline TArray<FBreakerMarkPrimitive> ShapeFor(EBreakerEnemyMark Mark)
    {
        using P = FBreakerMarkPrimitive;
        TArray<P> Shape;
        auto HollowSquare = [&Shape]()
        {
            Shape.Add(P::Rect(2.0f, 2.0f, 12.0f, 2.0f));   // 03-nameplates
            Shape.Add(P::Rect(2.0f, 12.0f, 12.0f, 2.0f));  // 03-nameplates
            Shape.Add(P::Rect(2.0f, 4.0f, 2.0f, 8.0f));    // 03-nameplates
            Shape.Add(P::Rect(12.0f, 4.0f, 2.0f, 8.0f));   // 03-nameplates
        };
        switch (Mark)
        {
        case EBreakerEnemyMark::HollowSquare:
            HollowSquare();
            break;
        case EBreakerEnemyMark::FilledDiamond:
            Shape.Add(P::Diamond(8.0f, 8.0f, 5.0f));                            // 03-nameplates
            break;
        case EBreakerEnemyMark::RightTriangle:
            Shape.Add(P::TriangleRight(2.0f, 1.0f, 14.0f, 8.0f, 2.0f, 15.0f));  // 03-nameplates
            break;
        case EBreakerEnemyMark::InvertedT:
            Shape.Add(P::Rect(2.0f, 12.0f, 12.0f, 3.0f));                       // 03-nameplates
            Shape.Add(P::Rect(7.0f, 2.0f, 2.0f, 12.0f));                        // 03-nameplates
            break;
        case EBreakerEnemyMark::BarsHorizontal:
            Shape.Add(P::Rect(2.0f, 3.0f, 12.0f, 3.0f));                        // 03-nameplates
            Shape.Add(P::Rect(2.0f, 12.0f, 12.0f, 3.0f));                       // 03-nameplates
            break;
        case EBreakerEnemyMark::Ring:
            Shape.Add(P::Ring(8.0f, 8.0f, 7.0f, 2.0f));                         // 03-nameplates
            break;
        case EBreakerEnemyMark::Chevron:
            Shape.Add(P::Line(3.0f, 3.0f, 13.0f, 3.0f, 3.0f));                  // 03-nameplates
            Shape.Add(P::Line(3.0f, 3.0f, 3.0f, 13.0f, 3.0f));                  // 03-nameplates
            break;
        case EBreakerEnemyMark::BarsVertical:
            Shape.Add(P::Rect(3.0f, 2.0f, 3.0f, 12.0f));                        // 03-nameplates
            Shape.Add(P::Rect(12.0f, 2.0f, 3.0f, 12.0f));                       // 03-nameplates
            break;
        case EBreakerEnemyMark::Steps:
            Shape.Add(P::Rect(2.0f, 10.0f, 4.0f, 4.0f));                        // 03-nameplates
            Shape.Add(P::Rect(6.0f, 6.0f, 4.0f, 8.0f));                         // 03-nameplates
            Shape.Add(P::Rect(10.0f, 2.0f, 4.0f, 12.0f));                       // 03-nameplates
            break;
        case EBreakerEnemyMark::SquareDot:
            HollowSquare();
            Shape.Add(P::Rect(6.0f, 6.0f, 4.0f, 4.0f));                         // 03-nameplates
            break;
        default:
            break;
        }
        return Shape;
    }
}
