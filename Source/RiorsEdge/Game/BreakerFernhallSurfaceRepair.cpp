#include "Game/BreakerFernhallSurfaceRepair.h"
#include "Game/BreakerZoneBuilder.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

void BreakerRepairFernhallSurface(UStaticMeshComponent* Component, const FBreakerZonePiece& Piece)
{
    if (!Component || !Component->GetStaticMesh()) return;
    const FString& Name = Piece.Name;
    const bool Wall = Name.StartsWith(TEXT("wall_")) && !Name.Contains(TEXT("seam"))
        && !Name.Contains(TEXT("bay")) && !Name.Contains(TEXT("dock")) && !Name.Contains(TEXT("gleg"));
    const bool Beam = Name.Contains(TEXT("_gleg")) || Name.Contains(TEXT("_grail"))
        || Name.Contains(TEXT("_gbrace")) || Name.Contains(TEXT("_rail")) || Name.Contains(TEXT("_cwrail"));
    const bool Column = Name.StartsWith(TEXT("dress_")) && (Name.Contains(TEXT("col"))
        || (Name.EndsWith(TEXT("s")) && Name.Len()>1 && FChar::IsDigit(Name[Name.Len()-2])));
    if (!Wall && !Beam && !Column) return;
    auto* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Breaker/Materials/M_BreakerWall.M_BreakerWall"));
    if (!Cube || !Material) return;
    const FBox Bounds = Component->GetStaticMesh()->GetBoundingBox();
    const FVector Center=Bounds.GetCenter();
    FVector Size=Bounds.GetSize();
    // Imported column planes read as suspended banners. Keep their base and
    // roof height, with a solid support profile. O2 PLACEHOLDER 50 cm width.
    if (Column) Size.X=Size.Y=50.f;
    // Replace the malformed tiled facade as one coherent structural surface.
    // Keep the authored outer bounds, route openings and cover footprints.
    Component->SetStaticMesh(Cube);
    Component->SetWorldLocation(Center);
    Component->SetWorldScale3D(Size/100.f);
    auto Paint = [&](UStaticMeshComponent* Mesh,FLinearColor Color)
    {
        auto* Tint=UMaterialInstanceDynamic::Create(Material,Mesh);
        Tint->SetVectorParameterValue(TEXT("Color"),Color);Mesh->SetMaterial(0,Tint);
    };
    Paint(Component,Wall ? FLinearColor(.28f,.30f,.28f) : FLinearColor(.16f,.19f,.18f)); // O2 PLACEHOLDER palette.
    if (!Wall) return;
    auto Detail=[&](FVector Position,FVector Dimensions,FLinearColor Color)
    {
        auto* Actor=Component->GetWorld()->SpawnActor<AStaticMeshActor>(Position,FRotator::ZeroRotator);
        if (!Actor) return;
        auto* Mesh=Actor->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cube);Mesh->SetWorldScale3D(Dimensions/100.f);
        Mesh->SetCollisionProfileName(TEXT("NoCollision"));Mesh->SetCanEverAffectNavigation(false);
        Paint(Mesh,Color);Mesh->SetMobility(EComponentMobility::Static);
        Actor->Tags.Add(TEXT("FernhallFacadeRepair"));Actor->SetActorTickEnabled(false);
    };
    // Continuous coping and plinth anchor the roof and ground; no floating
    // banners or miniature roof hardware. O2 PLACEHOLDER dimensions/colors.
    Detail(FVector(Center.X,Center.Y,Bounds.Max.Z-8),FVector(Size.X+6,Size.Y+6,16),FLinearColor(.13f,.15f,.14f));
    Detail(FVector(Center.X,Center.Y,Bounds.Min.Z+25),FVector(Size.X+4,Size.Y+4,50),FLinearColor(.20f,.21f,.19f));
    const bool AlongX = Size.X >= Size.Y;
    const float Length=AlongX?Size.X:Size.Y;
    const float Depth=AlongX?Size.Y:Size.X;
    for (float Along=-Length*.5f+200; Along < Length*.5f-100; Along+=400)
        for (float Side : {-1.f,1.f})
        {
            FVector At=Center;At.Z=Bounds.Min.Z+Size.Z*.5f;
            if (AlongX) { At.X+=Along;At.Y+=Side*(Depth*.5f+2); }
            else { At.Y+=Along;At.X+=Side*(Depth*.5f+2); }
            Detail(At,AlongX?FVector(5,4,Size.Z-60):FVector(4,5,Size.Z-60),FLinearColor(.17f,.19f,.18f));
            if (Size.Z>400)
            {
                At.Z=Bounds.Min.Z+300;
                Detail(At,AlongX?FVector(180,6,80):FVector(6,180,80),FLinearColor(.07f,.10f,.10f));
            }
        }
}
