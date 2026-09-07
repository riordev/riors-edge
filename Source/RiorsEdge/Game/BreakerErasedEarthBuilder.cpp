#include "Game/BreakerErasedEarthBuilder.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

FBreakerErasedEarthLayout UBreakerErasedEarthBuilder::Build(UWorld* World, const FTransform& Origin)
{
    FBreakerErasedEarthLayout Result;
    if (!World) return Result;
    // Translation/yaw frame; centimetre-authored capsule clearances never scale.
    const FTransform Frame(Origin.GetRotation(), Origin.GetLocation(), FVector::OneVector);
    auto Position = [&](FVector Local) { return Frame.TransformPosition(Local); };
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    const FLinearColor Stone(0.34f, 0.31f, 0.24f), PaleStone(0.60f, 0.55f, 0.41f);
    const FLinearColor Timber(0.18f, 0.10f, 0.055f), Clay(0.38f, 0.16f, 0.09f);
    const FLinearColor Soil(0.12f, 0.085f, 0.045f), Leaf(0.12f, 0.25f, 0.065f), Moss(0.21f, 0.29f, 0.10f);
    auto Shape = [&](const TCHAR* Label, UStaticMesh* MeshAsset, FVector At, FVector Size, FLinearColor Color,
        bool bCollision = true, FRotator Rotation = FRotator::ZeroRotator)
    {
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Position(At), (Frame.GetRotation() * Rotation.Quaternion()).Rotator());
        if (!Actor) return;
        Actor->Tags.Add(TEXT("ErasedEarth.Geometry"));
#if WITH_EDITOR
        Actor->SetActorLabel(Label);
#endif
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(MeshAsset); Mesh->SetWorldScale3D(Size / 100.0f);
        Mesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Mesh->SetCollisionResponseToAllChannels(ECR_Block);
        if (Material)
        {
            UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(Material, Mesh);
            Tint->SetVectorParameterValue(TEXT("Color"), Color); Mesh->SetMaterial(0, Tint);
        }
        Mesh->SetMobility(EComponentMobility::Static); Actor->SetActorTickEnabled(false);
    };
    // Broad playable spaces have different silhouettes and purposes; the
    // narrow intact centre of the broken causeway is the connecting route.
    Shape(TEXT("Earth.GardenGround"), Cube, FVector(1500, 0, -90), FVector(4400, 3100, 180), Moss);
    Shape(TEXT("Earth.StoneSettlement"), Cube, FVector(4550, 400, -90), FVector(3000, 2900, 180), Stone);
    Shape(TEXT("Earth.ExtractionTerrace"), Cube, FVector(9100, -400, -120), FVector(2400, 2200, 240), PaleStone);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const float X = 6100.0f + Index * 520.0f;
        const float Y = 100.0f - Index * 100.0f;
        Shape(TEXT("Earth.CausewayIntactSpine"), Cube, FVector(X, Y, -45), FVector(650, 820, 90), PaleStone);
        Shape(TEXT("Earth.CausewayPier"), Cube, FVector(X, Y, -430), FVector(230, 300, 720), Stone);
        for (int32 Side : { -1, 1 })
        {
            if ((Index + Side + 2) % 3 != 0)
                Shape(TEXT("Earth.BrokenParapet"), Cube, FVector(X + 80, Y + Side * 390, 60), FVector(270, 55, 120), Stone);
            Shape(TEXT("Earth.FallenCausewayStone"), Cube, FVector(X + 180, Y + Side * 660, -400 - Index * 40),
                FVector(320, 190, 90), Stone, true, FRotator(25, Index * 27, Side * 35));
        }
    }
    auto House = [&](FVector Centre, bool bShelter)
    {
        const float X = Centre.X, Y = Centre.Y;
        Shape(TEXT("Earth.StoneHouseBack"), Cube, FVector(X - 180, Y, 150), FVector(35, 440, 300), PaleStone);
        Shape(TEXT("Earth.StoneHouseSide"), Cube, FVector(X, Y - 210, 150), FVector(380, 30, 300), PaleStone);
        Shape(TEXT("Earth.StoneHouseSide"), Cube, FVector(X, Y + 210, 150), FVector(380, 30, 300), PaleStone);
        // An open front is a real shelter/doorway, never a sealed decorative box.
        for (int32 Side : { -1, 1 })
        {
            Shape(TEXT("Earth.TimberDoorPost"), Cube, FVector(X + 185, Y + Side * 185, 155), FVector(25, 30, 310), Timber);
            Shape(TEXT("Earth.ClayGableRoof"), Cube, FVector(X, Y + Side * 125, 350), FVector(450, 300, 28), Clay, true, FRotator(0, 0, Side * 30));
        }
        if (bShelter) Shape(TEXT("Earth.ShelterGardenJar"), Cylinder, FVector(X + 100, Y - 130, 40), FVector(50, 50, 80), Clay);
    };
    House(FVector(-20, -700, 0), true);
    House(FVector(1800, -1050, 0), false); House(FVector(2900, 1050, 0), false);
    House(FVector(4100, -500, 0), false); House(FVector(4850, 1350, 0), false); House(FVector(5350, -550, 0), false);
    for (int32 Bed = 0; Bed < 6; ++Bed)
    {
        const float X = 500.0f + (Bed % 3) * 430.0f;
        const float Y = Bed < 3 ? 950.0f : -1100.0f;
        Shape(TEXT("Earth.RaisedGardenBed"), Cube, FVector(X, Y, 25), FVector(330, 230, 50), Stone);
        Shape(TEXT("Earth.CultivatedSoil"), Cube, FVector(X, Y, 53), FVector(290, 190, 12), Soil, false);
        for (int32 Plant = 0; Plant < 3; ++Plant)
            Shape(TEXT("Earth.GardenGrowth"), Sphere, FVector(X - 90 + Plant * 90, Y, 85), FVector(65, 90, 70), Leaf, false);
    }
    for (const FVector Tree : { FVector(700, 1300, 0), FVector(2300, -1250, 0), FVector(3500, 1400, 0), FVector(9200, 500, 0) })
    {
        Shape(TEXT("Earth.OrchardTrunk"), Cylinder, Tree + FVector(0, 0, 150), FVector(65, 65, 300), Timber);
        Shape(TEXT("Earth.OrchardCanopy"), Sphere, Tree + FVector(0, 0, 350), FVector(420, 420, 300), Leaf, false);
    }
    // A dry village well and low stone partitions create cover without
    // invading the swept escort lane through the square.
    Shape(TEXT("Earth.VillageWell"), Cylinder, FVector(4450, 850, 55), FVector(220, 220, 110), Stone);
    for (const FVector Cover : { FVector(2100, 450, 0), FVector(2600, -400, 0), FVector(4750, 0, 0), FVector(8600, 200, 0), FVector(9250, -1050, 0) })
        Shape(TEXT("Earth.LowStoneWall"), Cube, Cover + FVector(0, 0, 55), FVector(260, 55, 110), Stone);
    for (int32 Side : { -1, 1 })
        Shape(TEXT("Earth.ExtractionGatePost"), Cube, FVector(9650, -500 + Side * 330, 230), FVector(100, 100, 460), PaleStone);
    Shape(TEXT("Earth.ExtractionGateLintel"), Cube, FVector(9650, -500, 470), FVector(130, 770, 90), PaleStone);
    Shape(TEXT("Earth.ExtractionThreshold"), Cube, FVector(9650, -500, -5), FVector(430, 600, 10), Clay, false);

    Result.PlayerArrival = Position(FVector(-420, -350, 90));
    Result.SurvivorShelter = Position(FVector(80, -700, 90));
    Result.Extraction = Position(FVector(9650, -500, 90));
    Result.ArrivalFacing = Frame.GetRotation().Rotator();
    auto Point = [&](FVector At, int32 Pocket = INDEX_NONE) { Result.Route.Add({Position(At), Pocket}); };
    Point(FVector(400, -700, 90)); Point(FVector(800, -350, 90));
    Point(FVector(1300, 0, 90), 0); Point(FVector(2850, 0, 90));
    Point(FVector(3500, 350, 90), 1); Point(FVector(4400, 350, 90)); Point(FVector(5600, 350, 90));
    Point(FVector(6100, 100, 90)); Point(FVector(6620, 0, 90)); Point(FVector(7140, -100, 90));
    Point(FVector(7660, -200, 90)); Point(FVector(8180, -300, 90), 2);
    Point(FVector(9000, -500, 90)); Point(FVector(9650, -500, 90));
    auto Enemy = [&](int32 Pocket, FVector At, bool bRanged)
    {
        Result.Enemies.Add({Position(At), bRanged ? ABreakerRangedEnemy::StaticClass() : ABreakerEnemy::StaticClass(), Pocket});
    };
    Enemy(0, FVector(1850, 200, 100), false); Enemy(0, FVector(2300, -230, 100), false);
    Enemy(0, FVector(2700, 250, 100), false); Enemy(0, FVector(2700, 700, 100), true);
    Enemy(1, FVector(3850, 600, 100), false); Enemy(1, FVector(4500, -100, 100), false);
    Enemy(1, FVector(4950, 650, 100), false); Enemy(1, FVector(5300, 1000, 100), true); Enemy(1, FVector(5500, 750, 100), true);
    Enemy(2, FVector(8650, -650, 100), false); Enemy(2, FVector(8900, -100, 100), false);
    Enemy(2, FVector(9350, -750, 100), false); Enemy(2, FVector(9700, 200, 100), false);
    Enemy(2, FVector(9500, 450, 100), true); Enemy(2, FVector(9900, -1000, 100), true);
    return Result;
}
