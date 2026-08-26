# KIT

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## The empty second slot's repair semantics (O176 as landed)

A stale foreign-class id in Swift's slot two now repairs to EMPTY rather than
to a playable default — for every other class the stale-save repair hands the
class default so the slice stays playable, and for Swift it now hands the
ruled empty slot. **Question:** ruled shape, or unwanted side effect of the
empty-slot feature? (KIT read the ruling as covering it; flagged because the
repair path predates the ruling.)

## Sightline's cover clause (O175)

"Cannot be blocked by cover-state enemies" is recorded absent — nothing on
the shot path consults a cover state; a Skirmisher's InCover is a position
behind ordinary geometry. The one actor-attached shot blocker in the game is
the Warden's shield. **Question:** scope the clause to Warden-shield
penetration, or retire it?

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

## Photographing unlockable abilities

`-BreakerAbilityProbe[=Class]` casts only DEFAULT loadouts through the real
grant site, so an unlockable ability (five of Swift's six class abilities,
and every non-starter elsewhere) cannot be photographed headlessly until a
character has levels and tokens. **Question:** acceptable — or rule a
probe-only dev token grant so the capture harness can reach the whole kit?

## Ultimate screen feel

Overdrive's accidental full-screen violet wash (camera inside an additive
primitive) was removed as a blind. If a deliberate brief tint on ultimate
ignition is WANTED as a cue, that is a GLASS post-process question, not
pooled primitives. Flagged only.
