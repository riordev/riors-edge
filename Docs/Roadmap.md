# Production roadmap — RETIRED

**HISTORICAL — RETIRED BY O28.** This document's five milestones are no
longer the operative plan. `Docs/Design/Decisions.md` is the only ruling
ledger; `CONTEXT.md`'s next-actions list is the operative plan (playtest →
report → fix). Kept for its history; do not plan from this document.

**Last reconciled against: O40**

**This document no longer governs anything.** Retired by ruling **O28**
(`Docs/Design/Decisions.md`).

The operative plan is the **Current milestone and next actions** section of
`CONTEXT.md`. Read that instead.

## Why it was retired

Two plans described the same work and disagreed, which is worse than having
one. This file's six milestones were written before the vertical slice took
its current shape, and its status lines drifted badly out of date — it
described the loot loop as "not started" long after item instances, affix
tiers, the roll pipeline, ground pickups, the eight-slot equipment component
and the inventory screen were all live and tested. A stale plan does not
merely fail to help; it actively misroutes anyone who trusts it.

The milestone framing itself was also the wrong shape for how the project
actually runs: work arrives as owner playtest findings and O-ledger rulings,
gets responded to in a wave, and is recorded in
`Docs/Playtest-Feedback-Log.md`. A linear milestone ladder never described
that loop.

## Where its content went

- **What is built** → the *Current state* section of `CONTEXT.md`.
- **What is next** → the *Current milestone and next actions* section of
  `CONTEXT.md`, kept in priority order.
- **Exit criteria and slice scope** → `Docs/Vertical-Slice.md`.
- **Why a thing is the way it is** → `Docs/Design/Decisions.md`.

The original milestone text remains in git history if it is ever wanted.
