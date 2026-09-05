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
