#include "AI/BreakerNavBounds.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

namespace
{
    const FName BreakerNavBoundsTag(TEXT("BreakerNavBounds"));
    constexpr double BreakerNavBoundsRecheckSeconds = 1.0;

    // Last time each world was checked, so eighty possessions in one frame
    // cost one iteration.
    TMap<TWeakObjectPtr<UWorld>, double> BreakerNavBoundsLastCheck;

    ANavMeshBoundsVolume* BreakerNavBoundsFind(UWorld* World)
    {
        for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
        {
            if (It->ActorHasTag(BreakerNavBoundsTag)) return *It;
        }
        return nullptr;
    }

    UBoxComponent* BreakerNavBoundsBox(ANavMeshBoundsVolume* Volume)
    {
        return Volume ? Volume->FindComponentByClass<UBoxComponent>() : nullptr;
    }
}

FBox BreakerNavBounds::ComputeCoverage(UWorld* World)
{
    FBox Union(ForceInit);
    if (!World) return Union;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        const AStaticMeshActor* Actor = *It;
        if (!Actor || !Actor->GetActorEnableCollision()) continue;
        const UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        if (!Mesh || !Mesh->GetStaticMesh() || Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;
        Union += Mesh->Bounds.GetBox();
    }
    if (!Union.IsValid) return Union;
    return Union.ExpandBy(FVector(LateralPaddingCm, LateralPaddingCm, VerticalPaddingCm));
}

void BreakerNavBounds::EnsureCoverage(UWorld* World)
{
    if (!World || World->GetNetMode() == NM_Client) return;
    const double Now = World->GetTimeSeconds();
    if (const double* Last = BreakerNavBoundsLastCheck.Find(World))
    {
        if (Now - *Last < BreakerNavBoundsRecheckSeconds) return;
    }
    BreakerNavBoundsLastCheck.Add(World, Now);

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSys) return;
    const FBox Coverage = ComputeCoverage(World);
    if (!Coverage.IsValid) return;

    ANavMeshBoundsVolume* Volume = BreakerNavBoundsFind(World);
    UBoxComponent* Box = BreakerNavBoundsBox(Volume);
    if (Volume && Box)
    {
        const FBox Current = Box->Bounds.GetBox();
        if (Current.IsInside(Coverage.Min) && Current.IsInside(Coverage.Max)) return;
    }

    const bool bSpawned = Volume == nullptr;
    if (!Volume)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Volume = World->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(),
            Coverage.GetCenter(), FRotator::ZeroRotator, Params);
        if (!Volume) return;
        Volume->Tags.Add(BreakerNavBoundsTag);
        if (USceneComponent* Root = Volume->GetRootComponent())
        {
            Root->SetMobility(EComponentMobility::Movable);
        }
        // A volume spawned at runtime has no brush, so its own bounds are a
        // point. The navigation system reads the volume's COMPONENT bounds
        // (GetComponentsBoundingBox(true)), and a box child is the honest way
        // to give it some: the box IS the bounds, and resizing it resizes them.
        Box = NewObject<UBoxComponent>(Volume, TEXT("BreakerNavBoundsBox"));
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Box->SetMobility(EComponentMobility::Movable);
        Box->SetupAttachment(Volume->GetRootComponent());
        Box->RegisterComponent();
    }
    if (!Box) return;

    Volume->SetActorLocation(Coverage.GetCenter());
    Box->SetWorldLocation(Coverage.GetCenter());
    Box->SetBoxExtent(Coverage.GetExtent(), false);
    NavSys->OnNavigationBoundsUpdated(Volume);
    // The bounds update spawns the navmesh actor only on the navigation
    // system's next tick and only if none exists; asking for it here makes
    // the first possession the moment the mesh starts building, and puts the
    // answer in the log where a capture can read it.
    const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::Create);
    UE_LOG(LogTemp, Display, TEXT("[BreakerNav] bounds %s: %.0f x %.0f x %.0f cm at (%.0f, %.0f, %.0f); navmesh %s."),
        bSpawned ? TEXT("spawned") : TEXT("grown"),
        Coverage.GetSize().X, Coverage.GetSize().Y, Coverage.GetSize().Z,
        Coverage.GetCenter().X, Coverage.GetCenter().Y, Coverage.GetCenter().Z,
        NavData ? *NavData->GetName() : TEXT("MISSING"));
}
