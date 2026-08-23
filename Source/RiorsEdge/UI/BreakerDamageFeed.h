#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// THE DAMAGE-NUMBER FEED — the rules, with no HUD under them
// ---------------------------------------------------------------------------
// art-and-ui: "Per-target aggregation inside a short window is MANDATORY, not a
// polish item... Damage-over-time ticks aggregate on a LONGER window and must
// never out-shout direct hits... A hard simultaneous cap culls OLDEST-FIRST."
//
// The aggregation existed and three of that rule's four clauses did not hold.
// The arithmetic lives here rather than on the HUD because every part of it —
// whether two hits are one number, how long a number stays open, which entry
// dies when the ring is full — is pure, and none of it was testable while it
// sat inside a Canvas draw. What stays on the HUD is projection, colour, size
// and the pop/rise/fade curves, which genuinely need a world.
namespace BreakerDamageFeed
{
    // HOW LONG A NUMBER STAYS OPEN, and it is TWO windows because the spec says
    // two. One value served both and could not satisfy either.
    //
    // DIRECT. Its job is SIMULTANEITY — a shotgun's eight pellets, a multishot
    // spread, a pierce down a line, the rounds inside one burst. It must sit
    // ABOVE the tightest of those and BELOW the fastest cadence a player can
    // deliberately produce, or the constant's own promise that "two deliberate
    // shots read as two numbers" is false. It was 0.18s against a Sidearm that
    // fires every 0.143s semi-automatically, so two aimed pulls became one
    // number. 0.12s clears the burst rifle's 0.083s in-burst interval and stays
    // under the Sidearm. Asserted, not asserted-by-comment:
    // RiorsEdge.UI.Damage.Aggregation reads both weapons out of the shipped
    // table rather than trusting these numbers.
    static constexpr float DirectMergeWindow = 0.12f;   // O2 PLACEHOLDER

    // DAMAGE OVER TIME. Its job is the opposite — one accumulating number for a
    // whole effect, because a Bleed that prints every tick is the wall of
    // digits the rule exists to stop. It must therefore exceed the TICK
    // INTERVAL, and the shipped Bleed ticks every 0.5s for 3.0s. At the old
    // shared 0.18s two consecutive ticks could never merge and a single Bleed
    // printed six separate numbers.
    static constexpr float DoTMergeWindow = 0.75f;   // O2 PLACEHOLDER

    // A number must still be ALIVE when the next tick arrives or the window
    // cannot be used. The DoT lifetime was 0.35s against a 0.5s tick, so each
    // tick's number was already dead when its successor landed — the second
    // half of the same bug, in a different constant.
    static constexpr float MinimumDoTLifetime = DoTMergeWindow;

    inline float MergeWindowFor(bool bFromDoT)
    {
        return bFromDoT ? DoTMergeWindow : DirectMergeWindow;
    }

    // WHETHER TWO HITS ARE ONE NUMBER.
    //
    // Crit and weak point never merge into a body hit: those are the reads the
    // whole system exists to make legible, and averaging them into a plain
    // number is the same as not showing them. DoT never merges into direct,
    // because they have different windows and because the spec says a tick must
    // never out-shout a hit.
    //
    // THE WINDOW IS MEASURED FROM BIRTH, NOT FROM THE LAST MERGE. It used to
    // refresh the timestamp on every merge, which made it a SLIDING window: a
    // held SMG trigger at 900 rounds per minute produced ONE number that
    // accumulated the entire magazine and never expired while the trigger was
    // down. "Inside a short window" cannot mean a window that outlives the
    // thing it is windowing.
    struct FMergeKey
    {
        const void* Target = nullptr;
        bool bCritical = false;
        bool bWeakPoint = false;
        bool bFromDoT = false;
    };

    inline bool ShouldMerge(const FMergeKey& Existing, double ExistingBirth,
                            const FMergeKey& Incoming, double Now)
    {
        if (Existing.Target != Incoming.Target) return false;
        if (Existing.Target == nullptr) return false;   // an expired weak pointer is not a target
        if (Existing.bCritical != Incoming.bCritical) return false;
        if (Existing.bWeakPoint != Incoming.bWeakPoint) return false;
        if (Existing.bFromDoT != Incoming.bFromDoT) return false;
        return (Now - ExistingBirth) <= MergeWindowFor(Incoming.bFromDoT);
    }

    // WHICH ENTRY DIES WHEN THE RING IS FULL.
    //
    // The spec says oldest-first and the ring evicted by WRITE CURSOR, which is
    // insertion order — and merges refresh a number in place without moving its
    // slot, so the most-refreshed number on the player's primary target was the
    // first thing thrown away once the buffer filled. An expired entry is taken
    // before any live one, which the old cursor also could not do: entries were
    // never reclaimed, so the array pinned at its cap forever after the first
    // full pass.
    //
    // Births is the birth stamp per slot, Deaths the time each slot expires.
    // Returns INDEX_NONE only for an empty set.
    inline int32 IndexToEvict(const TArray<double>& Births, const TArray<double>& Deaths, double Now)
    {
        int32 Best = INDEX_NONE;
        bool bBestExpired = false;
        double BestBirth = 0.0;
        for (int32 Index = 0; Index < Births.Num(); ++Index)
        {
            const bool bExpired = Deaths.IsValidIndex(Index) && Deaths[Index] <= Now;
            // An expired slot always beats a live one; among equals, oldest.
            const bool bBetter = (Best == INDEX_NONE)
                || (bExpired && !bBestExpired)
                || (bExpired == bBestExpired && Births[Index] < BestBirth);
            if (bBetter)
            {
                Best = Index;
                bBestExpired = bExpired;
                BestBirth = Births[Index];
            }
        }
        return Best;
    }
}
