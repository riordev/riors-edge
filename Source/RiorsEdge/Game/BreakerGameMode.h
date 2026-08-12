#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BreakerGameMode.generated.h"

UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ABreakerGameMode();
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    UFUNCTION(BlueprintCallable, Category="Playtest") void ResetPlaytestTargets();

private:
    bool bPlaytestTargetsSpawned = false;
    void SpawnPlaytestTargets(const APawn* Pawn);
    void SpawnMovementCourse(const APawn* Pawn);
    void SpawnCombatEncounter(const APawn* Pawn);
};
