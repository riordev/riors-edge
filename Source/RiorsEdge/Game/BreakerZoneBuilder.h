#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Game/BreakerCoverRegistry.h"
#include "BreakerZoneBuilder.generated.h"

class UWorld;

// One imported yard mesh, reduced to the two facts the builder and the
// grammar test both consume: its contract name and its bounds. The bounds are
// the imported asset's LOCAL bounds — the composer bakes every instance's
// world transform into its vertices, so local bounds ARE world placement and
// spawning the mesh at identity recovers the scene. That bake is checked at
// import (breaker_import_fernhall.py prints every origin) and again by the
// suite, which measures these same bounds through the cover grammar.
struct FBreakerZonePiece
{
    FString Name;
    FSoftObjectPath MeshPath;
    FVector Origin = FVector::ZeroVector;
    FVector Extent = FVector::ZeroVector;
};

// The three transforms the yard authors as marker meshes. All-or-nothing on
// purpose: a yard missing its rift marker is not a yard with no rift, it is a
// broken export, and the builder refuses it loudly rather than spawning a
// zone whose destination does not exist.
struct FBreakerZoneMarkers
{
    FVector PlayerStart = FVector::ZeroVector;
    FVector Rift = FVector::ZeroVector;
    FVector NPCContract = FVector::ZeroVector;
    bool bPlayerStart = false;
    bool bRift = false;
    bool bNPCContract = false;

    bool IsComplete() const { return bPlayerStart && bRift && bNPCContract; }
};

// Builds the Fernhall approach yard — the campaign's first authored zone —
// from the split meshes breaker_import_fernhall.py produced. Same zero-setup
// idiom as UBreakerHubBuilder, with one difference that is the whole point:
// the hub's geometry is authored in C++ as primitive spawns, the yard's is
// authored in Scripts/compose_fernhall.py as a scene, and this builder is
// only the assembly loop. The name prefix is the contract (see the composer's
// header): blk_full_/blk_chest_ are measured cover, wall_ is the unmeasured
// boundary, flr_ is ground, dress_ is collisionless dressing, marker_ is
// consumed as a transform and never spawned.
UCLASS()
class RIORSEDGE_API UBreakerZoneBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static const TCHAR* FernhallMeshFolder() { return TEXT("/Game/Breaker/Meshes/fernhall_yard"); }

    // Asset-registry sweep of one zone folder into the piece list. The single
    // source both the spawner and the suite's grammar test read, so what the
    // game assembles and what the suite measures cannot drift apart. False —
    // with the reason logged — when the folder is empty or a mesh fails to
    // load; an empty zone folder means the import step never ran, and building
    // an empty world silently would bury that.
    static bool CollectZonePieces(const FString& MeshFolder, TArray<FBreakerZonePiece>& OutPieces);

    static bool ExtractMarkers(const TArray<FBreakerZonePiece>& Pieces, FBreakerZoneMarkers& OutMarkers);

    // The yard's cover, in the field frame the grammar speaks: origin at the
    // player-start marker, forward toward the rift marker. Pure math over the
    // piece list — no world — which is what lets the suite run it headless.
    // Class comes from the name prefix; each piece is its own cluster, because
    // the yard's line breaks are authored standalone rather than generated in
    // cluster rings.
    static TArray<FBreakerCoverPiece> BuildCoverPieces(const TArray<FBreakerZonePiece>& Pieces,
        const FBreakerZoneMarkers& Markers);

    // The grammar params the yard is measured against. Band and exclusions are
    // the yard's own (combat band 14-89 m out from the start marker, entry
    // plaza as the safe zone, the central dash lane as the corridor); every
    // gym-specific exclusion is parked out of range rather than zeroed, so a
    // future copy-paste cannot inherit a gym rectangle that happens to overlap
    // this yard. All O2 PLACEHOLDER.
    static FBreakerCoverFieldParams FernhallFieldParams();

    // Assembles the yard: spawns every non-marker piece at identity, returns
    // the marker transforms for the game mode to place the player, the rift
    // site and the contract NPC. Fails loudly and spawns nothing on a missing
    // folder or an incomplete marker set.
    static bool BuildFernhallYard(UWorld* World, FBreakerZoneMarkers& OutMarkers);
};
