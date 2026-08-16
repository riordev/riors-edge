#include "Items/BreakerForgeLibrary.h"

#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemRules.h"

int32 FBreakerForgeWallet::Get(EBreakerForgeCurrency Currency) const
{
    const int32 Index = static_cast<int32>(Currency);
    return Amounts.IsValidIndex(Index) ? Amounts[Index] : 0;
}

void FBreakerForgeWallet::Add(EBreakerForgeCurrency Currency, int32 Amount)
{
    // A wallet deserialized from a save written before a currency existed is
    // short, not corrupt: grow it rather than dropping the grant on the floor.
    if (Amounts.Num() < CurrencyCount) Amounts.SetNumZeroed(CurrencyCount);
    const int32 Index = static_cast<int32>(Currency);
    if (!Amounts.IsValidIndex(Index)) return;
    Amounts[Index] = FMath::Max(0, Amounts[Index] + Amount);
}

bool FBreakerForgeWallet::CanAfford(const FBreakerForgeCost& Cost) const
{
    return Cost.IsFree() || Get(Cost.Currency) >= Cost.Amount;
}

bool FBreakerForgeWallet::Spend(const FBreakerForgeCost& Cost)
{
    if (!CanAfford(Cost)) return false;
    if (!Cost.IsFree()) Add(Cost.Currency, -Cost.Amount);
    return true;
}

namespace
{
    // Distinctively prefixed: this project has twice shipped a unity-build
    // collision between identically named helpers in two anonymous namespaces.

    // O2 PLACEHOLDER. What one item of each rarity is worth as material.
    // Aberrant and Anomalous are the only sources of Sigil, which is what makes
    // "should I wear this or melt it" a decision at exactly the rarities where
    // the player owns several and can equip at most three (O11).
    struct FBreakerForgeSalvageRow
    {
        int32 Slag;
        int32 Flux;
        int32 Sigil;
    };

    FBreakerForgeSalvageRow BreakerForgeSalvageRowFor(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
        case EBreakerItemRarity::Standard:    return {2, 0, 0};
        case EBreakerItemRarity::Uncommon:    return {4, 1, 0};
        case EBreakerItemRarity::Exceptional: return {8, 3, 0};
        case EBreakerItemRarity::Aberrant:    return {14, 7, 1};
        case EBreakerItemRarity::Anomalous:   return {20, 12, 3};
        default:                              return {1, 0, 0};
        }
    }

    // Item level scales the Slag yield only. Flux and Sigil stay rarity-pure so
    // that farming a low level cannot substitute for finding the rarity, which
    // is the shortest route from "minimal crafting" to "crafting replaces
    // looting".
    float BreakerForgeLevelScalar(int32 ItemLevel)
    {
        // Follows the O29 item-level ceiling rather than the old character cap:
        // an ilvl 120 item carries roughly three times the affix value of an
        // ilvl 50 one, so crafting it at level-50 prices would make the Forge
        // strictly cheaper the deeper the endgame goes.
        return 1.0f + (FMath::Clamp(ItemLevel, 1, UBreakerAffixLibrary::MaxItemLevel) - 1) * 0.06f;  // O2 PLACEHOLDER, x3.94 at 50, x8.14 at 120
    }

    // O2 PLACEHOLDER. Tempering INTO this tier costs this, in the currency the
    // tier demands. Costs rise steeply toward the spike because T0 and T-1 are
    // meant to be the end of a chase, not the end of an afternoon.
    FBreakerForgeCost BreakerForgeTemperCostForTargetTier(int32 TargetTier, int32 ItemLevel)
    {
        FBreakerForgeCost Cost;
        const float LevelScalar = BreakerForgeLevelScalar(ItemLevel);
        // The currency bands are re-sited onto the 12-tier ladder (O29). They
        // used to split an 8-tier ladder at T4 -- the top 3 normal tiers cost
        // Flux, the rest Slag. The same split of 11 steps puts the boundary at
        // T4 again by coincidence of arithmetic (3 of 11 rounds to 3), so
        // T12..T5 are the Slag band and T4..T1 the Flux band; only the linear
        // coefficients move, so that the cheapest step in each band still costs
        // what it did.
        if (TargetTier >= 5)
        {
            Cost.Currency = EBreakerForgeCurrency::Slag;
            Cost.Amount = FMath::RoundToInt((13 - TargetTier) * 6.0f * LevelScalar);
        }
        else if (TargetTier >= 1)
        {
            Cost.Currency = EBreakerForgeCurrency::Flux;
            Cost.Amount = FMath::RoundToInt((6 - TargetTier) * 8.0f * LevelScalar);
        }
        else
        {
            // T0 and T-1. The ONLY Sigil sink in the game, which is what makes
            // Sigil worth the Aberrant it came from.
            Cost.Currency = EBreakerForgeCurrency::Sigil;
            Cost.Amount = TargetTier == 0 ? 12 : 30;
        }
        return Cost;
    }
}

FBreakerForgeWallet UBreakerForgeLibrary::SalvageValue(const FBreakerItemInstance& Item)
{
    FBreakerForgeWallet Wallet;
    if (!Item.IsValid()) return Wallet;
    const FBreakerForgeSalvageRow Row = BreakerForgeSalvageRowFor(Item.Rarity);
    Wallet.Add(EBreakerForgeCurrency::Slag, FMath::Max(1, FMath::RoundToInt(Row.Slag * BreakerForgeLevelScalar(Item.ItemLevel))));
    Wallet.Add(EBreakerForgeCurrency::Flux, Row.Flux);
    Wallet.Add(EBreakerForgeCurrency::Sigil, Row.Sigil);
    return Wallet;
}

int32 UBreakerForgeLibrary::TemperCeilingForItem(const FBreakerItemInstance& Item)
{
    // Rarity caps tempering exactly as it caps the drop roll. Crafting must not
    // be the loophole that lets a Standard belt reach T-1, or rarity stops
    // meaning anything the same session this pass gave it a meaning.
    //
    // Prolific already resolves this item's affixes one tier better at
    // aggregation time, so its PRINTED ceiling tightens by the same step. Two
    // rules that each grant a tier must compose to one T-1 ceiling, not to a
    // T-2 that has no entry on the value curve at all.
    const int32 Uplift = UBreakerItemRuleLibrary::TierUpliftForItem(Item);
    return FMath::Max(UBreakerAffixLibrary::TierCapForRarity(Item.Rarity), -1) + Uplift;
}

FBreakerForgeCost UBreakerForgeLibrary::TemperCost(const FBreakerItemInstance& Item, int32 AffixIndex)
{
    FBreakerForgeCost Cost;
    if (!Item.Affixes.IsValidIndex(AffixIndex)) return Cost;
    const int32 TargetTier = Item.Affixes[AffixIndex].Tier - 1;
    if (TargetTier < TemperCeilingForItem(Item)) return Cost;
    return BreakerForgeTemperCostForTargetTier(TargetTier, Item.ItemLevel);
}

FBreakerForgeCost UBreakerForgeLibrary::ReforgeCost(const FBreakerItemInstance& Item)
{
    FBreakerForgeCost Cost;
    Cost.Currency = EBreakerForgeCurrency::Slag;
    // Scales on how much item there is to reroll, so rerolling a six-affix
    // Anomalous is not the same price as rerolling a one-line Standard.
    Cost.Amount = FMath::RoundToInt((6 + 4 * Item.Affixes.Num()) * BreakerForgeLevelScalar(Item.ItemLevel));  // O2 PLACEHOLDER
    return Cost;
}

FBreakerForgeCost UBreakerForgeLibrary::AttuneCost(const FBreakerItemInstance& Item)
{
    FBreakerForgeCost Cost;
    Cost.Currency = EBreakerForgeCurrency::Flux;
    // The expensive verb: it is the one that converts an item with the tiers
    // you wanted and the lines you did not into the item you were farming for.
    Cost.Amount = FMath::RoundToInt((4 + 3 * Item.Affixes.Num()) * BreakerForgeLevelScalar(Item.ItemLevel));  // O2 PLACEHOLDER
    return Cost;
}

EBreakerForgeResult UBreakerForgeLibrary::Temper(FBreakerItemInstance& Item, int32 AffixIndex,
    FBreakerForgeWallet& Wallet, bool bIsAtForge)
{
    if (!bIsAtForge) return EBreakerForgeResult::NotAtForge;
    if (!Item.IsValid()) return EBreakerForgeResult::InvalidItem;
    if (!Item.Affixes.IsValidIndex(AffixIndex)) return EBreakerForgeResult::InvalidAffix;

    FBreakerRolledAffix& Rolled = Item.Affixes[AffixIndex];
    const int32 TargetTier = Rolled.Tier - 1;
    if (TargetTier < TemperCeilingForItem(Item)) return EBreakerForgeResult::AtTierCeiling;

    const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Rolled.AffixId);
    if (!Definition) return EBreakerForgeResult::InvalidAffix;

    const FBreakerForgeCost Cost = BreakerForgeTemperCostForTargetTier(TargetTier, Item.ItemLevel);
    if (!Wallet.Spend(Cost)) return EBreakerForgeResult::Unaffordable;

    // The value is RE-DERIVED at the new tier rather than scaled from the old
    // one. Scaling would carry a bad in-band roll upward forever; re-deriving
    // means a temper is always exactly what that tier is worth, which is also
    // the only version a player can reason about.
    Rolled.Tier = TargetTier;
    Rolled.Value = UBreakerAffixLibrary::ValueForTier(*Definition, TargetTier);
    return EBreakerForgeResult::Success;
}

EBreakerForgeResult UBreakerForgeLibrary::Reforge(FBreakerItemInstance& Item, FBreakerForgeWallet& Wallet,
    bool bIsAtForge, int32 RandomSeed)
{
    if (!bIsAtForge) return EBreakerForgeResult::NotAtForge;
    if (!Item.IsValid() || Item.Affixes.IsEmpty()) return EBreakerForgeResult::InvalidItem;
    if (!Wallet.Spend(ReforgeCost(Item))) return EBreakerForgeResult::Unaffordable;

    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    FRandomStream Random(RandomSeed);
    for (FBreakerRolledAffix& Rolled : Item.Affixes)
    {
        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Rolled.AffixId);
        if (!Definition) continue;
        // The same in-band lerp the drop pipeline's step 5 uses, so a reforged
        // value and a dropped value are drawn from the identical distribution
        // and neither is secretly better.
        const float TierValue = UBreakerAffixLibrary::ValueForTier(*Definition, Rolled.Tier);
        const float NextValue = UBreakerAffixLibrary::ValueForTier(*Definition, FMath::Max(Rolled.Tier - 1, -1));
        Rolled.Value = FMath::Lerp(TierValue, NextValue, Random.FRand() * 0.5f);
    }
    return EBreakerForgeResult::Success;
}

EBreakerForgeResult UBreakerForgeLibrary::Attune(FBreakerItemInstance& Item, FBreakerForgeWallet& Wallet,
    bool bIsAtForge, int32 RandomSeed)
{
    if (!bIsAtForge) return EBreakerForgeResult::NotAtForge;
    if (!Item.IsValid() || Item.Affixes.IsEmpty()) return EBreakerForgeResult::InvalidItem;
    if (!Wallet.Spend(AttuneCost(Item))) return EBreakerForgeResult::Unaffordable;

    const TArray<FBreakerAffixDefinition>& Pool = UBreakerAffixLibrary::GetSliceAffixPool();
    FRandomStream Random(RandomSeed);

    // A legendary keeps its signature through an attune. Its identity is its
    // guaranteed lines plus its rule, and a craft that could roll those away
    // would turn the build-defining item into a lottery ticket.
    TSet<FName> Locked;
    if (Item.IsLegendary())
    {
        for (const FName AffixId : UBreakerItemRuleLibrary::FindLegendary(Item.LegendaryId).GuaranteedAffixIds)
        {
            Locked.Add(AffixId);
        }
    }
    // A special line is locked for the same reason a legendary signature is:
    // it IS the rarity's identity, and the candidate walk below iterates the
    // slice pool, so an attune could only ever reroll a special line INTO an
    // ordinary one — a strictly destructive outcome the player cannot see
    // coming. The bill rides its carrier: attuning away the downside while
    // keeping the payoff would make Attune a downside-remover, which is a
    // different (unruled) verb.
    for (const FBreakerRolledAffix& Existing : Item.Affixes)
    {
        const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(Pool, Existing.AffixId);
        if (Definition && Definition->IsSpecial())
        {
            Locked.Add(Existing.AffixId);
            if (!Definition->PairedAffixId.IsNone()) Locked.Add(Definition->PairedAffixId);
        }
    }

    // Tiers are preserved position by position, so attuning is a question about
    // WHICH lines and never about how good they are. Two verbs that both moved
    // tiers would make the cheaper one strictly redundant.
    TArray<FBreakerRolledAffix> Rerolled;
    Rerolled.Reserve(Item.Affixes.Num());
    int32 PrefixCount = 0;
    int32 SuffixCount = 0;
    for (const FBreakerRolledAffix& Existing : Item.Affixes)
    {
        if (Locked.Contains(Existing.AffixId))
        {
            Rerolled.Add(Existing);
            if (Existing.Category == EBreakerAffixCategory::Prefix) ++PrefixCount; else ++SuffixCount;
            continue;
        }

        // Slot-legal, no duplicates against what has already been chosen, and
        // the same prefix/suffix caps of four the drop pipeline enforces — an
        // attuned item has to be an item the game could have dropped.
        float TotalWeight = 0.0f;
        TArray<const FBreakerAffixDefinition*> Candidates;
        for (const FBreakerAffixDefinition& Affix : Pool)
        {
            if (!Affix.AllowsSlot(Item.Slot)) continue;
            if (Rerolled.ContainsByPredicate([&Affix](const FBreakerRolledAffix& Taken) { return Taken.AffixId == Affix.AffixId; })) continue;
            if (Affix.Category == EBreakerAffixCategory::Prefix && PrefixCount >= 4) continue;
            if (Affix.Category == EBreakerAffixCategory::Suffix && SuffixCount >= 4) continue;
            Candidates.Add(&Affix);
            TotalWeight += Affix.RollWeight;
        }
        if (Candidates.IsEmpty())
        {
            // Nothing legal left: keep the line rather than dropping it, so an
            // attune can never make an item smaller than it was.
            Rerolled.Add(Existing);
            continue;
        }

        const FBreakerAffixDefinition* Chosen = Candidates.Last();
        float WeightRoll = Random.FRand() * TotalWeight;
        for (const FBreakerAffixDefinition* Candidate : Candidates)
        {
            if ((WeightRoll -= Candidate->RollWeight) < 0.0f) { Chosen = Candidate; break; }
        }

        FBreakerRolledAffix Replacement;
        Replacement.AffixId = Chosen->AffixId;
        Replacement.Tier = Existing.Tier;
        Replacement.Category = Chosen->Category;
        const float TierValue = UBreakerAffixLibrary::ValueForTier(*Chosen, Replacement.Tier);
        const float NextValue = UBreakerAffixLibrary::ValueForTier(*Chosen, FMath::Max(Replacement.Tier - 1, -1));
        Replacement.Value = FMath::Lerp(TierValue, NextValue, Random.FRand() * 0.5f);
        Rerolled.Add(Replacement);
        if (Chosen->Category == EBreakerAffixCategory::Prefix) ++PrefixCount; else ++SuffixCount;
    }

    Item.Affixes = MoveTemp(Rerolled);
    return EBreakerForgeResult::Success;
}
