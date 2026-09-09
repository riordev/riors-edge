#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// REPOPULATION: WHEN A PATROL COMES BACK.
//
// The ordinary world has patrols and they return; a rift is an instance with a
// completion condition and a population that does not. That split is the
// owner's, and it REPLACES the reasoning of the older roam ruling rather than
// merely overriding it: finiteness used to be the thing that made the roam and
// the rift interior feel like different verbs, because they are the same
// ground. The distinction now rides on the rift's objective and its dilapidated
// dressing, which separate the two harder than a population count ever did.
//
// What lives here is only the arithmetic of WHEN, so it can be proved without a
// world. Where a body goes is not a decision at all: a returning patrol takes
// the slot it was placed in, already floor-traced and capsule-checked when the
// area was built. Re-deriving a formation would let the world drift away from
// the layout somebody authored.
// ---------------------------------------------------------------------------
namespace BreakerRepopulation
{
    // A slot has stood empty long enough to be worth refilling. Held as its own
    // predicate because the clock is the whole design: too short and a cleared
    // pocket was never cleared, too long and the world is a museum.
    inline bool IsDue(float EmptySeconds, float DelaySeconds)
    {
        return DelaySeconds > 0.0f && EmptySeconds >= DelaySeconds;
    }

    // A body never returns within this of the player. Repopulation is something
    // you come back to, not something you watch happen — and the floor under
    // the number is not taste: it must exceed the enemy's own detection range,
    // or a patrol arrives already able to see the player, which is the "appears
    // on top of you" complaint wearing a different hat.
    inline bool IsClearOfPlayer(double DistanceSquaredCm, float MinimumCm)
    {
        if (MinimumCm <= 0.0f) return true;
        return DistanceSquaredCm >= static_cast<double>(MinimumCm) * static_cast<double>(MinimumCm);
    }

    // ONE AT A TIME, MOST OVERDUE FIRST.
    //
    // A pocket that refilled all at once would be a wave arriving, and waves are
    // the rift's verb. Returning the single most-overdue slot makes a cleared
    // set piece rebuild itself body by body, so a player walking back through
    // finds a patrol reforming rather than a fight that respawned.
    //
    // Ties break on the lower index, so a pocket refills in the order it was
    // authored and two runs of the same area never disagree about themselves.
    inline int32 NextDueSlot(TArrayView<const float> EmptySeconds, float DelaySeconds)
    {
        int32 Best = INDEX_NONE;
        float BestEmpty = 0.0f;
        for (int32 Index = 0; Index < EmptySeconds.Num(); ++Index)
        {
            if (!IsDue(EmptySeconds[Index], DelaySeconds)) continue;
            if (Best == INDEX_NONE || EmptySeconds[Index] > BestEmpty)
            {
                Best = Index;
                BestEmpty = EmptySeconds[Index];
            }
        }
        return Best;
    }
}
