#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerWorldBasics.generated.h"

// The world's baseline conditions: light to see by and ground to stand on.
//
// The three shipped maps (Lvl_FrontEnd / Lvl_Anchor / Lvl_Gym) are empty
// shells — no PlayerStart, no directional or sky light, no floor. Everything
// the player sees in them is runtime-built (BreakerHubBuilder, the gym's
// Spawn* family), and this library extends that same author-nothing idiom to
// the two things those builders assumed the map provided.
//
// A6 (Docs/Design/Decisions.md pending): whether lighting is ultimately
// runtime-built or asset-authored is an OWNER decision. This library is the
// runtime answer, and it deliberately yields: an authored ADirectionalLight
// in a map suppresses the whole rig, so the day a lit map is authored this
// code stops doing anything in it, with no edit here.
UCLASS()
class RIORSEDGE_API UBreakerWorldBasics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Spawns a sun (directional light), a sky atmosphere and a real-time
    // captured sky light if — and only if — the world has no authored
    // directional light. Idempotent per world by that same check.
    UFUNCTION(BlueprintCallable, Category="World")
    static void EnsureWorldLighting(UWorld* World);

    // A ground plate under FloorTopCenter (its top surface AT that point).
    // Only the front end needs this: the Anchor builds a plaza and the gym
    // builds an apron, but the front end deliberately builds nothing
    // (BreakerGameMode's front-end branch), which left the pawn falling
    // through an empty map underneath the title menu.
    UFUNCTION(BlueprintCallable, Category="World")
    static void EnsureBootFloor(UWorld* World, const FVector& FloorTopCenter);
};
