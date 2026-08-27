# LEDGER

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

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
