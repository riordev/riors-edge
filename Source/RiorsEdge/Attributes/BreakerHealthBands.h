#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerMonsterChassis.h"

// ---------------------------------------------------------------------------
// Health bands, pure. Bands are STATE, and this header is the one answer to
// "which band is this health value in" — the bar, the TargetBandBroken
// condition and any future consumer all read the same arithmetic, so display
// and state can draw different amounts of it but can never disagree about it.
//
// BANDS ARE STATE ON EVERY RANK, Trash included. SegmentCountFor returns a
// real count for Trash even though the readability pack draws Trash as a
// single unsegmented bar: a 12-pixel bar not showing four divisions is a
// display limit, and a display limit encoded here would silently turn every
// band-gated line into an Elite-and-above line — the predicate
// Core.Ruin.Siege already occupies. Display showing less than state knows is
// fine; display and state disagreeing is not. If Trash is ever ruled bandless,
// the edit is here, on purpose, with that consequence stated.
//
// The BreakerShieldMath/BreakerMonsterChassis shape: no actor, no world, no
// subsystem — only the rank enum, which lives in the same module.
// ---------------------------------------------------------------------------
namespace BreakerHealthBands
{
    // How many bands a rank's health pool divides into. Sized to fight
    // length (O18: trash sub-second, elite ~3 s, boss 20-45 s): a band break
    // is the "chunk" beat of a fight, so short fights get few and the boss
    // gets more. All O2 PLACEHOLDER.
    inline int32 SegmentCountFor(EBreakerMonsterRank Rank)
    {
        switch (Rank)
        {
        case EBreakerMonsterRank::Trash:           return 4;   // O2 PLACEHOLDER
        case EBreakerMonsterRank::Elite:           return 4;   // O2 PLACEHOLDER
        case EBreakerMonsterRank::ModifierBearing: return 4;   // O2 PLACEHOLDER
        case EBreakerMonsterRank::Boss:            return 8;   // O2 PLACEHOLDER
        }
        return 4;
    }

    // The 0-based band a health value occupies, counted from the bottom:
    // full health sits in band SegmentCount-1, and health at or below zero
    // sits in band 0. A value exactly on a boundary belongs to the band
    // BELOW it — the segment above is fully drained, which is what the bar
    // shows and what "removed a band" means. Degenerate inputs (no pool, no
    // segments) answer 0 so a comparison of two calls can never invent a
    // crossing out of garbage.
    //
    // "This hit removed a band" is IndexOf(post) < IndexOf(pre) with the
    // same MaxHealth and SegmentCount on both sides. Both indices clamp to
    // [0, SegmentCount-1], so a killing blow that starts inside band 0
    // removes nothing — the target is dead, and the band it was already in
    // did not "break" a second time on the way past zero.
    inline int32 IndexOf(float Health, float MaxHealth, int32 SegmentCount)
    {
        if (MaxHealth <= 0.0f || SegmentCount <= 0) return 0;
        const int32 Index = FMath::CeilToInt32(static_cast<float>(SegmentCount) * Health / MaxHealth) - 1;
        return FMath::Clamp(Index, 0, SegmentCount - 1);
    }
}
