#include "Items/BreakerLootLibrary.h"

#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemRules.h"

namespace
{
    // The lean, resolved for one candidate line on one item. Armour returns
    // exactly 1.0 and never consults the table, so every non-weapon roll is
    // bit-identical to what it was before archetypes existed — which is what
    // keeps the existing loot tests meaningful.
    //
    // Named with the file's subject rather than something generic: this
    // project has twice shipped a unity-build collision between identically
    // named helpers in two anonymous namespaces.
    float BreakerLootArchetypeAffixWeight(const FBreakerItemInstance& Item, FName AffixId)
    {
        if (!Item.IsWeapon()) return 1.0f;
        return UBreakerAffixLibrary::ArchetypeAffixWeightMultiplier(Item.WeaponArchetype, AffixId);
    }
}

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
    return RollItemInternal(DefinitionId, Slot, Rarity, ItemLevel, RandomSeed, /*bAllowLegendary=*/true);
}

FBreakerItemInstance UBreakerLootLibrary::RollItemInternal(FName DefinitionId, EBreakerEquipSlot Slot, EBreakerItemRarity Rarity,
    int32 ItemLevel, int32 RandomSeed, bool bAllowLegendary)
{
    FRandomStream Random(RandomSeed);

    FBreakerItemInstance Item;
    Item.ItemId = FGuid::NewGuid();
    Item.DefinitionId = DefinitionId;
    Item.Slot = Slot;
    Item.Rarity = Rarity;
    Item.ItemLevel = FMath::Clamp(ItemLevel, 1, 50);

    // A weapon drop decides WHICH GUN IT IS before it decides its affixes,
    // because the archetype bends the affix odds (owner: "certain guns have
    // certain leans towards affixes"). Rolled from the same deterministic
    // stream, so a seed still reproduces an item exactly.
    //
    // Uniform across archetypes on purpose: a weighted table here would be a
    // rarity system for gun CLASSES, which is a separate design nobody has
    // ruled on. Every archetype is equally likely; what varies is what rolls
    // ON it.
    if (FBreakerItemInstance::IsWeaponSlot(Slot))
    {
        Item.WeaponArchetype = static_cast<EBreakerWeaponArchetype>(
            Random.RandRange(0, static_cast<int32>(EBreakerWeaponArchetype::Count) - 1));
    }

    int32 MinimumAffixes = 1;
    int32 MaximumAffixes = 1;
    UBreakerAffixLibrary::AffixCountRangeForRarity(Rarity, MinimumAffixes, MaximumAffixes);
    const int32 AffixCount = Random.RandRange(MinimumAffixes, MaximumAffixes);

    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const int32 BestLevelTier = UBreakerAffixLibrary::BestTierForItemLevel(Item.ItemLevel);
    // Rarity caps below T1 clamp the ceiling; T0/T-1 rarity ceilings do not
    // unlock tiers item level has not reached — those come from crafting.
    const int32 BestTier = FMath::Max(BestLevelTier, UBreakerAffixLibrary::TierCapForRarity(Rarity));

    // ABERRANT IS FOCUSED. Rarity used to gate affix COUNT and a tier ceiling
    // and nothing else, so an Aberrant was an Exceptional with a line or two
    // more and the step between them was arithmetic rather than an event.
    //
    // A Focused item spends its first affix at one tier better than item level
    // alone allows — still rarity-capped, so it cannot leap the ceiling, but it
    // is the reason to pick the thing up. It is deliberately ONE slot: O11
    // reserves Aberrant's "1-2 unique modifier affixes" for the owner to name
    // and design, and this is the seat those will occupy when they land, not a
    // guess at what they are.
    const bool bFocused = Rarity == EBreakerItemRarity::Aberrant;

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
            // The archetype lean rides the EXISTING roll weight rather than
            // replacing it, so a rare line stays relatively rare on the gun
            // that likes it. On armour, and on any pairing with no authored
            // opinion, this is exactly 1.0 and the arithmetic is unchanged.
            TotalWeight += Affix.RollWeight * BreakerLootArchetypeAffixWeight(Item, Affix.AffixId);
        }
        if (Candidates.IsEmpty()) break;

        const FBreakerAffixDefinition* Chosen = Candidates.Last();
        float WeightRoll = Random.FRand() * TotalWeight;
        for (const FBreakerAffixDefinition* Candidate : Candidates)
        {
            if ((WeightRoll -= Candidate->RollWeight * BreakerLootArchetypeAffixWeight(Item, Candidate->AffixId)) < 0.0f) { Chosen = Candidate; break; }
        }

        // Tier roll: worst available tier is most likely, each step toward
        // the ceiling halves the odds, so T1+ feels earned. The focused slot
        // rolls against a ceiling one step higher, and still cannot pass the
        // rarity cap.
        const int32 SlotBestTier = (bFocused && Index == 0)
            ? FMath::Max(BestTier - 1, UBreakerAffixLibrary::TierCapForRarity(Rarity))
            : BestTier;
        const int32 WorstTier = 8;
        int32 Tier = WorstTier;
        for (int32 Candidate = WorstTier - 1; Candidate >= SlotBestTier; --Candidate)
        {
            if (Random.FRand() < 0.5f) break;
            Tier = Candidate;
        }
        // ...and the focused slot never rolls WORSE than the ordinary ceiling,
        // so "focused" is a floor as well as a raised roof. Without this the
        // headline property of the rarity would be invisible on most drops,
        // which is exactly how the first version of the archetype leans shipped
        // with the sidearm's lean measurably doing nothing.
        if (bFocused && Index == 0) Tier = FMath::Min(Tier, BestTier);

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

    // ANOMALOUS CARRIES A RULE. This is the whole answer to "what does rarity
    // mean above Exceptional", and it is drawn LAST on purpose: every rarity
    // below Anomalous consumes exactly the draws it always did, so no
    // previously-recorded roll moves and the existing determinism tests keep
    // meaning what they meant.
    //
    // A legendary is picked here rather than upstream so the slot and item
    // level are already resolved and the ordinary Anomalous path stays the
    // default. The three legendaries occupy three of the eight slots, so five
    // slots always take the generic branch.
    if (Rarity == EBreakerItemRarity::Anomalous)
    {
        const FBreakerLegendaryDefinition Legendary = bAllowLegendary
            ? UBreakerItemRuleLibrary::FindLegendaryForSlot(Slot)
            : FBreakerLegendaryDefinition();
        if (Legendary.IsValid() && Random.FRand() < LegendaryChanceWithinAnomalous)
        {
            // Re-rolled from a derived seed rather than continuing this stream,
            // so a legendary is reproducible from its own id and seed alone —
            // which is what lets a dev grant and a drop produce the same item.
            return RollLegendary(Legendary.LegendaryId, Item.ItemLevel, RandomSeed ^ 0x1EDA5EED);
        }
        Item.Rule = UBreakerItemRuleLibrary::RollRule(Random.RandRange(0, MAX_int32 - 1));
    }

    return Item;
}

FBreakerItemInstance UBreakerLootLibrary::RollLegendary(FName LegendaryId, int32 ItemLevel, int32 RandomSeed)
{
    const FBreakerLegendaryDefinition Definition = UBreakerItemRuleLibrary::FindLegendary(LegendaryId);
    if (!Definition.IsValid()) return FBreakerItemInstance();

    // Built on top of an ordinary Anomalous roll rather than beside it, so a
    // legendary is a real item: it has an item level, its affix values come off
    // the same tier curve, and two of them differ. The signature is what is
    // guaranteed; everything else is still loot.
    FBreakerItemInstance Item = RollItemInternal(LegendaryId, Definition.Slot, EBreakerItemRarity::Anomalous,
        ItemLevel, RandomSeed, /*bAllowLegendary=*/false);
    Item.DefinitionId = LegendaryId;
    Item.LegendaryId = LegendaryId;
    Item.Rule = Definition.Rule;

    // The signature. Any guaranteed line the ordinary roll already produced is
    // left exactly as it rolled — overwriting it would quietly re-roll a good
    // value down to the floor of its tier. Missing lines are added at the
    // item's own best tier, because a legendary's IDENTITY should not depend on
    // whether the affix draw happened to cooperate.
    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    const int32 BestTier = FMath::Max(
        UBreakerAffixLibrary::BestTierForItemLevel(Item.ItemLevel),
        UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity::Anomalous));
    for (const FName AffixId : Definition.GuaranteedAffixIds)
    {
        if (Item.Affixes.ContainsByPredicate([AffixId](const FBreakerRolledAffix& Rolled) { return Rolled.AffixId == AffixId; })) continue;
        const FBreakerAffixDefinition* Affix = UBreakerAffixLibrary::FindAffix(Pool, AffixId);
        // A signature naming a line that is not legal on the slot is a content
        // bug, and it is caught by RiorsEdge.Items.Legendary.Signature rather
        // than shipping as a line that silently never appears.
        if (!Affix || !Affix->AllowsSlot(Definition.Slot)) continue;

        FBreakerRolledAffix Rolled;
        Rolled.AffixId = AffixId;
        Rolled.Tier = BestTier;
        Rolled.Category = Affix->Category;
        Rolled.Value = UBreakerAffixLibrary::ValueForTier(*Affix, BestTier);
        Item.Affixes.Add(Rolled);
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
