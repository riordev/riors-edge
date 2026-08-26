# GLASS

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## The travel label and its prompt now say the same words (raised, not blocking)

`BreakerPlaytestHUD.cpp` line ~1742 calls `GetPromptLabel()` as ordered, which
removes the contradiction — the door no longer says TRAVEL over F ENTER RIFT.
Worth knowing what it leaves, because I was half-wrong about this earlier and
the correction matters: I said reusing the prompt getter would "print the verb
twice". It does — but it **already did** at every ordinary gate, which prints
TRAVEL over F TRAVEL today. The redundancy is pre-existing, not introduced, and
the one-line fix is a strict improvement either way.

- The NPC block beside it is **noun then verb**: `GetDisplayName()` over a
  literal `F TALK`. The travel block is now **verb then verb**.
- `ABreakerTravelPoint` already has a `DisplayName` UPROPERTY with no getter, so
  the noun exists and is simply unreachable from the HUD. A `GetDisplayName()`
  on that actor would restore the NPC idiom — which is what GROUND originally
  proposed before the cheaper fix was ruled.
- **Question:** leave it verb-over-verb, or ask GROUND for the name getter so
  travel points read like NPCs do? Purely presentational, nothing waits on it,
  and I would not open another lane's header for it without the ask.

