# KIT

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Part One-R, corrected by the movement recon: a mantle already EXISTS

`ABreakerCharacter::TryMantle` (BreakerCharacter.cpp:798-846) is a working
mantle — forward wall probe, downward top probe, 35-150 cm height window,
capsule clearance sweep, smoothstep execution — wired into the jump key
THIRD, behind TryWallJump and slide-jump. ORDERS' "the movement component
implements no mantle" is true of Movement/ and false of the game: the verb
is on the pawn, unpublished, untested, and possibly starved by input
ordering. So One-R's KIT work is extract-publish-test, not build.
**Question:** confirm the shape — the mantle's predicate moves into
Movement/ as pure rules (the house pattern, currently violated by 48 fused
lines), the pawn keeps only execution, and MantleStepHeight publishes from
there?

## Wall-jump's fate, and Terminal Velocity's half-orphaned text

Wall-JUMP is hard-gated on wall-RIDE as written, but it is separable: the
standalone FindRunnableWall trace can feed it directly, so removal can keep
the jump and delete only the 48-line ride state machine. **Question:** does
wall-jump survive One-R, or retire with the ride? And Overdrive's Terminal
Velocity keystone suspends the wall-ride timer as half its stated rewrite
(K12) — the removal orphans that half; rule its replacement or its
retirement (LEDGER's node file cites the same text).

## One mantle height, currently three

MantleMaximumHeight = 150 (pawn), MantleStepHeight = 145 (game mode, the
grammar's number), and the engine's enforced MaxStepHeight = 45 — unauthored
anywhere, the only one that actually does anything today. The publish-once
number One-R orders needs ONE ruled value; the grammar validated against
145 while the pawn accepts 150 and the engine steps 45. Also:
BreakerGameMode.h:196 cites "Movement-Design: 35-150 cm" and
Docs/Movement-Design.md contains no such text. **Question:** rule the
value (KIT proposes 145, the grammar's, since a yard was validated against
it) and whether MaxStepHeight gets authored to agree.

## The probe equip surface (One-U item 16, approved — waiting on LEDGER)

The probe-only grant is ruled YES with the never-touch-a-save guard. KIT's
half is landed (the guard refuses character-state writes for any probe
session; Sightline's clause retirement rode the same commit). The equip path
dead-ends at Progression's one-writer unlock refusal, so a dev-only
DevForceEquipAbility surface is requested from LEDGER (message sent,
2026-08-26); the probe's =Class:AbilityId form wires up when it lands. Not a
seat question — recorded here so the mirror stays complete.
