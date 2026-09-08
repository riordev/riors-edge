#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCoreLoudRuntimeObserver.generated.h"

UCLASS()
class UBreakerCoreLoudRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    TArray<FBreakerHitContext> Hits;
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
};
