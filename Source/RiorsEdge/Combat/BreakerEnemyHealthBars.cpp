// ---------------------------------------------------------------------------
// The enemy health bar, and nothing else.
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
// friend declaration. The five visibility constants moved here whole because
// nothing outside this pass ever read them.
//
// What did NOT move: DrawMinimap consumes EnemyBlips, which the enemy loop
// below fills. That ordering contract is restated at the loop; it is the one
// coupling that survives the cut.
// ---------------------------------------------------------------------------

#include "UI/BreakerPlaytestHUD.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/BreakerCharacter.h"
#include "Attributes/BreakerHealthBands.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemyModifiers.h"
#include "Combat/BreakerModifierComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerTargetDummy.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UI/BreakerUIStyle.h"

// Named for this pass, not BreakerHUD: unity builds merge translation units,
// and a second `namespace BreakerHUD` carrying its own constants under the old
// names is a redefinition the moment the two files land in one blob. The
// project has shipped that collision twice under other names.
namespace BreakerEnemyBar
{
    // Enemy bar visibility rules.
    static constexpr float MaxDistance = 5000.0f;
    // Aim cone for "the enemy I am asking about". Presentation, not balance:
    // it decides which enemy gets a bar and a verbose label, never anything
    // about damage or aim. Roughly matches the loot focus cone so the two
    // agree about what the player is pointing at.
    static constexpr float FocusMinimumDot = 0.985f;
    // How long a trash bar lingers after the aim leaves it (selective bars;
    // above-Trash ranks are always barred). O2 PLACEHOLDER.
    static constexpr float FocusFadeSeconds = 0.6f;

    // --- THE BAR BELONGS TO A BODY, SO IT IS SIZED BY THAT BODY -----------
    // The bar was a fixed 180 px scaled only by a 1.0 -> 0.55 lerp across the
    // whole 50 m, which makes it very nearly distance-INVARIANT while the body
    // it describes shrinks with 1/d. Measured off the bar probe: 167 px of bar
    // against a ~72 px silhouette at 12 m, and 126 px against ~25 px at 35 m.
    // Adjacent bars butt together into one continuous horizontal stripe, which
    // is exactly what the owner reported seeing, and it gets WORSE with
    // distance because the bar barely shrinks. Rank and segmentation were both
    // drawing correctly the whole time.
    //
    // WIDTH ONLY. Height, text and border keep the gentle scale: a bar 25 px
    // wide at 35 m is correct, a bar 1 px TALL at 35 m is gone. The defect was
    // never that the bar is too big, it is that its width does not belong to
    // anything — so width becomes a function of the silhouette and the other
    // axes stay as they were.
    //
    // A ratio of 1.0 means the bar spans the body exactly, which is what makes
    // it read as THAT enemy's bar rather than as screen furniture. O2
    // PLACEHOLDER, and the first number the owner will want to move.
    static constexpr float WidthToBodyRatio = 1.0f;
    // Below this a bar stops being a bar — no fill is legible and no band can
    // draw. The floor is what keeps a distant enemy readable at all, and it is
    // also the reason the rank GLYPH exists: past the distance where the floor
    // binds, width has stopped carrying information and something else must.
    // O2 PLACEHOLDER.
    static constexpr float MinimumWidthPixels = 32.0f;
    // Half the humanoid silhouette, matching BodyCollision's 45 cm capsule
    // radius. Not read off the component: the probe's frozen bodies and the
    // dummy share this path, and a per-actor query here would cost a component
    // fetch per enemy per frame to recover a number that is the same for every
    // body in the game.
    static constexpr float BodyHalfWidthCm = 45.0f;

    // --- The rank glyph's proportions -------------------------------------
    // Half-extent as a fraction of bar HEIGHT (see the glyph block below for
    // why height and not width). At the near end that is a mark a little
    // taller than the bar; at range it is the bar's own height, which is the
    // smallest thing on screen still known to resolve. Both O2 PLACEHOLDER.
    static constexpr float GlyphHeightRatio = 0.9f;
    static constexpr float GlyphGapPixels = 3.0f;

    // --- A1: how narrow a band may get before it stops being drawn --------
    // BreakerHealthBands::SegmentCountFor is the ONE source of the count and
    // is never second-guessed here. This is a DISPLAY limit and it is keyed on
    // PIXELS, never on rank: LEDGER's header rules that display may show less
    // than state knows but the two may never disagree, and a rank test here
    // would be display quietly editing state. It also gives the distance
    // behaviour for free — the bar shrinks with range, so bands fade out as a
    // pack gets further away rather than at an authored cutoff. O2 PLACEHOLDER.
    static constexpr float MinimumSegmentPixels = 7.0f;

    // --- A9: the three label bands ----------------------------------------
    // Words up close, marks beyond. O2 PLACEHOLDER, and the NEAR edge is the
    // one that matters: past it a modifier is announced by SHAPE rather than
    // by a word, which is what stops eighty enemies printing prose at range.
    static constexpr float NearDistance = 1500.0f;
    static constexpr float MidDistance = 3000.0f;
    // At most this many marks. Encounter-Design caps a body at three
    // modifiers, so this is the cap and not a truncation nobody stated.
    static constexpr int32 MaximumMarks = 3;

    // --- A8: the shield hatch ---------------------------------------------
    // The shield must read as a different MATERIAL than health, not a
    // different hue: colour is already carrying rank, health and family, and
    // a fourth claim on it would not be heard. O2 PLACEHOLDER.
    static constexpr float HatchTickPixels = 2.0f;
    static constexpr float HatchGapPixels = 3.0f;

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
    // Where the bar sits and how big it is. Both loops below place it
    // identically — centred on the projected point, shrinking with distance —
    // and both then hang a label off the same rectangle, so this returns the
    // rectangle rather than drawing it.
    //
    // ScaleUnit is the HUD's S(1.0f) passed in, because S() is private and
    // these are free functions. S(x) is x * UIScale and ScaleUnit IS UIScale,
    // so x * ScaleUnit is the same product of the same two floats.
    struct FBreakerEnemyBarRect
    {
        float X = 0.0f;
        float Y = 0.0f;
        float W = 0.0f;
        float H = 0.0f;
        // The distance scale itself, exposed because the label under the bar
        // and the elite edge on it both shrink on the same curve.
        float Scale = 1.0f;
    };

    FBreakerEnemyBarRect BreakerEnemyBarPlace(const FVector& Projected, float Distance,
        float ScaleUnit, float BodyWidthPixels)
    {
        // Gentle distance scaling: readable up close, unobtrusive far away.
        // This still drives HEIGHT, text and border — see the width block above
        // for why width left it.
        const float DistanceAlpha = FMath::Clamp(
            (Distance - 500.0f) / (BreakerEnemyBar::MaxDistance - 500.0f), 0.0f, 1.0f);

        FBreakerEnemyBarRect Rect;
        Rect.Scale = FMath::Lerp(1.0f, 0.55f, DistanceAlpha);
        Rect.H = BreakerUI::HudEnemyBarHeight * ScaleUnit * Rect.Scale;
        // Capped at what the bar used to be, so nothing grows: this change only
        // ever makes a bar narrower. Floored so it stays a bar.
        const float Ceiling = BreakerUI::HudEnemyBarWidth * ScaleUnit * Rect.Scale;
        Rect.W = FMath::Clamp(BodyWidthPixels * BreakerEnemyBar::WidthToBodyRatio,
            FMath::Min(BreakerEnemyBar::MinimumWidthPixels * ScaleUnit, Ceiling), Ceiling);
        Rect.X = Projected.X - Rect.W * 0.5f;
        Rect.Y = Projected.Y;
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

    // --- THE RANK GLYPH (readability pack, ORDERS Part One-B item 4) ------
    // Never routed to a lane until now, and it is the only rank carrier that
    // survives distance. Past roughly 35 m the gold edge and the rank word are
    // both at 0.55 scale and unreadable, so a body's rank is carried by a WORD
    // THAT CANNOT BE READ at exactly the range where knowing the rank matters
    // most. A shape can be read when a word cannot.
    //
    // SIZED OFF THE BAR'S HEIGHT, NEVER ITS WIDTH, and that is deliberate.
    // Width is now the projected width of the silhouette and clamps to a 32 px
    // floor at range; height keeps the gentle 1.0 -> 0.55 scale. Tying the
    // glyph to width would shrink it fastest exactly where it becomes the only
    // carrier — the opposite of what it is for.
    //
    // Drawn OUTSIDE the left cap rather than inside it: inside would eat fill,
    // and fill is the health read.
    void BreakerEnemyBarDrawDiamond(AHUD& HUD, float CentreX, float CentreY, float HalfExtent,
        const FLinearColor& Colour, float Thickness, bool bFilled)
    {
        if (bFilled)
        {
            // Scanline fill: the canvas cannot rotate a rect, and a diamond of
            // three or four pixels is a handful of rows rather than a texture.
            const int32 Rows = FMath::Max(1, FMath::RoundToInt(HalfExtent));
            for (int32 Row = -Rows; Row <= Rows; ++Row)
            {
                const float RowHalf = HalfExtent * (1.0f - FMath::Abs(Row) / static_cast<float>(Rows));
                if (RowHalf <= 0.0f) continue;
                HUD.DrawRect(Colour, CentreX - RowHalf, CentreY + Row, RowHalf * 2.0f, 1.0f);
            }
            return;
        }
        HUD.DrawLine(CentreX, CentreY - HalfExtent, CentreX + HalfExtent, CentreY, Colour, Thickness);
        HUD.DrawLine(CentreX + HalfExtent, CentreY, CentreX, CentreY + HalfExtent, Colour, Thickness);
        HUD.DrawLine(CentreX, CentreY + HalfExtent, CentreX - HalfExtent, CentreY, Colour, Thickness);
        HUD.DrawLine(CentreX - HalfExtent, CentreY, CentreX, CentreY - HalfExtent, Colour, Thickness);
    }

    // Anchored by its RIGHT edge so the mark grows leftward away from the bar,
    // and a two-glyph rank never pushes the bar sideways.
    void BreakerEnemyBarDrawRankGlyph(AHUD& HUD, float RightEdgeX, float CentreY, float HalfExtent,
        EBreakerMonsterRank Rank, const FLinearColor& Colour, float Thickness)
    {
        switch (Rank)
        {
        case EBreakerMonsterRank::Elite:
            // A hollow SQUARE: the one rank whose mark is a different shape
            // rather than a different count, so elite never reads as "some
            // number of diamonds" at the distance where counting gets hard.
            HUD.DrawLine(RightEdgeX - HalfExtent * 2.0f, CentreY - HalfExtent, RightEdgeX, CentreY - HalfExtent, Colour, Thickness);
            HUD.DrawLine(RightEdgeX, CentreY - HalfExtent, RightEdgeX, CentreY + HalfExtent, Colour, Thickness);
            HUD.DrawLine(RightEdgeX, CentreY + HalfExtent, RightEdgeX - HalfExtent * 2.0f, CentreY + HalfExtent, Colour, Thickness);
            HUD.DrawLine(RightEdgeX - HalfExtent * 2.0f, CentreY + HalfExtent, RightEdgeX - HalfExtent * 2.0f, CentreY - HalfExtent, Colour, Thickness);
            break;
        case EBreakerMonsterRank::ModifierBearing:
            // TWO hollow diamonds. Champion is elite-and-more, so its mark is
            // the trash mark twice rather than a third unrelated shape.
            BreakerEnemyBarDrawDiamond(HUD, RightEdgeX - HalfExtent, CentreY, HalfExtent, Colour, Thickness, false);
            BreakerEnemyBarDrawDiamond(HUD, RightEdgeX - HalfExtent * 3.0f, CentreY, HalfExtent, Colour, Thickness, false);
            break;
        case EBreakerMonsterRank::Boss:
            // Two FILLED. Fill is the last thing to survive shrinking — an
            // outline closes up into a blob long before a solid does — so the
            // rank that must never be mistaken gets the most robust mark.
            BreakerEnemyBarDrawDiamond(HUD, RightEdgeX - HalfExtent, CentreY, HalfExtent, Colour, Thickness, true);
            BreakerEnemyBarDrawDiamond(HUD, RightEdgeX - HalfExtent * 3.0f, CentreY, HalfExtent, Colour, Thickness, true);
            break;
        default:
            // Trash: one filled diamond, the smallest mark that is still a
            // mark. It draws only when a trash bar draws at all, which is
            // while aimed at or fading.
            BreakerEnemyBarDrawDiamond(HUD, RightEdgeX - HalfExtent, CentreY, HalfExtent, Colour, Thickness, true);
            break;
        }
    }

    // ONE bar body, drawn by both loops below. It was two: the enemy loop and
    // the dummy loop each carried the fill line and the seven-line shield
    // block near-verbatim. A segmented bar is coming, and authored into only
    // one of these, the gym — the copy the owner actually plays — would have
    // been the one surface where the new bar was invisible.
    //
    // Alpha is the only axis the two ever differed on: the enemy loop fades a
    // trash bar out, the dummy loop never fades. BreakerUI::Alpha SETS the
    // alpha channel rather than scaling it, and every colour here arrives from
    // Hex() at A=1, so Alpha(C, 1.0f) is exactly C — the dummy's appearance is
    // unchanged by routing it through the fading path.
    void BreakerEnemyBarDrawBody(AHUD& HUD, const FBreakerEnemyBarRect& Bar,
        float Health, float MaxHealth, float Shield, float MaxShield, float ScaleUnit, float BarAlpha,
        int32 SegmentCount)
    {
        if (Shield > 0.0f && MaxShield > UE_SMALL_NUMBER)
        {
            // A8: HATCHED, above, depletes first. The hatch is the point — a
            // shield is a different MATERIAL than health, and saying so in
            // hue would be a fourth claim on a channel already carrying rank,
            // family and health. Ticks read as "mesh" at every distance the
            // bar is legible at, and when they get too fine to resolve the
            // fill degrades to a solid cyan bar, which is where it started.
            const float ShieldH = FMath::Max(Bar.H * 0.45f, 2.0f * ScaleUnit);
            const float ShieldY = Bar.Y - ShieldH - ScaleUnit;
            const float FilledW = Bar.W * FMath::Clamp(Shield / MaxShield, 0.0f, 1.0f);
            HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Panel10, BarAlpha), Bar.X, ShieldY, Bar.W, ShieldH);

            const float Tick = BreakerEnemyBar::HatchTickPixels * ScaleUnit * Bar.Scale;
            const float Period = Tick + BreakerEnemyBar::HatchGapPixels * ScaleUnit * Bar.Scale;
            if (Tick < 1.0f || Period < 2.0f)
            {
                HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Cyan, BarAlpha), Bar.X, ShieldY, FilledW, ShieldH);
            }
            else
            {
                for (float Offset = 0.0f; Offset < FilledW; Offset += Period)
                {
                    HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Cyan, BarAlpha),
                        Bar.X + Offset, ShieldY, FMath::Min(Tick, FilledW - Offset), ShieldH);
                }
            }
        }

        HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Panel10, BarAlpha), Bar.X, Bar.Y, Bar.W, Bar.H);
        HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Harm, BarAlpha), Bar.X, Bar.Y,
            Bar.W * FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f), Bar.H);

        // A1: THE BANDS, DRAWN FROM THE ONE COUNT THAT DEFINES THEM. The
        // divider index the bar paints and the band index the damage path
        // counts come out of BreakerHealthBands and nowhere else — a local
        // constant that agreed today is exactly the disagreement this exists
        // to prevent. Dividers are cut OUT of the fill in the track colour, so
        // a band boundary is a gap rather than an added line and the bar never
        // grows when it segments.
        if (SegmentCount > 1)
        {
            const float SegmentW = Bar.W / static_cast<float>(SegmentCount);
            if (SegmentW >= BreakerEnemyBar::MinimumSegmentPixels * ScaleUnit)
            {
                const float DividerW = FMath::Max(1.0f, ScaleUnit * Bar.Scale);
                for (int32 i = 1; i < SegmentCount; ++i)
                {
                    HUD.DrawRect(BreakerUI::Alpha(BreakerUI::Panel10, BarAlpha),
                        Bar.X + SegmentW * i - DividerW * 0.5f, Bar.Y, DividerW, Bar.H);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------
// §Anchors — enemy bars 180x8 with the rank word at 11px beneath. Above-Trash
// ranks are barred whenever they are inside MaxDistance; trash is barred only
// while it is under the crosshair, and fades for FocusFadeSeconds after the
// aim leaves. See the selective-bar block below for why that is the rule and
// not a recency window.
// --------------------------------------------------------------------------
void ABreakerPlaytestHUD::DrawEnemyHealthBars(const ABreakerCharacter* Character)
{
    // ORDERING CONTRACT: this runs before DrawMinimap, and it is the ONE enemy
    // iteration the HUD makes. Reset (not Empty) keeps the capacity, so the
    // array stops allocating after the first busy frame — DrawHUD runs every
    // frame and a container built inside it is a per-frame allocation.
    EnemyBlips.Reset();

    UWorld* World = GetWorld();
    if (!World || !Character) return;
    const FVector ViewerLocation = Character->GetActorLocation();
    // S() is private and the geometry helpers above are free functions, so the
    // scale crosses that boundary as a value. Resolved once per frame.
    const float ScaleUnit = S(1.0f);

    // Which enemy the player is actually asking about. This is what decides
    // whether a trash mob is barred at all, and it is the only enemy that gets
    // the verbose state line; see the label block below for that split. Same
    // aim-cone shape DrawLootPickups already uses to pick its focused pickup,
    // so "what am I pointing at" means one thing across the whole HUD.
    // The camera's right, resolved once per frame: every bar's width is the
    // projected width of its own silhouette, and that needs a screen-space
    // direction to measure across. Defaults to world Y so a missing camera
    // manager degrades to a floored bar rather than to no bar.
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

    // The fade map is the one piece of state here that outlives a frame, so it
    // is the one that can grow without a bound. Pruned by the same clock that
    // reads it: an entry older than the fade is finished, and a body that died
    // or was parked for the pool goes stale and drops on the same pass.
    {
        const double Now = World->GetTimeSeconds();
        for (auto It = FocusBarReleaseTimes.CreateIterator(); It; ++It)
        {
            if (!It.Key().IsValid() || Now - It.Value() >= BreakerEnemyBar::FocusFadeSeconds)
            {
                It.RemoveCurrent();
            }
        }
    }

    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        const ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;

        // PRODUCER END OF A CROSS-LANE CONTRACT (O155). This fill belongs to
        // the COMBAT lane; the consumer is the UI lane's DrawMinimap, which
        // iterates nothing and reads exactly what this loop leaves behind.
        // The ordering is load-bearing — the fill must run before the read in
        // the same frame or the map draws last frame's hostiles — and neither
        // the compiler nor the suite can see it break, because the two halves
        // are members of one class. A change to the shape, the meaning or the
        // fill order of EnemyBlips is a declared crossing: tell the UI lane
        // before it lands.
        //
        // Collected BEFORE the health-bar culls, because the two readouts want
        // different ranges: a bar is pointless past 50 m, and a minimap is
        // mostly useful for the hostiles that are further away than that.
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
        if (Distance > BreakerEnemyBar::MaxDistance) continue;

        const UAbilitySystemComponent* EnemyAbilitySystem = Enemy->GetAbilitySystemComponent();
        const UBreakerAttributeSet* EnemyAttributes = EnemyAbilitySystem ? EnemyAbilitySystem->GetSet<UBreakerAttributeSet>() : nullptr;
        if (!EnemyAttributes) continue;

        const float Health = EnemyAttributes->GetHealth();
        const float MaxHealth = EnemyAttributes->GetMaxHealth();
        if (Health <= 0.0f || MaxHealth <= UE_SMALL_NUMBER) continue;

        // SELECTIVE BARS (ruled for the crowd): in a fight of eighty, eighty
        // bars is a rendering cost AND the noise that hides the read the bars
        // exist for. The rules, built now with the visual to swap in later:
        //  * ABOVE TRASH — always barred inside MaxDistance. Elites and
        //    champions are the fight's anchors; their health is standing
        //    information. IsEliteOrBetter, NEVER IsElite: rank == Elite
        //    exactly would exclude ModifierBearing and Boss — the two ranks
        //    ABOVE the one meant — and the project has shipped that predicate
        //    bug twice (the enemy header records both).
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
        // A7: THE FADE IS PER ENEMY, and it was not. LastFocusBarEnemy held
        // ONE body, so sweeping the crosshair from A to B overwrote A's clock
        // the same frame — A hard-cut to nothing while B lit, which is the
        // strobe the fade exists to prevent and is visible any time the aim
        // crosses a pack. Only the most recently released enemy could fade,
        // and only until the next one was focused. A map gives every body its
        // own clock, so a sweep across six trash mobs leaves six trails.
        const bool bAboveTrash = Enemy->IsEliteOrBetter();
        float BarAlpha = 1.0f;
        if (!bAboveTrash)
        {
            const double Now = World->GetTimeSeconds();
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

        const FVector Projected = Project(Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), false);
        if (Projected.Z <= 0.0f) continue;

        // The gold edge and the rank word follow the SAME predicate as the
        // permanent bar. ModifierBearing prints ELITE — it is, by the enemy
        // header's own definition, a modifier-bearing elite, and its modifier
        // banner (always printed below) is what distinguishes it further.
        const bool bElite = Enemy->IsEliteOrBetter();
        const bool bBossRank = Enemy->GetMonsterRank() == EBreakerMonsterRank::Boss;

        const float BodyWidthPixels = BreakerEnemyBarBodyWidthPixels(Projected, CameraRight,
            Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
            [this](const FVector& P) { return Project(P, false); });
        const FBreakerEnemyBarRect Bar = BreakerEnemyBarPlace(Projected, Distance, ScaleUnit, BodyWidthPixels);
        BreakerEnemyBarDrawBody(*this, Bar, Health, MaxHealth,
            EnemyAttributes->GetShield(), EnemyAttributes->GetMaxShield(), ScaleUnit, BarAlpha,
            BreakerHealthBands::SegmentCountFor(Enemy->GetMonsterRank()));
        if (bElite)
        {
            // Gold edge, not a gold fill: the health colour must stay readable.
            DrawBorder(Bar.X, Bar.Y, Bar.W, Bar.H, BreakerUI::Alpha(BreakerUI::Gold, BarAlpha), ScaleUnit * Bar.Scale);
        }

        // The rank glyph, outside the left cap. Colour follows the rank word it
        // outlives, so the two agree while both are legible and the glyph
        // carries alone once the word is not.
        {
            const float GlyphHalf = FMath::Max(2.0f, Bar.H * BreakerEnemyBar::GlyphHeightRatio);
            const float GlyphRight = Bar.X - BreakerEnemyBar::GlyphGapPixels * ScaleUnit * Bar.Scale;
            BreakerEnemyBarDrawRankGlyph(*this, GlyphRight, Bar.Y + Bar.H * 0.5f, GlyphHalf,
                Enemy->GetMonsterRank(),
                BreakerUI::Alpha(bElite ? BreakerUI::Gold : BreakerUI::TextMuted, BarAlpha),
                FMath::Max(1.0f, ScaleUnit * Bar.Scale));
        }

        // ---- The label, and how much of it -----------------------------
        // Owner: "theres a lot of text bloat on enemies". Every enemy printed
        // ELITE/HOSTILE plus the whole of GetEnemyStateLabel(), which is up to
        // THREE stacked lines (family banner, modifier banner, state) — and
        // nothing checked whether two enemies' labels landed on the same
        // pixels. Six enemies in a pocket produced the overlapping mush in the
        // report: "WARDED | VOLATILE" printed through "HOSTILE · WIND-UP"
        // printed through "CHASE".
        //
        // What survives, and why that split:
        //  - The MODIFIER banner is load-bearing and always prints.
        //    Encounter-Design §1.2's first acceptance test is that a modifier
        //    is identifiable within 1.5s of the enemy entering view, and an
        //    unannounced modifier is an unfair death rather than a challenge.
        //    Culling it to declutter would trade legibility for legibility.
        //  - The STATE line (CHASE / CLOSING / WIND-UP) prints only for the
        //    enemy under the crosshair. It was the loudest line and the least
        //    informative: the enemy's own telegraph already shows a wind-up as
        //    a scaling, brightening emitter, so the text was restating in
        //    words, six times over, something the world was already saying.
        //  - ELITE prints; HOSTILE does not. "HOSTILE" on every hostile is not
        //    information — the health bar already says it is an enemy.
        // A9: THE MODIFIER ANNOUNCEMENT GOES DISTANCE-PROGRESSIVE. It printed
        // in full at every range and was most of the text bloat at distance —
        // "OVERCHARGE | WARDED | CAUTERIZE" is three words of prose on a body
        // forty metres away, times eighty bodies. The rule keeps §1.2's
        // acceptance test (a modifier identifiable within 1.5s of entering
        // view) and pays for it in the currency the range can afford:
        //  * NEAR — the words, unchanged. Close enough to read prose.
        //  * MID and FAR — one MARK per modifier: a square and the modifier
        //    name's first letter, drawn below. Shape carries the count, which
        //    is the read that actually matters at range ("that one has three
        //    of something"), and the letter disambiguates on approach.
        // The letter comes from GetModifierName, never a second table: a mark
        // alphabet authored here would be a second source for a name the
        // modifier layer already owns.
        const bool bFocused = (Enemy == FocusedEnemy);
        const bool bNear = Distance <= BreakerEnemyBar::NearDistance;
        const FString ModifierBanner = Enemy->GetEnemyModifierBanner();
        TArray<FString> Lines;
        if (bElite) Lines.Add(bBossRank ? TEXT("BOSS") : TEXT("ELITE"));
        if (bNear && !ModifierBanner.IsEmpty()) Lines.Add(ModifierBanner);
        if (bFocused) Lines.Add(Enemy->GetEnemyStateLabel());

        TArray<FString> Marks;
        if (!bNear)
        {
            if (const UBreakerEnemyModifierComponent* Modifiers = Enemy->GetModifierComponent())
            {
                for (EBreakerEnemyModifier Modifier : Modifiers->GetModifiers())
                {
                    if (Marks.Num() >= BreakerEnemyBar::MaximumMarks) break;
                    const FString Name = UBreakerEnemyModifierLibrary::GetModifierName(Modifier);
                    Marks.Add(Name.IsEmpty() ? TEXT("?") : Name.Left(1).ToUpper());
                }
            }
        }

        if (Lines.Num() > 0 || Marks.Num() > 0)
        {
            const FString Label = FString::Join(Lines, TEXT("\n"));
            const float LabelY = Bar.Y + Bar.H + S(3.0f);
            // Screen-space overlap suppression. Two enemies standing in line
            // with the camera project to nearly the same point, and the second
            // label lands on top of the first — unreadable, and worse than
            // showing one. The focused enemy is drawn regardless, because it is
            // the one the player is deliberately asking about.
            // The mark row is part of the label for occlusion purposes: it
            // occupies the same column under the same bar, so letting it out
            // of the bounds test would reintroduce the overlap the test
            // exists for, in glyphs instead of words.
            const float MarkPitch = S(11.0f) * Bar.Scale;
            const float MarkRowH = Marks.Num() > 0 ? S(9.0f) * Bar.Scale : 0.0f;
            const float LineCount = static_cast<float>(Lines.Num());
            const float LabelH = S(13.0f) * Bar.Scale * LineCount + MarkRowH;
            const float LabelW = FMath::Max(Bar.W, MarkPitch * Marks.Num());
            bool bOccluded = false;
            if (!bFocused)
            {
                for (const FVector4& Taken : DrawnLabelBounds)
                {
                    if (FMath::Abs(Projected.X - Taken.X) < (LabelW + Taken.Z) * 0.5f
                        && FMath::Abs(LabelY - Taken.Y) < (LabelH + Taken.W) * 0.5f)
                    {
                        bOccluded = true;
                        break;
                    }
                }
            }
            if (!bOccluded)
            {
                DrawnLabelBounds.Emplace(Projected.X, LabelY, LabelW, LabelH);
                const FLinearColor LabelColor = bElite ? BreakerUI::Gold : BreakerUI::TextMuted;
                if (Lines.Num() > 0)
                {
                    DrawSpecTextCentered(Label, Projected.X, LabelY, LabelColor, 11.0f * Bar.Scale);
                }
                if (Marks.Num() > 0)
                {
                    const float MarkY = LabelY + S(13.0f) * Bar.Scale * LineCount;
                    const float Square = FMath::Max(2.0f, S(4.0f) * Bar.Scale);
                    const float RowX = Projected.X - MarkPitch * Marks.Num() * 0.5f;
                    for (int32 i = 0; i < Marks.Num(); ++i)
                    {
                        const float CellX = RowX + MarkPitch * i;
                        DrawRect(BreakerUI::Alpha(LabelColor, BarAlpha), CellX, MarkY + Square * 0.25f, Square, Square);
                        DrawSpecTextCentered(Marks[i], CellX + MarkPitch * 0.68f, MarkY,
                            LabelColor, 9.0f * Bar.Scale);
                    }
                }
            }
        }
    }

    // --- Target dummies (ruled with the reaction extraction) ----------------
    // A looter shooter where you cannot tell whether you are doing damage has
    // no feedback loop, and the gym's own targets had no readout at all. The
    // CORE of the enemy bar — shield pip, health fill, distance scaling — over
    // every live dummy, plus its profile label; no blips, no elite border, no
    // focus line: those are enemy facts.
    //
    // The visibility rule here is the RECENCY window, deliberately, and the
    // enemy path above going aimed-at-only does not take it with it. See
    // BreakerEnemyBar::RecentDamageSeconds for the ruling.
    for (TActorIterator<ABreakerTargetDummy> It(World); It; ++It)
    {
        const ABreakerTargetDummy* Dummy = *It;
        if (!Dummy) continue;
        const float Distance = FVector::Distance(ViewerLocation, Dummy->GetActorLocation());
        if (Distance > BreakerEnemyBar::MaxDistance) continue;
        const UAbilitySystemComponent* DummyAbilitySystem = Dummy->GetAbilitySystemComponent();
        const UBreakerAttributeSet* DummyAttributes = DummyAbilitySystem ? DummyAbilitySystem->GetSet<UBreakerAttributeSet>() : nullptr;
        if (!DummyAttributes) continue;
        const float Health = DummyAttributes->GetHealth();
        const float MaxHealth = DummyAttributes->GetMaxHealth();
        if (Health <= 0.0f || MaxHealth <= UE_SMALL_NUMBER) continue;
        const UBreakerCombatComponent* DummyCombat = Dummy->FindComponentByClass<UBreakerCombatComponent>();
        const bool bRecentlyDamaged = DummyCombat && DummyCombat->GetSecondsSinceDamage() < BreakerEnemyBar::RecentDamageSeconds;
        if (!bRecentlyDamaged && Distance > BreakerEnemyBar::AlwaysDistance) continue;
        const FVector Projected = Project(Dummy->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), false);
        if (Projected.Z <= 0.0f) continue;

        const float BodyWidthPixels = BreakerEnemyBarBodyWidthPixels(Projected, CameraRight,
            Dummy->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
            [this](const FVector& P) { return Project(P, false); });
        const FBreakerEnemyBarRect Bar = BreakerEnemyBarPlace(Projected, Distance, ScaleUnit, BodyWidthPixels);
        // Full opacity: a dummy never fades, and Alpha(C, 1.0f) is exactly C.
        // UNSEGMENTED, and that is the same ruling as the dummy's paint: a
        // band is a fight beat, a dummy is an instrument, and dividing its bar
        // into four would be the gym telling the player something about the
        // gym. It shares the body so the bar stays one implementation; it
        // passes 1 because it has no rank to ask about.
        BreakerEnemyBarDrawBody(*this, Bar, Health, MaxHealth,
            DummyAttributes->GetShield(), DummyAttributes->GetMaxShield(), ScaleUnit, 1.0f, 1);
        DrawSpecTextCentered(Dummy->GetProfileLabel(), Projected.X, Bar.Y + Bar.H + S(3.0f),
            BreakerUI::TextMuted, 10.0f * Bar.Scale);
    }
}
