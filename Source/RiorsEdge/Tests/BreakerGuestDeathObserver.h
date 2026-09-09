#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BreakerGuestDeathObserver.generated.h"

UCLASS()
class UBreakerGuestDeathObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 Deaths = 0;
    int32 Restores = 0;
    UFUNCTION() void OnDeath() { ++Deaths; }
    UFUNCTION() void OnRestore() { ++Restores; }
};
