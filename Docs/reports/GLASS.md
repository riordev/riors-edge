# GLASS

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Twenty of the thirty-two remaining gameplay tags have no consumer — do they stay?

H1 removed the ten tags that name systems the game does not have. The sweep
that proved them gone also shows that only twelve of the remaining
thirty-two tags in `Config/DefaultGameplayTags.ini` are requested anywhere in
`Source/`, `Content/` or `Config/`. The other twenty name systems that DO
exist — `Ability.Movement.Dash`, `Ability.Weapon.Fire`, the five class
resources, `Status.Shock`, `Damage.Physical`, `Combat.Critical`,
`State.Dead`, `State.Blocking` and their siblings — but nothing in code or
content resolves them; those systems run on enums and components, not on
these tags.

- H1's criterion was "none referenced by code", and by that criterion these
  twenty qualify too. I did not delete them because the ten you listed share
  a different property — no system behind them — and I read the list as
  chosen by that property, not by the grep.
- **Question:** are tags for real systems that nothing requests declared
  vocabulary waiting for a consumer, or the same drift one step later? If the
  latter, the deletion is one commit; if the former, the header comment on
  the ini should say so, so the next sweep does not re-ask.

## O179 names no colour for a death — the world flash wears the kill confirm's

GLASS-1's four moments are muzzle, impact, cast and death. O179 assigns
colour by verb and covers the first three (weapon/heat for the gun's
economy, the weak-point promise in Gold, the caster's verb for a cast). It
says nothing about a body leaving the fight.

- Built as: Harm red, Gold on a weak-point kill — the exact pair the
  crosshair kill confirm already draws, so one event has one colour on the
  HUD and in the world. Recorded at `UI/BreakerEffectMomentMath.h` and pinned
  by `RiorsEdge.UI.EffectMoment.ColourLaw`, so a ruling changes one line and
  one assertion.
- **Question:** is a death the target's colour (Harm, the enemy's role), the
  reward's (Gold — a kill is a payment received, the same argument O179 makes
  for a heal), or the killing verb's (the weapon's Orange, the ability's own)?
  The third reading means the HUD cannot draw it alone; `OnHitDealt` carries
  the delivery but not the verb's colour.

## Who authors the four Niagara systems, and is GLASS-1 done before they exist?

The editor Python surface cannot build an emitter or read a system's exposed
parameters (probed headless), and `.uasset` is never hand-edited, so no lane
can author `NS_Muzzle`, `NS_Impact`, `NS_Cast`, `NS_Death` from a Code seat.
The renderer resolves each from `/Game/Breaker/FX/` lazily and draws the
pooled fallback until it exists; a system needs one `Color` linear-colour
user parameter to receive the O179 colour.

- **Question:** does GLASS-1 close on the slots and fallback, with the four
  systems an owner item in the editor, or does it stay open until the
  "feedback needs to be better" frame shows Niagara? Its done-when also names
  Part One-B's four items, three of which are FIELD's (labels, bars, glyphs).
