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
};

// ---------------------------------------------------------------------------
// SBreakerLoadingScreen — the deployment beat's pane, drawn to the pack's
// loading spec (README-UE5.txt) on the tokens' 64px margin, every field live
// from the briefing. Shown on the game WINDOW's overlay by
// UBreakerGameInstance around a travel: it animates while Slate ticks (the
// deploy hold and the arrival beat), freezes honestly for the blocking
// OpenLevel between them, and both animations are indeterminate by design —
// O120: loading progress is never a percentage.
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

    // The composer, static and world-free so the drift test can call it with
    // no widget: EliteBonus is the enemy's authored loot bonus (read from the
    // CDO by the caller, passed in per the rift library's own contract), and
    // EndgameDeathsRemaining feeds O123's readout — campaign ignores it.
    static FBreakerDeploymentBriefing MakeBriefing(const FBreakerRiftDefinition& Rift,
        int32 EliteBonus, int32 EndgameDeathsRemaining);

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
    // One active timer drives the lattice, the cursor blink and the crawl —
    // imperative writes on a clock, never paint-time attributes, exactly the
    // menu's rule. It stops with the widget.
    EActiveTimerReturnType Animate(double CurrentTime, float DeltaTime);

    TSharedPtr<STextBlock> StageText;
    TSharedPtr<SBorder> BlinkBlock;
    TArray<TSharedPtr<SBorder>> LatticeCells;
    TSharedPtr<SBox> CrawlFill;
    float CrawlFillWidth = 422.0f;
    double StartSeconds = 0.0;
};
