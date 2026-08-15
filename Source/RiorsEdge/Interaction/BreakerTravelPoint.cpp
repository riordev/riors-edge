#include "Interaction/BreakerTravelPoint.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

const FName ABreakerTravelPoint::GymDestinationId(TEXT("Gym"));
const FName ABreakerTravelPoint::HubDestinationId(TEXT("Hub"));

ABreakerTravelPoint::ABreakerTravelPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    Body = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Body"));
    Body->InitCapsuleSize(40.0f, 100.0f);
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(Body);

    // A distinct silhouette from ABreakerNPC's cube-and-sphere read — a
    // vertical marker (cylinder) rather than a person shape, so a player
    // scanning the hub can tell "thing to walk into" from "person to talk
    // to" before they are in range.
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetRelativeScale3D(FVector(0.5f, 0.5f, 2.0f));
    Visual->SetRelativeLocation(FVector(0.0f, 0.0f, -12.0f));
    if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
    {
        Visual->SetStaticMesh(Cylinder);
    }
}

TArray<FBreakerTravelDestination> ABreakerTravelPoint::GetAvailableDestinations() const
{
    TArray<FBreakerTravelDestination> Available;
    for (const FBreakerTravelDestination& Destination : GetFallbackRegistry())
    {
        // A travel point never offers the place it stands in. Without this the
        // hub's gate would list "The Anchor" and teleport the player half a
        // metre, which reads as a broken button rather than as a no-op.
        if (Destination.bEnabled && Destination.Id != ExcludedDestinationId)
        {
            Available.Add(Destination);
        }
    }
    return Available;
}

bool ABreakerTravelPoint::SelectDestination(FName DestinationId, APawn* RequestingPawn)
{
    FBreakerTravelDestination Destination;
    if (!FindDestination(DestinationId, Destination) || !Destination.bEnabled)
    {
        return false;
    }
    OnDestinationSelected.Broadcast(DestinationId, RequestingPawn);
    return true;
}

const TArray<FBreakerTravelDestination>& ABreakerTravelPoint::GetFallbackRegistry()
{
    static TArray<FBreakerTravelDestination> Registry;
    if (Registry.Num() > 0)
    {
        return Registry;
    }

    // THE ONLY DESTINATION. The owner's brief: "for now the only option in
    // that interactable should be the gym which is the same as it currently
    // is with the wave mechanics for play testing." This entry names a
    // place, nothing more — it carries no target transform, no travel logic.
    // Whoever binds OnDestinationSelected (the game mode, wired outside this
    // file's territory) owns deciding what "go to the gym" actually does,
    // which today is: nothing needs to change, because the gym field is
    // already built at HandleStartingNewPlayer exactly as before. See
    // BreakerTravelPoint.h's class comment and this module's test/report for
    // the reachability path.
    FBreakerTravelDestination Gym;
    Gym.Id = ABreakerTravelPoint::GymDestinationId;
    Gym.DisplayName = FText::FromString(TEXT("The Gym"));
    Gym.Description = TEXT("The wave-mode playtest field — safe pad, Anchor camp, elite arena, F1-F4.");
    Gym.bEnabled = true;

    // THE WAY BACK. Travel shipped one-way: the hub is where a session starts
    // and the gym was the only destination, so a player who travelled had no
    // route home and the hub's vendors and story start became unreachable for
    // the rest of the session. A destination the player can enter and not
    // leave is a trap, not a location.
    //
    // Both directions are ONE registry rather than a per-point list, and the
    // point filters by where it is — see ExcludedDestinationId. That keeps the
    // "what places exist" question answered in exactly one place.
    FBreakerTravelDestination Hub;
    Hub.Id = ABreakerTravelPoint::HubDestinationId;
    Hub.DisplayName = FText::FromString(TEXT("The Anchor"));
    Hub.Description = TEXT("The hub — vendors, the Forge Keeper, and the way into the story.");
    Hub.bEnabled = true;
    Registry.Add(Gym);
    Registry.Add(Hub);

    return Registry;
}

bool ABreakerTravelPoint::FindDestination(FName DestinationId, FBreakerTravelDestination& OutDestination)
{
    for (const FBreakerTravelDestination& Destination : GetFallbackRegistry())
    {
        if (Destination.Id == DestinationId)
        {
            OutDestination = Destination;
            return true;
        }
    }
    return false;
}
