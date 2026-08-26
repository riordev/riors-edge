# FIELD

The lane's open questions for the design seat, in one place. Answered questions
are deleted; git holds them. Findings and status live in the session reports,
never here.

## The health ramp's terminal hue — is magenta the death read?

O146 applies the readability pack's authored offset in the display domain it was
authored in, which fixed the magnitude problem: travel spread across the twelve
(family, rank) pairs fell from 30.8 to 11.1 dE76 and the worst case rose from
27.7 to 38.8. But **a delta carries magnitude, not hue direction.** The Boss row
barely moves blue and all three family bases are blue-ish, so every family's Boss
dies MAGENTA rather than red — Vestige runs `#A099AD` to `#C669BD`.

Three answers, none obviously right:

- give the delta a hue-direction term, so the travel aims at the authored
  destination rather than merely covering the authored distance;
- re-author the four rank rows against the bases they actually land on, which
  makes the pack's table a function of the family list;
- accept magenta as the boss's death colour and say so at the table.

**Question:** which. Readability measures the same under all three — this is a
look question, and FIELD will not pick it alone.

## The fracture mask — does it get a project-owned material?

The pack's second near-death carrier (emissive crack mask `#FF4040`, coverage
0.00 / 0.04 / 0.15 / 0.40 / 0.75, pulsing 2 Hz below 15%) has nothing to write
to. Measured on the body's material, `/Engine/BasicShapes/BasicShapeMaterial`:
exactly two parameters, vector `Color` and scalar `Roughness`. No emissive, no
texture parameter, no static switch — and it is an ENGINE asset, so it cannot be
edited. There are still **zero** `SetScalarParameterValue` sites anywhere in
non-test `Source/RiorsEdge`.

The mask needs a project material carrying a mask input and an emissive path.
That is editor asset work and needs the owner's authorisation.

**Question:** authorise it, or rule that colour carries near-death alone. The
second is a real answer — but it is the single point of failure A3 was written to
remove, and it means a colour-blind screen, fog, or a red wall each take the
whole read.

## What photographs the enemy bar?

Nothing does, and A1/A7/A8/A9 shipped unphotographed because of it. The bar culls
at 50 m measured from the PLAYER, and every route in is blocked:

- autoplay spawns the player facing a berm with no enemy inside that radius;
- the crowd probe's grid stands at 60 m, past the cull;
- `-BreakerCaptureTour` moves the CAMERA while the cull still measures from the
  player, so a vantage standing among enemies culls every bar anyway.

The same gap hides O129's ramp, which needs a DAMAGED body and has no headless
way to make one — no `Breaker.` command sets health or deals damage.

FIELD can build the instrument (a console command spreading live enemies' health
across the authored stops photographs both) and **intends to unless told
otherwise** — that half needs no ruling. What is not FIELD's to decide is the
camera-versus-player mismatch: it spans the capture tour and the bar, and it is
either a tour defect or a deliberate limit nobody has written down.

**Question:** which of those two, and whose.

## Whether enemies deal elemental damage at all

Carried in `DECISIONS.md`'s Open list as a bare line since before this lane
existed. It is FIELD's to build, so the measurement belongs here.

Measured across non-test `Source/RiorsEdge`, counting every mention of the
enumerator including UPROPERTY defaults: **35 `Physical`, 4 `TrueDamage`, 3
`Elemental`** — and all three Elemental sites are player-side
(`Abilities/BreakerAbility_Resonance`, `Abilities/BreakerAbility_Siphon`,
`Items/BreakerItemTypes.h`). In `Combat/` it is 22 Physical, 4 TrueDamage and
**zero** Elemental, so no enemy path can produce an element today.

Two things wait on the answer: O5's per-element resistance has nothing incoming
to resist, and the shield-break asymmetry stays parked because `bBypassShield`
has no reason to fire.

**Question:** do enemies get elements. It changes the damage pipeline's shape
rather than a table, which is why it has sat unanswered rather than being cheap.

## Two claims dropped rather than assembled

Recorded because "I checked and it is not true any more" is worth as much as a
question, and both were things this lane would otherwise have kept repeating.

- **The Phase D impact-signal misalignment is stale.** The standing note says
  three "round landed" signals fire at three different times — hit sound at
  trigger, crosshair at +16 ms, world spark at flight time. `ScheduleArrivalSound`
  now puts the hit cue on the round's own flight clock, so at least the sound
  half is already fixed. Not re-measured end to end; not asserted.
- **The elemental figure this lane was carrying was not its own.** It arrived as
  "11 Physical + 1 TrueDamage" and does not match anything measurable now. The
  numbers above are FIELD's own, with the scope stated. Passing on a second-hand
  number is the O145 mistake wearing different clothes.
