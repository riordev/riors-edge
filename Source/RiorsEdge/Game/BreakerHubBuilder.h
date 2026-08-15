#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerHubBuilder.generated.h"

class ABreakerTravelPoint;

// Builds the MMO hangout hub: the owner's brief is "a hub area (all players
// essentially hangout here the mmo aspect) with the vendors and a way to
// start the main story quest / players should then be able to have an
// interactable to take them to other maps, quests, missions".
//
// Follows the same zero-setup construction idiom as
// ABreakerGameMode::SpawnAnchorCamp / SpawnSafeZone: runtime-spawned engine
// primitives (cube/cylinder/sphere basic shapes with a dynamic material
// instance for color), zero editor content, everything parameterised off a
// frame built from an origin rather than hardcoded world coordinates. Unlike
// the gym field this builder does not own a Frame member on an actor — it is
// a pure static entry point, so the frame is a local struct built fresh from
// the FTransform argument each call.
UCLASS()
class RIORSEDGE_API UBreakerHubBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Spawns the full hub (plaza, boundary, vendors, travel point) at
    // HubOrigin and returns the travel point actor so the caller (the game
    // mode, out of this file's territory) can bind ABreakerTravelPoint's
    // OnDestinationSelected delegate. HubOrigin's location is the plaza
    // centre on the ground; its rotation's forward vector is the hub's
    // "front" the way BuildFieldFrame's Pawn forward is the gym's front.
    // Returns nullptr if World is null — mirrors every Spawn* function in
    // BreakerGameMode.cpp, which all early-out the same way.
    UFUNCTION(BlueprintCallable, Category="Hub")
    static ABreakerTravelPoint* BuildHub(UWorld* World, const FTransform& HubOrigin);

private:
    // Split out for testability of the shape even though it spawns actors
    // (a world is still required — see BreakerHubTests.cpp for what is and
    // is not covered without one).
    static void BuildPlazaAndBoundary(UWorld* World, const struct FBreakerHubFrame& Frame);
    static void BuildVendors(UWorld* World, const struct FBreakerHubFrame& Frame);
    static ABreakerTravelPoint* BuildTravelPoint(UWorld* World, const struct FBreakerHubFrame& Frame);
};
