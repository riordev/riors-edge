#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Game/BreakerArrivalMath.h"
#include "Game/BreakerRiftDefinition.h"
#include "Containers/Ticker.h"
#include "BreakerGameInstance.generated.h"

class SWidget;

// ---------------------------------------------------------------------------
// WHAT SURVIVES A LEVEL LOAD
// ---------------------------------------------------------------------------
// The project ran in ONE map, so the title menu, the hub and the gym were the
// same level with a widget over the top — which is why "loading in still takes
// you to the game": the gym was always already built and ticking underneath
// the front end, and the hub was six thousand centimetres away in the same
// world rather than a separate place.
//
// Splitting them into three maps introduces a problem one map never had:
// OpenLevel destroys every actor, including the pawn that knows which
// character is being played. The GameInstance is the only object that outlives
// a level transition, so it is where that survives.
//
// Deliberately NOT a second save system. This carries the ACTIVE CHARACTER ID
// and nothing else; the character's data still lives in its own save slot and
// is loaded from disk on arrival. Anything else stored here would be a second
// source of truth for state the save file already owns.
// ---------------------------------------------------------------------------
UCLASS()
class RIORSEDGE_API UBreakerGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    // Which character the player picked on the front end. Invalid means "no
    // character chosen" — a capture run, a PIE drop-in, or the front end
    // itself — and every consumer treats that as "use the legacy single slot",
    // which is what keeps those paths working exactly as they did.
    UPROPERTY(BlueprintReadWrite, Category="Breaker|Session") FGuid ActiveCharacterId;

    // Where the player is headed. Read once on arrival so the game mode knows
    // whether it is building a hub or a gym, then left alone.
    UPROPERTY(BlueprintReadWrite, Category="Breaker|Session") FName PendingDestinationId;

    // The rift being travelled to — the data model the loading screen wears
    // (see BreakerRiftDefinition.h). TRANSIENT TRAVEL STATE like the two
    // fields above, not a second save system: set at the door, read by the
    // destination's build, gone with the session. Unset (AreaLevel 0) keeps
    // every legacy path on the game mode's GymAreaLevel dev fallback.
    UPROPERTY(BlueprintReadWrite, Category="Breaker|Session") FBreakerRiftDefinition PendingRift;

    // THE DEATH BUDGET'S COUNTER (O82). Here and not on the game mode because
    // RETRY THE RIFT is a level travel that constructs a new game mode, and a
    // budget that reset on every retry would be no budget. Seeded to the solo
    // allowance at the rift door, spent by ABreakerGameMode::SpendDeath, reset
    // beside PendingRift by an ordinary travel. Read by nothing in a campaign
    // rift: the tier decides whether it means anything
    // (Game/BreakerDeathBudgetMath.h).
    UPROPERTY(BlueprintReadWrite, Category="Breaker|Session")
    int32 EndgameDeathsRemaining = UBreakerRiftLibrary::SoloEndgameDeathBudget;

    // THE DEPLOYMENT BEAT (route ruled by the owner; O120 and O123 govern the
    // pane). Not a loading screen — a loading screen hides a wait, and these
    // maps load in fractions of a second; this is a BRIEFING that names where
    // the player is arriving, and it replaced the MoviePlayer plate outright:
    // one mechanism, not two. It lives on the game WINDOW's overlay — the
    // surface MoviePlayer itself used, which survives LoadMap and exists in
    // PIE, where the engine's IsMoviePlayerEnabled() (!GIsEditor) makes a
    // MoviePlayer screen structurally impossible. The animated time sits
    // AROUND the blocking OpenLevel: an animated deploy hold before, a frozen
    // frame during, an arrival beat after.
    //
    // EVERY TRAVEL ARRIVES UNDER COVER. A travel with a set PendingRift wears
    // the briefing; every other travel wears a plain black. Either cover
    // lifts through the same gated reveal (BreakerArrivalMath.h): the
    // destination's first rendered frames are cold Lumen and a re-adapting
    // exposure, and the cover stays until both the frame and the seconds
    // gate are met, then fades. Only a headless run — no game window to put
    // a cover on — travels bare.
    virtual void Init() override;
    virtual void Shutdown() override;

    // The gate's one source: the runtime reads it every frame of a reveal,
    // the suite asserts it. Owner-tunable live through the Breaker.Arrive*
    // console variables, which bind to the same struct.
    static const FBreakerArrivalHold& ShippedArrivalHold();

    // The cover as a seam for beats that are not travels. HoldBlack puts the
    // plain black up if no cover is up; ReleaseBlack starts the gated reveal
    // from this moment. The death beat's black is meant to call into these.
    void HoldBlack();
    void ReleaseBlack();
    // True while any cover is on the window, opaque or mid-fade. The capture
    // harness's -BreakerCaptureArrival waits on this rather than on a clock.
    bool IsArrivalCoverUp() const { return Cover.IsValid(); }

private:
    void HandlePostLoadMap(UWorld* LoadedWorld);
    // The travel's steps: cover, hold (briefing only), load, reveal.
    void BeginTravel(FName MapName);
    // Puts a cover on the game window. Returns false with no window to put it
    // on. Removes any cover already up first, so a cover never stacks.
    bool AddCover(TSharedRef<SWidget> Widget);
    // The plain black: an opaque SBorder over the whole window.
    static TSharedRef<SWidget> MakeBlackCover();
    // Starts the per-frame ticker that reads the reveal alpha onto the
    // cover's opacity and calls RevealWorld at 1. Never adds twice.
    void BeginReveal();
    // The cover leaves, in whatever state it is in.
    void RevealWorld();
    // The cover on the window: the briefing pane or the plain black.
    TSharedPtr<SWidget> Cover;
    // Set only while the cover is the briefing pane, for the stage line.
    TSharedPtr<class SBreakerLoadingScreen> DeployScreen;
    TWeakPtr<class SWindow> CoverWindow;
    FTSTicker::FDelegateHandle RevealTicker;
    // Bounds the wait for PostLoadMapWithWorld: a travel the engine refuses
    // (a bad map name in PIE stays in the current world and broadcasts
    // nothing) must not leave an opaque cover on the window. Armed when a
    // cover goes up for a travel, disarmed by the post-load delegate.
    FTSTicker::FDelegateHandle CoverWatchdog;
    void ArmCoverWatchdog();
    void DisarmCoverWatchdog();
    // A travel is in the deploy hold and its OpenLevel is scheduled.
    bool bTravelPending = false;
    // False from the door until PostLoadMapWithWorld; a reveal cannot start
    // against a world that is not there.
    bool bWorldReady = true;
    double ReadyTimeSeconds = 0.0;
    uint64 ReadyFrame = 0;

public:

    // ---- Map identity ---------------------------------------------------
    // The map names are the contract between the level assets and the code.
    // Kept here rather than in the game mode because both the game mode and
    // the travel path need them, and a second copy of a string that must match
    // an asset path is exactly how a rename becomes a silent no-op.
    //
    // RENAMING A MAP ASSET WITHOUT EDITING THE STRINGS BELOW FAILS SILENTLY.
    // There is no compile error and no log line: the renamed map simply stops
    // matching its name here, and IsGymMap's "none of the named maps" fallback
    // swallows it, so the map becomes the gym. A renamed Lvl_Anchor builds a
    // gym field in the hub and nothing anywhere says why. These strings and
    // the .umap short names are one contract.
    static const TCHAR* FrontEndMapName() { return TEXT("Lvl_FrontEnd"); }
    static const TCHAR* AnchorMapName()   { return TEXT("Lvl_Anchor"); }
    static const TCHAR* GymMapName()      { return TEXT("Lvl_Gym"); }
    static const TCHAR* FernhallMapName() { return TEXT("Lvl_Fernhall"); }

    // What a map is FOR. The game mode is shared across all of them, so it
    // asks this rather than carrying a subclass per map — the alternative is
    // near-identical game modes and a config entry per map to keep in sync.
    UFUNCTION(BlueprintPure, Category="Breaker|Session")
    static bool IsFrontEndMap(const UObject* WorldContext);
    UFUNCTION(BlueprintPure, Category="Breaker|Session")
    static bool IsAnchorMap(const UObject* WorldContext);
    UFUNCTION(BlueprintPure, Category="Breaker|Session")
    static bool IsFernhallMap(const UObject* WorldContext);
    UFUNCTION(BlueprintPure, Category="Breaker|Session")
    static bool IsGymMap(const UObject* WorldContext);
    // The fallback rule itself, name-in bool-out so the suite can hold the
    // exclusion list: a named map that this returns true for is a map that
    // silently fills with the gym's targets and boss key.
    static bool IsGymMapName(const FString& Name);

    // Travels, carrying the active character across the load.
    UFUNCTION(BlueprintCallable, Category="Breaker|Session")
    static void TravelTo(const UObject* WorldContext, FName MapName);
};
