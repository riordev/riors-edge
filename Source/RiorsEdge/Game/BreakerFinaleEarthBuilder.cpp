#include "Game/BreakerFinaleEarthBuilder.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    // O2 visual blockout: fixed centimetre clearances, translated/yawed as a whole.
    struct FEarthGeometry
    {
        UWorld* World;
        FTransform Frame;
        UStaticMesh* Cube;
        UStaticMesh* Cylinder;
        UStaticMesh* Sphere;
        UMaterialInterface* Material;
        FEarthGeometry(UWorld* InWorld, const FTransform& Origin) : World(InWorld),
            Frame(Origin.GetRotation(), Origin.GetLocation(), FVector::OneVector)
        {
            Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
            Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
            Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        }
        FVector At(FVector Local) const { return Frame.TransformPosition(Local); }
        void Shape(const TCHAR* Name, FVector Position, FVector Size, FLinearColor Color,
            bool bSolid = true, UStaticMesh* Mesh = nullptr, FRotator Rotation = FRotator::ZeroRotator)
        {
            AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(At(Position), (Frame.GetRotation() * Rotation.Quaternion()).Rotator());
            if (!Actor) return;
            Actor->Tags.Add(bSolid ? TEXT("FinaleEarth.Geometry") : TEXT("FinaleEarth.Backdrop"));
#if WITH_EDITOR
            Actor->SetActorLabel(Name);
#endif
            UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
            Component->SetMobility(EComponentMobility::Movable);
            Component->SetStaticMesh(Mesh ? Mesh : Cube); Component->SetWorldScale3D(Size / 100.0f);
            // StaticMeshActor defaults to the asset's external collision profile.
            // SetCollisionEnabled alone is overwritten when physics state is
            // recreated (including the final mobility change). This override
            // explicitly disables bUseDefaultCollision on StaticMeshComponent.
            Component->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
            Component->SetCanEverAffectNavigation(bSolid);
            if (Material)
            {
                UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(Material, Component);
                Tint->SetVectorParameterValue(TEXT("Color"), Color); Component->SetMaterial(0, Tint);
            }
            Component->SetMobility(EComponentMobility::Static);
        }
        void Horizon(bool bWon)
        {
            const FLinearColor Terrain = bWon ? FLinearColor(.21f,.31f,.22f) : FLinearColor(.25f,.24f,.23f);
            // Beneath the walkable stage and outside it: visible horizon, never a navigation shortcut.
            Shape(TEXT("Earth.DistantGround"), FVector(3500,0,-650), FVector(40000,40000,300), Terrain, false);
            for (int32 Index = 0; Index < 16; ++Index)
            {
                const float Angle = Index * 2 * PI / 16;
                const FVector Location(3500 + FMath::Cos(Angle)*12500, FMath::Sin(Angle)*11500, 0);
                Shape(TEXT("Earth.DistantRidge"), Location, FVector(6000,4500,1800 + (Index%3)*700), Terrain, false, Sphere);
                if (Index%2 == 0) Shape(TEXT("Earth.DistantCity"), Location*.72f + FVector(1000,0,400),
                    FVector(700,900,bWon ? 1900 : 1200), FLinearColor(.35f,.39f,.40f), false);
            }
        }
    };
}

FBreakerFinaleEarthLayout UBreakerFinaleEarthBuilder::BuildStripped(UWorld* World, const FTransform& Origin)
{
    FBreakerFinaleEarthLayout Result;
    if (!World) return Result;
    FEarthGeometry G(World, Origin);
    const FLinearColor Concrete(.40f,.43f,.43f), Steel(.16f,.21f,.23f), Copper(.48f,.27f,.12f), Dark(.10f,.13f,.14f);
    G.Horizon(false);
    // The civic avenue remains traversable; the side plazas have been excavated.
    G.Shape(TEXT("Stripped.CivicAvenue"), FVector(3900,0,-100), FVector(9200,1500,200), Concrete);
    G.Shape(TEXT("Stripped.PumpSquare"), FVector(2200,0,-140), FVector(3200,3800,280), Concrete);
    G.Shape(TEXT("Stripped.ExchangeSquare"), FVector(5400,0,-140), FVector(3000,3600,280), Concrete);
    G.Shape(TEXT("Stripped.WorkingArchive"), FVector(8200,0,-140), FVector(1900,2500,280), Concrete);
    for (int32 Index = 0; Index < 7; ++Index)
    {
        const float X = 850 + Index*1050;
        for (int32 Side : {-1,1})
        {
            G.Shape(TEXT("Stripped.UtilityPylon"), FVector(X,Side*1050,480), FVector(110,110,960), Steel);
            G.Shape(TEXT("Stripped.CutUtilityChannel"), FVector(X-180,Side*1050,920), FVector(620,200,130), Copper);
            G.Shape(TEXT("Stripped.EmptyMount"), FVector(X,Side*650,30), FVector(240,200,60), Dark);
        }
    }
    // Recognizable former towers: bare frames, missing floors and ripped service trunks.
    for (const FVector Centre : {FVector(2200,-1450,0), FVector(5500,1400,0), FVector(8250,-950,0)})
    {
        for (int32 Side : {-1,1})
            G.Shape(TEXT("Stripped.TowerSkeleton"), Centre+FVector(Side*350,0,1100), FVector(90,90,2200), Steel);
        for (int32 Floor = 1; Floor < 5; ++Floor)
            G.Shape(TEXT("Stripped.MissingFloorRim"), Centre+FVector(0,0,Floor*450), FVector(820,90,70), Concrete);
        G.Shape(TEXT("Stripped.ExposedServiceConduit"), Centre+FVector(100,100,450), FVector(80,80,900), Copper, true, G.Cylinder);
    }
    for (const FVector Cover : {FVector(1800,450,55),FVector(3000,-450,55),FVector(4800,-400,55),FVector(5900,450,55),FVector(7800,480,55)})
        G.Shape(TEXT("Stripped.RemovedMachineCradle"), Cover, FVector(350,90,110), Steel);
    Result.PlayerArrival = G.At(FVector(-300,0,90));
    Result.InteractionLocation = G.At(FVector(8350,0,90));
    Result.ArrivalFacing = G.Frame.Rotator(); Result.InteractionFacing = (G.Frame.GetRotation()*FRotator(0,180,0).Quaternion()).Rotator();
    Result.PocketCount = 3;
    for (float X : {-300.f,1300.f,3500.f,6200.f,8200.f}) Result.Route.Add(G.At(FVector(X,0,90)));
    auto Enemy = [&](int32 Pocket, float X, float Y, bool Ranged) {
        Result.Enemies.Add({G.At(FVector(X,Y,100)), Ranged ? ABreakerRangedEnemy::StaticClass() : ABreakerEnemy::StaticClass(), Pocket});
    };
    Enemy(0,1900,-250,false); Enemy(0,2400,300,false); Enemy(0,2750,900,true); Enemy(0,3200,-800,true);
    Enemy(1,4700,300,false); Enemy(1,5300,-250,false); Enemy(1,5750,900,true); Enemy(1,6100,-850,true);
    Enemy(2,7600,-250,false); Enemy(2,8000,300,false); Enemy(2,8600,650,true); Enemy(2,8700,-450,true);
    return Result;
}

FBreakerFinaleEarthLayout UBreakerFinaleEarthBuilder::BuildWon(UWorld* World, const FTransform& Origin)
{
    FBreakerFinaleEarthLayout Result;
    if (!World) return Result;
    FEarthGeometry G(World, Origin);
    const FLinearColor Stone(.64f,.62f,.52f), Blue(.22f,.38f,.43f), Green(.15f,.35f,.18f), Warm(.70f,.52f,.26f);
    G.Horizon(true);
    G.Shape(TEXT("Won.InhabitedCivicSquare"), FVector(1900,0,-100), FVector(5800,4600,200), Stone);
    G.Shape(TEXT("Won.PromenadeInlay"), FVector(1900,0,1), FVector(5300,450,2), Warm, false);
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const float X = Index*780;
        for (int32 Side : {-1,1})
        {
            G.Shape(TEXT("Won.IntactColonnade"), FVector(X,Side*700,300), FVector(100,100,600), Stone, true, G.Cylinder);
            G.Shape(TEXT("Won.ContinuousPediment"), FVector(X+350,Side*700,620), FVector(860,180,90), Stone);
            G.Shape(TEXT("Won.OccupiedResidence"), FVector(X,Side*1650,480), FVector(580,720,960), Blue);
            G.Shape(TEXT("Won.WindowLight"), FVector(X,Side*1280,500), FVector(330,12,330), Warm, false);
            G.Shape(TEXT("Won.PlantedTerrace"), FVector(X+320,Side*1040,35), FVector(400,320,70), Stone);
            G.Shape(TEXT("Won.LivingCanopy"), FVector(X+320,Side*1040,190), FVector(320,280,320), Green, false, G.Sphere);
        }
    }
    G.Shape(TEXT("Won.CivicAssemblyBackdrop"), FVector(4450,0,650), FVector(200,1300,1300), Blue);
    G.Shape(TEXT("Won.OpenAssemblyArch"), FVector(4200,0,800), FVector(600,1600,150), Stone);
    Result.PlayerArrival = G.At(FVector(-400,0,90));
    Result.InteractionLocation = G.At(FVector(3900,0,90));
    Result.ArrivalFacing = G.Frame.Rotator(); Result.InteractionFacing = (G.Frame.GetRotation()*FRotator(0,180,0).Quaternion()).Rotator();
    for (float X : {-400.f,800.f,2100.f,3750.f}) Result.Route.Add(G.At(FVector(X,0,90)));
    return Result;
}
