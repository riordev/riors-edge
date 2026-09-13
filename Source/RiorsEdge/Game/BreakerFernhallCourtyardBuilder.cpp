#include "Game/BreakerFernhallCourtyardBuilder.h"
#include "Game/BreakerZoneBuilder.h"
#include "Game/BreakerEnvironmentDressing.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"

bool BreakerFernhallCourtyard::MakePlan(const TArray<FBreakerZonePiece>& Pieces, FPlan& Out, FString& Error)
{
    Out = FPlan();
    Error.Reset();
    const auto* Entry = Pieces.FindByPredicate([](const auto& P){ return P.Name == TEXT("flr_yard"); });
    const auto* Sub = Pieces.FindByPredicate([](const auto& P){ return P.Name == TEXT("flr_yard_sub"); });
    if (!Entry || !Sub)
    {
        Error = TEXT("Missing authored yard floors");
        return false;
    }
    const float Side = Entry->Origin.Y < Sub->Origin.Y ? -1.f : 1.f;
    const FBreakerZonePiece* Wall = nullptr;
    double Best = TNumericLimits<double>::Max();
    const float DesiredX = Entry->Origin.X - Entry->Extent.X / 3;
    for (const auto& P : Pieces)
    {
        if (!P.Name.StartsWith(TEXT("wall_")) || P.Name.StartsWith(TEXT("wall_sub"))
            || P.Name.StartsWith(TEXT("wall_seam")) || P.Extent.X < 450 || P.Extent.X > 600
            || P.Extent.Y > 250 || (P.Origin.Y-Entry->Origin.Y)*Side < Entry->Extent.Y-600) continue;
        const double Distance = FMath::Abs(P.Origin.X-DesiredX);
        if (Distance < Best) { Best=Distance; Wall=&P; }
    }
    if (!Wall)
    {
        Error = TEXT("No safe full-width entry boundary bay");
        return false;
    }
    Out.ReplacedBoundaryPiece=Wall->Name;
    Out.GroundZ=Entry->Origin.Z+Entry->Extent.Z;
    Out.Origin=FVector(Wall->Origin.X,Wall->Origin.Y,Out.GroundZ);
    Out.Forward=FVector(0,Side,0);
    Out.Right=FVector(1,0,0);
    Out.EntranceFloorStart = (Entry->Origin.Y + Side * Entry->Extent.Y - Out.Origin.Y) * Side;
    if (Out.EntranceFloorStart >= 1200)
    {
        Error = TEXT("Boundary bay is not adjacent to the entry floor");
        return false;
    }
    // O2 PLACEHOLDER: 8 m mouth, 26 m dogleg, 36 x 42 m maintenance court.
    auto Footprint=[&](float U0,float V0,float U1,float V1)
    { FBox B(ForceInit); B+=Out.At(U0,V0); B+=Out.At(U1,V1); Out.GroundFootprints.Add(B); };
    Footprint(Out.EntranceFloorStart,-400,2000,400);
    Footprint(1200,400,2000,1800);
    Footprint(1200,1800,4800,6000);
    Out.RoutePoints={Out.At(-650,0),Out.At(1600,0),Out.At(1600,2200),Out.At(1600,3100),Out.At(3000,3900)};
    Out.MeleeSpawns={Out.At(2500,2800),Out.At(3800,3000),Out.At(2600,4400)};
    Out.RangedSpawn=Out.At(4000,5000);
    Out.CoverCenters={Out.At(3100,3300),Out.At(2300,4800)};
    return true;
}

bool BreakerFernhallCourtyard::Build(UWorld* World, const FPlan& P)
{
    if (!World || P.ReplacedBoundaryPiece.IsEmpty()) return false;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Breaker/Materials/M_BreakerWall.M_BreakerWall"));
    auto* GroundMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Breaker/Materials/M_BreakerGround.M_BreakerGround"));
    if (!Cube || !Material) return false;
    bool Success=true;
    TArray<AActor*> Created;
    int32 PieceIndex = 0;
    auto Box=[&](float U,float V,float Z,FVector Size,FLinearColor Color,bool Ground=false)
    {
        auto* A=World->SpawnActor<AStaticMeshActor>(P.At(U,V,Z),P.Forward.Rotation());
        if (!A)
        {
            Success = false;
            return;
        }
        Created.Add(A);
        auto* M=A->GetStaticMeshComponent();
        M->SetMobility(EComponentMobility::Movable);
        M->SetStaticMesh(Cube);
        M->SetWorldScale3D(Size/100.f);
        M->SetCollisionProfileName(TEXT("BlockAll"));
        M->SetCanEverAffectNavigation(true);
        auto* Tint=UMaterialInstanceDynamic::Create(Ground && GroundMaterial ? GroundMaterial : Material,M);
        Tint->SetVectorParameterValue(TEXT("Color"),Color);
        M->SetMaterial(0,Tint);
        M->SetMobility(EComponentMobility::Static);
        A->Tags.Add(TEXT("FernhallMaintenance"));
        if (Ground) A->Tags.Add(TEXT("BreakerMapGround"));
#if WITH_EDITOR
        A->SetActorLabel(FString::Printf(TEXT("Fernhall_Maintenance_%s_%02d"), Ground ? TEXT("Floor") : TEXT("Architecture"), PieceIndex++));
#endif
    };
    const FLinearColor Stone(.31f,.33f,.29f), Wall(.38f,.36f,.29f), Rust(.31f,.17f,.08f);
    Box((P.EntranceFloorStart+2000)*.5f,0,-20,FVector(2000-P.EntranceFloorStart,800,40),Stone,true);
    Box(1600,1100,-20,FVector(800,1400,40),Stone,true);
    Box(3000,3900,-20,FVector(3600,4200,40),Stone,true);
    // The imported floor and native box both reject a downward ray exactly
    // on their shared edge. Bridge only that collision crack: visible floor
    // surfaces remain disjoint, and this support is not another map surface.
    auto* Seam = World->SpawnActor<AActor>(P.At(P.EntranceFloorStart,0,-20),P.Forward.Rotation());
    if (Seam)
    {
        Created.Add(Seam);
        auto* Support = NewObject<UBoxComponent>(Seam);
        Seam->AddInstanceComponent(Support);
        Seam->SetRootComponent(Support);
        Support->SetBoxExtent(FVector(2,400,20));
        Support->SetCollisionProfileName(TEXT("BlockAll"));
        Support->SetCanEverAffectNavigation(true);
        Support->SetHiddenInGame(true);
        Support->RegisterComponent();
        Support->SetWorldLocationAndRotation(P.At(P.EntranceFloorStart,0,-20),P.Forward.Rotation());
        Seam->Tags.Add(TEXT("FernhallMaintenance"));
#if WITH_EDITOR
        Seam->SetActorLabel(TEXT("Fernhall_Maintenance_CollisionSeam"));
#endif
    }
    else Success = false;
    for (float Along : {400.f,1800.f})
    {
        auto* Join = World->SpawnActor<AActor>(P.At(1600,Along,-20),P.Forward.Rotation());
        if (!Join)
        {
            Success = false;
            continue;
        }
        Created.Add(Join);
        auto* Support = NewObject<UBoxComponent>(Join);
        Join->AddInstanceComponent(Support);
        Join->SetRootComponent(Support);
        Support->SetBoxExtent(FVector(400,2,20));
        Support->SetCollisionProfileName(TEXT("BlockAll"));
        Support->SetCanEverAffectNavigation(true);
        Support->SetHiddenInGame(true);
        Support->RegisterComponent();
        Support->SetWorldLocationAndRotation(P.At(1600,Along,-20),P.Forward.Rotation());
        Join->Tags.Add(TEXT("FernhallMaintenance"));
#if WITH_EDITOR
        Join->SetActorLabel(TEXT("Fernhall_Maintenance_CorridorCollisionJoin"));
#endif
    }
    // Bay shoulders and lintel close the replaced building except its doorway.
    Box(0,-450,350,FVector(300,100,700),Wall); Box(0,450,350,FVector(300,100,700),Wall);
    Box(0,0,625,FVector(300,800,150),Wall);
    Box(1000,-450,250,FVector(2000,100,500),Wall);
    Box(550,450,250,FVector(1100,100,500),Wall);
    Box(2050,650,250,FVector(100,2100,500),Wall);
    Box(1150,1100,250,FVector(100,1400,500),Wall);
    Box(1150,3900,250,FVector(100,4200,500),Wall);
    Box(4850,3900,250,FVector(100,4300,500),Wall);
    Box(3000,6050,250,FVector(3800,100,500),Wall);
    Box(3450,1750,250,FVector(2900,100,500),Wall);
    Box(3100,3300,200,FVector(500,300,400),Wall);
    Box(2300,4800,200,FVector(300,500,400),Wall);
    Box(4100,4000,60,FVector(350,120,120),Rust);
    Box(1900,2700,60,FVector(120,350,120),Rust);
    if (!Success)
    {
        for (AActor* Actor : Created) Actor->Destroy();
        return false;
    }
    if (auto* A=BreakerPlaceEnvironmentDressing(World,TEXT("Column_Pipes"),P.At(4550,5550),650,0)) A->Tags.Add(TEXT("FernhallMaintenance"));
    if (auto* A=BreakerPlaceEnvironmentDressing(World,TEXT("Door_Metal"),P.At(4750,4000),320,0)) A->Tags.Add(TEXT("FernhallMaintenance"));
    // O2 PLACEHOLDER maintenance corner: original textured hardware frames
    // an asymmetric service gantry, entirely outside the walked route.
    for (float Along : {5100.f,5800.f})
        if (auto* A = BreakerPlaceEnvironmentDressing(World, TEXT("Column_MetalSupport"), P.At(4300,Along), 850, 0))
        {
            A->Tags.Add(TEXT("FernhallMaintenance"));
#if WITH_EDITOR
            A->SetActorLabel(TEXT("Fernhall_Maintenance_GantrySupport"));
#endif
        }
    auto Detail = [&](float U, float V, float Z, FVector Size, FLinearColor Color, const TCHAR* Label)
    {
        auto* A = World->SpawnActor<AStaticMeshActor>(P.At(U,V,Z), P.Forward.Rotation());
        if (!A) return static_cast<AStaticMeshActor*>(nullptr);
        auto* M = A->GetStaticMeshComponent();
        M->SetMobility(EComponentMobility::Movable);
        M->SetStaticMesh(Cube);
        M->SetWorldScale3D(Size / 100.f);
        M->SetCollisionProfileName(TEXT("NoCollision"));
        M->SetCanEverAffectNavigation(false);
        M->SetCastShadow(Z > 20);
        auto* Tint = UMaterialInstanceDynamic::Create(Material,M);
        Tint->SetVectorParameterValue(TEXT("Color"),Color);
        M->SetMaterial(0,Tint);
        M->SetMobility(EComponentMobility::Static);
        A->SetActorEnableCollision(false);
        A->Tags.Add(TEXT("FernhallMaintenance"));
#if WITH_EDITOR
        A->SetActorLabel(Label);
#endif
        return A;
    };
    Detail(4300,5450,850,FVector(110,850,100),Rust,TEXT("Fernhall_Maintenance_GantryBeam"));
    // Shallow decals represented by separated slabs: their bottoms sit above
    // the floor, and alternating stripes never share a coplanar surface.
    for (int32 Index=0; Index<8; ++Index)
        Detail(3800,2400+Index*400,2,FVector(30,180,2),FLinearColor(.60f,.51f,.29f),TEXT("Fernhall_Maintenance_ServiceStripe"));
    Detail(4600,3650,2,FVector(28,2300,2),FLinearColor(.10f,.12f,.11f),TEXT("Fernhall_Maintenance_Drain"));
    if (auto* Lamp = Detail(4300,5450,740,FVector(100,100,18),FLinearColor(.75f,.65f,.41f),TEXT("Fernhall_Maintenance_ServiceLamp")))
    {
        auto* Light = NewObject<UPointLightComponent>(Lamp);
        Lamp->AddInstanceComponent(Light);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensity(12000);
        Light->SetAttenuationRadius(1500);
        Light->SetLightColor(FLinearColor(1.f,.72f,.42f));
        Light->SetCastShadows(false);
        Light->RegisterComponent();
        Light->SetWorldLocation(P.At(4200,5450,650));
    }
    return Success;
}
