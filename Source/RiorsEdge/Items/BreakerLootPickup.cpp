#include "Items/BreakerLootPickup.h"

#include "Characters/BreakerCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // Same trick the gym dressing uses: the stock basic-shape material exposes
    // one "Color" vector parameter, so a dynamic instance per primitive covers
    // the whole palette with no new assets. Kept local so the pickup never
    // reaches into game mode internals.
    void ApplyPickupColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Dynamic);
        }
    }

    FString UppercaseEnumName(const UEnum* Enum, int64 Value)
    {
        if (!Enum) return FString();
        return Enum->GetNameStringByValue(Value).ToUpper();
    }
}

ABreakerLootPickup::ABreakerLootPickup()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    // Five minutes on the ground, then it is gone.
    InitialLifeSpan = 300.0f;

    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    SetRootComponent(PickupSphere);
    PickupSphere->SetSphereRadius(60.0f);
    // Query-only and blocking nothing: loot must never shove the player or
    // stop a bullet.
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupSphere->SetGenerateOverlapEvents(true);

    ItemVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemVisual"));
    ItemVisual->SetupAttachment(PickupSphere);
    ItemVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ItemVisual->SetRelativeScale3D(FVector(0.3f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) ItemVisual->SetStaticMesh(CubeMesh.Object);

    RarityBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RarityBeam"));
    RarityBeam->SetupAttachment(PickupSphere);
    RarityBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RarityBeam->SetRelativeScale3D(FVector(0.12f, 0.12f, 8.0f));
    RarityBeam->SetRelativeLocation(FVector(0.0f, 0.0f, 380.0f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (BeamMesh.Succeeded()) RarityBeam->SetStaticMesh(BeamMesh.Object);
}

void ABreakerLootPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABreakerLootPickup, Item);
}

void ABreakerLootPickup::BeginPlay()
{
    Super::BeginPlay();
    if (ItemVisual) VisualBaseLocation = ItemVisual->GetRelativeLocation();
    ApplyRarityVisuals();
}

void ABreakerLootPickup::SetItem(const FBreakerItemInstance& NewItem)
{
    Item = NewItem;
    ApplyRarityVisuals();
}

void ABreakerLootPickup::OnRep_Item() { ApplyRarityVisuals(); }

FLinearColor ABreakerLootPickup::ColorForRarity(EBreakerItemRarity Rarity)
{
    switch (Rarity)
    {
    case EBreakerItemRarity::Uncommon:    return FLinearColor(0.25f, 0.55f, 1.00f);
    case EBreakerItemRarity::Exceptional: return FLinearColor(0.72f, 0.40f, 1.00f);
    case EBreakerItemRarity::Aberrant:    return FLinearColor(1.00f, 0.25f, 0.25f);
    // Anomalous is a rift-class object, so it earns the reserved teal.
    case EBreakerItemRarity::Anomalous:   return FLinearColor(0.15f, 0.95f, 0.85f);
    case EBreakerItemRarity::Standard:
    default:                              return FLinearColor(0.90f, 0.90f, 0.90f);
    }
}

void ABreakerLootPickup::ApplyRarityVisuals()
{
    const FLinearColor Color = ColorForRarity(Item.Rarity);
    ApplyPickupColor(ItemVisual, Color);
    // The beam has no translucency to work with, so "alpha" is expressed as
    // colour intensity: a dimmer column that still reads as the rarity hue.
    ApplyPickupColor(RarityBeam, Color * 0.55f);
}

void ABreakerLootPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!ItemVisual) return;
    BobTime += DeltaSeconds;
    ItemVisual->SetRelativeLocation(VisualBaseLocation + FVector(0.0f, 0.0f, FMath::Sin(BobTime * 2.0f) * 6.0f));
    ItemVisual->AddLocalRotation(FRotator(0.0f, 45.0f * DeltaSeconds, 0.0f));
}

FText ABreakerLootPickup::GetDisplayLabel() const
{
    const FString RarityName = UppercaseEnumName(StaticEnum<EBreakerItemRarity>(), static_cast<int64>(Item.Rarity));
    const FString SlotName = UppercaseEnumName(StaticEnum<EBreakerEquipSlot>(), static_cast<int64>(Item.Slot));
    return FText::FromString(FString::Printf(TEXT("%s %s"), *RarityName, *SlotName));
}

bool ABreakerLootPickup::TryPickup(ABreakerCharacter* Character)
{
    if (!Character || !HasAuthority()) return false;
    UBreakerEquipmentComponent* Equipment = Character->GetEquipment();
    if (!Equipment) return false;
    Equipment->AddToBackpack(Item);
    Destroy();
    return true;
}
