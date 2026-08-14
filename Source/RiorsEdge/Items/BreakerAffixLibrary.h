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

    // ---- Per-archetype affix leans ----------------------------------------
    // Owner's instruction, verbatim: "make sure certain guns have certain
    // leans towards affixes -- like smg fire rate, lmg damage, sidearm slide
    // speed and so on -- NOT REQUIRED STATS but they can drop more likely
    // with those affixes."
    //
    // So this is a WEIGHT MULTIPLIER, never a filter. Every affix legal on a
    // weapon slot stays legal on every archetype; a lean only bends the odds.
    // That distinction is the whole design: a hard restriction would make an
    // SMG with a huge damage roll impossible, and the item you were not
    // supposed to get is the one that makes a looter interesting. It also
    // means a lean can be retuned to 1.0 to switch the whole feature off
    // without any item becoming unrollable.
    //
    // Returns 1.0 for any pairing with no authored opinion, and for every
    // non-weapon slot, so armour rolls exactly as it did before this existed.
    //
    // O2 PLACEHOLDER: the multipliers are shape, not balance.
    UFUNCTION(BlueprintPure, Category="Items|Affixes")
    static float ArchetypeAffixWeightMultiplier(EBreakerWeaponArchetype Archetype, FName AffixId);

    // True when the affix moves outgoing damage in any bucket or under any
    // condition. The content test uses it to assert that no slot is
    // structurally incapable of raising damage; the UI can use it to group.
    static bool IsOffensiveTarget(EBreakerStatTarget Target);

    static const FBreakerAffixDefinition* FindAffix(const TArray<FBreakerAffixDefinition>& Pool, FName AffixId);
};
