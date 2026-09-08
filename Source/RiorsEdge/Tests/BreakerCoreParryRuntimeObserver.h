#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCoreParryRuntimeObserver.generated.h"

class UBreakerCombatComponent;

UCLASS()
class UBreakerCoreParryRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY() FBreakerDamageRequest Reentry;
    int32 HealingEvents = 0;
    bool bReenter = false;
    bool bReentryParried = false;
    UFUNCTION() void OnHealed(const FBreakerHealResult& Result);
};
