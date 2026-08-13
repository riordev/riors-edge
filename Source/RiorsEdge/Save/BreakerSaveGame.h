#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/BreakerItemTypes.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerSaveGame.generated.h"

// Everything a character carries between sessions. Stable ids and rolled
// numbers only — never pointers or calculated attribute totals.
UCLASS()
class RIORSEDGE_API UBreakerSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static const TCHAR* DefaultSlotName() { return TEXT("BreakerSave0"); }

    UPROPERTY() FBreakerProgressionState Progression;
    UPROPERTY() TArray<FBreakerItemInstance> EquippedItems;
    UPROPERTY() TArray<FBreakerItemInstance> BackpackItems;
    UPROPERTY() EBreakerWeaponArchetype SlotOneArchetype = EBreakerWeaponArchetype::Rifle;
    UPROPERTY() EBreakerWeaponArchetype SlotTwoArchetype = EBreakerWeaponArchetype::Shotgun;
    UPROPERTY() int32 SaveVersion = 1;
};
