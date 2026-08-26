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

// WHAT A MARKER IS FOR. A role, a yard, and a place — the transforms a zone
// authors as marker meshes so the builder can put a player, a rift door or a
// mission giver somewhere without any of that being hardcoded in C++.
//
// THIS WAS THREE FIXED FIELDS AND IT COULD NOT DESCRIBE THE WORLD. One
// FVector per role and an all-or-nothing IsComplete meant a zone had exactly
// one player start, exactly one rift and exactly one NPC — so Fernhall
// growing into connected yards, each with its own door and its own giver,
// was not awkward to express, it was UNEXPRESSIBLE. The same assumption was
// hardcoded in three places (this struct, breaker_import_fernhall.py, and
// the piece-count test) and all three move together.
enum class EBreakerZoneMarkerRole : uint8
{
    PlayerStart,
    Rift,
    NPCContract,
    // THE YARD'S OWN ANCHOR (ruled, shape one). A zone has exactly ONE player
    // start, so the rule that anchors the entry yard's frame cannot anchor a
    // second yard \u2014 and a yard's grammar is measured in its own frame, because
    // FBreakerCoverPiece is field-space by construction.
    //
    // The alternatives were rejected on the record: two markers per yard
    // doubles the authoring for a second marker that means nothing alone, and
    // dropping frames for non-entry yards makes every yard's grammar depend on
    // how the composer happened to be rotated \u2014 the exact failure the derived
    // frame exists to prevent.
    //
    // A yard marker's ROTATION is not read (the composer bakes world transforms
    // into vertices and markers are placed axis-aligned), so a yard's forward
    // is derived the same way the entry yard's is: from what it points at.
    Yard,
};

// One authored marker. Yard is NAME_None for the ENTRY yard, which is what
// keeps every existing `marker_rift`-style name valid with no re-export: a
// name with no yard suffix belongs to the yard the player arrives in.
struct FBreakerZoneMarker
{
    EBreakerZoneMarkerRole Role = EBreakerZoneMarkerRole::PlayerStart;
    FName Yard = NAME_None;
    FVector Location = FVector::ZeroVector;
};

struct FBreakerZoneMarkers
{
    TArray<FBreakerZoneMarker> All;

    // Nullptr when absent, which is the point: a yard with no rift door is now
    // a legal yard rather than a broken export, so every caller has to say what
    // it does when there is none instead of reading a silent zero vector.
    const FBreakerZoneMarker* Find(EBreakerZoneMarkerRole Role, FName Yard = NAME_None) const;
    bool Has(EBreakerZoneMarkerRole Role, FName Yard = NAME_None) const { return Find(Role, Yard) != nullptr; }
    // Every marker of a role across every yard — how the builder spawns a door
    // per rift rather than one door per zone.
    TArray<FBreakerZoneMarker> OfRole(EBreakerZoneMarkerRole Role) const;

    // THE ONE HARD REQUIREMENT IS A PLAYER START, and exactly one of them: a
    // zone with none has nowhere to arrive, and a zone with two is an export
    // that silently picks. Everything else is optional and per-yard. The
    // second rule is that no (role, yard) pair may repeat — two rift doors in
    // one yard is not two doors, it is a naming mistake that would spawn one
    // on top of the other.
    //
    // THE THIRD RULE IS THE YARD ANCHOR: every named yard must carry a Yard
    // marker. A yard that a door or a giver names but nothing anchors has no
    // frame to be measured in, so its grammar would silently be measured in
    // the entry yard's — which is the failure the anchor exists to prevent,
    // arriving as a passing test rather than a missing one. The ENTRY yard is
    // exempt: the player start anchors it.
    bool IsComplete(FString& OutReason) const;

    // Every yard named by any marker, entry included (as NAME_None).
    TArray<FName> Yards() const;
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

    // THE NAME CONTRACT, parsed rather than matched against a fixed list:
    //   marker_<role>            — the entry yard
    //   marker_<role>_<yard>     — that yard
    // Roles are matched LONGEST FIRST, because `npc_contract` contains an
    // underscore and a shortest-match parse would read `marker_npc_contract`
    // as role `npc` in yard `contract`. False for a name that is not a marker
    // or whose role is unknown — an unknown role is refused rather than
    // guessed, so a typo in the composer is a loud failure and not a marker
    // that silently does not exist.
    static bool ParseMarkerName(const FString& Name, EBreakerZoneMarkerRole& OutRole, FName& OutYard);
    static const TCHAR* MarkerRoleName(EBreakerZoneMarkerRole Role);

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
