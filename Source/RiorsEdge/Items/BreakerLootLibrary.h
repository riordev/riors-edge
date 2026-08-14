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

    // Step 6, finally: the fixed signature. Rolls the named legendary at this
    // item level — its guaranteed affixes first, then ordinary rolls to fill
    // out an Anomalous affix count, then its rule.
    //
    // A legendary is always Anomalous, which means the existing equip cap of
    // ONE Anomalous piece is also the cap on legendaries. That is the design,
    // not a side effect: "three build-defining items" means the build is
    // defined by the one you chose, and three that stack would be a set bonus.
    UFUNCTION(BlueprintPure, Category="Items|Loot")
    static FBreakerItemInstance RollLegendary(FName LegendaryId, int32 ItemLevel, int32 RandomSeed);

    // O2 PLACEHOLDER. Chance that an Anomalous drop in a slot that HAS a
    // legendary is that legendary instead of an ordinary rule roll. Anomalous
    // is already ~0.5% of drops before drop chance, and only three of eight
    // slots have one, so the effective rate is deliberately rare.
    static constexpr float LegendaryChanceWithinAnomalous = 0.25f;

private:
    // The real pipeline. RollItem is this with the legendary branch armed and
    // RollLegendary is this with it disarmed — without the flag, rolling a
    // legendary's ordinary affixes would re-enter the legendary branch and
    // recurse until the stack ran out.
    static FBreakerItemInstance RollItemInternal(FName DefinitionId, EBreakerEquipSlot Slot, EBreakerItemRarity Rarity,
        int32 ItemLevel, int32 RandomSeed, bool bAllowLegendary);
};
