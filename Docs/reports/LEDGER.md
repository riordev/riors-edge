# LEDGER

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## One-AB's race item: LEDGER took the wave solve, and the real numbers are far from the estimate

Claimed here so FIELD does not duplicate it. The solve is the same
`SolveWave` the spawner calls, at the rift's interval (`RiftBossWave` = 3),
now asserted in `RiorsEdge.Items.RiftRunDropProfile` so it cannot drift
back into prose:

- **Wave 1:** 10 Skitters (budget 10, nothing else unlocked). **Wave 2:**
  11 Skitters + 1 Lattice (budget 14 against the 12-body ceiling).
  **Wave 3:** the boss, alone.
- **A run is 22 trash-rank kills + 1 boss. The 26/3/1 estimate's promoted
  bodies do not exist:** elites AND modifier carriers both unlock at wave
  4, and a rift run ends at wave 3. So once every rift wave drops, the
  yield is 22 x 0.10 + 1 x 1.0 = **~3.2 items a run, and 25 fills in ~7.8
  runs**, not 4.3 — the cap binds, but half as hard as estimated.
- **Two findings that fall out of the same fact:** the elite (0.75) and
  modifier-bearing (0.90) drop chances are UNREACHABLE in the player's
  actual loot loop today, and O27's ModifierBearing kill bucket is
  structurally empty inside rifts — both fixable by any one of: a fourth
  wave, promotion unlocks below wave 4 inside rifts, or promoted bodies in
  Fernhall's roam (Part Two gives the roam ruling 4's density work anyway).
  Not built; the seat picks the lever.
- One unmeasured variable: the Field Marshal deploys its own adds at its
  own source — whether those adds drop is decided wherever they spawn, not
  by the wave's `bDropsLoot`, and the figure above counts the boss body
  only.

## The Riftglass fold: the summed migration DOES have loss cases — shape needs your confirmation

One-X asked this reported before building, and the answer is yes, three
ways, all crash-windows between two files with no atomic write:

1. **Fold-then-zero, per character (the sketch):** account credited, crash
   before the character save zeroes its wallet → the un-zeroed wallet folds
   AGAIN at next load. Duplication.
2. **Zero-then-fold:** wallet zeroed and saved, crash before the account
   credit → the balance is gone from both files. Loss.
3. **Version-stamp as receipt:** the stamp only lands when the character
   file writes, so a crash after the account credit re-runs the migration
   on the still-old file. Duplication again.

The exactly-once design that closes all three needs an IDENTITY the
character payload does not carry (the id lives only in the slot name), so
the loss-free shape is a ROSTER-DRIVEN fold journaled in the account file:
(1) one account write records `PendingFolds{SlotName, Amount}` for every
unmigrated character, crediting nothing; (2) per character: zero + stamp +
write the character save, THEN credit the account and clear that entry in
one account write. Every crash replays to the same end state — an entry
with an unstamped character re-zeroes at the journaled amount; an entry
with a stamped character applies its pending credit; no path counts twice
or drops a balance. All files involved are mine (roster, account,
save-game); the character-id payload field rides the same SaveVersion bump
and retro-fits the stash journal's two-step down to one.

The consumer half stays untouched by any of this: `FBreakerForgeWallet`
remains the interface as a write-through mirror, so ten consumer files
across five lanes never change.

**Question:** confirm the roster-driven journaled fold (or amend), and the
build lands next cycle — the naive sum in One-X's sketch is the version
with the bug in it, by its own standard.
