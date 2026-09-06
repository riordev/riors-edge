// ---------------------------------------------------------------------------
// The enemy nameplate, and nothing else.
//
// WHY THIS FILE EXISTS, AND WHY IT IS IN Combat/ RATHER THAN UI/. Two lanes
// shipped a trash-bar rule on the same day — b44c0fc and d967342 — and both
// merged clean, because they touched different lines of one 3,596-line HUD
// file. That is not a merge accident to be handled better next time; it is two
// owners on one question, and it recurs until the question has one owner. The
// bar answers "which enemy am I fighting, and how close is it to dead", which
// is a combat read, so the directory that names its owner is this one.
//
// The function below is still a member of ABreakerPlaytestHUD and still draws
// through the HUD's canvas. It has to be: a bar is projected world-space
// geometry over a Canvas the HUD owns. A member function's DEFINITION may live
// in any translation unit and keeps full private access, so this split cost
// the HUD class exactly nothing — no widened access, no exported helpers, no
// friend declaration.
//
// The arithmetic is Combat/BreakerEnemyBarMath.h's. This file projects, asks
// and draws.
//
// What did NOT move: EnemyBlips, which the enemy loop below fills. Its reader
// is gone; the fill continues until a consumer returns, because stopping it is
// this lane's edit and that consumer's cue. That contract is restated at the
// loop.
// ---------------------------------------------------------------------------

#include "UI/BreakerPlaytestHUD.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemyBarMath.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerTargetDummy.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UI/BreakerHUDMath.h"
#include "UI/BreakerUIStyle.h"

// Named for this pass, not BreakerHUD: unity builds merge translation units,
// and a second `namespace BreakerHUD` carrying its own constants under the old
// names is a redefinition the moment the two files land in one blob. The
// project has shipped that collision twice under other names.
namespace BreakerEnemyBar
{
    // Aim cone for "the enemy I am asking about". Presentation, not balance:
    // it decides which enemy gets a bar, never anything about damage or aim.
    // Roughly matches the loot focus cone so the two agree about what the
    // player is pointing at.
    static constexpr float FocusMinimumDot = 0.985f;
    // How long a trash bar lingers after the aim leaves it (selective bars;
    // above-Trash ranks are always barred). O2 PLACEHOLDER.
    static constexpr float FocusFadeSeconds = 0.6f;
    // Half the humanoid silhouette, matching BodyCollision's 45 cm capsule
    // radius. Not read off the component: the probe's frozen bodies and the
    // dummy share this path, and a per-actor query here would cost a component
    // fetch per enemy per frame to recover a number that is the same for every
    // body in the game.
    static constexpr float BodyHalfWidthCm = 45.0f;
    // The head anchor when a body has no capsule to read (the dummy is a bare
    // actor). O2 PLACEHOLDER.
    static constexpr float FallbackHeadCm = 120.0f;
    // The name line's box, in spec pixels, before the range scale. O2
    // PLACEHOLDER.
    static constexpr float NamePixels = 11.0f;
    static constexpr float NameLinePixels = 13.0f;

    // --- The dummy block's own two constants -------------------------------
    // These feed the TARGET DUMMY loop and nothing else, and that is a ruling
    // rather than an accident of where the code sits. A dummy is a gym
    // INSTRUMENT, not a crowd member: there are four of them, they never move,
    // and the single question they exist to answer is "did that hurt". A
    // recency window answers exactly that question, which is why the enemy
    // path dropping it does not take the dummy path with it. The enemies went
    // aimed-at-only because at fifty to a hundred concurrent with cleave in
    // the kit, recency is not a filter — one AoE lights the whole pack. Four
    // stationary targets are not a pack.
    //
    // So this is not a rebase artifact that happens to still work. Do not
    // "finish" the selective-bar change by deleting it.
    //
    // The 6.0s -> 1.5s retune stands on its own: at six seconds a bar outlives
    // the shot that earned it. O2 PLACEHOLDER — the owner tunes this in hand.
    static constexpr float RecentDamageSeconds = 1.5f;
    static constexpr float AlwaysDistance = 1500.0f;
}

namespace
{
    // The bar's rectangle on screen. Both loops below place it identically —
    // the bottom of a column that hangs above the head — and both then draw
    // the same body into it, so this returns the rectangle rather than drawing
    // it.
    //
    // ScaleUnit is the HUD's S(1.0f) passed in, because S() is private and
    // these are free functions. S(x) is x * UIScale and ScaleUnit IS UIScale,
    // so x * ScaleUnit is the same product of the same two floats. Scale is
    // the RANGE scale from BreakerEnemyBarMath; the two multiply for anything
    // that shrinks with distance and only ScaleUnit applies to anything that
    // does not (the border, the strokes).
    struct FBreakerEnemyBarRect
    {
        float X = 0.0f;
        float Y = 0.0f;
        float W = 0.0f;
        float H = 0.0f;
        float FillH = 0.0f;
        float Scale = 1.0f;
    };

    FBreakerEnemyBarRect BreakerEnemyBarPlace(const FVector& HeadProjected, EBreakerMonsterRank Rank,
        float Scale, float ScaleUnit)
    {
        const BreakerEnemyBarMath::FBarSize Size = BreakerEnemyBarMath::BarSizeFor(Rank, Scale);
        FBreakerEnemyBarRect Rect;
        Rect.Scale = Scale;
        Rect.W = Size.W * ScaleUnit;
        Rect.H = Size.H * ScaleUnit;
        Rect.FillH = Size.FillH * ScaleUnit;
        Rect.X = HeadProjected.X - Rect.W * 0.5f;
        Rect.Y = HeadProjected.Y - BreakerEnemyBarMath::PlateAboveHeadPx * Scale * ScaleUnit - Rect.H;
        return Rect;
    }

    // The silhouette's on-screen width, by projecting its edge rather than by
    // trigonometry. Projection is already correct under any FOV and aspect —
    // including the aim-down-sights FOV change, which a hand-rolled tangent
    // would have to be told about and would silently miss.
    float BreakerEnemyBarBodyWidthPixels(const FVector& Projected, const FVector& CameraRight,
        const FVector& WorldAnchor, TFunctionRef<FVector(const FVector&)> ProjectFn)
    {
        const FVector Edge = ProjectFn(WorldAnchor + CameraRight * BreakerEnemyBar::BodyHalfWidthCm);
        if (Edge.Z <= 0.0f) return 0.0f;
        return FMath::Abs(Edge.X - Projected.X) * 2.0f;
    }

    // Scanline fill: the canvas cannot rotate a rect, and a diamond of a few
    // pixels is a handful of rows rather than a texture.
    void BreakerEnemyBarDrawFilledDiamond(AHUD& HUD, float CentreX, float CentreY, float HalfExtent,
        const FLinearColor& Colour)
    {
        const int32 Rows = FMath::Max(1, FMath::RoundToInt(HalfExtent));
        for (int32 Row = -Rows; Row <= Rows; ++Row)
        {
            const float RowHalf = HalfExtent * (1.0f - FMath::Abs(Row) / static_cast<float>(Rows));
            if (RowHalf <= 0.0f) continue;
            HUD.DrawRect(Colour, CentreX - RowHalf, CentreY + Row, RowHalf * 2.0f, 1.0f);
        }
    }

    // A triangle with a vertical left edge and its apex to the right, as rows.
    void BreakerEnemyBarDrawFilledTriangleRight(AHUD& HUD, float LeftX, float TopY, float BottomY,
        float ApexX, float ApexY, const FLinearColor& Colour)
    {
        const int32 Rows = FMath::Max(1, FMath::RoundToInt(BottomY - TopY));
        const float Reach = ApexX - LeftX;
        for (int32 Row = 0; Row <= Rows; ++Row)
        {
            const float Y = TopY + (BottomY - TopY) * Row / static_cast<float>(Rows);
            const float Half = FMath::Max(ApexY - TopY, BottomY - ApexY);
            const float T = Half > 0.0f ? 1.0f - FMath::Abs(Y - ApexY) / Half : 1.0f;
            const float W = Reach * FMath::Clamp(T, 0.0f, 1.0f);
            if (W <= 0.0f) continue;
            HUD.DrawRect(Colour, LeftX, Y, W, 1.0f);
        }
    }

    // An ellipse as a closed line-segment polygon. Enough sides that it reads
    // as round at the near end; few enough that at the floor the segments are
    // still longer than the stroke is thick. O2 PLACEHOLDER.
    constexpr int32 BreakerEnemyBarEllipseSegments = 24;

    void BreakerEnemyBarDrawEllipse(AHUD& HUD, float CentreX, float CentreY, float RadiusX, float RadiusY,
        const FLinearColor& Colour, float Thickness)
    {
        const float Step = 2.0f * PI / static_cast<float>(BreakerEnemyBarEllipseSegments);
        float PreviousX = CentreX + RadiusX;
        float PreviousY = CentreY;
        for (int32 Segment = 1; Segment <= BreakerEnemyBarEllipseSegments; ++Segment)
        {
            const float Angle = Step * static_cast<float>(Segment);
            const float X = CentreX + RadiusX * FMath::Cos(Angle);
            const float Y = CentreY + RadiusY * FMath::Sin(Angle);
            HUD.DrawLine(PreviousX, PreviousY, X, Y, Colour, Thickness);
            PreviousX = X;
            PreviousY = Y;
        }
    }

    // The chip's hatch: 135° stripes, harm over harm-dim, HudHealthChipHatch
    // period and stripe, anchored to the screen by BreakerHUDMath so two
    // adjacent chips line up. Drawn as rows because the canvas cannot clip a
    // diagonal line to a rectangle; a bar is at most eight rows tall.
    void BreakerEnemyBarDrawHatch(AHUD& HUD, float X, float Y, float W, float H, float ScaleUnit, float Alpha)
    {
        if (W <= 0.0f || H <= 0.0f) return;
        HUD.DrawRect(BreakerUI::Alpha(BreakerUI::HarmDeep, Alpha), X, Y, W, H);
        const float Period = BreakerUI::HudHealthChipHatchPeriod * ScaleUnit;
        const float Step = BreakerHUDMath::HatchDiagonalStep(Period);
        const float Band = BreakerHUDMath::HatchDiagonalStep(BreakerUI::HudHealthChipHatchStripe * ScaleUnit);
        if (Step <= 0.0f) return;
        const FLinearColor Stripe = BreakerUI::Alpha(BreakerUI::Harm, Alpha);
        for (float Row = Y; Row < Y + H; Row += 1.0f)
        {
            const float RowH = FMath::Min(1.0f, Y + H - Row);
            for (float Sum = BreakerHUDMath::HatchStripeStart(X, Row, Period); Sum < X + W + Row; Sum += Step)
            {
                const float X0 = FMath::Max(Sum - Row, X);
                const float X1 = FMath::Min(Sum - Row + Band, X + W);
                if (X1 > X0) HUD.DrawRect(Stripe, X0, Row, X1 - X0, RowH);
            }
        }
    }

    // One modifier mark: the shape's primitives from the maths, scaled from the
    // 16-unit cell into CellPx at (CellX, CellY).
    void BreakerEnemyBarDrawMark(AHUD& HUD, float CellX, float CellY, float CellPx,
        BreakerEnemyBarMath::EBreakerEnemyMark Mark, const FLinearColor& Colour)
    {
        using namespace BreakerEnemyBarMath;
        const float Unit = CellPx / MarkCellPx;
        auto At = [&](const FVector2D& P) { return FVector2D(CellX + P.X * Unit, CellY + P.Y * Unit); };
        for (const FBreakerMarkPrimitive& P : ShapeFor(Mark))
        {
            switch (P.Kind)
            {
            case EBreakerMarkPrimitive::Rect:
            {
                const FVector2D Origin = At(P.A);
                HUD.DrawRect(Colour, Origin.X, Origin.Y, P.B.X * Unit, P.B.Y * Unit);
                break;
            }
            case EBreakerMarkPrimitive::Line:
            {
                const FVector2D A = At(P.A);
                const FVector2D B = At(P.B);
                HUD.DrawLine(A.X, A.Y, B.X, B.Y, Colour, FMath::Max(1.0f, P.Thickness * Unit));
                break;
            }
            case EBreakerMarkPrimitive::FilledDiamond:
            {
                const FVector2D Centre = At(P.A);
                BreakerEnemyBarDrawFilledDiamond(HUD, Centre.X, Centre.Y, P.B.X * Unit, Colour);
                break;
            }
            case EBreakerMarkPrimitive::FilledTriangleRight:
            {
                const FVector2D A = At(P.A);
                const FVector2D Apex = At(P.B);
                const FVector2D C = At(P.C);
                BreakerEnemyBarDrawFilledTriangleRight(HUD, A.X, A.Y, C.Y, Apex.X, Apex.Y, Colour);
                break;
            }
            case EBreakerMarkPrimitive::Ring:
            {
                const FVector2D Centre = At(P.A);
                BreakerEnemyBarDrawEllipse(HUD, Centre.X, Centre.Y, P.B.X * Unit, P.B.X * Unit,
                    Colour, FMath::Max(1.0f, P.Thickness * Unit));
                break;
            }
            }
        }
    }

    // ONE bar body, drawn by both loops below. It was two: the enemy loop and
    // the dummy loop each carried the fill and the shield block near-verbatim,
    // and a change authored into only one of them would have been invisible
    // on the surface the owner actually plays.
    //
    // Plate, one-pixel border, system fill at the live fraction; the chip's
    // hatch from the live fraction to the fraction the bar was showing; the
    // shield as a line along the top of the fill at its own fraction. The
    // chip and the shield are detail and the caller hides them past
    // ChipAndShieldHideCm. Bands are STATE (O135), not ticks: the bar draws no
    // dividers, and BreakerHealthBands is untouched by this.
    //
    // Alpha is the only axis the two loops differ on: the enemy loop fades a
    // trash bar out, the dummy loop never fades. BreakerUI::Alpha SETS the
    // alpha channel rather than scaling it, and every colour here arrives from
    // Hex() at A=1, so Alpha(C, 1.0f) is exactly C.
    void BreakerEnemyBarDrawBody(AHUD& HUD, const FBreakerEnemyBarRect& Bar,
        float HealthFraction, float ChipFraction, float ShieldFraction, bool bShowChipAndShield,
        float ScaleUnit, float BarAlpha)
    {
        const float Border = BreakerEnemyBarMath::BorderPx * ScaleUnit;
        HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Panel10, BarAlpha), Bar.X, Bar.Y, Bar.W, Bar.H);
        const FLinearColor Edge = BreakerUI::Alpha(BreakerUI::BorderEmphasis, BarAlpha);
        HUD.DrawRect(Edge, Bar.X, Bar.Y, Bar.W, Border);
        HUD.DrawRect(Edge, Bar.X, Bar.Y + Bar.H - Border, Bar.W, Border);
        HUD.DrawRect(Edge, Bar.X, Bar.Y, Border, Bar.H);
        HUD.DrawRect(Edge, Bar.X + Bar.W - Border, Bar.Y, Border, Bar.H);

        const float InnerX = Bar.X + Border;
        const float InnerW = FMath::Max(0.0f, Bar.W - 2.0f * Border);
        const float InnerH = Bar.FillH;
        const float InnerY = Bar.Y + (Bar.H - InnerH) * 0.5f;

        const float Health = FMath::Clamp(HealthFraction, 0.0f, 1.0f);
        HUD.DrawRect(BreakerUI::Alpha(BreakerUI::System, BarAlpha), InnerX, InnerY, InnerW * Health, InnerH);

        if (!bShowChipAndShield) return;

        const float Chip = FMath::Clamp(ChipFraction, 0.0f, 1.0f);
        if (Chip > Health)
        {
            BreakerEnemyBarDrawHatch(HUD, InnerX + InnerW * Health, InnerY, InnerW * (Chip - Health), InnerH,
                ScaleUnit, BarAlpha);
        }

        const float Shield = FMath::Clamp(ShieldFraction, 0.0f, 1.0f);
        if (Shield > 0.0f)
        {
            const float LineH = FMath::Min(BreakerEnemyBarMath::ShieldLinePx * ScaleUnit, InnerH);
            HUD.DrawRect(BreakerUI::Alpha(BreakerUI::TextSecondary, BarAlpha), InnerX, InnerY, InnerW * Shield, LineH);
        }
    }
}

// --------------------------------------------------------------------------
// §Anchors — the nameplate column above the head: marks, name, bar. Above-Trash
// ranks are barred whenever they are inside DrawCm; trash is barred only while
// it is under the crosshair, and fades for FocusFadeSeconds after the aim
// leaves. See the selective-bar block below for why that is the rule and not
// a recency window.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawEnemyHealthBars(const ABreakerCharacter* Character)
{
    // This is the ONE enemy iteration the HUD makes, and EnemyBlips is what it
    // leaves behind for a consumer that is currently absent. Reset (not Empty)
    // keeps the capacity, so the array stops allocating after the first busy
    // frame — DrawHUD runs every frame and a container built inside it is a
    // per-frame allocation.
    EnemyBlips.Reset();

    UWorld* World = GetWorld();
    if (!World || !Character) return;
    const FVector ViewerLocation = Character->GetActorLocation();
    // S() is private and the geometry helpers above are free functions, so the
    // scale crosses that boundary as a value. Resolved once per frame.
    const float ScaleUnit = S(1.0f);
    const double Now = World->GetTimeSeconds();
    const float FrameSeconds = World->GetDeltaSeconds();

    // Which enemy the player is actually asking about. This is what decides
    // whether a trash mob is barred at all. Same aim-cone shape
    // DrawLootPickups already uses to pick its focused pickup, so "what am I
    // pointing at" means one thing across the whole HUD.
    // The camera's right, resolved once per frame: the elite's ellipse is as
    // wide as the silhouette by a ratio, and that needs a screen-space
    // direction to measure across. Defaults to world Y so a missing camera
    // manager degrades to a narrow ellipse rather than to none.
    FVector CameraRight = FVector::RightVector;
    if (PlayerOwner && PlayerOwner->PlayerCameraManager)
    {
        CameraRight = FRotationMatrix(PlayerOwner->PlayerCameraManager->GetCameraRotation())
            .GetScaledAxis(EAxis::Y);
    }

    const ABreakerEnemy* FocusedEnemy = nullptr;
    if (PlayerOwner && PlayerOwner->PlayerCameraManager)
    {
        const FVector CameraLocation = PlayerOwner->PlayerCameraManager->GetCameraLocation();
        const FVector CameraForward = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
        float BestDot = BreakerEnemyBar::FocusMinimumDot;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            const ABreakerEnemy* Candidate = *It;
            if (!Candidate || Candidate->IsDeadEnemy()) continue;
            const FVector ToEnemy = (Candidate->GetActorLocation() - CameraLocation);
            if (ToEnemy.IsNearlyZero()) continue;
            const float Dot = FVector::DotProduct(CameraForward, ToEnemy.GetSafeNormal());
            if (Dot > BestDot)
            {
                BestDot = Dot;
                FocusedEnemy = Candidate;
            }
        }
    }

    // Reset per frame: these are screen-space rectangles, and last frame's are
    // meaningless the moment the camera moves.
    DrawnLabelBounds.Reset();

    // The two maps are the state here that outlives a frame, so they are the
    // state that can grow without a bound. Pruned by the same clock that
    // reads them.
    //  * A fade entry older than the fade is finished, and a body that died
    //    or was parked for the pool goes stale and drops on the same pass.
    //  * A chip entry drops when its body is invalid, dead or past DrawCm. NOT
    //    when the chip has settled: a settled chip carries the fraction the
    //    bar last showed (see the chip block in the loop), and dropping it
    //    would make the next hit's origin unknowable. Bounded by the live
    //    enemies inside DrawCm, which is the pool.
    for (auto It = FocusBarReleaseTimes.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || Now - It.Value() >= BreakerEnemyBar::FocusFadeSeconds)
        {
            It.RemoveCurrent();
        }
    }
    for (auto It = ShownEnemyHealth.CreateIterator(); It; ++It)
    {
        const ABreakerEnemy* Keyed = It.Key().Get();
        if (!Keyed || Keyed->IsDeadEnemy()
            || FVector::Distance(ViewerLocation, Keyed->GetActorLocation()) > BreakerEnemyBarMath::DrawCm)
        {
            It.RemoveCurrent();
        }
    }

    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        const ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;

        // PRODUCER END OF A CROSS-LANE CONTRACT (O155). This fill belongs to
        // the COMBAT lane; the consumer end is the UI lane's, and is currently
        // empty — the minimap that read it is deleted. The fill continues
        // until a consumer returns: stopping it is this lane's edit and that
        // consumer's cue. A change to the shape, the meaning or the fill order
        // of EnemyBlips is a declared crossing: tell the UI lane before it
        // lands.
        //
        // Collected BEFORE the nameplate culls, because the two readouts want
        // different ranges: a plate is pointless past DrawCm, and a map read
        // is mostly useful for the hostiles that are further away than that.
        if (!Enemy->IsDeadEnemy())
        {
            FBreakerHUDMapBlip& Blip = EnemyBlips.AddDefaulted_GetRef();
            Blip.World = Enemy->GetActorLocation();
            // The same rank-predicate shape as the bar fix beside it: == Elite
            // exactly blipped a ModifierBearing champion as TRASH. It blips as
            // an elite; Boss keeps its own mark.
            Blip.bElite = Enemy->GetMonsterRank() == EBreakerMonsterRank::Elite
                || Enemy->GetMonsterRank() == EBreakerMonsterRank::ModifierBearing;
            Blip.bBoss = Enemy->GetMonsterRank() == EBreakerMonsterRank::Boss;
        }

        const float Distance = FVector::Distance(ViewerLocation, Enemy->GetActorLocation());
        if (Distance > BreakerEnemyBarMath::DrawCm) continue;

        const UAbilitySystemComponent* EnemyAbilitySystem = Enemy->GetAbilitySystemComponent();
        const UBreakerAttributeSet* EnemyAttributes = EnemyAbilitySystem ? EnemyAbilitySystem->GetSet<UBreakerAttributeSet>() : nullptr;
        if (!EnemyAttributes) continue;

        const float Health = EnemyAttributes->GetHealth();
        const float MaxHealth = EnemyAttributes->GetMaxHealth();
        if (Health <= 0.0f || MaxHealth <= UE_SMALL_NUMBER) continue;
        const float Fraction = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);

        // SELECTIVE BARS (ruled for the crowd): in a fight of eighty, eighty
        // bars is a rendering cost AND the noise that hides the read the bars
        // exist for. The rules:
        //  * ABOVE TRASH — always barred inside DrawCm. Elites and champions
        //    are the fight's anchors; their health is standing information.
        //    IsEliteOrBetter, NEVER IsElite: rank == Elite exactly would
        //    exclude ModifierBearing and Boss — the two ranks ABOVE the one
        //    meant — and the project has shipped that predicate bug twice (the
        //    enemy header records both).
        //  * TRASH — barred only while AIMED AT, fading for FocusFadeSeconds
        //    after the aim leaves so glancing across a pack reads as a sweep,
        //    not a strobe. An unfocused trash hit shows no bar: the damage
        //    numbers already carry "it hurt", and a recency rule at ANY window
        //    would light the whole pack the moment a cleave landed.
        //
        // What this rule deliberately does not carry: the trash mob at 8%
        // health in a pack of eighty, which is the highest-value information
        // on screen and is invisible here unless the player happens to be
        // crosshaired on it. That read lives on the BODY — the tint ramp and
        // the fracture mask — not on the bar. Focus-only is correct here
        // BECAUSE the body carries near-death; the two are halves of one rule.
        // THE FADE IS PER ENEMY: a map gives every body its own clock, so a
        // sweep across six trash mobs leaves six trails rather than one hard
        // cut per body overtaken.
        const bool bAboveTrash = Enemy->IsEliteOrBetter();
        float BarAlpha = 1.0f;
        if (!bAboveTrash)
        {
            if (Enemy == FocusedEnemy)
            {
                FocusBarReleaseTimes.Add(Enemy, Now);
            }
            else if (const double* Released = FocusBarReleaseTimes.Find(Enemy))
            {
                const double Elapsed = Now - *Released;
                if (Elapsed >= BreakerEnemyBar::FocusFadeSeconds) continue;
                BarAlpha = 1.0f - static_cast<float>(Elapsed / BreakerEnemyBar::FocusFadeSeconds);
            }
            else
            {
                continue;
            }
        }

        // Head and feet off the capsule root, so the plate rides a tall body
        // and the ellipse sits on the ground whatever the chassis height is.
        float HalfHeight = BreakerEnemyBar::FallbackHeadCm;
        if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Enemy->GetRootComponent()))
        {
            HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        }
        const FVector HeadWorld = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight);
        const FVector FeetWorld = Enemy->GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
        const FVector Projected = Project(HeadWorld, false);
        if (Projected.Z <= 0.0f) continue;

        const EBreakerMonsterRank Rank = Enemy->GetMonsterRank();
        const float Scale = BreakerEnemyBarMath::ScaleFor(Distance);
        const FBreakerEnemyBarRect Bar = BreakerEnemyBarPlace(Projected, Rank, Scale, ScaleUnit);
        const bool bShowChipAndShield = Distance < BreakerEnemyBarMath::ChipAndShieldHideCm;

        // ---- THE CHIP: what the bar was showing, for the hatch on a drop ----
        // BreakerHUDMath's chip is the player's, so the two bars cannot
        // disagree about a hold or a recovery. Its struct alone cannot SEE a
        // drop, because HealthChipShown returns the live value for an unstruck
        // chip; the player's HUD keeps a second member for that and this map
        // holds only the chip. So while a chip is unstruck (Time < 0, where
        // HealthChipShown ignores From) From carries the fraction the bar last
        // showed, and a live value under it is the drop. A hit that lands
        // INSIDE a hold cannot be seen that way — the chip is above the live
        // value by design — so it is read off the combat component's damage
        // clock, and re-arms from what the chip is showing, which is the
        // high-water rule HealthChipOnDrop states.
        const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        BreakerHUDMath::FHealthChip& Chip = ShownEnemyHealth.FindOrAdd(Enemy);
        if (Chip.Time < 0.0)
        {
            if (Fraction < Chip.From - UE_KINDA_SMALL_NUMBER)
            {
                Chip = BreakerHUDMath::HealthChipOnDrop(Chip, Chip.From, Now);
            }
            else
            {
                Chip.From = Fraction;
            }
        }
        else
        {
            const float Shown = BreakerHUDMath::HealthChipShown(Chip, Fraction, Now);
            const bool bSettled = Now - Chip.Time
                >= BreakerUI::HudHealthChipHoldSeconds + BreakerUI::HudHealthChipRecoverSeconds;
            const bool bHitThisFrame = EnemyCombat && EnemyCombat->GetSecondsSinceDamage() <= FrameSeconds;
            if (bSettled)
            {
                Chip = BreakerHUDMath::FHealthChip();
                Chip.From = Fraction;
            }
            else if (bHitThisFrame && Fraction < Shown - UE_KINDA_SMALL_NUMBER)
            {
                Chip = BreakerHUDMath::HealthChipOnDrop(Chip, Shown, Now);
            }
        }
        const float ChipFraction = BreakerHUDMath::HealthChipShown(Chip, Fraction, Now);

        // The shield line is the ward plus the standing front pool (O198),
        // read through the component that holds both figures.
        const float ShieldNow = EnemyCombat ? EnemyCombat->GetDisplayShield() : EnemyAttributes->GetShield();
        const float ShieldMax = EnemyCombat ? EnemyCombat->GetDisplayMaxShield() : EnemyAttributes->GetMaxShield();
        const float ShieldFraction = ShieldMax > UE_SMALL_NUMBER ? ShieldNow / ShieldMax : 0.0f;

        // ---- Rank, as geometry ---------------------------------------------
        // No gold edge (O129: colour carries health, not rank), no rank glyph,
        // no rank word. Elite is the ellipse at the feet (O203); the champion
        // is a filled diamond off each end of its wider bar; trash is the bar
        // alone; boss draws nothing new until its own pass.
        if (Rank == EBreakerMonsterRank::Elite)
        {
            const FVector Feet = Project(FeetWorld, false);
            if (Feet.Z > 0.0f)
            {
                const float BodyWidthPixels = BreakerEnemyBarBodyWidthPixels(Feet, CameraRight, FeetWorld,
                    [this](const FVector& P) { return Project(P, false); });
                BreakerEnemyBarDrawEllipse(*this, Feet.X, Feet.Y,
                    BodyWidthPixels * BreakerEnemyBarMath::HaloWidthRatio * 0.5f,
                    BreakerEnemyBarMath::HaloHeightFor(Scale) * ScaleUnit * 0.5f,
                    BreakerUI::System, BreakerEnemyBarMath::HaloStrokePx * ScaleUnit);
            }
        }
        else if (Rank == EBreakerMonsterRank::ModifierBearing)
        {
            const float Half = BreakerEnemyBarMath::ChampionDiamondFor(Scale) * ScaleUnit * 0.5f;
            const float Gap = BreakerEnemyBarMath::ChampionDiamondGapFor(Scale) * ScaleUnit;
            const float MidY = Bar.Y + Bar.H * 0.5f;
            BreakerEnemyBarDrawFilledDiamond(*this, Bar.X - Gap - Half, MidY, Half, BreakerUI::System);
            BreakerEnemyBarDrawFilledDiamond(*this, Bar.X + Bar.W + Gap + Half, MidY, Half, BreakerUI::System);
        }

        // ---- The column above the bar: marks, then the name ------------------
        // No prose. A modifier is announced by SHAPE at every range, and the
        // shape is the same one at every range, so what the player learns at
        // ten metres is what they read at fifty. The only word left is BOSS.
        TArray<BreakerEnemyBarMath::EBreakerEnemyMark> Marks;
        if (const UBreakerEnemyModifierComponent* Modifiers = Enemy->GetModifierComponent())
        {
            for (EBreakerEnemyModifier Modifier : Modifiers->GetModifiers())
            {
                if (Marks.Num() >= BreakerEnemyBarMath::MaximumMarks) break;
                const BreakerEnemyBarMath::EBreakerEnemyMark Mark = BreakerEnemyBarMath::MarkFor(Modifier);
                if (Mark != BreakerEnemyBarMath::EBreakerEnemyMark::None) Marks.Add(Mark);
            }
        }
        const bool bBossRank = Rank == EBreakerMonsterRank::Boss;

        const float Gap = BreakerEnemyBarMath::ColumnGapPx * Scale * ScaleUnit;
        const float NameH = bBossRank ? BreakerEnemyBar::NameLinePixels * Scale * ScaleUnit : 0.0f;
        const float NameY = Bar.Y - Gap - NameH;
        const float Cell = BreakerEnemyBarMath::MarkCellPx * Scale * ScaleUnit;
        const float Pitch = Cell + BreakerEnemyBarMath::MarkGapFor(Scale) * ScaleUnit;
        const float MarksW = Marks.Num() > 0 ? Cell + Pitch * (Marks.Num() - 1) : 0.0f;
        const float MarksY = (bBossRank ? NameY : Bar.Y) - (Marks.Num() > 0 ? Gap + Cell : 0.0f);
        const float ColumnTop = Marks.Num() > 0 ? MarksY : (bBossRank ? NameY : Bar.Y);
        const float ColumnW = FMath::Max(Bar.W, MarksW);
        const float ColumnH = Bar.Y + Bar.H - ColumnTop;

        // Screen-space overlap suppression over the WHOLE column. Two enemies
        // standing in line with the camera project to nearly the same point,
        // and the second column lands on top of the first — unreadable, and
        // worse than showing one. The bar always draws: a hidden health read
        // on the body behind is worse than two bars touching. The marks and
        // the name yield, because those are what turned to mush. The focused
        // enemy is drawn regardless, because it is the one the player is
        // deliberately asking about.
        const bool bFocused = (Enemy == FocusedEnemy);
        bool bOccluded = false;
        if (!bFocused)
        {
            for (const FVector4& Taken : DrawnLabelBounds)
            {
                if (FMath::Abs(Projected.X - Taken.X) < (ColumnW + Taken.Z) * 0.5f
                    && FMath::Abs(ColumnTop - Taken.Y) < (ColumnH + Taken.W) * 0.5f)
                {
                    bOccluded = true;
                    break;
                }
            }
        }
        if (!bOccluded)
        {
            DrawnLabelBounds.Emplace(Projected.X, ColumnTop, ColumnW, ColumnH);
        }

        BreakerEnemyBarDrawBody(*this, Bar, Fraction, ChipFraction, ShieldFraction, bShowChipAndShield,
            ScaleUnit, BarAlpha);

        if (!bOccluded)
        {
            if (bBossRank)
            {
                DrawSpecTextCentered(TEXT("BOSS"), Projected.X, NameY, BreakerUI::TextSecondary,
                    BreakerEnemyBar::NamePixels * Scale, BarAlpha);
            }
            if (Marks.Num() > 0)
            {
                const FLinearColor MarkColour = BreakerUI::Alpha(BreakerUI::System, BarAlpha);
                const float RowX = Projected.X - MarksW * 0.5f;
                for (int32 i = 0; i < Marks.Num(); ++i)
                {
                    BreakerEnemyBarDrawMark(*this, RowX + Pitch * i, MarksY, Cell, Marks[i], MarkColour);
                }
            }
        }
    }

    // --- Target dummies (ruled with the reaction extraction) ----------------
    // A looter shooter where you cannot tell whether you are doing damage has
    // no feedback loop, and the gym's own targets had no readout at all. The
    // CORE of the bar — shield line, health fill, range scaling — over every
    // live dummy, in Trash placement, plus its profile label in the name slot;
    // no blips, no rank geometry, no focus line: those are enemy facts.
    //
    // NO CHIP. The chip map is keyed by ABreakerEnemy and a dummy is a bare
    // actor, so the dummy's bar shows the live fraction and nothing behind it.
    // The gap is recorded here rather than faked with a second map: a dummy
    // is an instrument, and the chip is a fight read.
    //
    // The visibility rule here is the RECENCY window, deliberately, and the
    // enemy path above going aimed-at-only does not take it with it. See
    // BreakerEnemyBar::RecentDamageSeconds for the ruling.
    for (TActorIterator<ABreakerTargetDummy> It(World); It; ++It)
    {
        const ABreakerTargetDummy* Dummy = *It;
        if (!Dummy) continue;
        const float Distance = FVector::Distance(ViewerLocation, Dummy->GetActorLocation());
        if (Distance > BreakerEnemyBarMath::DrawCm) continue;
        const UAbilitySystemComponent* DummyAbilitySystem = Dummy->GetAbilitySystemComponent();
        const UBreakerAttributeSet* DummyAttributes = DummyAbilitySystem ? DummyAbilitySystem->GetSet<UBreakerAttributeSet>() : nullptr;
        if (!DummyAttributes) continue;
        const float Health = DummyAttributes->GetHealth();
        const float MaxHealth = DummyAttributes->GetMaxHealth();
        if (Health <= 0.0f || MaxHealth <= UE_SMALL_NUMBER) continue;
        const UBreakerCombatComponent* DummyCombat = Dummy->FindComponentByClass<UBreakerCombatComponent>();
        const bool bRecentlyDamaged = DummyCombat && DummyCombat->GetSecondsSinceDamage() < BreakerEnemyBar::RecentDamageSeconds;
        if (!bRecentlyDamaged && Distance > BreakerEnemyBar::AlwaysDistance) continue;
        const FVector Projected = Project(Dummy->GetActorLocation() + FVector(0.0f, 0.0f, BreakerEnemyBar::FallbackHeadCm), false);
        if (Projected.Z <= 0.0f) continue;

        const float Scale = BreakerEnemyBarMath::ScaleFor(Distance);
        const FBreakerEnemyBarRect Bar = BreakerEnemyBarPlace(Projected, EBreakerMonsterRank::Trash, Scale, ScaleUnit);
        const float Fraction = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);
        const float ShieldMax = DummyAttributes->GetMaxShield();
        const float ShieldFraction = ShieldMax > UE_SMALL_NUMBER ? DummyAttributes->GetShield() / ShieldMax : 0.0f;
        // Full opacity: a dummy never fades, and Alpha(C, 1.0f) is exactly C.
        BreakerEnemyBarDrawBody(*this, Bar, Fraction, Fraction, ShieldFraction,
            Distance < BreakerEnemyBarMath::ChipAndShieldHideCm, ScaleUnit, 1.0f);
        const float NameY = Bar.Y - BreakerEnemyBarMath::ColumnGapPx * Scale * ScaleUnit
            - BreakerEnemyBar::NameLinePixels * Scale * ScaleUnit;
        DrawSpecTextCentered(Dummy->GetProfileLabel(), Projected.X, NameY,
            BreakerUI::TextSecondary, BreakerEnemyBar::NamePixels * Scale);
    }
}
