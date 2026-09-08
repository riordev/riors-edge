#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerLootPickupRuntimeObserver.generated.h"
class ABreakerLootPickup;
class ABreakerCharacter;
UCLASS()
class UBreakerLootPickupRuntimeObserver : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<ABreakerLootPickup> Pickup;
    UPROPERTY() TObjectPtr<ABreakerCharacter> Player;
    int32 Calls=0;
    bool bReentryAccepted=false;
    UFUNCTION() void Acquired(const FBreakerItemInstance& Item);
};
