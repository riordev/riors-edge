#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/BreakerItemTypes.h"
#include "BreakerAffixLibrary.generated.h"

UCLASS()
class RIORSEDGE_API UBreakerAffixLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Linear from T8 to T1, then T0 = 1.4x T1 and T-1 = 1.8x T1.
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static float ValueForTier(const FBreakerAffixDefinition& Affix, int32 Tier);

    // Item level gates the best rollable tier. Level 1 rolls only T8; the
    // full T1 band opens at 50. T0/T-1 never come from item level alone.
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static int32 BestTierForItemLevel(int32 ItemLevel);

    // Rarity soft-caps the tier ceiling (Standard T3, Uncommon T1, rest T-1).
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static int32 TierCapForRarity(EBreakerItemRarity Rarity);

    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static void AffixCountRangeForRarity(EBreakerItemRarity Rarity, int32& OutMinimum, int32& OutMaximum);

    // The vertical-slice affix pool: universal core six (Elemental DR waits
    // on a resistance model), one movement affix per weapon archetype, and
    // both crit stats. Twelve entries prove the pipeline.
    static const TArray<FBreakerAffixDefinition>& GetSliceAffixPool();

    static const FBreakerAffixDefinition* FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId);
};
