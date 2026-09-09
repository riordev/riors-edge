#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

// Configure only this decorative component, before assigning its imported mesh.
RIORSEDGE_API void BreakerConfigureDressingNanite(UStaticMeshComponent* Component, UStaticMesh* Mesh);

// Original materials and uniform proportions; only decorative placement.
// Desired height is O2 blockout tuning. Ground is the bottom of the mesh bounds.
inline AStaticMeshActor* BreakerPlaceEnvironmentDressing(UWorld* World, const TCHAR* Name,
    const FVector& Ground, float HeightCm, float YawDegrees)
{
    if (!World || HeightCm <= 0) return nullptr;
    const FString Path = FString::Printf(TEXT("/Game/Breaker/EnvironmentKit/%s/%s/StaticMeshes/%s.%s"),
        Name, Name, Name, Name);
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
    if (!Mesh || Mesh->GetBounds().BoxExtent.Z <= UE_SMALL_NUMBER) return nullptr;
    // This kit support is authored lying on its Y axis (Z is only 4.4 cm
    // thick). Orient it before sizing; scaling thickness made a giant plane.
    const bool bHorizontalSupport = FCString::Strcmp(Name, TEXT("Column_MetalSupport")) == 0;
    const FRotator Rotation(0, YawDegrees, bHorizontalSupport ? 90.0f : 0.0f);
    const FBox OrientedBounds = Mesh->GetBoundingBox().TransformBy(FTransform(Rotation));
    const float OrientedHeight = OrientedBounds.GetSize().Z;
    if (OrientedHeight <= UE_SMALL_NUMBER) return nullptr;
    const float Scale = HeightCm / OrientedHeight;
    const FVector Center = OrientedBounds.GetCenter();
    const FVector Bottom(Center.X, Center.Y, OrientedBounds.Min.Z);
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Ground - Bottom * Scale, Rotation);
    if (!Actor) return nullptr;
    UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
    Component->SetMobility(EComponentMobility::Movable);
    BreakerConfigureDressingNanite(Component, Mesh);
    Component->SetStaticMesh(Mesh);
    Component->SetWorldScale3D(FVector(Scale));
    Component->SetCollisionProfileName(TEXT("NoCollision"));
    Component->SetCanEverAffectNavigation(false);
    Component->SetMobility(EComponentMobility::Static);
    Actor->SetActorEnableCollision(false);
    Actor->SetActorTickEnabled(false);
    Actor->Tags.Add(TEXT("BreakerEnvironmentDressing"));
#if WITH_EDITOR
    Actor->SetActorLabel(FString::Printf(TEXT("Runtime_Environment_%s"), Name));
#endif
    return Actor;
}
