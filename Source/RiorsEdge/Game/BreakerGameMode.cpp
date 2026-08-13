#include "Game/BreakerGameMode.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerTargetDummy.h"
#include "Combat/BreakerEnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "UI/BreakerPlaytestHUD.h"
#include "EngineUtils.h"
#include "UObject/UObjectGlobals.h"

ABreakerGameMode::ABreakerGameMode()
{
    DefaultPawnClass = ABreakerCharacter::StaticClass();
    static const TCHAR* PlayerBlueprintPath =
        TEXT("/Game/ProjectBreaker/Characters/BP_BreakerCharacter.BP_BreakerCharacter_C");
    if (UClass* PlayerBlueprint = StaticLoadClass(
        ABreakerCharacter::StaticClass(), nullptr, PlayerBlueprintPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        DefaultPawnClass = PlayerBlueprint;
    }
    HUDClass = ABreakerPlaytestHUD::StaticClass();
}

void ABreakerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    if (bPlaytestTargetsSpawned || !NewPlayer || !NewPlayer->GetPawn() || !GetWorld()) return;
    SpawnSafeZone(NewPlayer->GetPawn());
    SpawnPlaytestTargets(NewPlayer->GetPawn());
    SpawnMovementCourse(NewPlayer->GetPawn());
    SpawnCombatEncounter(NewPlayer->GetPawn());
}

namespace
{
    AStaticMeshActor* SpawnGymBlock(UWorld* World, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        if (!World) return nullptr;
        AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(Location, Rotation);
        if (!Block) return nullptr;
        UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetWorldScale3D(Scale);
        Mesh->SetMobility(EComponentMobility::Static);
        Block->SetActorLabel(TEXT("Runtime_PlaytestFacility"));
        return Block;
    }
}

void ABreakerGameMode::SpawnPlaytestTargets(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;

    const FVector Origin = Pawn->GetActorLocation();
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    const FVector TargetOffsets[] =
    {
        Forward * 1200.0f - Right * 300.0f,
        Forward * 2400.0f + Right * 350.0f,
        Forward * 4500.0f,
        Forward * 2100.0f - Right * 850.0f
    };
    const EBreakerTargetProfile Profiles[] =
    {
        EBreakerTargetProfile::Health,
        EBreakerTargetProfile::Shielded,
        EBreakerTargetProfile::Armored,
        EBreakerTargetProfile::Moving
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TargetOffsets); ++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerTargetDummy* Target = GetWorld()->SpawnActor<ABreakerTargetDummy>(ABreakerTargetDummy::StaticClass(), Origin + TargetOffsets[Index], FRotator::ZeroRotator, Params))
        {
            Target->ConfigureProfile(Profiles[Index]);
        }
    }
    bPlaytestTargetsSpawned = true;
}

void ABreakerGameMode::SpawnMovementCourse(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    const FVector Origin = Pawn->GetActorLocation();
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();

    // Mantle staircase: 50 / 100 / 145 cm tops.
    SpawnGymBlock(GetWorld(), Origin - Right * 900.0f + Forward * 900.0f + FVector(0, 0, 25), FVector(1.5f, 1.5f, 0.5f));
    SpawnGymBlock(GetWorld(), Origin - Right * 900.0f + Forward * 1250.0f + FVector(0, 0, 50), FVector(1.5f, 1.5f, 1.0f));
    SpawnGymBlock(GetWorld(), Origin - Right * 900.0f + Forward * 1650.0f + FVector(0, 0, 72.5f), FVector(1.5f, 1.5f, 1.45f));

    // Dash distance markers and staggered collision gates.
    for (int32 Marker = 1; Marker <= 4; ++Marker)
    {
        SpawnGymBlock(GetWorld(), Origin + Right * 1050.0f + Forward * (Marker * 500.0f) + FVector(0, 0, 60), FVector(0.08f, 2.0f, 1.2f));
    }

    // Gap platforms and parallel walls for wall-ride / wall-jump checks.
    SpawnGymBlock(GetWorld(), Origin + Forward * 3000.0f - Right * 1300.0f + FVector(0, 0, 15), FVector(4.0f, 3.0f, 0.3f));
    SpawnGymBlock(GetWorld(), Origin + Forward * 4200.0f - Right * 1300.0f + FVector(0, 0, 15), FVector(4.0f, 3.0f, 0.3f));
    SpawnGymBlock(GetWorld(), Origin + Forward * 3400.0f + Right * 1350.0f + FVector(0, 0, 180), FVector(12.0f, 0.25f, 3.6f));
    SpawnGymBlock(GetWorld(), Origin + Forward * 3400.0f + Right * 2050.0f + FVector(0, 0, 180), FVector(12.0f, 0.25f, 3.6f));

    // Flat and sloped slide lanes.
    SpawnGymBlock(GetWorld(), Origin - Forward * 900.0f - Right * 1200.0f + FVector(0, 0, 20), FVector(8.0f, 2.5f, 0.2f));
    SpawnGymBlock(GetWorld(), Origin - Forward * 2100.0f + Right * 1200.0f + FVector(0, 0, 240), FVector(8.0f, 2.5f, 0.2f), FRotator(0, 0, -12.0f));
}

void ABreakerGameMode::SpawnSafeZone(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    SafeZoneCenter = Pawn->GetActorLocation() - FVector(0.0f, 0.0f, 88.0f);
    bSafeZoneSet = true;

    // Visible pad so the boundary reads at a glance: wide flat cylinder with
    // a thin rim ring at the zone radius.
    AStaticMeshActor* Pad = GetWorld()->SpawnActor<AStaticMeshActor>(SafeZoneCenter + FVector(0, 0, 2.0f), FRotator::ZeroRotator);
    if (Pad)
    {
        UStaticMeshComponent* Mesh = Pad->GetStaticMeshComponent();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
        Mesh->SetWorldScale3D(FVector(SafeZoneRadius / 50.0f, SafeZoneRadius / 50.0f, 0.04f));
        Mesh->SetMobility(EComponentMobility::Static);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Pad->SetActorLabel(TEXT("Runtime_SafeZone"));
    }
}

bool ABreakerGameMode::IsInSafeZone(const FVector& Location) const
{
    return bSafeZoneSet && FVector::DistSquared2D(Location, SafeZoneCenter) <= FMath::Square(SafeZoneRadius);
}

void ABreakerGameMode::SpawnCombatEncounter(const APawn* Pawn)
{
    if (!Pawn || !GetWorld()) return;
    const FVector Origin = Pawn->GetActorLocation();
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    const float LateralOffsets[] = { -450.0f, 0.0f, 450.0f };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        // Spawn well outside the safe zone so the fight starts on the
        // player's terms.
        const FVector SpawnLocation = Origin + Forward * (SafeZoneRadius + 1200.0f + Index * 300.0f) + Right * LateralOffsets[Index];
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (ABreakerEnemy* Enemy = GetWorld()->SpawnActor<ABreakerEnemy>(ABreakerEnemy::StaticClass(), SpawnLocation, FRotator::ZeroRotator, Params))
        {
            Enemy->ConfigureEncounter(SpawnLocation, Index * 1.7f);
        }
    }
}

void ABreakerGameMode::ResetPlaytestTargets()
{
    if (!GetWorld()) return;
    for (TActorIterator<ABreakerTargetDummy> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It) It->Destroy();
    bPlaytestTargetsSpawned = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get(); PC && PC->GetPawn())
        {
            SpawnPlaytestTargets(PC->GetPawn());
            SpawnCombatEncounter(PC->GetPawn());
            break;
        }
    }
}
