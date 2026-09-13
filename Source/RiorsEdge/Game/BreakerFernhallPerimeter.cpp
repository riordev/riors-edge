#include "Game/BreakerFernhallPerimeter.h"
#include "Game/BreakerEnvironmentDressing.h"
#include "Game/BreakerZoneBuilder.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void BreakerBuildFernhallPerimeter(UWorld* World,const TArray<FBreakerZonePiece>& Pieces)
{
    if(!World)return;
    auto Bounds=[&](const TCHAR* Name,FBox& Out)
    {
        const auto* Piece=Pieces.FindByPredicate([&](const auto& P){return P.Name==Name;});
        auto* Mesh=Piece?Cast<UStaticMesh>(Piece->MeshPath.TryLoad()):nullptr;
        if(!Mesh)return false;Out=Mesh->GetBoundingBox();return true;
    };
    FBox Entry,Sub;if(!Bounds(TEXT("flr_yard"),Entry)||!Bounds(TEXT("flr_yard_sub"),Sub))return;
    auto Place=[&](const TCHAR* Mesh,FVector Ground,float Height,float Yaw,FName Landmark)
    {
        if(auto* Actor=BreakerPlaceEnvironmentDressing(World,Mesh,Ground,Height,Yaw))
        {
            Actor->Tags.Add(TEXT("FernhallPerimeterPass"));Actor->Tags.Add(Landmark);
        }
    };
    // O2 PLACEHOLDER composition. Outside-facing banks only: no inner seam,
    // courtyard, enemy/cache/gate transform or original collision is changed.
    const float EntrySide=Entry.GetCenter().Y>=Sub.GetCenter().Y?1.f:-1.f;
    const float EntryEdge=Entry.GetCenter().Y+EntrySide*Entry.GetExtent().Y;
    for(int32 Cluster=0;Cluster<3;++Cluster)
    {
        const float X=Entry.GetCenter().X+(Cluster-1)*2200.f;
        Place(TEXT("CommonTree_1"),FVector(X,EntryEdge+EntrySide*1900,0),1700+Cluster*220,35+Cluster*67,TEXT("Fernhall.Landmark.ReclaimedGrove"));
        Place(TEXT("DeadTree_1"),FVector(X+510,EntryEdge+EntrySide*2400,0),2100-Cluster*150,110+Cluster*41,TEXT("Fernhall.Landmark.ReclaimedGrove"));
        Place(TEXT("Bush_Common"),FVector(X-310,EntryEdge+EntrySide*1200,0),240,Cluster*73,TEXT("Fernhall.Landmark.ReclaimedGrove"));
        Place(TEXT("Fern_1"),FVector(X+150,EntryEdge+EntrySide*1000,0),150,Cluster*37,TEXT("Fernhall.Landmark.ReclaimedGrove"));
    }
    const float SubSide=-EntrySide;
    const float SubEdge=Sub.GetCenter().Y+SubSide*Sub.GetExtent().Y;
    for(int32 Stack=0;Stack<3;++Stack)
    {
        const float X=Sub.GetCenter().X+(Stack-1)*1500.f;
        Place(TEXT("Column_Pipes"),FVector(X,SubEdge+SubSide*(2700+Stack*500),0),2600+Stack*550,90,TEXT("Fernhall.Landmark.PipeWorks"));
        Place(TEXT("Column_MetalSupport"),FVector(X-400,SubEdge+SubSide*2200,0),1900+Stack*240,90,TEXT("Fernhall.Landmark.PipeWorks"));
        Place(TEXT("Bush_Common"),FVector(X+350,SubEdge+SubSide*1500,0),280,Stack*47,TEXT("Fernhall.Landmark.PipeWorks"));
    }
    // Reclaimed ground stays in deliberate bands beside the route and wall
    // bases. Instanced low ferns add ground cover without collision or
    // hundreds of ticking actors. O2 PLACEHOLDER heights, density and palette.
    auto* Blade = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Breaker/EnvironmentKit/Fern_1/Fern_1/StaticMeshes/Fern_1.Fern_1"));
    if (Blade && Blade->GetBoundingBox().GetSize().Z > UE_SMALL_NUMBER)
    {
        auto* Grass = World->SpawnActor<AActor>();
        if (!Grass) return;
        auto* Mesh = NewObject<UInstancedStaticMeshComponent>(Grass);
        Grass->AddInstanceComponent(Mesh); Grass->SetRootComponent(Mesh);
        BreakerConfigureDressingNanite(Mesh,Blade);
        Mesh->SetStaticMesh(Blade);Mesh->SetCollisionProfileName(TEXT("NoCollision"));
        Mesh->SetCanEverAffectNavigation(false);Mesh->SetMobility(EComponentMobility::Static);
        if (auto* LeafMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Breaker/Materials/M_BreakerUnderstory.M_BreakerUnderstory")))
            for (int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot) Mesh->SetMaterial(Slot,LeafMaterial);
        Mesh->RegisterComponent();Grass->Tags.Add(TEXT("FernhallGroundCover"));Grass->SetActorTickEnabled(false);
        FRandomStream Random(913);
        for (const FBox& Floor : {Entry,Sub})
            for (float Side : {-1.f,1.f})
                for (float Along=Floor.Min.X+700; Along<Floor.Max.X-700; Along+=350)
                    for (float Band : {900.f,float(Floor.GetExtent().Y-450)})
                    {
                        const FVector Base(Along,Floor.GetCenter().Y+Side*Band,Floor.Max.Z);
                        for (int32 Index=0;Index<6;++Index)
                        {
                            const FVector Foot=Base+FVector(Random.FRandRange(-110,110),Random.FRandRange(-100,100),0);
                            FHitResult Hit; FCollisionQueryParams Query;
                            if (!World->LineTraceSingleByChannel(Hit,Foot+FVector(0,0,100),Foot-FVector(0,0,100),ECC_WorldStatic,Query)
                                || FMath::Abs(Hit.ImpactPoint.Z-Floor.Max.Z)>15 || Hit.ImpactNormal.Z<.9f) continue;
                            const float Scale=Random.FRandRange(28,48)/Blade->GetBoundingBox().GetSize().Z;
                            const FRotator Rotation(0,Random.FRandRange(0,360),0);
                            const FBox Oriented=Blade->GetBoundingBox().TransformBy(FTransform(Rotation));
                            const FVector Bottom(Oriented.GetCenter().X,Oriented.GetCenter().Y,Oriented.Min.Z);
                            Mesh->AddInstance(FTransform(Rotation,Hit.ImpactPoint-Bottom*Scale,FVector(Scale)),true);
                        }
                    }
    }

}
