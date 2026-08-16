#include "Items/BreakerForgeLibrary.h"

#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerItemRules.h"

void FBreakerForgeWallet::Add(int32 Amount)
{
    // Clamped at zero: a bug in a spend path must not be able to leave the
    // wallet in an unrepresentable state.
    Riftglass = FMath::Max(0, Riftglass + Amount);
}

bool FBreakerForgeWallet::CanAfford(const FBreakerForgeCost& Cost) const
{
    return Cost.IsFree() || Riftglass >= Cost.Amount;
}

bool FBreakerForgeWallet::Spend(const FBreakerForgeCost& Cost)
{
    if (!CanAfford(Cost)) return false;
    if (!Cost.IsFree()) Add(-Cost.Amount);
    return true;
}

bool FBreakerForgeWallet::CollapseLegacyDenominations()
{
    if (Amounts.IsEmpty()) return false;

    // O2 PLACEHOLDER conversion, stated once: 1 Slag = 1 Riftglass, 1 Flux =
    // 6, 1 Sigil = 60. The rates come from the old temper ladder itself — the
    // cheapest Flux temper (16 Flux) had to outprice the priciest Slag one
    // (48 Slag), and the cheapest Sigil temper (12 Sigil) the priciest Flux
    // one (40 Flux) — so every craft a saved balance could afford before the
    // consolidation, it can still afford after, at the same rung of the new
    // ladder. Total value preserved; nothing rounds because the rates are
    // integers.
    constexpr int32 LegacyRates[LegacyDenominationCount] = { 1, 6, 60 };  // Slag, Flux, Sigil
    for (int32 Index = 0; Index < Amounts.Num() && Index < LegacyDenominationCount; ++Index)
    {
        Add(FMath::Max(0, Amounts[Index]) * LegacyRates[Index]);
    }
    Amounts.Empty();
    return true;
}

namespace
{
    // Distinctively prefixed: this project has twice shipped a unity-build
    // collision between identically named helpers in two anonymous namespaces.

    // O2 PLACEHOLDER (touched by the one-currency consolidation: the old
    // Slag/Flux/Sigil rows are re-expressed in Riftglass at the migration's
    // stated 1/6/60 conversion, so a melt pays what it always paid). What one
    // item of each rarity is worth as material, split into a level-scaled base
    // (the old Slag part) and a FLAT rarity bonus (the old Flux+Sigil part),
    // which is what keeps "should I wear this or melt it" a decision at
    // exactly the rarities where the player owns several and can equip at most
    // three (O11).
    struct FBreakerForgeSalvageRow
    {
        int32 ScaledBase;   // multiplied by BreakerForgeLevelScalar
        int32 FlatBonus;    // rarity-pure, never level-scaled
    };

    FBreakerForgeSalvageRow BreakerForgeSalvageRowFor(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
        case EBreakerItemRarity::Standard:    return {2, 0};
        case EBreakerItemRarity::Uncommon:    return {4, 6};      // was 1 Flux
        case EBreakerItemRarity::Exceptional: return {8, 18};     // was 3 Flux
        case EBreakerItemRarity::Aberrant:    return {14, 102};   // was 7 Flux + 1 Sigil
        case EBreakerItemRarity::Anomalous:   return {20, 252};   // was 12 Flux + 3 Sigil
        default:                              return {1, 0};
        }
    }

    // Item level scales the base yield only. The rarity bonus stays flat so
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

    // O2 PLACEHOLDER (touched by the one-currency consolidation: the old
    // Slag/Flux/Sigil bands are re-expressed in Riftglass at the migration's
    // stated 1/6/60 conversion, so every rung costs the same real effort it
    // did — only the denomination merged). Tempering INTO this tier costs
    // this. Costs rise steeply toward the spike because T0 and T-1 are meant
    // to be the end of a chase, not the end of an afternoon.
    //
    // THE LADDER, so the whole economy reads in one place. Kill credits are
    // FBreakerCurrencyDropParams' per-rank ranges (BreakerDropTable.h); costs
    // are below and in ReforgeCost/AttuneCost. Both sides scale by the same
    // 0.06/level scalar (spike tiers excepted), so the ratios hold at any
    // item level. At ilvl 1, average kill pay: Trash ~0.5, Elite ~3,
    // Modifier-bearer ~11, Boss ~55 Riftglass.
    //
    //   Verb / rung          Cost (ilvl 1)   ...in kills' worth (ilvl 1)
    //   Temper into T12            6         ~2 Elites of warm-up
    //   Temper into T5            48         ~1 Boss, or ~16 Elites
    //   Temper into T4            96         ~2 Bosses
    //   Temper into T1           240         ~4-5 Bosses
    //   Reforge (3 lines)         18         ~6 Elites — the cheap verb
    //   Attune (3 lines)          78         ~1.5 Bosses — the expensive verb
    //   Temper into T0          2400 flat    ~11 Bosses at endgame (ilvl 50)
    //   Temper into T-1         6000 flat    ~28 Bosses at endgame (ilvl 50)
    //
    // T0/T-1 are priced FLAT — no level scalar — which is what replaced the
    // old "Sigil-only" gate: at low item level they cost a week, at endgame
    // boss income they cost a chase, and no amount of trash farming makes
    // them an afternoon. The flat price sits ABOVE the T1 rung even at the
    // O29 ilvl-120 ceiling (T1 there is 240 * 8.14 ~ 1954 < 2400), so the
    // ladder can never invert and offer the spike for less than the rung
    // below it.
    FBreakerForgeCost BreakerForgeTemperCostForTargetTier(int32 TargetTier, int32 ItemLevel)
    {
        FBreakerForgeCost Cost;
        const float LevelScalar = BreakerForgeLevelScalar(ItemLevel);
        // The band boundaries are unchanged from the pre-consolidation ladder
        // (re-sited onto the 12-tier ladder by O29): T12..T5 is the shallow
        // ramp, T4..T1 the steep one, T0/T-1 the flat-priced spike.
        if (TargetTier >= 5)
        {
            Cost.Amount = FMath::RoundToInt((13 - TargetTier) * 6.0f * LevelScalar);
        }
        else if (TargetTier >= 1)
        {
            // (6-T)*8 in old Flux, at 6 Riftglass per Flux.
            Cost.Amount = FMath::RoundToInt((6 - TargetTier) * 48.0f * LevelScalar);
        }
        else
        {
            // The spike. Sized so the effort at endgame boss income matches
            // what the old boss-only Sigil economy demanded (~10 and ~28
            // bosses), and so it clears the scaled T1 rung at every legal
            // item level — see the ladder-inversion note above.
            Cost.Amount = TargetTier == 0 ? 2400 : 6000;
        }
        return Cost;
    }
}

FBreakerForgeWallet UBreakerForgeLibrary::SalvageValue(const FBreakerItemInstance& Item)
{
    FBreakerForgeWallet Wallet;
    if (!Item.IsValid()) return Wallet;
    const FBreakerForgeSalvageRow Row = BreakerForgeSalvageRowFor(Item.Rarity);
    Wallet.Add(FMath::Max(1, FMath::RoundToInt(Row.ScaledBase * BreakerForgeLevelScalar(Item.ItemLevel))) + Row.FlatBonus);
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
    // Scales on how much item there is to reroll, so rerolling a six-affix
    // Anomalous is not the same price as rerolling a one-line Standard. See
    // the ladder table above BreakerForgeTemperCostForTargetTier.
    Cost.Amount = FMath::RoundToInt((6 + 4 * Item.Affixes.Num()) * BreakerForgeLevelScalar(Item.ItemLevel));  // O2 PLACEHOLDER
    return Cost;
}

FBreakerForgeCost UBreakerForgeLibrary::AttuneCost(const FBreakerItemInstance& Item)
{
    FBreakerForgeCost Cost;
    // The expensive verb: it is the one that converts an item with the tiers
    // you wanted and the lines you did not into the item you were farming for.
    // Old (4+3N) Flux at the migration's 6 Riftglass per Flux; see the ladder
    // table above BreakerForgeTemperCostForTargetTier.
    Cost.Amount = FMath::RoundToInt((24 + 18 * Item.Affixes.Num()) * BreakerForgeLevelScalar(Item.ItemLevel));  // O2 PLACEHOLDER (consolidation-touched)
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
