# LEDGER

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## The ability-built fixture: estimated ≈0.5, and the re-base needs your ruling

Part One-J asked what an ability-built tree fixture would measure, before
authoring it. The analytic estimate, method stated: anchor the weapon
denominator at the tip's measured layers (flat 1.154, increased 5.789, More
1.9349); rebuild the ability build's tree spend as the mirror of the weapon
fixture's structure — ARC complete (+45 ability, +4 shared), Reservoir
complete (+18 ability, +18 shared), eight Ability travel picks (+120), the
Velocity chain bought for Terminal Velocity's shared More, floor 16.25 —
giving increased ≈ ×4.3 against 5.789 (ratio ≈ 0.74 vs today's 0.461), More
1.664 vs 1.9349 (0.860 vs 0.672), crit falling to ≈0.90 (the ability spend
gives up Precision's flats — diverges for the first time once the trees
split), flat 0.867 structural. **Product ≈ 0.49, range 0.48–0.51.**

So the answer to your either/or is BOTH, split roughly in half: the fixture
question accounts for 0.27 → ~0.5, and ~0.5 → 0.85 is genuine content — gear
ability-affix breadth (the increased layer is gear-dominated and the ability
pool is one seeded line per slot), the atlas's one-ability-wheel-against-
four-weapon-wheels asymmetry (ARC's 45 authored ability points against
Precision+Volley+Velocity+Vector), and the structural flat half. Tuning
content against 0.27 would overshoot by roughly 2x.

**Question:** rule the re-base — the ability fixture buys an ability-built
tree (a measurement-basis change; the pinned number will roughly double
without the game moving, which is exactly why it is yours to rule, and the
estimate above is the prediction to check it against) — and the content
routes then tune against the honest remainder.

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
