#include "Interaction/BreakerRiftDoor.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Save/BreakerQuestJournal.h"

bool ABreakerRiftDoor::CanEnterRift(const FBreakerRiftDefinition& Definition, const APawn* Pawn, FText& OutReason)
{
    OutReason = FText::GetEmpty();
    const auto* Character = Cast<ABreakerCharacter>(Pawn);
    if (!IsValid(Character) || !Character->GetCombat() || Character->GetCombat()->IsDead())
    {
        OutReason = NSLOCTEXT("Breaker", "RiftEntryLiving", "You must be alive to enter a Rift.");
        return false;
    }
    if (!Definition.IsSet())
    {
        OutReason = NSLOCTEXT("Breaker", "RiftEntryUnset", "This Rift is unavailable.");
        return false;
    }
    if (Definition.EncounterId == FName(TEXT("breach.marshalling")))
    {
        const auto* Journal = Character->GetQuestJournal();
        if (!Journal || !Journal->HasFlag(TEXT("Quest.AlteredContact.TurnedIn")))
        {
            OutReason = NSLOCTEXT("Breaker", "RiftEntryUniform", "Complete A DIFFERENT UNIFORM first.");
            return false;
        }
        if (!Journal->HasFlag(TEXT("Quest.Breach.Accepted")))
        {
            OutReason = NSLOCTEXT("Breaker", "RiftEntryBreach", "Accept THE BREACH first.");
            return false;
        }
    }
    return true;
}

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
        FText Reason;
        if (!CanEnterRift(Rift, RequestingPawn, Reason)) return false;
        // Validated against the same registry every other id is, so a stale
        // selection — one made before a destination was disabled — fails
        // closed here exactly as it does in the base.
        FBreakerTravelDestination Destination;
        if (!FindDestination(DestinationId, Destination) || !Destination.bEnabled)
        {
            return false;
        }
        // Campaign entry has no key or currency cost. The authored Breach
        // mission prerequisites are checked above before this request.
        OnRiftEntryRequested.Broadcast(Rift, RequestingPawn);
        return true;
    }
    return Super::SelectDestination(DestinationId, RequestingPawn);
}
