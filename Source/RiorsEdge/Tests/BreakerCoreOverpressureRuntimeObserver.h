#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCoreOverpressureRuntimeObserver.generated.h"
class UBreakerProgressionComponent;
class UBreakerCombatComponent;
UCLASS()
class UBreakerCoreOverpressureRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    TArray<FBreakerHitContext> Hits;
    UPROPERTY() TObjectPtr<UBreakerProgressionComponent> Progression;
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> SourceCombat;
    bool bRevokeOnParent=false;
    bool bRespecSucceeded=false;
    bool bKillSource=false;
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
};
