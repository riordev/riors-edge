#pragma once

#include "CoreMinimal.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// THE ITEM NAME, as a pure rule (owner-ruled: PoE grammar, prefix + base +
// suffix). World-free and derived, never stored: a name baked into
// FBreakerItemInstance would freeze today's wording into every save ever
// written, which is the mistake BreakerItemRequirements.h already refuses for
// the equip level.
//
// The name is the item's HEADLINE, not its stat block. An item wearing six
// rolled lines shows two of them by name — the strongest prefix and the
// strongest suffix — and the rest are read on the card. That is the whole
// point of the change: the inventory currently prints every affix on every
// row, so nothing stands out and the list is unreadable at a glance.
//
// TIERS COUNT DOWN: T1 is the best roll and T12 the worst, so "strongest"
// means the SMALLEST tier. An untiered line (-1) is a special that has no
// ladder at all; it outranks every tiered line, because on an item that has
// one it is the reason the item exists.
// ---------------------------------------------------------------------------
namespace BreakerItemNaming
{
    // The comparison the whole rule rests on. Returns true when Candidate is a
    // better headline than Best. Ties break on affix id so two items rolled
    // with the same lines never disagree about their own name.
    inline bool OutranksForName(const FBreakerRolledAffix& Candidate, const FBreakerRolledAffix& Best)
    {
        const int32 CandidateRank = Candidate.Tier < 0 ? 0 : Candidate.Tier;
        const int32 BestRank = Best.Tier < 0 ? 0 : Best.Tier;
        if (CandidateRank != BestRank) return CandidateRank < BestRank;
        return Candidate.AffixId.LexicalLess(Best.AffixId);
    }

    // The strongest rolled line of one category, or nullptr when the item
    // carries none. A Standard item with no affixes has no headline and is
    // named by its base alone, which is correct rather than a fallback.
    inline const FBreakerRolledAffix* StrongestOfCategory(
        const TArray<FBreakerRolledAffix>& Affixes, EBreakerAffixCategory Category)
    {
        const FBreakerRolledAffix* Best = nullptr;
        for (const FBreakerRolledAffix& Affix : Affixes)
        {
            if (Affix.Category != Category) continue;
            if (!Best || OutranksForName(Affix, *Best)) Best = &Affix;
        }
        return Best;
    }

    // Compose the three parts. Either side may be empty; an item with only a
    // suffix is "Boots of Cast Speed" and one with only a prefix is "Havoc
    // Boots", which is the grammar doing its job rather than a special case.
    //
    // Words come from the affix's own display name until flavour words are
    // authored: the RULE is what this file owns, and a later data pass that
    // adds a proper name word changes the words without touching the grammar.
    inline FString Compose(const FString& PrefixWord, const FString& BaseName, const FString& SuffixWord)
    {
        FString Name;
        if (!PrefixWord.IsEmpty()) Name += PrefixWord + TEXT(" ");
        Name += BaseName;
        if (!SuffixWord.IsEmpty()) Name += TEXT(" of ") + SuffixWord;
        return Name;
    }
}
