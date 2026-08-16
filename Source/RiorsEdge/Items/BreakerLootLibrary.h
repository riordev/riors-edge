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
    // The UNGATED rarity roll: the weighted table with the Drop Chance bonus
    // shifting weight out of Standard, and every gate open.
    //
    // NOT the drop pipeline. `Items/BreakerDropTable.h` is, and content must
    // call `UBreakerDropTableLibrary::RollDrop` — which runs a per-rank DROP
    // CHANCE step first and gates the high rarities on item level and monster
    // rank. This overload existing as the whole pipeline is precisely what the
    // owner's "every single enemy dropped an item" / "way way too many
    // Aberrants" playtest was reporting. It survives for callers that
    // legitimately have no rank and no item level (dev grants, fixtures,
    // crafting previews) and delegates to the same one weight table.
    UFUNCTION(BlueprintPure, Category="Items|Loot")
    static EBreakerItemRarity RollRarity(int32 RandomSeed, float DropChanceBonusPercent);

    // Steps 2-5: affix count, affix selection (no duplicates, prefix/suffix
    // caps of four each), tier per affix (item level gated, rarity capped,
    // weighted toward the low end), then value within the tier.
    UFUNCTION(BlueprintPure, Category="Items|Loot")
    static FBreakerItemInstance RollItem(FName DefinitionId, EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel, int32 RandomSeed);

    // Which equip slot a kill's drop lands in — THE production slot draw, and
    // the tests must draw it from here too (a test that picks slots its own
    // way is how the bug this fixes stayed invisible; see the .cpp).
    UFUNCTION(BlueprintPure, Category="Items|Loot")
    static EBreakerEquipSlot RollDropSlot(int32 RandomSeed);

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
