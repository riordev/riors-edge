#include "Interaction/BreakerTravelPoint.h"
#include "Game/BreakerPrototypeDestinations.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Save/BreakerQuestJournal.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/BreakerGlowMaterial.h"
#include "UI/BreakerUIStyle.h"

const FName ABreakerTravelPoint::GymDestinationId(TEXT("Gym"));
const FName ABreakerTravelPoint::HubDestinationId(TEXT("Hub"));
const FName ABreakerTravelPoint::FernhallDestinationId(TEXT("Fernhall"));
const FName ABreakerTravelPoint::ErasedEarthDestinationId(TEXT("Earth.Unindustrialized"));
const FName ABreakerTravelPoint::StrippedEarthDestinationId(TEXT("Earth.Stripped"));
const FName ABreakerTravelPoint::WinningEarthDestinationId(TEXT("Earth.Won"));
const FName ABreakerTravelPoint::RiftDestinationId(TEXT("Rift.Local"));

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
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Cylinder)
    {
        Visual->SetStaticMesh(Cylinder);
    }

    // The beacon column: ~14 m of thin unlit teal rising out of the marker.
    // Tall enough to clear the boundary pillars (2.6-scale, ~2.6 m) many times
    // over, so it reads over every rooftop-height prop on the plaza from any
    // approach — the same "you can navigate by it" job Destiny's Tower beacons
    // do, in one primitive.
    Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
    Beacon->SetupAttachment(Body);
    Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Beacon->SetRelativeScale3D(FVector(0.18f, 0.18f, 14.0f));
    Beacon->SetRelativeLocation(FVector(0.0f, 0.0f, 640.0f));
    if (Cylinder)
    {
        Beacon->SetStaticMesh(Cylinder);
    }

    BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
    BeaconLight->SetupAttachment(Body);
    BeaconLight->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f));
    BeaconLight->SetLightColor(BreakerUI::TealHardware);
    BeaconLight->SetIntensity(2400.0f);
    BeaconLight->SetAttenuationRadius(1600.0f);
    BeaconLight->SetCastShadows(false);
}

void ABreakerTravelPoint::BeginPlay()
{
    Super::BeginPlay();

    // The marker body wears hardware teal as PAINT (lit, shaded), the column
    // wears Unwritten teal as LIGHT (unlit additive, MakeGlowMaterial): the
    // object is teal because it is a rift object, and the beacon glows because
    // it must survive distance, fog and shadow. Teal here is canon-legal —
    // the reserve exists exactly so that rift objects, and only rift objects,
    // get to spend it.
    if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Visual))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), BreakerUI::TealHardware);
            Visual->SetMaterial(0, Dynamic);
        }
    }
    if (UMaterialInstanceDynamic* GlowMaterial = BreakerUI::MakeGlowMaterial(Beacon))
    {
        // Intensity past 1.0 is what pushes the column into bloom; 4.0 reads
        // as a light column without whiting out the sky behind it.
        BreakerUI::SetGlowColor(GlowMaterial, BreakerUI::TealUnwritten, 4.0f);
    }
}

TArray<FBreakerTravelDestination> ABreakerTravelPoint::GetAvailableDestinations() const
{
    TArray<FBreakerTravelDestination> Available;
    const APawn* Player = GetWorld() && GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    for (const FBreakerTravelDestination& Destination : GetFallbackRegistry())
    {
        // A travel point never offers the place it stands in. Without this the
        // hub's gate would list "The Anchor" and teleport the player half a
        // metre, which reads as a broken button rather than as a no-op.
        //
        // And it never offers a DOOR-ONLY destination. The rift is entered
        // through the door standing in front of it, so a general gate listing
        // "Local Rift" would let a player walk into a rift from the Anchor
        // with no door having authored which rift it is — PendingRift unset,
        // and the destination silently falling back to the dev area level.
        if (Destination.bEnabled && !Destination.bDoorOnly && Destination.Id != ExcludedDestinationId)
        {
            if (Destination.Id == ErasedEarthDestinationId && !CanEnterErasedEarth(Player)) continue;
            if ((Destination.Id == StrippedEarthDestinationId || Destination.Id == WinningEarthDestinationId)
                && !CanEnterFinaleEarth(Destination.Id, Player)) continue;
            Available.Add(Destination);
        }
    }
    return Available;
}

bool ABreakerTravelPoint::SelectDestination(FName DestinationId, APawn* RequestingPawn)
{
    if (DestinationId == ErasedEarthDestinationId && !CanEnterErasedEarth(RequestingPawn)) return false;
    if ((DestinationId == StrippedEarthDestinationId || DestinationId == WinningEarthDestinationId)
        && !CanEnterFinaleEarth(DestinationId, RequestingPawn)) return false;
    FBreakerTravelDestination Destination;
    if (!FindDestination(DestinationId, Destination) || !Destination.bEnabled)
    {
        return false;
    }
    // A DOOR-ONLY DESTINATION IS REFUSED HERE TOO, not merely hidden. The
    // filter above keeps it out of the picker, which is what a player sees;
    // this is what stops the id being ACCEPTED if it ever reaches a general
    // point another way. Hiding a choice and refusing it are different
    // guarantees, and only the second one holds when the caller is not the UI.
    // ABreakerRiftDoor takes the rift id in its override before this runs.
    if (Destination.bDoorOnly)
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
    Hub.DisplayName = FText::FromString(TEXT("Anchor 13"));
    Hub.Description = TEXT("The hub — vendors, the Forge Keeper, and the way into the story.");
    Hub.bEnabled = true;
    // THE FIRST AUTHORED ZONE. The vertical slice's place: a kit-bashed
    // approach yard with the rift site at its far end. Named for the rift
    // definition it fronts (FBreakerRiftDefinition's Fernhall) so the travel
    // list and the deployment briefing speak the same name.
    FBreakerTravelDestination Fernhall;
    Fernhall.Id = ABreakerTravelPoint::FernhallDestinationId;
    Fernhall.DisplayName = FText::FromString(TEXT("Fernhall Approach"));
    Fernhall.Description = TEXT("The overgrown yard outside Fernhall — the First Contract, and the rift.");
    Fernhall.bEnabled = true;

    // THE LOCAL RIFT, and it is DOOR-ONLY (see FBreakerTravelDestination).
    // O122: a campaign rift is entered FREELY — no key, no cost, no gate — so
    // this carries no entry condition and the door refuses nothing. The
    // consumable half arrives with endgame rifts and is not represented here.
    //
    // It is in the registry so every id validates against one list, but no
    // general gate offers it: it is reached by walking to the door in
    // Fernhall, which is what makes it a place in the world rather than a
    // menu entry.
    FBreakerTravelDestination Rift;
    Rift.Id = ABreakerTravelPoint::RiftDestinationId;
    Rift.DisplayName = FText::FromString(TEXT("Enter the Rift"));
    Rift.Description = TEXT("The tear at the far end of the yard. Campaign rift — entered freely, unlimited respawn.");
    Rift.bEnabled = true;
    Rift.bDoorOnly = true;

    Registry.Add(Gym);
    Registry.Add(Hub);
    Registry.Add(Fernhall);
    Registry.Add(Rift);
    FBreakerTravelDestination Earth;
    Earth.Id = ErasedEarthDestinationId;
    Earth.DisplayName = FText::FromString(TEXT("The Quiet Earth"));
    Earth.Description = TEXT("A world of stone gardens and broken paths. Find the living signal and bring its survivor home.");
    Registry.Add(Earth);
    FBreakerTravelDestination Stripped;
    Stripped.Id = StrippedEarthDestinationId;
    Stripped.DisplayName = FText::FromString(TEXT("The Stripped Earth"));
    Stripped.Description = TEXT("A solved world, taken apart. Recover the working fragment from its civic systems.");
    Registry.Add(Stripped);
    FBreakerTravelDestination Won;
    Won.Id = WinningEarthDestinationId;
    Won.DisplayName = FText::FromString(TEXT("The Winning Earth"));
    Won.Description = TEXT("An intact world beyond Rior's reach. Follow the reconstructed signal.");
    Registry.Add(Won);
    // Prototype entries exist only after their actual map packages have been
    // authored. An absent map never produces a dead travel card.
    for (const auto& Definition : BreakerPrototypeDestinations::All())
    {
        if (!BreakerPrototypeDestinations::HasMapPackage(Definition)) continue;
        FBreakerTravelDestination Place;
        Place.Id=Definition.Id; Place.DisplayName=FText::FromString(Definition.DisplayName);
        Place.Description=Definition.Description; Registry.Add(Place);
    }

    return Registry;
}

bool ABreakerTravelPoint::CanEnterErasedEarth(const APawn* RequestingPawn)
{
    const ABreakerCharacter* Player = Cast<ABreakerCharacter>(RequestingPawn);
    const UBreakerQuestJournal* Journal = Player ? Player->GetQuestJournal() : nullptr;
    return Player && Player->GetCombat() && !Player->GetCombat()->IsDead() && Journal
        && Journal->HasFlag(TEXT("Quest.Breach.TurnedIn")) && Journal->HasFlag(TEXT("Quest.Survivor.Accepted"))
        && !Journal->HasFlag(TEXT("Quest.Survivor.Extracted"));
}

bool ABreakerTravelPoint::CanEnterFinaleEarth(FName DestinationId, const APawn* RequestingPawn)
{
    const ABreakerCharacter* Player = Cast<ABreakerCharacter>(RequestingPawn);
    const UBreakerQuestJournal* Journal = Player ? Player->GetQuestJournal() : nullptr;
    if (!Player || !Player->GetCombat() || Player->GetCombat()->IsDead() || !Journal
        || !Journal->HasFlag(TEXT("Quest.Survivor.TurnedIn")) || !Journal->HasFlag(TEXT("Quest.Finale.Accepted"))) return false;
    if (DestinationId == StrippedEarthDestinationId) return !Journal->HasFlag(TEXT("Quest.Finale.FragmentRecovered"));
    if (DestinationId == WinningEarthDestinationId) return Journal->HasFlag(TEXT("Quest.Finale.Reconstructed"));
    return false;
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
