---
paths:
  - "Source/RiorsEdge/UI/**"
  - "Source/RiorsEdge/Combat/BreakerEnemyHealthBars.cpp"
---

# UI and presentation (GLASS)

- Teal is a noun: only rift objects, suppression hardware, top-rarity frames,
  beams and name text. Never damage numbers, chrome, focus rings or icons.
- Never signal state by colour alone. Never reflow the HUD between fights.
- No item score, no DPS meter, anywhere, ever.
- Every non-alphanumeric mark is drawn geometry, never a glyph.
- Nothing reads its own arrangement: no container sized from allotted space
  inside a scroll box, no per-frame widget rebuilds, no auto-wrap where width
  matters, no box sized to its shortest label.
- Disabled controls are painted, never faded.
- Colour by verb (O179): cyan movement/cleanse, orange weapon/explosion/
  deployable, gold heal/leech/weak-point, harm-red taunt, violet ultimate.
- Cast moments get world flashes; windows stay HUD bars.
- `BreakerMenu.cpp` is over 11,000 lines: a new screen goes in its own TU.
- Run `/photograph` and read your own frames before reporting visual work.
