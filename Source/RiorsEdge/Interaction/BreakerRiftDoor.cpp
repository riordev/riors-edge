#include "Interaction/BreakerRiftDoor.h"

ABreakerRiftDoor::ABreakerRiftDoor()
{
    // The prompt says what happens, not what kind of object this is. The base
    // point says "Travel" because it offers a list; a door offers one thing
    // and can name it.
    PromptLabel = FText::FromString(TEXT("Enter Rift"));

    // A door has no place to exclude. ExcludedDestinationId answers "do not
    // offer where I already stand", and the rift is not somewhere the player
    // is standing when they are in front of it — they are in Fernhall.
    ExcludedDestinationId = NAME_None;
}

TArray<FBreakerTravelDestination> ABreakerRiftDoor::GetAvailableDestinations() const
{
    TArray<FBreakerTravelDestination> Available;
    for (const FBreakerTravelDestination& Destination : GetFallbackRegistry())
    {
        if (Destination.bEnabled && Destination.bDoorOnly)
        {
            Available.Add(Destination);
        }
    }
    return Available;
}

bool ABreakerRiftDoor::SelectDestination(FName DestinationId, APawn* RequestingPawn)
{
    if (DestinationId == ABreakerTravelPoint::RiftDestinationId)
    {
        // Validated against the same registry every other id is, so a stale
        // selection — one made before a destination was disabled — fails
        // closed here exactly as it does in the base.
        FBreakerTravelDestination Destination;
        if (!FindDestination(DestinationId, Destination) || !Destination.bEnabled)
        {
            return false;
        }
        // O122: entry is FREE. There is deliberately no condition between this
        // line and the broadcast — no key, no cost, no quest flag. If one ever
        // appears it belongs to endgame rifts and it arrives with the
        // consumable half, not as a quiet addition here.
        OnRiftEntryRequested.Broadcast(Rift, RequestingPawn);
        return true;
    }
    return Super::SelectDestination(DestinationId, RequestingPawn);
}
