#include "Items/BreakerLootLibrary.h"

#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerDropTable.h"
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

    // The count is the finished item, including bills. Identity occupies seats
    // before generic variance; neither signatures nor downsides append past it.
    bool BreakerLootAllocateSpecial(FBreakerItemInstance& Item, FRandomStream& Random, int32 Budget,
        int32 BestTier, const FBreakerLegendaryDefinition& Legendary)
    {
        const auto& Generic = UBreakerAffixLibrary::GetSliceAffixPool();
        const auto& Downsides = UBreakerAffixLibrary::GetSpecialDownsidePool();
        const bool bAberrant = Item.Rarity == EBreakerItemRarity::Aberrant;
        const auto& Specials = bAberrant ? UBreakerAffixLibrary::GetAberrantAffixPool() : UBreakerAffixLibrary::GetUnwrittenAffixPool();
        auto Has = [&](FName Id) { return Item.Affixes.ContainsByPredicate([Id](const FBreakerRolledAffix& A) { return A.AffixId == Id; }); };
        auto Count = [&](EBreakerAffixCategory Category) { return UBreakerLootLibrary::CountAffixesOfCategory(Item, Category); };
        auto Add = [&](const FBreakerAffixDefinition& Definition, int32 Tier, bool bVariance)
        {
            FBreakerRolledAffix Rolled;
            Rolled.AffixId = Definition.AffixId; Rolled.Tier = Tier; Rolled.Category = Definition.Category;
            Rolled.Value = UBreakerAffixLibrary::ValueForTier(Definition, Tier);
            if (bVariance)
                Rolled.Value = UBreakerAffixLibrary::RollValueForTier(Definition, Tier, Random.FRand());
            Item.Affixes.Add(Rolled);
        };
        auto Draw = [&](const TArray<FBreakerAffixDefinition>& Pool, bool bSpecial, bool bFocused) -> bool
        {
            TArray<const FBreakerAffixDefinition*> Candidates;
            float TotalWeight = 0;
            for (const FBreakerAffixDefinition& Definition : Pool)
            {
                const int32 CandidateCeiling = bFocused ? FMath::Max(BestTier - 1, UBreakerAffixLibrary::TierCapForRarity(Item.Rarity)) : BestTier;
                if (!UBreakerAffixLibrary::IsEligibleForItem(Definition, Item, CandidateCeiling) || Has(Definition.AffixId)) continue;
                const FBreakerAffixDefinition* Bill = bSpecial && !Definition.PairedAffixId.IsNone()
                    ? Downsides.FindByPredicate([&](const FBreakerAffixDefinition& Entry) { return Entry.AffixId == Definition.PairedAffixId; }) : nullptr;
                if (bSpecial && !Definition.PairedAffixId.IsNone() && (!Bill || !Bill->AllowsSlot(Item.Slot) || Has(Bill->AffixId))) continue;
                if (Item.Affixes.Num() + 1 + (Bill ? 1 : 0) > Budget) continue;
                int32 Prefixes = Definition.Category == EBreakerAffixCategory::Prefix ? 1 : 0;
                int32 Suffixes = 1 - Prefixes;
                if (Bill) { if (Bill->Category == EBreakerAffixCategory::Prefix) ++Prefixes; else ++Suffixes; }
                if (Count(EBreakerAffixCategory::Prefix) + Prefixes > 4 || Count(EBreakerAffixCategory::Suffix) + Suffixes > 4) continue;
                Candidates.Add(&Definition);
                TotalWeight += Definition.RollWeight * (bSpecial ? 1.0f : BreakerLootArchetypeAffixWeight(Item, Definition.AffixId));
            }
            if (Candidates.IsEmpty()) return false;
            const FBreakerAffixDefinition* Chosen = Candidates.Last();
            float Weight = Random.FRand() * TotalWeight;
            for (const FBreakerAffixDefinition* Candidate : Candidates)
                if ((Weight -= Candidate->RollWeight * (bSpecial ? 1.0f : BreakerLootArchetypeAffixWeight(Item, Candidate->AffixId))) < 0)
                { Chosen = Candidate; break; }
            const int32 Ceiling = bFocused ? FMath::Max(BestTier - 1, UBreakerAffixLibrary::TierCapForRarity(Item.Rarity)) : BestTier;
            int32 Tier = UBreakerAffixLibrary::WorstEligibleTier(*Chosen);
            for (int32 Candidate = Tier - 1; Candidate >= Ceiling; --Candidate)
            {
                if (Random.FRand() >= UBreakerAffixLibrary::TierUpgradeChance) break;
                Tier = Candidate;
            }
            if (bFocused) Tier = FMath::Min(Tier, BestTier);
            Add(*Chosen, Tier, true);
            if (bSpecial && !Chosen->PairedAffixId.IsNone())
                Add(*Downsides.FindByPredicate([&](const FBreakerAffixDefinition& Entry) { return Entry.AffixId == Chosen->PairedAffixId; }), Tier, false);
            return true;
        };
        if (bAberrant && !Draw(Generic, false, true)) return false;
        for (FName Id : Legendary.GuaranteedAffixIds)
        {
            // Membership, not rolled-ID resolution: FindAffix intentionally
            // falls back into special pools and cannot validate a signature.
            const FBreakerAffixDefinition* Definition = Generic.FindByPredicate([Id](const FBreakerAffixDefinition& Entry) { return Entry.AffixId == Id; });
            if (!Definition || !UBreakerAffixLibrary::IsEligibleForItem(*Definition, Item, BestTier) || Has(Id)
                || Item.Affixes.Num() >= Budget || Count(Definition->Category) >= 4) return false;
            Add(*Definition, BestTier, false);
        }
        const int32 SpecialCount = bAberrant ? Random.RandRange(1, 2) : 1;
        for (int32 Index = 0; Index < SpecialCount; ++Index)
            if (!Draw(Specials, true, false)) { if (Index == 0) return false; break; }
        while (Item.Affixes.Num() < Budget)
            if (!Draw(Generic, false, false)) return false;
        return true;
    }
}

EBreakerItemRarity UBreakerLootLibrary::RollRarity(int32 RandomSeed, float DropChanceBonusPercent)
{
    // THE UNGATED ROLL. This function used to hold the weight table inline, and
    // it was the ENTIRE loot pipeline: `ABreakerEnemy::GrantLoot` called it on
    // every death and spawned whatever came back. That is the shape the owner's
    // playtest report describes from both ends — every kill dropped, and a flat
    // 2.5% Aberrant weight applied at area level 1 to a trash mob exactly as it
    // did to a boss at 50.
    //
    // The table now lives in FBreakerDropTableParams so the owner can retune it
    // without a recompile (O2), and the gates live beside it. This overload is
    // kept because a caller that genuinely has no rank and no item level to
    // supply — a dev grant, a test fixture, a crafting preview — should not have
    // to invent one; it delegates with every gate open, so its behaviour is the
    // table's behaviour and there is one weight table in the project rather than
    // two that can drift.
    //
    // CONTENT should not call this. Content calls
    // UBreakerDropTableLibrary::RollDrop, which runs the chance step first.
    return UBreakerDropTableLibrary::RollGatedRarity(RandomSeed, MAX_int32, EBreakerMonsterRank::Boss,
        DropChanceBonusPercent, FBreakerDropTableParams());
}

FBreakerItemInstance UBreakerLootLibrary::RollItem(FName DefinitionId, EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel, int32 RandomSeed)
{
    return RollItemInternal(DefinitionId, Slot, Rarity, ItemLevel, RandomSeed, /*bAllowLegendary=*/true);
}

EBreakerEquipSlot UBreakerLootLibrary::RollDropSlot(int32 RandomSeed)
{
    // THE SALT IS THE FIX. GrantLoot used to draw the slot from
    // FRandomStream(Seed) — and RollItemInternal opens FRandomStream(Seed)
    // on the SAME seed, so the slot draw and the item's first draw were the
    // same number. For a weapon slot the first draw is the archetype, over
    // the same 0..7 range: archetype index == slot index, always. Primary(6)
    // was every Machinegun, Secondary(7) every Sidearm, and the other six
    // archetypes never dropped from any kill at any level. For armour the
    // first draw is the affix count, so the slot fully determined it.
    //
    // Salting the slot draw decorrelates both symptoms at once. NOTE: this
    // changes every historical drop seed's outcome — unavoidable, since the
    // old outcomes were the bug. The salt composes with the seed rather than
    // replacing BreakerDropTable.cpp's salts, which must not change.
    FRandomStream Random(HashCombine(static_cast<uint32>(RandomSeed), 0x510Cu));
    return static_cast<EBreakerEquipSlot>(Random.RandRange(0, static_cast<int32>(EBreakerEquipSlot::Count) - 1));
}

FBreakerItemInstance UBreakerLootLibrary::RollItemInternal(FName DefinitionId, EBreakerEquipSlot Slot, EBreakerItemRarity Rarity,
    int32 ItemLevel, int32 RandomSeed, bool bAllowLegendary, FName ForcedLegendaryId)
{
    FRandomStream Random(RandomSeed);

    FBreakerItemInstance Item;
    Item.ItemId = FGuid::NewGuid();
    Item.DefinitionId = DefinitionId;
    Item.Slot = Slot;
    Item.Rarity = Rarity;
    // O29: item level runs to 120, past the character cap of 50 and past the
    // area-level ceiling of 100. This is THE endgame power source -- deeper
    // item level means deeper affix tiers and a bigger WeaponBase(ilvl).
    Item.ItemLevel = FMath::Clamp(ItemLevel, 1, UBreakerAffixLibrary::MaxItemLevel);

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
    else
    {
        // An armour drop decides WHICH POOL IT FEEDS, the same way a weapon
        // decides which gun it is. Even odds on purpose — the commitment
        // mechanism is the sustain asymmetry, not scarcity. Drawn from its
        // OWN stream derived off the same seed, never from the main one:
        // consuming the shared stream here would shift every affix a seeded
        // armour item has ever rolled, which is a silent rewrite of existing
        // drops and of every test that pins a seed.
        FRandomStream ArchetypeStream(static_cast<int32>(HashCombine(
            static_cast<uint32>(RandomSeed), 0xA83Du)));
        Item.ArmourArchetype = ArchetypeStream.RandRange(0, 1) == 0
            ? EBreakerArmourArchetype::Life : EBreakerArmourArchetype::Shield;
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

    if (Rarity == EBreakerItemRarity::Aberrant || Rarity == EBreakerItemRarity::Unwritten)
    {
        const FBreakerLegendaryDefinition Legendary = UBreakerItemRuleLibrary::FindLegendary(ForcedLegendaryId);
        if (bAllowLegendary && Rarity == EBreakerItemRarity::Unwritten)
        {
            const FBreakerLegendaryDefinition Candidate = UBreakerItemRuleLibrary::FindLegendaryForSlot(Slot);
            if (Candidate.IsValid() && Random.FRand() < LegendaryChanceWithinUnwritten)
                return RollLegendary(Candidate.LegendaryId, Item.ItemLevel, RandomSeed ^ 0x1EDA5EED);
        }
        if (Legendary.IsValid())
        {
            Item.LegendaryId = Legendary.LegendaryId;
            Item.DefinitionId = Legendary.LegendaryId;
            Item.Rule = Legendary.Rule;
        }
        else if (Rarity == EBreakerItemRarity::Unwritten)
            Item.Rule = UBreakerItemRuleLibrary::RollRule(Random.RandRange(0, MAX_int32 - 1));
        if (!BreakerLootAllocateSpecial(Item, Random, AffixCount, BestTier, Legendary))
        {
            UE_LOG(LogTemp, Error, TEXT("[Loot] No legal affix allocation for %s within %d lines"), *DefinitionId.ToString(), AffixCount);
            return FBreakerItemInstance();
        }
        return Item;
    }

    // ABERRANT IS FOCUSED. Rarity used to gate affix COUNT and a tier ceiling
    // and nothing else, so an Aberrant was an Exceptional with a line or two
    // more and the step between them was arithmetic rather than an event.
    //
    // A Focused item spends its first affix at one tier better than item level
    // alone allows — still rarity-capped, so it cannot leap the ceiling.
    //
    // KEPT, deliberately, now that the reserved seat is occupied: O11's "1-2
    // unique modifier affixes" are real (the special draw at the end of this
    // function) and they are the rarity's IDENTITY; Focused stays as the
    // rarity's baseline tier feel. Removing it would silently regress the
    // measured tier advantage RiorsEdge.Items.Rules.RarityGatesRules pins, and
    // "the special line replaced the thing you could already feel" is a trade
    // the owner never asked for.
    const bool bFocused = Rarity == EBreakerItemRarity::Aberrant;

    int32 PrefixCount = 0;
    int32 SuffixCount = 0;
    for (int32 Index = 0; Index < AffixCount; ++Index)
    {
        float TotalWeight = 0.0f;
        TArray<const FBreakerAffixDefinition*> Candidates;
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            const int32 CandidateCeiling = (bFocused && Index == 0)
                ? FMath::Max(BestTier - 1, UBreakerAffixLibrary::TierCapForRarity(Rarity)) : BestTier;
            if (!UBreakerAffixLibrary::IsEligibleForItem(Affix, Item, CandidateCeiling)) continue;
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
        // RE-DERIVED FOR THE 12-TIER LADDER (O29). The walk is unchanged in
        // shape -- start at the worst tier, take a step toward the ceiling
        // until a roll fails -- but the number of steps went from 7 to 11, and
        // the per-step probability cannot stay at one half through that.
        //
        // The arithmetic: reaching the top of the ladder from the bottom used
        // to be 0.5^7 = 1/128. At 0.5 over eleven steps it would be 0.5^11 =
        // 1/2048, which is not "rare", it is a tier nobody sees. Solving
        // q^11 = 1/128 gives q = 0.643, so:
        //
        //     0.64^11 = 1/136, against the old 0.50^7 = 1/128
        //
        // The rarity of a full climb is preserved almost exactly while the
        // ladder tripled in length. What DID change, correctly, is the middle:
        // each individual step is now likelier, so an ordinary drop lands a
        // tier or two above the floor more often than it used to. That is the
        // point of a back-loaded value curve -- the low tiers are cheap
        // because they are worth little, and the top is expensive because it
        // is worth a lot.
        const int32 WorstTier = UBreakerAffixLibrary::WorstEligibleTier(*Chosen);
        int32 Tier = WorstTier;
        for (int32 Candidate = WorstTier - 1; Candidate >= SlotBestTier; --Candidate)
        {
            if (Random.FRand() >= UBreakerAffixLibrary::TierUpgradeChance) break;
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
        Rolled.Value = UBreakerAffixLibrary::RollValueForTier(*Chosen, Tier, Random.FRand());
        Item.Affixes.Add(Rolled);

        if (Chosen->Category == EBreakerAffixCategory::Prefix) ++PrefixCount;
        else ++SuffixCount;
    }

    return Item;
}

FBreakerItemInstance UBreakerLootLibrary::RollLegendary(FName LegendaryId, int32 ItemLevel, int32 RandomSeed)
{
    const FBreakerLegendaryDefinition Definition = UBreakerItemRuleLibrary::FindLegendary(LegendaryId);
    if (!Definition.IsValid()) return FBreakerItemInstance();

    // Signatures enter the allocation before generic variance, inside the
    // same final budget and category caps as their special line and its bill.
    return RollItemInternal(LegendaryId, Definition.Slot, EBreakerItemRarity::Unwritten,
        ItemLevel, RandomSeed, false, LegendaryId);
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
