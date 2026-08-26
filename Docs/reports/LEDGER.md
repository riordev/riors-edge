# LEDGER

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Longstride landed as DISTANCE — overturnable in one commit

The count you asked for: one `EBreakerNodeStatTarget` entry, one
`FBreakerNodeStats` field + mapping line (the wire), one read in `TryDash` —
the cheap side of your "one enum entry and one read, or more" line, so your
own conditional resolved it and I landed distance rather than burning a
round-trip (O139; the grant mechanics are exactly your ruling — seeded rank
1, cost 0, respec-proof, all five arrival paths). **If four sites reads as
"more" to you, say so:** converting Longstride to the cooldown reading is one
commit, and nothing downstream depends on which.

## O-number allocation still happens at ruling time

The mechanism that produced the O120/O125 collisions stands. **Question:**
may the working rule become "allocate the number at push time, from the
rebased file"? One line in CLAUDE.md's session discipline; the file is yours.
