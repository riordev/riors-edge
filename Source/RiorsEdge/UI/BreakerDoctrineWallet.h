#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// WHEN THE DOCTRINE WALLET IS OPEN.
//
// Owner, playtest 2026-09-10: "class points shouldnt be shown till a quests
// completion and you reach the threshold to pick your subclass ... so much
// information everywhere".
//
// A level pays no doctrine point — O111 retired that currency's level grant —
// so the only thing that ever pays one is a mission Unlock beat, of which the
// campaign authors four. A character who has not reached the first turn-in is
// therefore carrying a wallet nothing has offered them, and the skill screen's
// header spent three lines saying 0 UNSPENT · 0 SPENT about it.
//
// THIS IS VISIBILITY, NOT A GATE. O215 sites doctrine commitment at the first
// Kess turn-in and never gates the commitment on it; a threshold here would be
// a ruled-against feature wearing this one's words. Nothing about what the
// player may DO changes with this predicate.
// ---------------------------------------------------------------------------
namespace BreakerDoctrineWallet
{
    // Granted is the settled entitlement counter (LevelDoctrinePointsGranted),
    // which is the state's record that a story benchmark has paid.
    //
    // ONCE OPEN, IT STAYS OPEN. Spending every point is not a reason for the
    // wallet to vanish again — a counter that disappears when it reads zero
    // teaches the player that zero is impossible, and then the first refusal
    // has nothing to point at.
    inline bool IsOpen(int32 Unspent, int32 Spent, int32 Granted)
    {
        return Unspent > 0 || Spent > 0 || Granted > 0;
    }
}
