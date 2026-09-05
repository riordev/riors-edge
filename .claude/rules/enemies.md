---
paths:
  - "Source/RiorsEdge/Combat/**"
  - "Source/RiorsEdge/AI/**"
---

# Enemies and combat (FIELD, NAV)

- Damage resolves in the order `Docs/spec/combat.md` states. A new step is
  written there first.
- Enemy taxonomy is three orthogonal fields: Archetype, Rank, Modifiers (O9).
  Modifier count drives rank; boss is authored. Content-scaled to area level,
  never player-scaled (O27).
- Every modifier passes three tests: readable in graybox, answerable by
  base-kit movement, not a stat. A modifier with no visible tell is not done.
- Family-to-mesh mapping is DATA: swapping every placeholder mesh must be a
  content change with no C++ diff.
- One resolver owns body `Color`; layers compose forward, none reads back
  (O128). Colour carries health, not rank (O129).
- Locomotion: an enemy moves through its `AAIController` and movement
  component on the NavMesh. `AddActorWorldOffset` is not locomotion. The
  band classifier (`ClassifyBand`) picks the goal; MoveTo reaches it.
- TTK targets are per archetype × rank (O18). A figure that does not name its
  archetype asserts nothing.
