#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerStatusComponent.h"
#include "BreakerLongDarkRuntimeObserver.generated.h"
class UBreakerCombatComponent;
UCLASS()
class UBreakerLongDarkRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> ReentryTarget;
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> KillTarget;
    UPROPERTY() TObjectPtr<AActor> DestroyTarget;
    FBreakerDamageRequest ReentryHit;
    int32 Consumed = 0;
    bool bReenter = false;
    UFUNCTION() void OnConsumed(const FBreakerActiveStatus& Status);
};
