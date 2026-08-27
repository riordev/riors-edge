#pragma once

#include "CoreMinimal.h"
#include "Game/BreakerRiftDefinition.h"
#include "Interaction/BreakerTravelPoint.h"
#include "BreakerRiftDoor.generated.h"

// THE RIFT DOOR — the marked site at the far end of the Fernhall yard, and
// the first thing in this project that turns FBreakerRiftDefinition from a
// data model into a place you can walk into.
//
// IT IS A TRAVEL POINT, not a new kind of interactable, and that is a
// deliberate reuse rather than a shortcut. Three things already say so:
//
//   1. ABreakerTravelPoint's own beacon is painted in the reserved rift-teal
//      because "teal is canon-reserved for rift objects, and travel IS the
//      rift verb". The door is the case that sentence was written about.
//   2. ABreakerCharacter finds interactables with a TActorIterator over
//      ABreakerTravelPoint and the menu drives the picker through a base
//      pointer, so a subclass reaches the F key and the travel screen with NO
//      change in Characters/ or UI/ — neither of which this lane owns. A
//      separate actor class would have needed a hook in both.
//   3. Entering a rift IS a level load carrying session state, which is
//      exactly what the travel path already does.
//
// WHAT MAKES IT A DOOR RATHER THAN A GATE is the destination filter. The
// Local Rift is registered bDoorOnly, so no general travel point offers it;
// this actor overrides GetAvailableDestinations to offer it and nothing else.
// Walking up to the rift asks one question, and it is not "where would you
// like to go".
//
// O122 GOVERNS ENTRY: a campaign rift is entered FREELY. No key, no cost, no
// condition — so this actor refuses nothing and holds no entry gate. The
// consumable half belongs to endgame rifts and is not represented here; a
// gate on a free instance would be a loading screen with a lock drawn on it.
//
// The door does NOT travel. It hands its authored definition to whoever binds
// OnRiftEntryRequested, in the same division of labour ABreakerTravelPoint
// already keeps: this actor knows WHICH rift it fronts, and the game mode
// knows what a rift's interior currently is.
DECLARE_MULTICAST_DELEGATE_TwoParams(FBreakerRiftEntryRequested,
    const FBreakerRiftDefinition& /*Rift*/, APawn* /*RequestingPawn*/);

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerRiftDoor : public ABreakerTravelPoint
{
    GENERATED_BODY()

public:
    ABreakerRiftDoor();

    // The rift this door fronts. Authored by whoever places the door — the
    // Fernhall build fills it from the zone's own definition — because a door
    // that does not know which rift it opens would leave PendingRift unset and
    // the destination would silently fall back to the dev area level.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rift")
    FBreakerRiftDefinition Rift;

    // Exactly the door-only rift entries, and nothing else. The base class
    // filters bDoorOnly OUT; this is the one actor that filters it IN, which
    // is what keeps "the rift is reached by walking to the rift" true.
    virtual TArray<FBreakerTravelDestination> GetAvailableDestinations() const override;

    // Raises OnRiftEntryRequested for the rift id and defers everything else
    // to the base, so a door is still a legal travel point if a future layout
    // ever gives one an ordinary destination.
    virtual bool SelectDestination(FName DestinationId, APawn* RequestingPawn) override;

    // A DOOR NAMES THE PLACE IT OPENS, not its own kind. "Fernhall Substation"
    // over "F ENTER RIFT" is the NPC idiom; "TRAVEL" over "F ENTER RIFT" was
    // one object saying two things about itself.
    virtual FText GetDisplayName() const override
    {
        return Rift.AreaName.IsEmpty() ? Super::GetDisplayName() : Rift.AreaName;
    }

    // THE WHOLE DIFFICULTY GAUGE IN ONE NUMBER. The required level derives from
    // item level rather than being stored (One-AA), so a rift's area level IS
    // what "level 23 content" means — there is no second figure to show and no
    // gate to read it against. Empty when unset, because a door with no rift on
    // it should say nothing rather than "AREA 0".
    virtual FText GetDisplayDetail() const override
    {
        return Rift.IsSet()
            ? FText::FromString(FString::Printf(TEXT("AREA %d"), Rift.EffectiveAreaLevel()))
            : FText::GetEmpty();
    }

    FBreakerRiftEntryRequested OnRiftEntryRequested;
};
