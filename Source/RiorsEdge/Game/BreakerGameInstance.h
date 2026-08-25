#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Game/BreakerRiftDefinition.h"
#include "BreakerGameInstance.generated.h"

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

    // The travel loading screen: the flat Fieldplate plate, shown over every
    // Anchor/Gym load. The widget rebuild (live fields) comes later; the
    // plate alone replaces a black hitch with a place.
    virtual void Init() override;

private:
    void HandlePreLoadMap(const FString& MapName);
    // The plate texture, loaded from the raw PNG at first travel and kept
    // for the session. Runtime-loaded rather than an imported .uasset: no
    // DXT (the flat panels band), no mips, sRGB — exactly the pack's import
    // demands, satisfied by construction.
    UPROPERTY(Transient) TObjectPtr<class UTexture2D> LoadingPlateTexture;
    TSharedPtr<struct FSlateBrush> LoadingPlateBrush;

public:

    // ---- Map identity ---------------------------------------------------
    // The map names are the contract between the level assets and the code.
    // Kept here rather than in the game mode because both the game mode and
    // the travel path need them, and a second copy of a string that must match
    // an asset path is exactly how a rename becomes a silent no-op.
    //
    // RENAMING A MAP ASSET WITHOUT EDITING THE THREE STRINGS BELOW FAILS
    // SILENTLY. There is no compile error and no log line: the renamed map
    // simply stops matching FrontEnd or Anchor, and IsGymMap's "neither of the
    // other two" fallback swallows it, so the map becomes the gym. A renamed
    // Lvl_Anchor builds a gym field in the hub and nothing anywhere says why.
    // These three strings and the .umap short names are one contract.
    static const TCHAR* FrontEndMapName() { return TEXT("Lvl_FrontEnd"); }
    static const TCHAR* AnchorMapName()   { return TEXT("Lvl_Anchor"); }
    static const TCHAR* GymMapName()      { return TEXT("Lvl_Gym"); }

    // What a map is FOR. The game mode is shared across all three, so it asks
    // this rather than carrying three subclasses — the alternative is three
    // near-identical game modes and a config entry per map to keep in sync.
    UFUNCTION(BlueprintPure, Category="Breaker|Session")
    static bool IsFrontEndMap(const UObject* WorldContext);
    UFUNCTION(BlueprintPure, Category="Breaker|Session")
    static bool IsAnchorMap(const UObject* WorldContext);
    UFUNCTION(BlueprintPure, Category="Breaker|Session")
    static bool IsGymMap(const UObject* WorldContext);

    // Travels, carrying the active character across the load.
    UFUNCTION(BlueprintCallable, Category="Breaker|Session")
    static void TravelTo(const UObject* WorldContext, FName MapName);
};
