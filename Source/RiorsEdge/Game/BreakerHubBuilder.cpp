#include "Game/BreakerHubBuilder.h"

#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerTravelPoint.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

// The one frame every spawn helper below works in, built fresh from the
// origin transform passed to BuildHub. Same shape as
// ABreakerGameMode::FFieldFrame (Ground/Forward/Right/At) so the idiom reads
// identically, but it is not that type — FFieldFrame is private to
// ABreakerGameMode and this builder is deliberately parameterised off an
// FTransform instead of a possessed Pawn, since a hub has no "the player who
// triggered this" the way the gym's first-spawn frame does.
struct FBreakerHubFrame
{
    FVector Ground = FVector::ZeroVector;
    FVector Forward = FVector::ForwardVector;
    FVector Right = FVector::RightVector;

    explicit FBreakerHubFrame(const FTransform& Origin)
    {
        Ground = Origin.GetLocation();
        Forward = Origin.GetRotation().GetForwardVector().GetSafeNormal2D();
        if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;
        Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal2D();
    }

    FVector At(float Fwd, float Rgt, float Up = 0.0f) const
    {
        return Ground + Forward * Fwd + Right * Rgt + FVector(0.0f, 0.0f, Up);
    }
};

namespace
{
    // O24 (Docs/Design/Decisions.md): overgrown Earth — vegetation over
    // ruins, slight sci-fi styling, weathered tech scattered through it.
    // These are the same RGB values ABreakerGameMode.cpp's anonymous
    // namespace uses for its palette (Palette*, ~line 514 of
    // BreakerGameMode.cpp) so the hub reads as the same world rather than a
    // separately-authored space — re-declared here rather than shared
    // because that palette is file-local to BreakerGameMode.cpp (out of
    // territory) and this builder must not depend on it.
    //
    // Teal is reserved for rift objects per the owner's brief and O24's
    // "rift approaches" read; the hub is not a rift space, so no teal
    // appears anywhere below.
    const FLinearColor HubPaletteEarth    (0.20f, 0.16f, 0.11f);
    const FLinearColor HubPaletteConcrete (0.33f, 0.35f, 0.30f);
    const FLinearColor HubPaletteStone    (0.24f, 0.26f, 0.23f);
    const FLinearColor HubPaletteRust     (0.34f, 0.20f, 0.09f);
    const FLinearColor HubPaletteAmber    (0.46f, 0.29f, 0.08f);
    const FLinearColor HubPaletteOffWhite (0.58f, 0.57f, 0.51f);
    const FLinearColor HubPaletteMoss     (0.14f, 0.26f, 0.11f);

    const TCHAR* ShapeCube     = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* ShapeCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

    // Same stock-material-plus-dynamic-instance trick as
    // BreakerGameMode.cpp's ApplyShapeColor: the basic shape material exposes
    // one "Color" vector param, so no content assets are needed for palette.
    void ApplyShapeColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
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

    AStaticMeshActor* SpawnShape(UWorld* World, const TCHAR* ShapePath, const FVector& Location, const FVector& Scale,
        const FRotator& Rotation, const FLinearColor& Color, bool bCollides, const TCHAR* Label)
    {
        if (!World) return nullptr;
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rotation);
        if (!Actor) return nullptr;
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, ShapePath));
        Mesh->SetWorldScale3D(Scale);
        ApplyShapeColor(Mesh, Color);
        if (!bCollides) Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetMobility(EComponentMobility::Static);
        Actor->SetActorEnableCollision(bCollides);
        Actor->SetActorTickEnabled(false);
        Actor->SetActorLabel(Label);
        return Actor;
    }

    void AttachPropLight(AActor* Owner, const FVector& RelativeOffset, const FLinearColor& Color, float Intensity, float Radius)
    {
        if (!Owner) return;
        UPointLightComponent* Light = NewObject<UPointLightComponent>(Owner);
        if (!Light) return;
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetupAttachment(Owner->GetRootComponent());
        Light->SetRelativeLocation(RelativeOffset);
        Light->SetLightColor(Color);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(Radius);
        Light->SetCastShadows(false);
        Light->RegisterComponent();
    }
}

// O2 PLACEHOLDER — every extent below is layout (how big the social space
// reads), not balance, but sized like a tunable so a later pass can retune
// the hub's footprint without hunting magic numbers.
namespace BreakerHubLayout
{
    constexpr float PlazaHalfExtent = 3500.0f;
    constexpr float BoundaryRadius = 3300.0f;
    constexpr int32 BoundaryPillarCount = 16;
    constexpr float VendorForward = 1800.0f;
    constexpr float VendorLateral = 900.0f;
    constexpr float TravelPointForward = -1800.0f;
}

void UBreakerHubBuilder::BuildPlazaAndBoundary(UWorld* World, const FBreakerHubFrame& Frame)
{
    using namespace BreakerHubLayout;

    // The bounded social space itself: a wide flat plaza (the "everyone
    // hangs out here" read) with a ring of pillars marking its edge rather
    // than walls, so it stays legible as open ground from any approach.
    SpawnShape(World, ShapeCube, Frame.At(0.0f, 0.0f, -16.0f),
        FVector(PlazaHalfExtent * 2.0f / 100.0f, PlazaHalfExtent * 2.0f / 100.0f, 0.32f),
        Frame.Forward.Rotation(), HubPaletteEarth, true, TEXT("Runtime_HubPlaza"));

    for (int32 Pillar = 0; Pillar < BoundaryPillarCount; ++Pillar)
    {
        const float Angle = Pillar * (360.0f / BoundaryPillarCount);
        const FVector Offset = Frame.Forward.RotateAngleAxis(Angle, FVector::UpVector) * BoundaryRadius;
        SpawnShape(World, ShapeCylinder, Frame.At(0.0f, 0.0f, 130.0f) + Offset,
            FVector(0.4f, 0.4f, 2.6f), FRotator::ZeroRotator, HubPaletteConcrete, true, TEXT("Runtime_HubBoundary"));
    }

    // A central landmark so the plaza reads as a destination, not just an
    // empty rectangle — a weathered obelisk, in the same overgrown-Earth
    // idiom as the gym's ruin dressing (moss over stone).
    if (AStaticMeshActor* Obelisk = SpawnShape(World, ShapeCube, Frame.At(0.0f, 0.0f, 260.0f),
        FVector(0.9f, 0.9f, 5.2f), FRotator(0.0f, Frame.Forward.Rotation().Yaw, 0.0f), HubPaletteStone, true, TEXT("Runtime_HubLandmark")))
    {
        AttachPropLight(Obelisk, FVector(0, 0, 260.0f), FLinearColor(0.85f, 0.78f, 0.60f), 1200.0f, 1400.0f);
    }
    SpawnShape(World, ShapeCylinder, Frame.At(0.0f, 0.0f, 20.0f),
        FVector(1.6f, 1.6f, 0.4f), FRotator::ZeroRotator, HubPaletteMoss, false, TEXT("Runtime_HubLandmark"));
}

void UBreakerHubBuilder::BuildVendors(UWorld* World, const FBreakerHubFrame& Frame)
{
    using namespace BreakerHubLayout;

    // Kess and the Quartermaster move here to live permanently, per the
    // brief: "The gym already spawns placeholder Kess (Forge Keeper) and a
    // Quartermaster — the hub is where they belong permanently." Spawned via
    // the exact same NPC construction as the gym camp (SpawnForgeKeeper /
    // SpawnQuartermaster, unmodified) so their dialogue, flags and vendor
    // hooks carry over with no new code.
    if (AStaticMeshActor* Forge = SpawnShape(World, ShapeCube, Frame.At(VendorForward, -VendorLateral, 110.0f),
        FVector(1.6f, 1.6f, 2.2f), FRotator::ZeroRotator, HubPaletteRust, true, TEXT("Runtime_HubForge")))
    {
        AttachPropLight(Forge, FVector(0, 0, 40.0f), FLinearColor(1.0f, 0.62f, 0.26f), 900.0f, 700.0f);
    }
    ABreakerNPC::SpawnForgeKeeper(World, Frame.At(VendorForward - 80.0f, -VendorLateral, 100.0f), (-Frame.Right).Rotation());

    // Quartermaster's stall doubles as the supply prop and, through her
    // EXISTING unmodified dialogue (BreakerNPC::MakeQuartermasterDialogue,
    // node "Job"), is THE story-start interactable this brief asks for: the
    // "Anything need doing around here?" choice already sets
    // BreakerQuestFlags::FirstContractOffered through the journal
    // (Save/BreakerQuestJournal::SetFlag via the dialogue system's
    // SetsQuestFlag plumbing), and Quest.FirstContract
    // (Save/BreakerQuestContent.cpp) is the only quest this build defines —
    // i.e. it IS the main story quest of the vertical slice. No new flag,
    // no new dialogue, no parallel quest mechanism was added; the vendor was
    // simply given a permanent home.
    SpawnShape(World, ShapeCube, Frame.At(VendorForward, VendorLateral, 60.0f),
        FVector(2.4f, 1.2f, 1.2f), FRotator::ZeroRotator, HubPaletteOffWhite, true, TEXT("Runtime_HubVendorStall"));
    if (AStaticMeshActor* Crate = SpawnShape(World, ShapeCube, Frame.At(VendorForward - 200.0f, VendorLateral, 55.0f),
        FVector(1.1f, 1.1f, 1.1f), FRotator(0.0f, 12.0f, 0.0f), HubPaletteAmber, true, TEXT("Runtime_HubSupplyCrate")))
    {
        AttachPropLight(Crate, FVector(0, 0, 90.0f), FLinearColor(1.0f, 0.68f, 0.28f), 700.0f, 600.0f);
    }
    ABreakerNPC::SpawnQuartermaster(World, Frame.At(VendorForward - 80.0f, VendorLateral, 100.0f), Frame.Right.Rotation());
}

ABreakerTravelPoint* UBreakerHubBuilder::BuildTravelPoint(UWorld* World, const FBreakerHubFrame& Frame)
{
    using namespace BreakerHubLayout;
    if (!World) return nullptr;

    // A small gate structure around the travel point so it reads as "the
    // way out" rather than another prop — two flanking posts framing the
    // actor, no teal (that stays reserved for rift objects).
    SpawnShape(World, ShapeCylinder, Frame.At(TravelPointForward, -220.0f, 160.0f),
        FVector(0.5f, 0.5f, 3.2f), FRotator::ZeroRotator, HubPaletteConcrete, true, TEXT("Runtime_HubGate"));
    SpawnShape(World, ShapeCylinder, Frame.At(TravelPointForward, 220.0f, 160.0f),
        FVector(0.5f, 0.5f, 3.2f), FRotator::ZeroRotator, HubPaletteConcrete, true, TEXT("Runtime_HubGate"));

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ABreakerTravelPoint* TravelPoint = World->SpawnActor<ABreakerTravelPoint>(
        ABreakerTravelPoint::StaticClass(), Frame.At(TravelPointForward, 0.0f, 100.0f), (-Frame.Forward).Rotation(), Params);
    if (TravelPoint)
    {
        TravelPoint->SetActorLabel(TEXT("Runtime_HubTravelPoint"));
    }
    return TravelPoint;
}

ABreakerTravelPoint* UBreakerHubBuilder::BuildHub(UWorld* World, const FTransform& HubOrigin)
{
    if (!World) return nullptr;

    const FBreakerHubFrame Frame(HubOrigin);
    BuildPlazaAndBoundary(World, Frame);
    BuildVendors(World, Frame);
    return BuildTravelPoint(World, Frame);
}
