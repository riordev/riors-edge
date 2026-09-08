#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "BreakerFeedstockPickup.generated.h"
class ABreakerCharacter;
struct FBreakerHitContext;
class UStaticMeshComponent;

// One claim per actual enemy life, including pooled bodies restored for another wave.
UCLASS()
class UBreakerFeedstockDeathClaim : public UActorComponent
{
    GENERATED_BODY()
public:
    bool bClaimed = false;
    UFUNCTION() void ResetClaim() { bClaimed = false; }
};

UCLASS()
class RIORSEDGE_API ABreakerFeedstockPickup : public AActor
{
    GENERATED_BODY()
public:
    ABreakerFeedstockPickup();
    virtual void BeginPlay() override;
    static ABreakerFeedstockPickup* SpawnForKill(ABreakerCharacter* Player, const FBreakerHitContext& Hit);
    bool CanCollect(const ABreakerCharacter* Player) const;
    bool TryCollect(ABreakerCharacter* Player);
    float GetInteractionRange() const { return 350.0f; }
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    bool bConsumed = false;
};
