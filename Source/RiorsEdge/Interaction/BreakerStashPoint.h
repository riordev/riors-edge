#pragma once

#include "CoreMinimal.h"
#include "Interaction/BreakerTravelPoint.h"
#include "BreakerStashPoint.generated.h"

// THE STASH POINT — the Anchor interactable that opens the account's stash
// (Save/BreakerAccountSave.h, Part One-X's transfer point).
//
// IT IS A TRAVEL POINT for one reason only, and it is ABreakerRiftDoor's
// reason: ABreakerCharacter finds interactables with a TActorIterator over
// ABreakerTravelPoint and hands the nearest to SBreakerMenu::ShowTravel, so a
// subclass reaches the F key with NO change in Characters/. ShowTravel diverts
// a stash point to ShowStash before it touches the picker. Nothing else about
// travel applies: this actor offers ZERO destinations, refuses every
// SelectDestination, and never broadcasts OnDestinationSelected.
//
// The stash is an ANCHOR interaction (UBreakerEquipmentComponent refuses
// deposit and withdrawal with bAtAnchor false), and this actor is what makes
// that a place rather than a flag: it is spawned by the hub builder on the
// arrival side of the vendor crossbar and nowhere else. The screen still asks
// UBreakerGameInstance::IsAnchorMap for the gate — the actor's existence is
// not the rule, the map is.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerStashPoint : public ABreakerTravelPoint
{
    GENERATED_BODY()

public:
    ABreakerStashPoint();

    // Repaints the marker off the rift's teal. Teal is a NOUN — rift objects,
    // suppression hardware, top-rarity frames — and a stash is none of them.
    // The base BeginPlay paints hardware teal onto Visual and glows the
    // beacon; this runs after it and overwrites the paint. The beacon and
    // its light are hidden in the constructor, so the glow paints nothing.
    virtual void BeginPlay() override;

    // A stash goes nowhere. Empty rather than the base's filtered registry,
    // so no code path can build a destination card for it.
    virtual TArray<FBreakerTravelDestination> GetAvailableDestinations() const override;

    // Refused for every id: a stash point that accepted a travel request
    // would be a second gate wearing a stash label.
    virtual bool SelectDestination(FName DestinationId, APawn* RequestingPawn) override;

    // GetDisplayName is NOT overridden: the base returns the prompt noun, and
    // the HUD prints the noun line only when it differs from the prompt word,
    // so this point draws ONE line ("F STASH"), never STASH over F STASH.
};
