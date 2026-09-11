#pragma once

#include "CoreMinimal.h"
#include "Game/BreakerRiftDefinition.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class SBox;
class STextBlock;

// ---------------------------------------------------------------------------
// THE DEPLOYMENT BRIEFING — everything the pane prints, composed ONCE.
//
// Not a loading screen. A loading screen hides a wait; this is a BRIEFING —
// an area name, a line of fiction, the area level, the item-level range, two
// monster multipliers and the death allowance (O123: always present, only
// the value moves). It is a struct rather than widget-side reads so the
// composition is world-free and a test can hold it against the libraries it
// derives from: every number here is DERIVED through the game's own
// functions, never transcribed, so the pane can never disagree with what the
// destination actually spawns.
// ---------------------------------------------------------------------------
struct RIORSEDGE_API FBreakerDeploymentBriefing
{
    FText AreaName;
    FText AreaLine;
    FString TierKicker;
    int32 AreaLevel = 1;
    int32 ItemLevelMin = 1;
    int32 ItemLevelMax = 1;
    float HealthMultiplier = 1.0f;
    float DamageMultiplier = 1.0f;
    FString DeathAllowance;
    // HOW READY THE PLAYER IS, and it is here because a briefing that states
    // the area's level without stating yours is only half a briefing.
    //
    // Owner: "i cant make it to the end of the rift ... my base character is
    // just so weak". Measured (RiorsEdge.Combat.PowerCurve.UnderLevelled): on
    // -level gear kills a trash body in 16.9 shots at EVERY area level — the
    // curves cancel exactly — and starter gear needs 87 in the Breach. He was
    // not weak everywhere; he was carrying item level 1 into area level 20,
    // and the door told him nothing. Entry to the Breach is gated on two story
    // flags and NOTHING else.
    //
    // A LINE, NOT A LOCK. Refusing entry would strand a campaign that sends
    // the player there; what was missing is the player being able to see it
    // coming.
    int32 PlayerItemLevel = 0;
    FString ReadinessLine;
};

// ---------------------------------------------------------------------------
// SBreakerRiftLattice — the card's pulse: the 7-cell lattice, the stage line
// with its cursor block, and the orange crawl, all driven by ONE active timer
// that stops with the widget. The lattice and stage line are this widget's
// own content; the crawl is built here too (the timer moves its fill) but is
// handed to the caller through GetCrawl() because it belongs on the card's
// bottom edge, full bleed, not in the content column. Shared by the
// deployment card and its twin, the rift debrief, so the two beats breathe
// identically; every animation is indeterminate by design — O120: progress is
// never a percentage.
// ---------------------------------------------------------------------------
class RIORSEDGE_API SBreakerRiftLattice : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBreakerRiftLattice) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // The stage line in words is the honest signal (O120). The caller moves
    // it: the deploy words while the beat holds, the arrival words on the far
    // side of the load, the closing words on the debrief.
    void SetStage(const FText& StageWords);

    // The crawl, for the card's bottom slot. Built in Construct, so this is
    // always valid after it.
    TSharedRef<SWidget> GetCrawl() const { return Crawl.ToSharedRef(); }

private:
    // One active timer drives the lattice, the cursor blink and the crawl —
    // imperative writes on a clock, never paint-time attributes, exactly the
    // menu's rule. It stops with the widget.
    EActiveTimerReturnType Animate(double CurrentTime, float DeltaTime);

    TSharedPtr<STextBlock> StageText;
    TSharedPtr<SBorder> BlinkBlock;
    TArray<TSharedPtr<SBorder>> LatticeCells;
    TSharedPtr<SBox> CrawlFill;
    TSharedPtr<SWidget> Crawl;
    float CrawlFillWidth = 422.0f;
    double StartSeconds = 0.0;
};

// ---------------------------------------------------------------------------
// SBreakerLoadingScreen — the deployment beat's pane, drawn to the pack's
// loading spec (README-UE5.txt) on the tokens' 64px margin, every field live
// from the briefing. Shown on the game WINDOW's overlay by
// UBreakerGameInstance around a travel: it animates while Slate ticks (the
// deploy hold and the arrival beat), freezes honestly for the blocking
// OpenLevel between them, and both animations are indeterminate by design —
// O120: loading progress is never a percentage.
//
// THE CARD IS A KIT. The chrome (MakeCardFrame), the headline block, the
// gold-railed side block and the stat row are static builders on this class
// so the rift debrief — the briefing's twin — is drawn from the same pieces
// at the same sizes rather than from a copy of them. Construct is a caller of
// the same builders: what the debrief renders is what the deployment renders.
// ---------------------------------------------------------------------------
class RIORSEDGE_API SBreakerLoadingScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBreakerLoadingScreen) {}
        SLATE_ARGUMENT(FBreakerDeploymentBriefing, Briefing)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // The stage line in words is the honest signal (O120). The game instance
    // moves it: the deploy words while the beat holds, the arrival words on
    // the far side of the load.
    void SetStage(const FText& StageWords);

    // The card's chrome: opaque BgBase, the 2px top rule, the insignia mark
    // and its caption in the corner, Content centred at the card's content
    // width, Crawl on the bottom edge. Static and world-free: the frame reads
    // nothing, it only places what it is handed.
    static TSharedRef<SWidget> MakeCardFrame(TSharedRef<SWidget> Content, TSharedRef<SWidget> Crawl);

    // The headline block: Name at the display scale, heavy, upper-cased and
    // wrapped at the name column's derived width, with Line under it at the
    // body scale.
    static TSharedRef<SWidget> MakeHeadline(const FString& Name, const FText& Line);

    // The side block: a gold rail on the block's edge, Body beside it.
    static TSharedRef<SWidget> MakeRailedBlock(TSharedRef<SWidget> Body);

    // The stat row: caption-over-value readouts split by 1px dividers, in the
    // order given.
    static TSharedRef<SWidget> MakeStatRow(const TArray<TPair<FString, FString>>& Stats);

    // The composer, static and world-free so the drift test can call it with
    // no widget: EliteBonus is the enemy's authored loot bonus (read from the
    // CDO by the caller, passed in per the rift library's own contract), and
    // EndgameDeathsRemaining feeds O123's readout — campaign ignores it.
    // PlayerItemLevel of 0 means "not known", and the readiness line is then
    // omitted rather than guessed — a briefing that invented a comparison
    // would be worse than one that made none.
    static FBreakerDeploymentBriefing MakeBriefing(const FBreakerRiftDefinition& Rift,
        int32 EliteBonus, int32 EndgameDeathsRemaining, int32 PlayerItemLevel = 0);

    // The pane must eat input: the world underneath is paused-or-loading, and
    // a click that fell through to a menu mid-beat would act on a screen the
    // player cannot see.
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override
    {
        return FReply::Handled();
    }
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override
    {
        return FReply::Handled();
    }

private:
    TSharedPtr<SBreakerRiftLattice> Lattice;
};
