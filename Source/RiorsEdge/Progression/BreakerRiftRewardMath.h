#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerMonsterChassis.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// The rift completion payout, pure (O168's third commit; O137's law). LEDGER
// owns this header; its one production consumer is
// UBreakerProgressionComponent's OnRiftCompleted handler.
//
// O137: reward composes from ONE effective-difficulty figure derived the way
// the chassis derives threat, and reward-per-second-of-expected-TTK holds
// across difficulty. A rift's monsters grow geometrically with area level
// (MonsterHealth ~ (1+g)^(AL-1)), so a completion bonus that did NOT grow the
// same way would shrink per minute of fight as the ladder climbs — the payout
// therefore rides THE SAME growth constant the chassis authors, read from the
// default-constructed params rather than restated (one number, one place; a
// second copy of g is the drift this project keeps finding). Both bases are
// O2 PLACEHOLDER; the growth is the chassis's own.
//
// The rift ARCHETYPE is deliberately absent: it does not exist on the
// definition yet (every rift is the same Fernhall Substation), and O117's
// grouped first-clears join this header the day a rift varies — recorded at
// the seam ruling, not invented here.
// ---------------------------------------------------------------------------
namespace BreakerRiftReward
{
    // Riftglass for completing a rift whose interior sits at area level 1.
    // Sized against the drop table's beats: a completion should read as a
    // boss-kill-sized purse (BossRiftglassMin is 40 there), earned once.
    constexpr int32 CompletionRiftglassBase = 40;   // O2 PLACEHOLDER
    // XP for the same completion at area level 1. O2 PLACEHOLDER.
    constexpr int32 CompletionXpBase = 200;         // O2 PLACEHOLDER

    // The shared scale: the chassis's own health growth at this area level,
    // read from the authored default so the two curves cannot drift apart.
    inline float CompletionScale(int32 EffectiveAreaLevel)
    {
        const int32 Clamped = UBreakerMonsterChassisLibrary::ClampAreaLevel(EffectiveAreaLevel);
        return FMath::Pow(1.0f + FBreakerMonsterChassisParams{}.HealthGrowthPerLevel,
            static_cast<float>(Clamped - 1));
    }

    inline int32 RiftglassForCompletion(int32 EffectiveAreaLevel)
    {
        return FMath::RoundToInt32(CompletionRiftglassBase * CompletionScale(EffectiveAreaLevel));
    }

    inline int32 XpForCompletion(int32 EffectiveAreaLevel)
    {
        return FMath::RoundToInt32(CompletionXpBase * CompletionScale(EffectiveAreaLevel));
    }

    // THE OFFER. Owner, after a rift that came back with nothing: "give you
    // at least two forced drops that are decent". Until this there was no
    // guaranteed item on completion at all — the purse was Riftglass and XP,
    // first clear only, and items came only from kills as ground pickups.
    // O270: a closed rift OFFERS this many items at the ceiling of the area's
    // item-level band, salted per run, on EVERY completion (the purse is the
    // ladder's pay; the offer is the rift's); the player chooses one on the
    // closing card and that one goes into the pack. The other two are gone.
    constexpr int32 CompletionOfferCount = 3;   // O2 PLACEHOLDER — O270: "offers three items"

    // "Decent" is the codebase's elite floor: the rarity an elite that has
    // decided to drop is lifted to (ABreakerEnemy::GrantLoot), and the quest
    // reward's default. Aberrant would be a jackpot, not a floor.
    constexpr EBreakerItemRarity CompletionRarityFloor = EBreakerItemRarity::Exceptional;   // O2 PLACEHOLDER — "decent": the elite floor

    // The floor composes with the drop table's gates rather than competing
    // with them, exactly as the elite floor does: a completion can never hand
    // out a rarity its item level forbids. Below the Exceptional unlock the
    // floor drops to Uncommon — the top ungated rarity, so the two items are
    // still never Standard. The rank lever reads Elite because the item level
    // the caller rolls at is the elite's (trash level plus the elite bonus):
    // the completion pays what an elite here would have paid, at the floor.
    inline EBreakerItemRarity CompletionRarity(int32 ItemLevel, const FBreakerDropTableParams& Table)
    {
        return UBreakerDropTableLibrary::IsRarityUnlocked(CompletionRarityFloor, ItemLevel, EBreakerMonsterRank::Elite, Table)
            ? CompletionRarityFloor
            : EBreakerItemRarity::Uncommon;
    }
}
