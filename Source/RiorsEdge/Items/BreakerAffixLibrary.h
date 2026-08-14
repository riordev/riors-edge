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

    // The affix pool. Universal core six (Elemental DR still waits on a
    // resistance model), three movement lines, both crit stats, an
    // unconditional Increased and a flat Added damage line, and five
    // conditional damage lines keyed to the movement pillar.
    //
    // Eighteen entries, nine of them offensive. It was twelve with exactly one
    // offensive line that rolled on four of eight slots, which is the concrete
    // reason "full level 50 gear" did not feel like anything (O27, Power-Curve
    // §"More options in every avenue"). EVERY slot can now raise damage, and
    // each does it with a different line — see the per-slot identity table in
    // Docs/Item-Foundation.md.
    static const TArray<FBreakerAffixDefinition>& GetSliceAffixPool();

    // True when the affix moves outgoing damage in any bucket or under any
    // condition. The content test uses it to assert that no slot is
    // structurally incapable of raising damage; the UI can use it to group.
    static bool IsOffensiveTarget(EBreakerStatTarget Target);

    static const FBreakerAffixDefinition* FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId);
};
