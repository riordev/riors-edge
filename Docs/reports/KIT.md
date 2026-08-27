# KIT

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Do vault and mantle generate Momentum?

The post-verbs recon (session report, 2026-08-27) measured a completed
traversal's Momentum yield at exactly zero: MOVE_Flying reads as neither
airborne nor sliding, and the zeroed velocity defeats the ground branch. The
wall-ride source the verbs replaced generated +10/s and nothing replaced it —
Swift's two newest deliberate verbs feed no loop. Whether they SHOULD is
authoring vocabulary (a new generation source), not KIT's to invent.
**Question:** rule it — a flat per-traversal grant (the dash's one-shot
shape), or deliberately nothing. Separately, a mechanical DEFECT in the same
read is KIT's regardless of the answer: a mantle taken mid-fall RESETS the
airborne generation credit (bAirborne false during MOVE_Flying refills the
3-second budget), so jump-mantle-fall refunds a window it should not — the
fix shape (traversal does not reset credits) is proposed and held for the
report to be read.

## When does the traversal move into a predicted movement mode?

The execution is pawn-side SetActorLocation in MOVE_Flying — zero impact on
the listen-server host (every playtest to date), and a hard rubber-band
failure for any remote client, graded in the report as "not a bug in the
slice; a wall the slice hits when a second player connects." A custom
movement mode + saved-move pass costs an estimated 250-300 lines, needs no
content or ruling, and also fixes three sibling defects at their shared root
(the viewmodel's dead pose during traversal, the Momentum credit reset, the
absent HUD/telemetry read — all predicates written against a mode that did
not exist). **Question:** schedule it, or hold until multiplayer is nearer —
the recon's numbers are in the report either way.
