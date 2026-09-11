#pragma once

#include "CoreMinimal.h"
#include "Game/BreakerRiftDefinition.h"
#include "Items/BreakerItemTypes.h"
#include "UI/BreakerItemNameText.h"

// ---------------------------------------------------------------------------
// THE DEBRIEF — what a closed rift hands back, composed ONCE and world-free.
//
// Owner-asked: "i really like the entering rift screen so maybe when a rift is
// closed we can add something very similar that shows the items we gained from
// completion/on completion kinda like a loot highlight".
//
// So it is deliberately the BRIEFING'S TWIN (UI/BreakerLoadingScreen.h): the
// same struct-first shape, the same rule that every field is composed here
// rather than read widget-side, for the same reason — a pane that reads the
// world cannot be held against what the world actually gave.
//
// WHAT MAKES IT NOT THE BRIEFING is what it is FOR. A briefing states terms
// before a fight; a debrief is a REWARD BEAT, so it leads with the best thing
// the player got rather than with the area's name, and it says nothing about
// multipliers or death allowances — those questions are answered and gone.
// ---------------------------------------------------------------------------
namespace BreakerRiftDebrief
{
    // The rarity ladder, written out rather than read off the enumerator. That
    // enum is serialized and append-only, so its integers are a storage detail
    // and sorting by them is a bug waiting for the next appended entry.
    inline int32 Rank(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return 1;
            case EBreakerItemRarity::Exceptional: return 2;
            case EBreakerItemRarity::Aberrant:    return 3;
            case EBreakerItemRarity::Unwritten:   return 4;
            default:                              return 0;
        }
    }

    // HOW MANY LINES THE PANE CAN HOLD. A highlight that scrolls is not a
    // highlight; past this the rest are counted rather than listed, which is
    // the honest thing to do with a haul the player will read in the
    // inventory anyway. O2 PLACEHOLDER.
    inline constexpr int32 MaxHighlights = 5;

    struct FLine
    {
        FString Name;
        EBreakerItemRarity Rarity = EBreakerItemRarity::Standard;
        int32 ItemLevel = 1;
    };

    // ONE OF THE OFFER (O270). The line is what the square prints; the
    // instance rides beside it because the square's hover draws the full
    // stats detail against the equipped piece, and that needs the item, not
    // a summary of it.
    struct FOfferRow
    {
        FLine Line;
        FBreakerItemInstance Item;
    };

    struct FModel
    {
        FText AreaName;
        FString TierKicker;
        FString Headline;
        TArray<FLine> Highlights;
        int32 MoreCount = 0;
        int32 Riftglass = 0;
        int32 Experience = 0;
        // THE OFFER, IN ROLLED ORDER AND NEVER SORTED. The index into this
        // array IS the claim key the progression component takes
        // (ClaimRiftCompletionOffer(Index)); a pane that re-ordered it for
        // presentation would hand the player a different item from the one
        // he clicked. The kill haul above sorts because nothing indexes it.
        TArray<FOfferRow> Offer;
        int32 ChosenIndex = INDEX_NONE;
        // A run can genuinely pay nothing in items, and saying so plainly is
        // better than an empty box the player reads as a bug.
        bool IsEmptyHanded() const { return Highlights.IsEmpty() && MoreCount == 0; }
        // The verb's gate: nothing to choose, or something chosen. An offer
        // left unchosen is the one state CONTINUE must refuse, because the
        // claim is what puts the item in the pack and leaving without it
        // forfeits all three.
        bool CanContinue() const { return Offer.IsEmpty() || Offer.IsValidIndex(ChosenIndex); }
    };

    // BEST FIRST, AND DETERMINISTICALLY SO. Rarity decides, then item level,
    // then the name — so two runs that took the same haul print the same list,
    // and a screenshot can be compared with the one before it. A tie broken by
    // nothing at all is how two panes end up disagreeing about the same run.
    inline bool Precedes(const FLine& A, const FLine& B)
    {
        if (Rank(A.Rarity) != Rank(B.Rarity)) return Rank(A.Rarity) > Rank(B.Rarity);
        if (A.ItemLevel != B.ItemLevel) return A.ItemLevel > B.ItemLevel;
        return A.Name < B.Name;
    }

    inline FLine LineFor(const FBreakerItemInstance& Item)
    {
        FLine Line;
        // THE ONE NAME PATH. A screen that spelled an item differently from
        // the inventory is the exact defect UI/BreakerItemNameText.h was
        // extracted to close, and a reward beat is the worst place to
        // reopen it.
        Line.Name = BreakerItemNameText::DisplayName(Item);
        Line.Rarity = Item.Rarity;
        Line.ItemLevel = Item.ItemLevel;
        return Line;
    }

    // Taken is the kill haul (sorted best-first, counted past the pane);
    // Offer is the rift's completion offer (O270), carried in the order it
    // was rolled because its index is the claim key.
    inline FModel Compose(const FBreakerRiftDefinition& Rift, const TArray<FBreakerItemInstance>& Taken,
        const TArray<FBreakerItemInstance>& Offer, int32 Riftglass, int32 Experience)
    {
        FModel Model;
        Model.AreaName = Rift.AreaName;
        Model.TierKicker = Rift.Tier == EBreakerRiftTier::Campaign
            ? TEXT("CAMPAIGN RIFT") : TEXT("ENDGAME RIFT");
        Model.Headline = TEXT("RIFT CLOSED");
        // NEVER NEGATIVE. A wallet or an XP total that went DOWN across a run
        // is a defect somewhere else, and printing "-40" would put that
        // defect's face on the reward beat instead of in a log.
        Model.Riftglass = FMath::Max(Riftglass, 0);
        Model.Experience = FMath::Max(Experience, 0);

        TArray<FLine> Lines;
        Lines.Reserve(Taken.Num());
        for (const FBreakerItemInstance& Item : Taken)
        {
            if (!Item.IsValid()) continue;
            Lines.Add(LineFor(Item));
        }
        Lines.Sort(Precedes);
        Model.MoreCount = FMath::Max(Lines.Num() - MaxHighlights, 0);
        Lines.SetNum(FMath::Min(Lines.Num(), MaxHighlights));
        Model.Highlights = MoveTemp(Lines);

        // ONE ROW PER OFFERED SLOT, invalid or not, and no sort: the row's
        // index must stay the progression component's index, so nothing here
        // may drop or move an entry. The component's roll never yields an
        // invalid instance; if one arrived, a blank square is the honest
        // picture of it and the claim would refuse it by its own rule.
        Model.Offer.Reserve(Offer.Num());
        for (const FBreakerItemInstance& Item : Offer)
        {
            FOfferRow& Row = Model.Offer.AddDefaulted_GetRef();
            Row.Line = LineFor(Item);
            Row.Item = Item;
        }
        return Model;
    }
}
