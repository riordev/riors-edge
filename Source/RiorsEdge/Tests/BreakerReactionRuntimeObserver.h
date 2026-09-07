#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "Combat/BreakerStatusComponent.h"
#include "UObject/Object.h"
#include "BreakerReactionRuntimeObserver.generated.h"

class UBreakerCombatComponent;

UCLASS()
class UBreakerReactionRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY() TObjectPtr<UBreakerStatusComponent> Status;
    UPROPERTY() TObjectPtr<AActor> Reactor;
    TArray<FBreakerHitContext> Hits;
    bool bReactOnRotTick = false;
    bool bAdvanceOnRotTick = false;
    bool bReenterOnConsume = false;
    bool bKillOnOuterHit = false;
    int32 Consumed = 0;
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
    UFUNCTION() void OnConsumed(const FBreakerActiveStatus& Active);
};
