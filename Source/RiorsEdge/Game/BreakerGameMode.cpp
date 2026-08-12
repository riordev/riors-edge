#include "Game/BreakerGameMode.h"

#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerTargetDummy.h"
#include "GameFramework/PlayerController.h"
#include "UI/BreakerPlaytestHUD.h"
#include "EngineUtils.h"

ABreakerGameMode::ABreakerGameMode()
{
    DefaultPawnClass = ABreakerCharacter::StaticClass();
    HUDClass = ABreakerPlaytestHUD::StaticClass();
}

void ABreakerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
    if (bPlaytestTargetsSpawned || !NewPlayer || !NewPlayer->GetPawn() || !GetWorld()) return;
    SpawnPlaytestTargets(NewPlayer->GetPawn());
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

void ABreakerGameMode::ResetPlaytestTargets()
{
    if (!GetWorld()) return;
    for (TActorIterator<ABreakerTargetDummy> It(GetWorld()); It; ++It) It->Destroy();
    bPlaytestTargetsSpawned = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get(); PC && PC->GetPawn())
        {
            SpawnPlaytestTargets(PC->GetPawn());
            break;
        }
    }
}
