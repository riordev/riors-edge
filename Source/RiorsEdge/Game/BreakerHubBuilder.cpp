#include "Game/BreakerHubBuilder.h"
#include "Game/BreakerEnvironmentDressing.h"

#include "Interaction/BreakerNPC.h"
#include "Interaction/BreakerStashPoint.h"
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

// UNITY-BUILD COLLISION, and it is why these helpers carry a Hub prefix.
// This file's anonymous namespace duplicates BreakerGameMode.cpp's private
// shape helpers by design (they are file-local there, so they cannot be
// shared), and an anonymous namespace is per-TRANSLATION-UNIT — which would
// be fine if each .cpp were its own TU. Unreal's unity build concatenates
// several .cpp files into one, at which point the two anonymous namespaces
// merge and every shared name is a redefinition.
//
// This did not show up for several builds because adaptive unity EXCLUDES
// recently-changed files from the blob: the collision only appears once the
// file stops being edited, i.e. on a clean build or on someone else's machine.
// Renaming is the fix that does not depend on build settings.
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
    // appears anywhere below. (The travel point ACTOR carries teal — its
    // beacon and marker paint themselves in ABreakerTravelPoint::BeginPlay —
    // because travel is the rift verb and the gate is a rift object. This
    // builder still spawns none.)
    const FLinearColor HubPaletteEarth    (0.20f, 0.16f, 0.11f);
    const FLinearColor HubPaletteConcrete (0.33f, 0.35f, 0.30f);
    const FLinearColor HubPaletteStone    (0.24f, 0.26f, 0.23f);
    const FLinearColor HubPaletteRust     (0.34f, 0.20f, 0.09f);
    const FLinearColor HubPaletteAmber    (0.46f, 0.29f, 0.08f);
    const FLinearColor HubPaletteOffWhite (0.58f, 0.57f, 0.51f);
    const FLinearColor HubPaletteMoss     (0.14f, 0.26f, 0.11f);

    const TCHAR* HubShapeCube     = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* HubShapeCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

    // Same stock-material-plus-dynamic-instance trick as
    // BreakerGameMode.cpp's HubApplyShapeColor: the basic shape material exposes
    // one "Color" vector param, so no content assets are needed for palette.
    void HubApplyShapeColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
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

    AStaticMeshActor* HubSpawnShape(UWorld* World, const TCHAR* ShapePath, const FVector& Location, const FVector& Scale,
        const FRotator& Rotation, const FLinearColor& Color, bool bCollides, const TCHAR* Label)
    {
        if (!World) return nullptr;
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rotation);
        if (!Actor) return nullptr;
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, ShapePath));
        Mesh->SetWorldScale3D(Scale);
        HubApplyShapeColor(Mesh, Color);
        if (!bCollides)
        {
            Mesh->SetCollisionProfileName(TEXT("NoCollision"));
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetCanEverAffectNavigation(false);
            if (FString(Label).Contains(TEXT("Paving")) || FString(Label).Contains(TEXT("Drain"))
                || FString(Label).Contains(TEXT("Walkway")) || FString(Label).Contains(TEXT("Apron"))
                || FString(Label).Contains(TEXT("Moss"))) Mesh->SetCastShadow(false);
        }
        Mesh->SetMobility(EComponentMobility::Static);
        Actor->SetActorEnableCollision(bCollides);
        Actor->SetActorTickEnabled(false);
        if (FString(Label).Contains(TEXT("Walkway")) || FString(Label).Contains(TEXT("Apron")))
            Actor->Tags.Add(TEXT("BreakerMapGround"));
        Actor->SetActorLabel(Label);
        return Actor;
    }

    void HubAttachPropLight(AActor* Owner, const FVector& RelativeOffset, const FLinearColor& Color, float Intensity, float Radius)
    {
        if (!Owner) return;
        UPointLightComponent* Light = NewObject<UPointLightComponent>(Owner);
        if (!Light) return;
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetupAttachment(Owner->GetRootComponent());
        // Offsets are centimetres, independent of the prop's mesh scale.
        Light->SetAbsolute(true, true, true);
        Light->SetWorldLocation(Owner->GetActorLocation() + RelativeOffset);
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
    constexpr float VendorApproachOffset = 350.0f;
    constexpr float TravelPointForward = -1800.0f;
}

namespace
{
    void BreakerHubBuildStreet(UWorld* World, const FBreakerHubFrame& Frame)
    {
        // O2 PLACEHOLDER architectural dimensions; the service route stays clear.
        const FRotator Yaw = Frame.Forward.Rotation();
        const FLinearColor Plaster(.46f, .43f, .35f), Timber(.13f, .095f, .065f);
        const FLinearColor Slate(.16f, .20f, .21f), Window(.075f, .105f, .115f);
        auto Box = [&](float X, float Y, float Z, FVector Size, FLinearColor Color, bool Collision, const TCHAR* Label)
        {
            return HubSpawnShape(World, HubShapeCube, Frame.At(X,Y,Z), Size / 100.0f,
                Yaw, Color, Collision, Label);
        };

        // A continuous inhabited street edge replaces the empty horizon.
        // Doors/windows face inward; alternating heights keep the skyline varied.
        for (const float Side : {-1.0f, 1.0f})
        {
            for (int32 Bay = 0; Bay < 3; ++Bay)
            {
                const float X = -1550.0f + Bay * 1050.0f;
                const float Height = 650.0f + ((Bay + (Side > 0 ? 1 : 0)) % 3) * 110.0f;
                Box(X, Side * 1980, Height / 2, FVector(980,760,Height),
                    Bay == 1 ? HubPaletteConcrete : Plaster, true, TEXT("Runtime_HubStreetBuilding"));
                Box(X, Side * 1970, Height + 24, FVector(1040,840,48), Slate, true, TEXT("Runtime_HubRoofCornice"));
                Box(X, Side * 1590, 65, FVector(1000,32,130), HubPaletteStone, true, TEXT("Runtime_HubStreetPlinth"));
                Box(X, Side * 1570, 310, FVector(1020,40,32), Timber, false, TEXT("Runtime_HubFloorBeam"));
                for (const float Across : {-330.0f, 0.0f, 330.0f})
                {
                    Box(X + Across, Side * 1578, 470, FVector(145,20,180), Window, false, TEXT("Runtime_HubWindowRecess"));
                    Box(X + Across, Side * 1557, 375, FVector(170,60,18), HubPaletteOffWhite, false, TEXT("Runtime_HubWindowSill"));
                    Box(X + Across, Side * 1557, 470, FVector(10,26,180), Timber, false, TEXT("Runtime_HubWindowMullion"));
                    Box(X + Across, Side * 1557, 470, FVector(145,26,10), Timber, false, TEXT("Runtime_HubWindowMullion"));
                }
                Box(X, Side * 1570, 135, FVector(140,28,240), Timber, false, TEXT("Runtime_HubStreetDoor"));
                // A pitched awning gives the pedestrian level its own silhouette.
                const FLinearColor Cloth = (Bay % 2) ? HubPaletteRust : HubPaletteOffWhite;
                auto* Awning = Box(X, Side * 1410, 280, FVector(740,380,18), Cloth, false, TEXT("Runtime_HubStreetAwning"));
                if (Awning) Awning->SetActorRotation(FRotator(0,Yaw.Yaw,Side * 7.0f));
                for (const float Post : {-340.0f, 340.0f})
                    Box(X+Post,Side*1240,130,FVector(16,16,260),Timber,true,TEXT("Runtime_HubAwningPost"));
                Box(X+330,Side*1910,Height+120,FVector(90,100,210),HubPaletteStone,true,TEXT("Runtime_HubRoofChimney"));
            }
            // Shop rear wings frame existing NPCs, without moving their access.
            Box(2780,Side*970,440,FVector(900,1450,880),Plaster,true,TEXT("Runtime_HubServiceWing"));
            Box(2780,Side*970,900,FVector(980,1500,40),Slate,true,TEXT("Runtime_HubServiceRoof"));
            for (float Across : {-410.0f,0.0f,410.0f})
            {
                Box(2320,Side*970+Across,600,FVector(25,150,190),Window,false,TEXT("Runtime_HubServiceWindow"));
                Box(2300,Side*970+Across,500,FVector(60,175,18),HubPaletteOffWhite,false,TEXT("Runtime_HubServiceSill"));
            }
            // Street furniture stays beyond the clear central and vendor paths.
            for (float X : {-1100.0f, 200.0f})
            {
                Box(X,Side*850,35,FVector(290,110,70),HubPaletteStone,true,TEXT("Runtime_HubPlanter"));
                Box(X,Side*850,75,FVector(260,90,20),HubPaletteMoss,false,TEXT("Runtime_HubPlanterGrowth"));
                Box(X+360,Side*1010,50,FVector(210,60,20),Timber,true,TEXT("Runtime_HubBenchSeat"));
                Box(X+360,Side*1040,88,FVector(210,12,65),Timber,true,TEXT("Runtime_HubBenchBack"));
                for(float Leg : {-75.0f,75.0f}) Box(X+360+Leg,Side*1010,22,FVector(15,48,44),Slate,true,TEXT("Runtime_HubBenchLeg"));
                auto* Lamp = Box(X,Side*1080,190,FVector(18,18,380),Slate,true,TEXT("Runtime_HubStreetLamp"));
                Box(X,Side*1080,390,FVector(75,75,18),Timber,false,TEXT("Runtime_HubLampCap"));
                Box(X,Side*1080,355,FVector(42,42,55),HubPaletteOffWhite,false,TEXT("Runtime_HubLampGlass"));
                HubAttachPropLight(Lamp,FVector(0,0,155),FLinearColor(1,.72f,.40f),8500,1250);
            }
        }
        // Material-preserving planted pockets and workshop hardware. All stay
        // outside the arrival/vendor routes; sizes are O2 environment tuning.
        for (const float Side : {-1.0f, 1.0f})
        {
            for (const float X : {-1100.0f, 200.0f})
            {
                BreakerPlaceEnvironmentDressing(World, TEXT("Bush_Common"), Frame.At(X, Side * 850, 75), 130, Yaw.Yaw + Side * 35);
                BreakerPlaceEnvironmentDressing(World, TEXT("Fern_1"), Frame.At(X - 90, Side * 850, 75), 85, Yaw.Yaw + 70);
            }
            BreakerPlaceEnvironmentDressing(World, TEXT("CommonTree_1"), Frame.At(-1800, Side * 1190, 0), 700, Yaw.Yaw + Side * 20);
            BreakerPlaceEnvironmentDressing(World, TEXT("Column_Pipes"), Frame.At(2260, Side * 1520, 0), 660, Yaw.Yaw);
            BreakerPlaceEnvironmentDressing(World, TEXT("Column_MetalSupport"), Frame.At(2250, Side * 400, 0), 560, Yaw.Yaw);
            BreakerPlaceEnvironmentDressing(World, TEXT("Door_Metal"), Frame.At(2300, Side * 970, 0), 330, Yaw.Yaw + 90);
        }
        // Actual paving joints and gutters break up the uniform slab at eye level.
        for (int32 Row = 0; Row < 25; ++Row)
        {
            const float X = -1850.0f + Row * 150.0f;
            for (int32 Column = 0; Column < 4; ++Column)
                Box(X,-234.0f+Column*156.0f,4,FVector(146,152,4),
                    (Row+Column)%4 == 0 ? HubPaletteStone : HubPaletteConcrete,false,TEXT("Runtime_HubPaving"));
        }
        for(float Side : {-1.0f,1.0f})
        {
            Box(-50,Side*345,3,FVector(3750,36,4),Slate,false,TEXT("Runtime_HubDrain"));
            for(int32 Joint=0;Joint<25;++Joint)
                Box(-1850+Joint*150,Side*345,6,FVector(8,36,3),HubPaletteRust,false,TEXT("Runtime_HubDrainGrate"));
        }
        // The suppression apparatus dominates the inhabited skyline. Its 200 m
        // height is setting scale; it sits beyond the accessible market footprint.
        Box(9200,-500,250,FVector(2800,2600,500),HubPaletteStone,true,TEXT("Runtime_HubPylonFoundation"));
        Box(9200,-500,10100,FVector(700,700,19800),HubPaletteConcrete,true,TEXT("Runtime_HubSuppressionPylon"));
        for(float Side : {-1.0f,1.0f})
            Box(9200,-500+Side*480,7200,FVector(240,220,14400),Slate,true,TEXT("Runtime_HubPylonButtress"));
        for(float Height : {2800.0f,7800.0f,13000.0f,18700.0f})
            Box(9200,-500,Height,FVector(1300,1300,160),HubPaletteRust,false,TEXT("Runtime_HubPylonCollar"));
        Box(9200,-500,19850,FVector(1700,1700,300),HubPaletteOffWhite,false,TEXT("Runtime_HubPylonCrown"));
    }
}

void UBreakerHubBuilder::BuildPlazaAndBoundary(UWorld* World, const FBreakerHubFrame& Frame)
{
    using namespace BreakerHubLayout;

    // The bounded social space itself: a wide flat plaza (the "everyone
    // hangs out here" read) with a ring of pillars marking its edge rather
    // than walls, so it stays legible as open ground from any approach.
    HubSpawnShape(World, HubShapeCube, Frame.At(0.0f, 0.0f, -16.0f),
        FVector(PlazaHalfExtent * 2.0f / 100.0f, PlazaHalfExtent * 2.0f / 100.0f, 0.32f),
        Frame.Forward.Rotation(), HubPaletteEarth, true, TEXT("Runtime_HubPlaza"));

    for (int32 Pillar = 0; Pillar < BoundaryPillarCount; ++Pillar)
    {
        const float Angle = Pillar * (360.0f / BoundaryPillarCount);
        const FVector Offset = Frame.Forward.RotateAngleAxis(Angle, FVector::UpVector) * BoundaryRadius;
        HubSpawnShape(World, HubShapeCylinder, Frame.At(0.0f, 0.0f, 130.0f) + Offset,
            FVector(0.4f, 0.4f, 2.6f), FRotator::ZeroRotator, HubPaletteConcrete, true, TEXT("Runtime_HubBoundary"));
        // Accent striping (owner: "everything just looks so stale"): a warm
        // band near each pillar's crown, so the boundary reads as built
        // hardware rather than sixteen identical grey tubes. Every fourth
        // pillar goes rust instead of amber — repetition with variation is
        // what a maintained perimeter looks like.
        HubSpawnShape(World, HubShapeCylinder, Frame.At(0.0f, 0.0f, 234.0f) + Offset,
            FVector(0.46f, 0.46f, 0.10f), FRotator::ZeroRotator,
            (Pillar % 4 == 0) ? HubPaletteRust : HubPaletteAmber, false, TEXT("Runtime_HubBoundaryBand"));
    }

    // ---- Ground variation (owner: "everything just looks so stale") -------
    // The plaza was ONE uniform earth-brown slab. These overlays sit 2 cm
    // proud of it (no collision, no gameplay) and break it into readable
    // ground: a worn concrete walk running the gate-to-vendors spine, an
    // apron where the vendors trade, and moss reclaiming the edges — the O24
    // overgrown-Earth read (vegetation over ruins) at the cost of six shapes.
    const FRotator PlazaYaw(0.0f, Frame.Forward.Rotation().Yaw, 0.0f);
    // The spine: gate (forward -1800) through the obelisk to the vendor row.
    HubSpawnShape(World, HubShapeCube, Frame.At(0.0f, 0.0f, 2.0f),
        FVector(38.0f, 3.2f, 0.04f), PlazaYaw, HubPaletteConcrete, false, TEXT("Runtime_HubWalkway"));
    // The vendor crossbar: Kess's forge to the Quartermaster's stall.
    HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward, 0.0f, 2.0f),
        FVector(3.2f, 22.0f, 0.04f), PlazaYaw, HubPaletteConcrete, false, TEXT("Runtime_HubWalkway"));
    // Stone aprons under each vendor's pitch, so the stalls sit ON something.
    HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward, -VendorLateral, 1.5f),
        FVector(7.0f, 7.0f, 0.03f), PlazaYaw, HubPaletteStone, false, TEXT("Runtime_HubVendorApron"));
    HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward, VendorLateral, 1.5f),
        FVector(7.0f, 7.0f, 0.03f), PlazaYaw, HubPaletteStone, false, TEXT("Runtime_HubVendorApron"));
    // Moss, off the walked line: flat discs where the plaza meets the ring.
    HubSpawnShape(World, HubShapeCylinder, Frame.At(-800.0f, 1700.0f, 1.0f),
        FVector(9.0f, 9.0f, 0.02f), FRotator::ZeroRotator, HubPaletteMoss, false, TEXT("Runtime_HubMoss"));
    HubSpawnShape(World, HubShapeCylinder, Frame.At(1100.0f, -1900.0f, 1.0f),
        FVector(12.0f, 12.0f, 0.02f), FRotator::ZeroRotator, HubPaletteMoss, false, TEXT("Runtime_HubMoss"));
    HubSpawnShape(World, HubShapeCylinder, Frame.At(-2100.0f, -700.0f, 1.0f),
        FVector(7.0f, 7.0f, 0.02f), FRotator::ZeroRotator, HubPaletteMoss, false, TEXT("Runtime_HubMoss"));

    // A low memorial beside the route leaves the service fronts visible;
    // the suppression pylon beyond the street is the primary landmark.
    if (AStaticMeshActor* Obelisk = HubSpawnShape(World, HubShapeCube, Frame.At(-400.0f, 650.0f, 110.0f),
        FVector(0.9f, 0.9f, 2.2f), FRotator(0.0f, Frame.Forward.Rotation().Yaw, 0.0f), HubPaletteStone, true, TEXT("Runtime_HubLandmark")))
    {
        HubAttachPropLight(Obelisk, FVector(0, 0, 120.0f), FLinearColor(0.85f, 0.78f, 0.60f), 1200.0f, 1400.0f);
    }
    HubSpawnShape(World, HubShapeCylinder, Frame.At(-400.0f, 650.0f, 20.0f),
        FVector(1.6f, 1.6f, 0.4f), FRotator::ZeroRotator, HubPaletteMoss, false, TEXT("Runtime_HubLandmark"));
}

void UBreakerHubBuilder::BuildVendors(UWorld* World, const FBreakerHubFrame& Frame)
{
    using namespace BreakerHubLayout;

    const FRotator StationYaw = Frame.Forward.Rotation();
    // Open fronts face arrival. Posts frame the stalls without occupying the
    // approach to either NPC; the rear walls and roofs give each shop a room.
    for (const float Side : { -VendorLateral, VendorLateral })
    {
        const bool bForge = Side < 0.0f;
        HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward - 100.0f, Side, 350.0f),
            FVector(10.0f, 9.0f, 0.30f), StationYaw,
            bForge ? HubPaletteRust : HubPaletteOffWhite, true, TEXT("Runtime_HubStallRoof"));
        HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward + 390.0f, Side, 160.0f),
            FVector(0.24f, 9.0f, 3.2f), StationYaw, HubPaletteConcrete, true, TEXT("Runtime_HubStallBack"));
        for (const float PostSide : { -410.0f, 410.0f })
        {
            HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward - 550.0f, Side + PostSide, 170.0f),
                FVector(0.30f, 0.30f, 3.4f), StationYaw, HubPaletteStone, true, TEXT("Runtime_HubStallPost"));
        }
        HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward - 555.0f, Side, 315.0f),
            FVector(0.26f, 8.5f, 0.30f), StationYaw, HubPaletteAmber, false, TEXT("Runtime_HubStallFascia"));
        if (auto* TaskLight = HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward - 480.0f, Side, 290.0f),
            FVector(.12f,2.2f,.08f),StationYaw,HubPaletteOffWhite,false,TEXT("Runtime_HubServiceTaskLight")))
            HubAttachPropLight(TaskLight,FVector::ZeroVector,FLinearColor(1.0f,.80f,.56f),18000.0f,950.0f); // O2 lighting
    }
    // The forge chimney and stocked rear shelves distinguish the two shops
    // without adding new NPCs or interaction targets.
    HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward + 220.0f, -VendorLateral - 230.0f, 260.0f),
        FVector(1.0f, 1.0f, 5.2f), StationYaw, HubPaletteStone, true, TEXT("Runtime_HubForgeChimney"));
    for (const float ShelfHeight : { 60.0f, 150.0f, 240.0f })
    {
        HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward + 300.0f, VendorLateral, ShelfHeight),
            FVector(1.2f, 6.0f, 0.16f), StationYaw, HubPaletteRust, true, TEXT("Runtime_HubSupplyShelf"));
        for (const float CrateSide : { -170.0f, 150.0f })
            HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward + 300.0f, VendorLateral + CrateSide, ShelfHeight + 30.0f),
                FVector(0.55f, 0.85f, 0.45f), StationYaw, HubPaletteAmber, true, TEXT("Runtime_HubStoredSupplies"));
    }

    // Kess and the Quartermaster move here to live permanently, per the
    // brief: "The gym already spawns placeholder Kess (Forge Keeper) and a
    // Quartermaster — the hub is where they belong permanently." Spawned via
    // the exact same NPC construction as the gym camp (SpawnForgeKeeper /
    // SpawnQuartermaster, unmodified) so their dialogue, flags and vendor
    // hooks carry over with no new code.
    if (AStaticMeshActor* Forge = HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward, -VendorLateral, 110.0f),
        FVector(1.6f, 1.6f, 2.2f), StationYaw, HubPaletteRust, true, TEXT("Runtime_HubForge")))
    {
        HubAttachPropLight(Forge, FVector(0, 0, 40.0f), FLinearColor(1.0f, 0.62f, 0.26f), 900.0f, 700.0f);
    }
    // NPC spawners add their capsule half-height; these positions are feet.
    ABreakerNPC::SpawnForgeKeeper(World, Frame.At(VendorForward - VendorApproachOffset, -VendorLateral), (-Frame.Forward).Rotation());

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
    HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward, VendorLateral, 60.0f),
        FVector(2.4f, 1.2f, 1.2f), StationYaw, HubPaletteOffWhite, true, TEXT("Runtime_HubVendorStall"));
    if (AStaticMeshActor* Crate = HubSpawnShape(World, HubShapeCube, Frame.At(VendorForward + 150.0f, VendorLateral + 230.0f, 55.0f),
        FVector(1.1f, 1.1f, 1.1f), StationYaw, HubPaletteAmber, true, TEXT("Runtime_HubSupplyCrate")))
    {
        HubAttachPropLight(Crate, FVector(0, 0, 90.0f), FLinearColor(1.0f, 0.68f, 0.28f), 700.0f, 600.0f);
    }
    ABreakerNPC::SpawnQuartermaster(World, Frame.At(VendorForward - VendorApproachOffset, VendorLateral), (-Frame.Forward).Rotation());

    // THE STASH POINT, on the arrival side of the vendor crossbar: the
    // player walks up the spine from the gate, and the stash stands on it
    // before the vendors, facing back down the walk. It is the Anchor's
    // transfer point (Save/BreakerAccountSave.h) made into a place —
    // UBreakerEquipmentComponent refuses every deposit and withdrawal outside
    // this map, so this is the only spawn of it. Six metres short of the
    // crossbar keeps it outside both NPCs' interaction radii, so F at the
    // stash cannot open a conversation. O2 PLACEHOLDER position.
    FActorSpawnParameters StashParams;
    StashParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if (ABreakerStashPoint* Stash = World->SpawnActor<ABreakerStashPoint>(
        ABreakerStashPoint::StaticClass(), Frame.At(VendorForward - 600.0f, 0.0f, 100.0f), (-Frame.Forward).Rotation(), StashParams))
    {
        Stash->SetActorLabel(TEXT("Runtime_HubStash"));
    }
}

FTransform UBreakerHubBuilder::ArrivalTransform(const FTransform& HubOrigin)
{
    using namespace BreakerHubLayout;
    const FBreakerHubFrame Frame(HubOrigin);
    // 320 cm plaza-side of the gate posts, +100 up so the capsule drops onto
    // the plaza surface rather than spawning intersecting it. Facing hub
    // forward: the obelisk and both vendors are in view on arrival, which is
    // the "you have arrived somewhere" frame the gate walk-through implies.
    return FTransform(Frame.Forward.Rotation(), Frame.At(TravelPointForward + 320.0f, 0.0f, 100.0f));
}

ABreakerTravelPoint* UBreakerHubBuilder::BuildTravelPoint(UWorld* World, const FBreakerHubFrame& Frame)
{
    using namespace BreakerHubLayout;
    if (!World) return nullptr;

    // A small gate structure around the travel point so it reads as "the
    // way out" rather than another prop — two flanking posts framing the
    // actor, no teal (that stays reserved for rift objects).
    HubSpawnShape(World, HubShapeCylinder, Frame.At(TravelPointForward, -220.0f, 160.0f),
        FVector(0.5f, 0.5f, 3.2f), FRotator::ZeroRotator, HubPaletteConcrete, true, TEXT("Runtime_HubGate"));
    HubSpawnShape(World, HubShapeCylinder, Frame.At(TravelPointForward, 220.0f, 160.0f),
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
    BreakerHubBuildStreet(World, Frame);
    return BuildTravelPoint(World, Frame);
}
