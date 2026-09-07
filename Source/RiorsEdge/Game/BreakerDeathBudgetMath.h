#pragma once

#include "CoreMinimal.h"
#include "Game/BreakerRiftDefinition.h"

// ---------------------------------------------------------------------------
// THE DEATH BUDGET (O82), as world-free maths.
//
// The rule in one place: the budget is endgame-only, a solo character has
// SoloEndgameDeathBudget deaths in an endgame instance, campaign respawn is
// unlimited, and a death inside a live boss encounter resets the encounter
// rather than spending anything. The game mode is the thin caller that reads
// the tier and the boss off the world and writes the counter back onto the
// game instance; the character asks Model() what the death screen says. No
// world here, so RiorsEdge.Game.DeathBudget proves the whole rule without
// dying.
//
// Every string below is the death sheet's own
// (Assets/design/04-death-banners/04-death-and-banners.html) or O82's; the
// separators and the empty-name headlines are O2 PLACEHOLDER.
// ---------------------------------------------------------------------------

// What the death screen draws. Built by BreakerDeathBudget::Model, read by
// SBreakerMenu::BuildDeathScreen, which decides nothing itself.
struct FBreakerDeathScreenModel
{
    // Display 40, uppercase.
    FString Headline;
    // Body 14: the run's position, the budget, the consequence.
    FString Line2;
    // RETRY THE RIFT is drawn and focused. False is the terminal variant:
    // RETURN TO ANCHOR alone.
    bool bRetry = true;
    // The tally's numerals and cells. Meaningful only when bShowTally.
    int32 DeathsRemaining = 0;
    int32 DeathBudget = 0;
    // The tally is drawn only for a budgeted tier: a campaign death has no
    // count to show and never draws one.
    bool bShowTally = false;
};

// Where the death happened, for the two lines. Everything optional: an
// unset piece is omitted from the line rather than faked.
struct FBreakerDeathScreenSite
{
    // The rift's authored name. Empty prints the headline without a place.
    FString AreaName;
    // The wave the death fell on and the run's boss wave; 0 omits the piece.
    int32 Wave = 0;
    int32 WaveTotal = 0;
};

namespace BreakerDeathBudget
{
    // Only an endgame instance carries a budget (O82).
    inline bool IsBudgeted(EBreakerRiftTier Tier)
    {
        return Tier == EBreakerRiftTier::Endgame;
    }

    // The counter after one death. Campaign spends nothing; a death inside a
    // live boss encounter spends nothing at any tier (the encounter resets
    // instead); an endgame death spends one and never goes below zero.
    inline int32 SpendDeath(EBreakerRiftTier Tier, int32 Remaining, bool bBossAlive)
    {
        if (!IsBudgeted(Tier) || bBossAlive) return Remaining;
        return FMath::Max(Remaining - 1, 0);
    }

    // Whether RETRY THE RIFT is offered after this death. Campaign always;
    // endgame while the budget holds.
    inline bool CanRetryRift(EBreakerRiftTier Tier, int32 Remaining)
    {
        return !IsBudgeted(Tier) || Remaining > 0;
    }

    // The sheet's own strings.
    inline const TCHAR* BossLine() { return TEXT("The encounter resets."); }
    inline const TCHAR* GearKeptLine() { return TEXT("gear kept"); }
    inline const TCHAR* RiftLostLine() { return TEXT("the rift is lost"); }
    inline const TCHAR* PieceSeparator() { return TEXT(" · "); }   // O2 PLACEHOLDER (the sheet's middle dot)

    // The death screen, decided here and not in Slate: the tally, the
    // terminal RETURN-alone variant and the boss line. Remaining is the
    // counter AFTER the spend — the screen states what is left, never what
    // was.
    inline FBreakerDeathScreenModel Model(EBreakerRiftTier Tier, int32 Remaining, int32 Budget, bool bBossAlive,
        const FBreakerDeathScreenSite& Site = FBreakerDeathScreenSite())
    {
        FBreakerDeathScreenModel M;
        M.bShowTally = IsBudgeted(Tier);
        M.DeathsRemaining = FMath::Clamp(Remaining, 0, FMath::Max(Budget, 0));
        M.DeathBudget = FMath::Max(Budget, 0);
        M.bRetry = CanRetryRift(Tier, Remaining);

        // Line 1 changes verb on the terminal variant (sheet: "Erased at
        // <place>" / "<place> closes"). The empty-name forms are O2
        // PLACEHOLDER; every rift today carries a name.
        if (M.bRetry)
        {
            M.Headline = Site.AreaName.IsEmpty()
                ? FString(TEXT("ERASED"))
                : FString::Printf(TEXT("ERASED AT %s"), *Site.AreaName.ToUpper());
        }
        else
        {
            M.Headline = Site.AreaName.IsEmpty()
                ? FString(TEXT("THE RIFT CLOSES"))
                : FString::Printf(TEXT("%s CLOSES"), *Site.AreaName.ToUpper());
        }

        // Line 2, left to right: the wave, the budget, the consequence. A
        // boss death ends on the boss line; a spent budget ends on the loss;
        // every other death ends on the gear.
        TArray<FString> Pieces;
        if (Site.Wave > 0 && Site.WaveTotal > 0)
        {
            Pieces.Add(FString::Printf(TEXT("Wave %d of %d"), Site.Wave, Site.WaveTotal));
        }
        if (M.bShowTally)
        {
            Pieces.Add(FString::Printf(TEXT("%d of %d deaths remain"), M.DeathsRemaining, M.DeathBudget));
        }
        if (bBossAlive)
        {
            Pieces.Add(BossLine());
        }
        else if (!M.bRetry)
        {
            Pieces.Add(RiftLostLine());
            Pieces.Add(GearKeptLine());
        }
        else
        {
            Pieces.Add(GearKeptLine());
        }
        M.Line2 = FString::Join(Pieces, PieceSeparator());
        return M;
    }
}
