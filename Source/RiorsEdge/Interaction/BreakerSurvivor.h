#pragma once

#include "CoreMinimal.h"
#include "Interaction/BreakerNPC.h"
#include "Game/BreakerErasedEarthBuilder.h"
#include "BreakerSurvivor.generated.h"

class ABreakerCharacter;
class ABreakerSurvivor;

UENUM(BlueprintType)
enum class EBreakerSurvivorEscortState : uint8 { Sheltered, Escorting, Failed, AtExtraction };

DECLARE_MULTICAST_DELEGATE_TwoParams(FBreakerSurvivorExtractionReady, ABreakerSurvivor*, ABreakerCharacter*);
DECLARE_MULTICAST_DELEGATE_OneParam(FBreakerSurvivorEscortFailed, ABreakerSurvivor*);

UCLASS()
class RIORSEDGE_API ABreakerSurvivor : public ABreakerNPC
{
    GENERATED_BODY()
public:
    ABreakerSurvivor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void ConfigureEscort(const FBreakerErasedEarthLayout& Layout);
    void ConfigureEscortRoute(FVector InShelter, FVector InExtraction, const TArray<FBreakerSurvivorRoutePoint>& Points, int32 PocketCount);
    bool TryBeginEscort(ABreakerCharacter* Player);
    void SetPocketCleared(int32 PocketIndex, bool bCleared = true);
    void SetDialoguePaused(bool bPaused);
    void ResetToShelter();
    bool IsEscortActive() const { return EscortState == EBreakerSurvivorEscortState::Escorting; }
    bool IsAtExtraction() const;
    bool AreAllPocketsCleared() const;
    ABreakerCharacter* GetEscortPlayer() const { return EscortPlayer.Get(); }
    float GetLucidityRemaining() const { return LucidityRemaining; }
    int32 GetRouteIndex() const { return RouteIndex; }
    EBreakerSurvivorEscortState GetEscortState() const { return EscortState; }
    FBreakerSurvivorExtractionReady OnEscortReadyForExtraction;
    FBreakerSurvivorEscortFailed OnEscortFailed;

    // O2 encounter tuning, independent of damage/build balance.
    UPROPERTY(EditAnywhere, Category="Survivor|Escort") float LuciditySeconds = 240.0f;
    UPROPERTY(EditAnywhere, Category="Survivor|Escort") float WalkSpeed = 220.0f;
    UPROPERTY(EditAnywhere, Category="Survivor|Escort") float FollowRange = 900.0f;
    UPROPERTY(EditAnywhere, Category="Survivor|Escort") float ExtractionRadius = 180.0f;
    UPROPERTY(EditAnywhere, Category="Survivor|Escort") float CheckpointRadius = 45.0f;

protected:
    virtual FName GetEscortDialogueId() const { return TEXT("Survivor"); }
    virtual bool IsEscortAdmitted(const ABreakerCharacter* Player) const;
private:
    void FailEscort();
    bool MoveAlongRoute(float DeltaSeconds);
    FVector Shelter = FVector::ZeroVector;
    FVector Extraction = FVector::ZeroVector;
    TArray<FBreakerSurvivorRoutePoint> Route;
    TArray<bool> ClearedPockets;
    TWeakObjectPtr<ABreakerCharacter> EscortPlayer;
    EBreakerSurvivorEscortState EscortState = EBreakerSurvivorEscortState::Sheltered;
    float LucidityRemaining = 0.0f;
    int32 RouteIndex = 0;
    bool bDialoguePaused = false;
};
