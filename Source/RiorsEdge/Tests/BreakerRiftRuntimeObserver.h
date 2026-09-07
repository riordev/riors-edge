#pragma once
#include "CoreMinimal.h"
#include "Combat/BreakerCombatTypes.h"
#include "UObject/Object.h"
#include "BreakerRiftRuntimeObserver.generated.h"

class UBreakerStatusComponent;

// Native delegate receiver for the isolated Rift transaction fixture. No
// production actor owns this object and it never loads or writes a save.
UCLASS()
class UBreakerRiftRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UBreakerStatusComponent> Status;
    TArray<FBreakerDamageResult> Results;
    bool bConsumeNext = false;
    float ConsumedBudget = 0;
    uint64 SeenSerial = 0;
    int32 Deaths = 0;
    UFUNCTION() void OnDamage(const FBreakerDamageResult& Result);
    UFUNCTION() void OnDeath();
};
