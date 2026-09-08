#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerThreatRuntimeObserver.generated.h"
UCLASS()
class UBreakerThreatRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<AActor> LastRewardOwner;
    UPROPERTY() TObjectPtr<AActor> LastThreatSource;
    int32 Hits = 0;
    UFUNCTION() void Observe(const FBreakerHitContext& Hit);
};
