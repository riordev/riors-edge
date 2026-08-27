#include "Items/BreakerLootPickup.h"

#include "Characters/BreakerCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UI/BreakerGlowMaterial.h"
// The rarity ramp itself, so the drop beam and the inventory frame cannot
// disagree: both now read BreakerUI::RarityColor.
#include "UI/BreakerUIStyle.h"
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

    // --- The rarity drama ladder, indexed by TierForRarity. ----------------
    // Every value O2 PLACEHOLDER, tuned by eye against capture stills. One
    // table so the whole presentation moves together per tier: a rarity's
    // beam, light, pulse and box size can never disagree about how loud it is.
    struct FBreakerLootDramaRow
    {
        float BeamHeightM;        // 0 = no beam at all
        float BeamThickness;      // relative XY scale of the column
        float BeamIntensity;      // emissive multiplier on the rarity colour
        float LightIntensity;     // 0 = no point light
        float LightRadiusCm;
        bool bPulses;             // Aberrant+ breathe so the eye snags on them
        float BoxScale;           // the item box grows a little with the tier
        float SpinDegreesPerSecond;
    };
    constexpr FBreakerLootDramaRow BreakerLootDrama[5] =
    {
        //  hM   thick  beamI  lightI   radius  pulse  box    spin
        { 0.0f, 0.00f,  0.0f,    0.0f,    0.0f, false, 0.38f,  45.0f },  // Standard
        { 2.2f, 0.05f,  0.8f,    0.0f,    0.0f, false, 0.40f,  60.0f },  // Uncommon
        { 4.0f, 0.07f,  1.6f, 1800.0f,  750.0f, false, 0.44f,  80.0f },  // Exceptional
        { 7.0f, 0.09f,  2.6f, 4500.0f, 1400.0f, true,  0.48f, 110.0f },  // Aberrant
        { 9.5f, 0.11f,  3.4f, 7000.0f, 1800.0f, true,  0.52f, 140.0f },  // Anomalous
    };
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
    ItemVisual->SetRelativeScale3D(FVector(0.38f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded()) ItemVisual->SetStaticMesh(CubeMesh.Object);

    RarityBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RarityBeam"));
    RarityBeam->SetupAttachment(PickupSphere);
    RarityBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RarityBeam->SetCastShadow(false);
    // Height, thickness and brightness are TIER-DRIVEN now — see the drama
    // table above; ApplyRarityVisuals poses this. The default here is the
    // Uncommon column so a beam with no item yet is the quiet one.
    RarityBeam->SetRelativeScale3D(FVector(0.05f, 0.05f, 2.2f));
    RarityBeam->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (BeamMesh.Succeeded()) RarityBeam->SetStaticMesh(BeamMesh.Object);

    // The Exceptional+ announcement: a rarity-coloured light so the drop is
    // visible off the surfaces around it, not only when directly in view.
    // Shadowless — it is a beacon, not a lamp.
    RarityLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RarityLight"));
    RarityLight->SetupAttachment(PickupSphere);
    RarityLight->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
    RarityLight->SetIntensity(0.0f);
    RarityLight->SetCastShadows(false);
    RarityLight->SetVisibility(false);
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

// THE RAMP IS THE UI'S RAMP, and this used to be a second copy of it in the
// WRONG COLOUR SPACE. Four of the five entries were the matching BreakerUI
// token's sRGB triple handed to a LINEAR FLinearColor constructor, so every
// beam rendered as a paler wash of the rarity it names and no drop matched its
// own inventory frame: Uncommon (0.25,0.55,1.00) against the token's linear
// (0.051,0.262,1.000), Aberrant (1.00,0.25,0.25) against (1.000,0.051,0.051),
// Anomalous (0.15,0.95,0.85) against (0.019,0.888,0.694). The Anomalous line
// even claimed in a comment to be "the reserved teal" while being a colour the
// reserved-teal predicate does not recognise.
//
// Delegating rather than correcting the five values is the point: a second copy
// of a ramp drifts again the next time one is retuned, and it drifted silently
// here because nothing compares a world colour to a UI colour.
FLinearColor ABreakerLootPickup::ColorForRarity(EBreakerItemRarity Rarity)
{
    return BreakerUI::RarityColor(Rarity);
}

int32 ABreakerLootPickup::TierForRarity(EBreakerItemRarity Rarity)
{
    switch (Rarity)
    {
    case EBreakerItemRarity::Uncommon:    return 1;
    case EBreakerItemRarity::Exceptional: return 2;
    case EBreakerItemRarity::Aberrant:    return 3;
    case EBreakerItemRarity::Anomalous:   return 4;
    case EBreakerItemRarity::Standard:
    default:                              return 0;
    }
}

void ABreakerLootPickup::ApplyRarityVisuals()
{
    CachedTier = FMath::Clamp(TierForRarity(Item.Rarity), 0, 4);
    const FBreakerLootDramaRow& Drama = BreakerLootDrama[CachedTier];
    CachedColor = ColorForRarity(Item.Rarity);
    CachedBeamIntensity = Drama.BeamIntensity;
    CachedLightIntensity = Drama.LightIntensity;

    ApplyPickupColor(ItemVisual, CachedColor);
    if (ItemVisual) ItemVisual->SetRelativeScale3D(FVector(Drama.BoxScale));

    // The beam is LIGHT now, not paint: the unlit-additive glow material (the
    // tracer's), so an Anomalous column reads across the arena and does not go
    // grey in shadow. Standard has no beam at all — silence is what makes the
    // tiers above it loud.
    if (RarityBeam)
    {
        if (Drama.BeamHeightM > 0.0f)
        {
            RarityBeam->SetVisibility(true);
            RarityBeam->SetRelativeScale3D(FVector(Drama.BeamThickness, Drama.BeamThickness, Drama.BeamHeightM));
            // Unit cylinder is 100 cm tall about its centre: lift by half the
            // height so the column grows UP from the drop.
            RarityBeam->SetRelativeLocation(FVector(0.0f, 0.0f, Drama.BeamHeightM * 50.0f));
            if (!BeamMaterial) BeamMaterial = BreakerUI::MakeGlowMaterial(RarityBeam);
            BreakerUI::SetGlowColor(BeamMaterial, CachedColor, CachedBeamIntensity);
        }
        else
        {
            RarityBeam->SetVisibility(false);
        }
    }

    if (RarityLight)
    {
        const bool bLit = Drama.LightIntensity > 0.0f;
        RarityLight->SetVisibility(bLit);
        if (bLit)
        {
            RarityLight->SetLightColor(CachedColor);
            RarityLight->SetIntensity(Drama.LightIntensity);
            RarityLight->SetAttenuationRadius(Drama.LightRadiusCm);
        }
        else
        {
            RarityLight->SetIntensity(0.0f);
        }
    }
    // S2 NOTE (unowned domain): the tiered drop chime would play once here,
    // pitched by CachedTier — noted, not built.
}

void ABreakerLootPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!ItemVisual) return;
    BobTime += DeltaSeconds;
    const FBreakerLootDramaRow& Drama = BreakerLootDrama[FMath::Clamp(CachedTier, 0, 4)];
    ItemVisual->SetRelativeLocation(VisualBaseLocation + FVector(0.0f, 0.0f, FMath::Sin(BobTime * 2.0f) * 6.0f));
    // Higher tiers spin faster: motion is the cheapest "look at me" there is.
    ItemVisual->AddLocalRotation(FRotator(0.0f, Drama.SpinDegreesPerSecond * DeltaSeconds, 0.0f));

    // Aberrant+ breathe: the beam and the light swell and settle together on
    // a slow pulse, which is what snags the eye at the edge of the screen.
    if (Drama.bPulses)
    {
        const float Pulse = 0.72f + 0.28f * FMath::Sin(BobTime * 3.2f);   // O2 PLACEHOLDER
        if (BeamMaterial) BreakerUI::SetGlowColor(BeamMaterial, CachedColor, CachedBeamIntensity * Pulse);
        if (RarityLight && CachedLightIntensity > 0.0f) RarityLight->SetIntensity(CachedLightIntensity * Pulse);
    }
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
    // One-AB: a full backpack REFUSES the pickup and the drop stays on the
    // ground — a drop the player can see and cannot take is a readable
    // problem where a drop that vanished is a bug report. The player's answer
    // is DiscardBackpackBelowRarity or SalvageFromBackpack, not this actor.
    if (!Equipment->AddToBackpack(Item, /*bRefuseWhenFull=*/true))
    {
        return false;
    }
    Destroy();
    return true;
}
