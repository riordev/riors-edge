# <System name>

<!-- 300 lines maximum. Present tense. Intent only.
     No status, no history, no cross-references to other specs. -->

## What this system is for

Two or three sentences. What player experience this system exists to produce,
and what it would mean for it to fail. If you cannot state the failure, the
system is not yet designed.

## The rules

The load-bearing decisions, stated flatly. One idea per line or short
paragraph. These are the things another system may rely on without asking.

Write the rule, not the reasoning. Derivations belong in the commit message
that introduced them.

## The model

The arithmetic, the shape, the tables. Only what is needed to implement or to
argue with. A derivation appears here only when the derivation *is* the design
— the power curve identity qualifies; most things do not.

Numbers are placeholders unless marked otherwise. Say which are which.

## Boundaries

What this system deliberately does not do, and which system does it instead.
Name the other system, do not cite its document.

## Asserted invariants

The properties held by automated tests, and the test name for each. If a
balance target is stated anywhere above, it appears here with its test, or it
is not a target.

| Invariant | Test |
|---|---|
| | |

## Open

Genuinely undecided things, one line each, no options analysis. When one is
decided it moves to DECISIONS.md and disappears from here.

<!-- Not in this template, deliberately, and not to be added:
     - status banners or build state
     - "last reconciled against" markers
     - superseded or struck-through text
     - provenance labels
     - references to other documents as authority
     - historical sections kept "for the record"
     - open-question ledgers with recommended defaults and cost analysis -->
