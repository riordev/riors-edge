#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCoreBreakRuntimeObserver.generated.h"

class UBreakerCombatComponent;

UCLASS()
class UBreakerCoreBreakRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY() FBreakerDamageRequest Reentry;
    TArray<FBreakerHitContext> Hits;
    TArray<float> ArmorAtCallback;
    bool bReenter = false;
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
};
