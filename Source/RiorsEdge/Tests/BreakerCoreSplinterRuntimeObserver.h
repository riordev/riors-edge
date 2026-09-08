#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCoreSplinterRuntimeObserver.generated.h"

class ABreakerRocketProjectile;
UCLASS()
class UBreakerCoreSplinterRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    TArray<FBreakerHitContext> Hits;
    UPROPERTY() TObjectPtr<ABreakerRocketProjectile> ReentryRocket;
    UPROPERTY() TObjectPtr<AActor> ReentryTarget;
    bool bReenter = false;
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
};
