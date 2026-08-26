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

## Two Swift leftovers in LEDGER's file, post-landing

Both survived the O176 landing and both now double-pay against the landed
abilities: the Marksman Sightline node's +2 Pierce authored stand-in (its own
comment says it comes out "the day the grant goes in" — the ability exists
now), and the `Swift.Marksman.Lead` node grant, which is a free route around
the token now that Lead is an unlockable (deleting it also trips
NoPhantomAbilityGrants' zero-grants guard, so it needs LEDGER sequencing).
**Question:** rule the retirement of each, or keep either deliberately.

## Presentation colour law, before more classes bake it in

Two choices KIT made from the existing palette roles, applied to Swift, Tank
and Support and about to spread: melee sweeps share ONE look regardless of
class (Rend reuses Cleave's cyan arc — one verb, one look), and heals pulse
GOLD (the reward family; a heal is a payment received — no palette role
existed for healing). **Question:** confirm both, or name a healing accent
and a per-class melee rule instead.

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
