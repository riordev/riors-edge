#include "Items/BreakerLootLibrary.h"

#include "Items/BreakerAffixLibrary.h"

EBreakerItemRarity UBreakerLootLibrary::RollRarity(int32 RandomSeed, float DropChanceBonusPercent)
{
    // Prototype weights; retune after a real time-to-kill exists. Drop
    // chance drains weight from Standard into the higher tiers.
    float StandardWeight = 62.0f;
    float UncommonWeight = 25.0f;
    float ExceptionalWeight = 10.0f;
    float AberrantWeight = 2.5f;
    float AnomalousWeight = 0.5f;

    const float Bonus = FMath::Clamp(DropChanceBonusPercent, 0.0f, 100.0f) / 100.0f;
    const float Shifted = StandardWeight * Bonus * 0.5f;
    StandardWeight -= Shifted;
    UncommonWeight += Shifted * 0.55f;
    ExceptionalWeight += Shifted * 0.30f;
    AberrantWeight += Shifted * 0.12f;
    AnomalousWeight += Shifted * 0.03f;

    FRandomStream Random(RandomSeed);
    float Roll = Random.FRand() * (StandardWeight + UncommonWeight + ExceptionalWeight + AberrantWeight + AnomalousWeight);
    if ((Roll -= StandardWeight) < 0.0f) return EBreakerItemRarity::Standard;
    if ((Roll -= UncommonWeight) < 0.0f) return EBreakerItemRarity::Uncommon;
    if ((Roll -= ExceptionalWeight) < 0.0f) return EBreakerItemRarity::Exceptional;
    if ((Roll -= AberrantWeight) < 0.0f) return EBreakerItemRarity::Aberrant;
    return EBreakerItemRarity::Anomalous;
}

FBreakerItemInstance UBreakerLootLibrary::RollItem(FName DefinitionId, EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel, int32 RandomSeed)
{
    FRandomStream Random(RandomSeed);

    FBreakerItemInstance Item;
    Item.ItemId = FGuid::NewGuid();
    Item.DefinitionId = DefinitionId;
    Item.Slot = Slot;
    Item.Rarity = Rarity;
    Item.ItemLevel = FMath::Clamp(ItemLevel, 1, 50);

    int32 MinimumAffixes = 1;
    int32 MaximumAffixes = 1;
    UBreakerAffixLibrary::AffixCountRangeForRarity(Rarity, MinimumAffixes, MaximumAffixes);
    const int32 AffixCount = Random.RandRange(MinimumAffixes, MaximumAffixes);

    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const int32 BestLevelTier = UBreakerAffixLibrary::BestTierForItemLevel(Item.ItemLevel);
    // Rarity caps below T1 clamp the ceiling; T0/T-1 rarity ceilings do not
    // unlock tiers item level has not reached — those come from crafting.
    const int32 BestTier = FMath::Max(BestLevelTier, UBreakerAffixLibrary::TierCapForRarity(Rarity));

    int32 PrefixCount = 0;
    int32 SuffixCount = 0;
    for (int32 Index = 0; Index < AffixCount; ++Index)
    {
        float TotalWeight = 0.0f;
        TArray<const FBreakerAffixDefinition*> Candidates;
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            if (!Affix.AllowsSlot(Slot)) continue;
            if (Item.Affixes.ContainsByPredicate([&Affix](const FBreakerRolledAffix& Rolled) { return Rolled.AffixId == Affix.AffixId; })) continue;
            if (Affix.Category == EBreakerAffixCategory::Prefix && PrefixCount >= 4) continue;
            if (Affix.Category == EBreakerAffixCategory::Suffix && SuffixCount >= 4) continue;
            Candidates.Add(&Affix);
            TotalWeight += Affix.RollWeight;
        }
        if (Candidates.IsEmpty()) break;

        const FBreakerAffixDefinition* Chosen = Candidates.Last();
        float WeightRoll = Random.FRand() * TotalWeight;
        for (const FBreakerAffixDefinition* Candidate : Candidates)
        {
            if ((WeightRoll -= Candidate->RollWeight) < 0.0f) { Chosen = Candidate; break; }
        }

        // Tier roll: worst available tier is most likely, each step toward
        // the ceiling halves the odds, so T1+ feels earned.
        const int32 WorstTier = 8;
        int32 Tier = WorstTier;
        for (int32 Candidate = WorstTier - 1; Candidate >= BestTier; --Candidate)
        {
            if (Random.FRand() < 0.5f) break;
            Tier = Candidate;
        }

        FBreakerRolledAffix Rolled;
        Rolled.AffixId = Chosen->AffixId;
        Rolled.Tier = Tier;
        Rolled.Category = Chosen->Category;
        // Step 5: value within the tier band — between this tier's value and
        // partway toward the next tier up.
        const float TierValue = UBreakerAffixLibrary::ValueForTier(*Chosen, Tier);
        const float NextValue = UBreakerAffixLibrary::ValueForTier(*Chosen, FMath::Max(Tier - 1, -1));
        Rolled.Value = FMath::Lerp(TierValue, NextValue, Random.FRand() * 0.5f);
        Item.Affixes.Add(Rolled);

        if (Chosen->Category == EBreakerAffixCategory::Prefix) ++PrefixCount;
        else ++SuffixCount;
    }

    return Item;
}

int32 UBreakerLootLibrary::CountAffixesOfCategory(const FBreakerItemInstance& Item, EBreakerAffixCategory Category)
{
    int32 Count = 0;
    for (const FBreakerRolledAffix& Affix : Item.Affixes)
    {
        if (Affix.Category == Category) ++Count;
    }
    return Count;
}
