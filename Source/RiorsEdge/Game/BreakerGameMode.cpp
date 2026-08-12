#include "Game/BreakerGameMode.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerTargetDummy.h"
#include "GameFramework/PlayerController.h"
#include "UI/BreakerPlaytestHUD.h"

ABreakerGameMode::ABreakerGameMode()
{
    DefaultPawnClass = ABreakerCharacter::StaticClass();
    HUDClass = ABreakerPlaytestHUD::StaticClass();
}

void ABreakerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    if (bPlaytestTargetsSpawned || !NewPlayer || !NewPlayer->GetPawn() || !GetWorld()) return;

    const APawn* Pawn = NewPlayer->GetPawn();
    const FVector Origin = Pawn->GetActorLocation();
    const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = Pawn->GetActorRightVector().GetSafeNormal2D();
    const FVector TargetOffsets[] =
    {
        Forward * 1200.0f - Right * 300.0f,
        Forward * 2400.0f + Right * 350.0f,
        Forward * 4500.0f
    };
    for (const FVector& Offset : TargetOffsets)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        GetWorld()->SpawnActor<ABreakerTargetDummy>(ABreakerTargetDummy::StaticClass(), Origin + Offset, FRotator::ZeroRotator, Params);
    }
    bPlaytestTargetsSpawned = true;
}
