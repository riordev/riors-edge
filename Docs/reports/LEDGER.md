# LEDGER

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Does the rider path carry a More bucket? — the full report, as ordered

You asked for this report by name in Three-D, so the whole analysis is here
rather than summarized.

**The sort cannot take a hit-time member, and that half of §3.3 is correct
forever.** The strongest-three selection decides membership at standing time;
a target-gated candidate makes the correct membership differ per hit, and the
request cannot express that: the source split is two floats
(`SourceIncreasedPercent`, `SourceMoreProduct`), and a specific member cannot
be removed from a product without knowing the members. Re-selection at the
hit would need the request to carry the full per-source list and would still
be wrong for anything in flight (a rocket snapshots its split at fire time).
Player-facing, the sheet's "N / 3 MORE" stops meaning anything when the three
change per target.

**The ceiling already operates at hit time, and that is the lawful opening.**
The outgoing-modifier chain multiplies a More into the request at submission
under `chain ≤ 2.197 / attribute-side product`, and the DoT tick path clamps
the same way — O34's words already cover it: "temporary ability windows ARE
Mores and count inside the budget." Hit-time Mores spend HEADROOM, not slots.
A target-gated rider More under the same law is one clamp at the
recomposition site: `Request.SourceMoreProduct` multiplies by the rider,
total clamped to `ComposedMoreCeiling()`, loud when it bites.

**What it costs, measured on the authored roster (zero commitment):** an
ability build (TV × Overflow standing = 1.664) has residual 1.320, so a
×1.30 rider pays in full — 2.1632 composed, 98.5% of the ceiling, the
ability More budget effectively spent by three nodes. A weapon build at
1.9349 has residual 1.1355, so the same rider pays ×1.1355 — about 48% of
its authored multiplier in log terms. A saturated 1.30³ build gets ×1.0000 —
nothing — silently unless the clamp logs. And because a rider spends no
slot, the node comes IN ADDITION to a full three-More build: the clamp eats
the overflow, which is exactly the competition the window chain's comment
already blesses as "the ruling's intent, not a defect."

**The two lawful dispositions — the question:** (a) rider Mores exist under
the window law, partial payment accepted and tooltip-stated, in which case
Collapse's intended shape (×1.30 shared, gated on TargetBandBroken) is legal
under O34 as worded and lands in one cycle; or (b) target-gated lines are
Increased-only permanently, Collapse gets a different design, and the node's
comment says why instead of "waiting." Either answer closes O125's open
bullet. My recommendation, stated rather than implied: (a) — the system
already crossed this line when windows shipped, and refusing riders what
windows have is a distinction no player can see.

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
