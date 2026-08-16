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
    int32 ItemLevel, int32 RandomSeed, bool bAllowLegendary)
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
        const int32 WorstTier = UBreakerAffixLibrary::WorstTier;
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
        const float TierValue = UBreakerAffixLibrary::ValueForTier(*Chosen, Tier);
        const float NextValue = UBreakerAffixLibrary::ValueForTier(*Chosen, FMath::Max(Tier - 1, UBreakerAffixLibrary::TopTier));
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

    // -----------------------------------------------------------------------
    // THE RESERVED SEAT, OCCUPIED — special affixes for the high rarities.
    // -----------------------------------------------------------------------
    // Aberrant draws 1-2 lines from its own pool (O11's "1-2 unique modifier
    // affixes", verbatim); Anomalous draws exactly ONE from its stronger pool,
    // BESIDE the rule it already rolled. Drawn LAST, from the SAME stream, for
    // the same reason the rule draw is last: every rarity below Aberrant
    // consumes exactly the draws it always did, and Anomalous' rule/legendary
    // draws sit BEFORE this block, so no previously-recorded rule or legendary
    // outcome moves either. A legendary reaches here through its own inner
    // RollItemInternal call, so a named item carries a signature line too —
    // the peak of the ladder is not exempt from the ladder's identity.
    //
    // The pools are EXCLUSIVE per rarity (Aberrant lines never on Anomalous,
    // and vice versa): each high rarity keeps its own identity instead of the
    // higher one being the lower one plus more. Archetype leans deliberately do
    // not apply — a pool of six-to-eight hand-authored lines is already an
    // identity, and bending it per gun would be a second rarity system nobody
    // ruled on.
    if (Rarity == EBreakerItemRarity::Aberrant || Rarity == EBreakerItemRarity::Anomalous)
    {
        const bool bAberrantSeat = Rarity == EBreakerItemRarity::Aberrant;
        const TArray<FBreakerAffixDefinition>& SpecialPool = bAberrantSeat
            ? UBreakerAffixLibrary::GetAberrantAffixPool()
            : UBreakerAffixLibrary::GetAnomalousAffixPool();
        const TArray<FBreakerAffixDefinition>& DownsidePool = UBreakerAffixLibrary::GetSpecialDownsidePool();

        const int32 SpecialCount = bAberrantSeat ? Random.RandRange(1, 2) : 1;  // O2 PLACEHOLDER (the 1-2 is O11's own number)
        for (int32 SpecialIndex = 0; SpecialIndex < SpecialCount; ++SpecialIndex)
        {
            // Candidates: slot-legal, not already rolled, and the prefix/suffix
            // caps of four must hold for the line AND its bill together — the
            // special seat obeys the same item shape as everything else.
            float TotalWeight = 0.0f;
            TArray<const FBreakerAffixDefinition*> Candidates;
            for (const FBreakerAffixDefinition& Affix : SpecialPool)
            {
                if (!Affix.AllowsSlot(Slot)) continue;
                if (Item.Affixes.ContainsByPredicate([&Affix](const FBreakerRolledAffix& Rolled) { return Rolled.AffixId == Affix.AffixId; })) continue;

                int32 NeededPrefixes = Affix.Category == EBreakerAffixCategory::Prefix ? 1 : 0;
                int32 NeededSuffixes = 1 - NeededPrefixes;
                const FBreakerAffixDefinition* Bill = Affix.PairedAffixId.IsNone()
                    ? nullptr
                    : UBreakerAffixLibrary::FindAffix(DownsidePool, Affix.PairedAffixId);
                if (Bill)
                {
                    if (Bill->Category == EBreakerAffixCategory::Prefix) ++NeededPrefixes; else ++NeededSuffixes;
                }
                if (PrefixCount + NeededPrefixes > 4) continue;
                if (SuffixCount + NeededSuffixes > 4) continue;

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

            // Same tier walk as the ordinary loop, against the ordinary
            // ceiling: a special line is special because of WHAT it is, and its
            // tier still has to be earned the same way — which also keeps the
            // Forge's Temper meaningful on it.
            const int32 WorstTier = UBreakerAffixLibrary::WorstTier;
            int32 Tier = WorstTier;
            for (int32 Candidate = WorstTier - 1; Candidate >= BestTier; --Candidate)
            {
                if (Random.FRand() >= UBreakerAffixLibrary::TierUpgradeChance) break;
                Tier = Candidate;
            }

            FBreakerRolledAffix Rolled;
            Rolled.AffixId = Chosen->AffixId;
            Rolled.Tier = Tier;
            Rolled.Category = Chosen->Category;
            const float TierValue = UBreakerAffixLibrary::ValueForTier(*Chosen, Tier);
            const float NextValue = UBreakerAffixLibrary::ValueForTier(*Chosen, FMath::Max(Tier - 1, UBreakerAffixLibrary::TopTier));
            Rolled.Value = FMath::Lerp(TierValue, NextValue, Random.FRand() * 0.5f);
            Item.Affixes.Add(Rolled);
            if (Chosen->Category == EBreakerAffixCategory::Prefix) ++PrefixCount; else ++SuffixCount;

            // The bill rides along with NO draw of its own: it is part of the
            // deal, not loot, and consuming stream draws for it would make the
            // downside a source of roll variance. Constant-anchored, so
            // ValueForTier returns the same figure at every normal tier.
            if (!Chosen->PairedAffixId.IsNone())
            {
                if (const FBreakerAffixDefinition* Bill = UBreakerAffixLibrary::FindAffix(DownsidePool, Chosen->PairedAffixId))
                {
                    FBreakerRolledAffix BillLine;
                    BillLine.AffixId = Bill->AffixId;
                    BillLine.Tier = Tier;
                    BillLine.Category = Bill->Category;
                    BillLine.Value = UBreakerAffixLibrary::ValueForTier(*Bill, Tier);
                    Item.Affixes.Add(BillLine);
                    if (Bill->Category == EBreakerAffixCategory::Prefix) ++PrefixCount; else ++SuffixCount;
                }
            }
        }
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
