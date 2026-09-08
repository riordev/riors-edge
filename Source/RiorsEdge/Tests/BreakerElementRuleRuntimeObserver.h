#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerStatusComponent.h"
#include "BreakerElementRuleRuntimeObserver.generated.h"
class UBreakerCombatComponent;
UCLASS()
class UBreakerElementRuleRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> Combat;
    UPROPERTY() TObjectPtr<UBreakerStatusComponent> Status;
    TArray<FBreakerHitContext> Hits;
    bool bRepeatRift = false;
    bool bReenterVoid = false;
    float BudgetAfterReentry = -1;
    float ConsumedBudget = -1;
    UFUNCTION() void OnHit(const FBreakerHitContext& Hit);
    UFUNCTION() void OnConsumed(const FBreakerActiveStatus& Active);
};
