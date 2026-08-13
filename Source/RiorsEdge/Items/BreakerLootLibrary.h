#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerLootLibrary.generated.h"

UCLASS()
class RIORSEDGE_API UBreakerLootLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Steps 1-2 of the roll pipeline: rarity from weighted table, with drop
    // chance bonus shifting weight out of Standard.
    UFUNCTION(BlueprintPure, Category="Items|Loot")
    static EBreakerItemRarity RollRarity(int32 RandomSeed, float DropChanceBonusPercent);

    // Steps 2-5: affix count, affix selection (no duplicates, prefix/suffix
    // caps of four each), tier per affix (item level gated, rarity capped,
    // weighted toward the low end), then value within the tier.
    UFUNCTION(BlueprintPure, Category="Items|Loot")
    static FBreakerItemInstance RollItem(FName DefinitionId, EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel, int32 RandomSeed);

    UFUNCTION(BlueprintPure, Category="Items|Loot")
    static int32 CountAffixesOfCategory(const FBreakerItemInstance& Item, EBreakerAffixCategory Category);
};
