#include "Interaction/BreakerStashPoint.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
    // The marker's paint. NOT teal: teal is the rift noun and a stash is a
    // crate. A warm, worn amber in the hub palette's family (HubPaletteAmber
    // is file-local to BreakerHubBuilder.cpp, so it cannot be shared) — read
    // as "supplies" beside the Quartermaster's crate rather than as a gate.
    // O2 PLACEHOLDER until the owner has seen it on the plaza.
    const FLinearColor BreakerStashMarkerColour(0.52f, 0.36f, 0.12f);
}

ABreakerStashPoint::ABreakerStashPoint()
{
    // THE ONE NEW PLAYER-FACING WORD. The HUD prints "F  STASH" off this
    // label, and a new interactable cannot exist without a prompt. Noted for
    // the O195 voice table; nothing else on this actor reaches the screen.
    PromptLabel = FText::FromString(TEXT("Stash"));

    // A stash is a crate you stand at, not a gate you walk through: the NPC
    // reach, not the gate's widened threshold. O2 PLACEHOLDER.
    InteractionRange = 300.0f;

    // No beacon: the fourteen-metre column is the travel gate's "find me
    // from anywhere" answer, and it is teal. Hidden rather than destroyed,
    // so the base class's component wiring stays intact.
    Beacon->SetVisibility(false);
    Beacon->SetHiddenInGame(true);
    BeaconLight->SetVisibility(false);
    BeaconLight->SetIntensity(0.0f);

    // A low, wide marker: the base's tall cylinder reads as "the way out"
    // from across the plaza, and this must not.
    Visual->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.6f));
    Visual->SetRelativeLocation(FVector(0.0f, 0.0f, -70.0f));

    // A stash has no place to exclude; it goes nowhere.
    ExcludedDestinationId = NAME_None;
}

void ABreakerStashPoint::BeginPlay()
{
    Super::BeginPlay();

    // Overwrites the base's hardware-teal paint on the marker. Same
    // stock-material-plus-dynamic-instance path the base and the hub builder
    // use, so no content asset is needed.
    if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Visual))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), BreakerStashMarkerColour);
            Visual->SetMaterial(0, Dynamic);
        }
    }
}

TArray<FBreakerTravelDestination> ABreakerStashPoint::GetAvailableDestinations() const
{
    return TArray<FBreakerTravelDestination>();
}

bool ABreakerStashPoint::SelectDestination(FName DestinationId, APawn* RequestingPawn)
{
    return false;
}
