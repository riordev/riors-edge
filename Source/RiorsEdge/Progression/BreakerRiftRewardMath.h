#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerMonsterChassis.h"

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
}
