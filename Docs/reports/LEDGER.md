# LEDGER

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## The dash-distance wiring cost, counted — which side of your line is it on?

You asked for the cost before the cooldown reading gets authored. Neither
route is literally "one entry and one wire," and the honest counts are:

- **Route 1, the DashCooldown precedent** (gear and tree sharing one additive
  bucket): a new aggregated attribute lane — `UBreakerAttributeSet` property
  with accessors, OnRep, lifetime-replication row and floor, the
  `EBreakerAggregatedAttribute` entry, the `EBreakerNodeStatTarget` entry, the
  `AggregateStats` mapping line, and Movement's composed read at the dash
  site. About eight sites across three files; Movement/ is not mine. This is
  the route that MUST exist the day a gear dash-distance affix is authored —
  two layers, one bucket, the bug class fixed everywhere else.
- **Route 2, the jump precedent** (tree-only): Movement already includes the
  progression component and binds `OnProgressionChanged` for the jump grant,
  so the shape is the node-target entry, an `FBreakerNodeStats` field, the
  mapping line, and ONE read beside `DashSpeedBonus` at the dash impulse.
  Four sites, one of them a single coordinated line in Movement/. Honest only
  while no gear dash line exists; the day one is authored this migrates to
  Route 1, and that would be recorded at the read.

**Question:** does Route 2 qualify as your "one enum entry and one
aggregation wire"? If yes, distance is the reading (your felt-immediately
argument stands and I agree with it) and I land Route 2 with the migration
note. If no, the cooldown reading stands per your closing condition. Either
way the seeded-free-rank grant is ruled and lands with it, and at-cap and
parity get re-measured and reported both ways in the landing report (the
O95 precedent, your third property).

## Does the rider path carry a More bucket? (the Collapse question, O125's bullet)

Reported in full two cycles ago. The short form: hit-time Mores cannot enter
the strongest-three sort (membership is unrepresentable in the request's
two-float split, and the sheet's "N / 3 MORE" stops meaning anything), but
they already exist under the window law — the outgoing chain spends ceiling
headroom, not a slot. A rider More under that law is one clamp at the
recomposition site. **Question:** is partial payment acceptable — full ×1.30
on an ability build (composing 2.1632 of 2.197), ×1.1355 on a weapon build at
1.9349, nothing at saturation, tooltip-stated? If yes, Collapse's intended
shape is lawful under O34 as already worded. If no, target-gated lines are
Increased-only permanently and Collapse gets a different design.

## O-number allocation still happens at ruling time

The O120 collision is repaired (the reward ruling is O137 now, loading keeps
O120, citations grepped — every one referenced loading) and the O125 pair was
settled by your renumber assignments. The mechanism that produced both stands.
**Question:** may the working rule become "allocate the number at push time,
from the rebased file"? One line in CLAUDE.md's session discipline; the file
is yours.
