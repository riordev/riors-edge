#include "Game/BreakerFernhallCourtyardEncounter.h"
#include "Game/BreakerFernhallCourtyardBuilder.h"
#include "Game/BreakerZoneBuilder.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedEnemy.h"
#include "Playtest/BreakerKillTelemetryComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

TArray<ABreakerEnemy*> BreakerSpawnFernhallCourtyardEncounter(UWorld* World, const BreakerFernhallCourtyard::FPlan& Plan)
{
    TArray<ABreakerEnemy*> Result;
    if (!World || Plan.MeleeSpawns.Num() != 3) return Result;
    auto Spawn = [&](const FVector& Point, bool Ranged)
    {
        FHitResult Floor;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CourtyardEnemyFloor), false);
        if (!World->LineTraceSingleByObjectType(Floor, Point + FVector(0,0,500), Point - FVector(0,0,500),
            FCollisionObjectQueryParams(ECC_WorldStatic), Query) || Floor.ImpactNormal.Z < .7f) return false;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Enemy = World->SpawnActor<ABreakerEnemy>(Ranged ? ABreakerRangedEnemy::StaticClass() : ABreakerEnemy::StaticClass(),
            Floor.ImpactPoint + FVector(0,0,200), (-Plan.Forward).Rotation(), Params);
        if (!Enemy) return false;
        Enemy->ConfigureWave(UBreakerZoneBuilder::FernhallRiftFor(NAME_None).EffectiveAreaLevel());
        auto* Capsule = Enemy->FindComponentByClass<UCapsuleComponent>();
        if (!Capsule) { Enemy->Destroy(); return false; }
        const FVector At = Floor.ImpactPoint + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+2);
        Query.AddIgnoredActor(Enemy);
        if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query))
        { Enemy->Destroy(); return false; }
        Enemy->SetActorLocation(At); Enemy->ConfigureEncounter(At, Result.Num()*1.3f);
        Enemy->Tags.Add(TEXT("Fernhall.Outdoor.Courtyard"));
        UBreakerKillTelemetryComponent::AttachTo(Enemy); Result.Add(Enemy);
        return true;
    };
    bool Valid = true;
    for (const FVector& At : Plan.MeleeSpawns) Valid = Spawn(At, false) && Valid;
    Valid = Spawn(Plan.RangedSpawn, true) && Valid;
    if (!Valid)
    {
        for (auto* Enemy : Result) Enemy->Destroy();
        Result.Reset();
        UE_LOG(LogTemp, Error, TEXT("Courtyard encounter refused incomplete or obstructed roster"));
    }
    return Result;
}
