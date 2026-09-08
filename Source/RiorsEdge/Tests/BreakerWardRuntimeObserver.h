#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerStatusComponent.h"
#include "BreakerWardRuntimeObserver.generated.h"
UCLASS()
class UBreakerWardRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UBreakerStatusComponent> Status;
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> Combat;
    int32 Refusals = 0;
    bool bReenter = false;
    UFUNCTION() void OnAvoided(const FBreakerActiveStatus& Active);
};
