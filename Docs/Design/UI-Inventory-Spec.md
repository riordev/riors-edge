# FIELDPLATE — Loadout / inventory screen spec

Owner-authored design canvas, transcribed from `Inventory.dc.html` on
2026-08-13. Reads on top of `UI-Style-Guide-Fieldplate.md`.

## Zones (1920×1080)

- **Header** 1920×88 at `bg/raised`: title `Loadout` (h1), meta line
  `BREAKER · SWIFT · LV 42 · GEAR SCORE 1 284` in mono caption, then the two
  equip-limit counters as railed chips (Aberrant 3/3 red rail, Anomalous 1/1
  teal rail and teal border), then the bulk-discard button at the right.
- **Character column** 560 wide: full-body render slot on top (560×660
  silhouette placeholder), gear totals pinned beneath it at 20 rows so the
  numbers are always on screen with the doll.
- **Equipment column** 400 wide: eight slots as full-width rows in wear order —
  head to foot, then trinkets, then weapons.
- **Backpack** takes the remaining 960: filter bar 64 tall (`All / Armour /
  Weapons / Trinkets`, plus `RMB / X DISCARD · LMB EQUIP` hint), then a
  3-across card grid at 16px gaps.
- **No footer** by design; input hints live in the backpack bar.

## Card anatomy

Rarity lives on the 3px left rail and the name only; every face stays
`panel/10`.

1. Name (h2, rarity colour) plus item level in mono at 18px — the two things
   scanned first.
2. Rarity and slot, mono caption, `text/muted`.
3. Affix list at 13px mono, each affix carrying its delta against the equipped
   piece: cyan ▲, red ▼, muted `=` for parity.
4. Footer line stating the consequence of clicking: which item it replaces, or
   that the slot is empty.

## Limit tells

Two counters sit in the header permanently, so the constraint is never a
surprise at click time. On an equipped card the rarity tag repeats under the
item level. On a backpack card that would exceed the limit, the footer reads
`LIMIT FULL 3/3` next to the name of the piece the swap will eject, and
hovering outlines that piece in red in the equipment column. The action is
never blocked — it is disclosed.

## Empty and bulk

Empty slots keep full geometry with a dashed border and the slot name — the
doll never looks broken, only unfinished. An empty backpack shows the five
rarity beams as vertical bars with one line tying the screen to the world:
*loot is found by colour*. Bulk discard is a two-step arm — the header button
turns gold and reads `CONFIRM`, then a modal states the count, the exclusions,
and the destructive label (`Destroy 14`, red border on `#2A1414`).

## Implementation status (2026-08-13, layout re-zoning pass)

Landed in `SBreakerMenu::BuildInventoryScreen`:

- Fieldplate palette throughout, rarity as a 3px left rail plus name colour on
  a `panel/10` face, Anomalous additionally bordered.
- Card anatomy lines 1, 2 and 3, the type scale, and the mono caption style.
- Empty slots as dashed-look plates that keep their geometry and slot name.
- The two-step bulk-discard arm (`CONFIRM` in gold), on the spec's colours.
- **The zone re-architecture.** The screen is no longer a centred vertical
  flow. `SBreakerMenu::BuildZonedFrame` provides an 88-tall header band at
  `bg/raised` on the cyan identity rail carrying the title, the meta line, the
  `EQUIPMENT | SKILL TREES` tab strip, both equip-limit chips, the bulk-discard
  arm and `BACK`; the body is a three-column row beneath it; there is no
  footer, per the spec.
- **Equip-limit counters, live.** Aberrant `N/3` on the harm rail, Anomalous
  `N/1` on the teal rail *and* a full teal border. The count turns to its rail
  colour at the limit. Both the counts and the caps now come from
  `UBreakerEquipmentComponent` (`CountEquippedOfRarity` /
  `EquipLimitForRarity`); the screen holds no second opinion about a rule that
  decides which of the player's items gets ejected.
- **Character column, 560 wide**: the render-slot placeholder fills the top,
  gear totals pinned beneath it as aligned label/value rows in a fixed 104px
  value column. The value carries its function family's colour — player/system
  survivability and movement cyan, weapon damage and crit orange, drop chance
  gold. (There is no gear-granted shield stat in `FBreakerEquipmentStats`, so
  health is the cyan survivability row until one exists.)
- **Equipment column, 400 wide**: all eight slots as full-width rows in wear
  order — helmet, body, gloves, waist, boots, then the necklace trinket, then
  primary and secondary — replacing the old two-column split.
- **Backpack fills the remainder**: a 64-tall filter bar carrying the count,
  the slot chips and the `RMB / X DISCARD · LMB EQUIP` hint, then the card
  grid beneath it.

## Implementation status (2026-08-13, disclosure pass)

The queries the previous pass was missing now exist on
`UBreakerEquipmentComponent`, and the four disclosure features are built on
them. **Comparison and cap arithmetic are game rules and live in C++ with
tests; the screen states their answers and works nothing out itself.**

- **Per-affix deltas.** Every affix line on a backpack card carries its delta
  against the equipped piece in that slot: cyan ▲ better, harm-red ▼ worse,
  muted `=` parity, in a fixed 14px glyph column so the affix names keep a
  straight left edge. Matching is by **(stat target, bucket)**, not affix id —
  two affixes feeding the same stat are one number to the player, and a flat
  +Health is not comparable against an Increased Health percentage. An empty
  slot makes every line an improvement. Polarity note: every stat target in the
  slice pool is "higher is better" (Dash Cooldown *Reduction* included); a
  target where lower is better would need a polarity flag on
  `FBreakerAffixDefinition` before it could be compared.
- **The equip-limit disclosure.** A backpack card that would break the cap
  carries a second footer line, `LIMIT FULL N/N · EJECTS <RARITY> <SLOT> iN`,
  in harm red, and hovering it outlines that equipment row in harm red. **The
  action is not blocked** — `EquipItem` performs the equip and ejects the named
  piece to the backpack. The displacement rule: the **weakest** equipped piece
  of that rarity leaves — lowest item level, ties broken by wear order — and
  the piece already in the candidate's own slot is excluded, because it is
  leaving regardless. So an Aberrant-for-Aberrant swap in the same slot ejects
  nothing extra even at 3/3. Items carry no display name field yet, so the
  doomed piece is named by rarity and slot, which is how its own card is
  titled. The hover is `OnHovered`/`OnUnhovered` painting one border
  imperatively — never a per-frame attribute, per the historical jitter bug.
  (`RestoreState` deliberately does *not* enforce the cap: loading a save must
  not reshuffle the player's loadout.)
- **The bulk-discard modal.** The header arm still turns gold and reads
  `CONFIRM`; the second click now opens a modal instead of committing. It
  states the count, the exclusions (equipped gear, Aberrant, Anomalous), and
  offers `Cancel` beside `DESTROY N` in harm red on the destructive face
  (`#2A1414`, new `BreakerUI::DestructiveFace` token) with a 2px harm ring.
  The scrim swallows clicks so the screen behind cannot be operated through it.
  The count comes from `CountBackpackBelowRarity`, which shares its predicate
  with `DiscardBackpackBelowRarity` — the number stated and the number
  destroyed cannot drift.
- **The empty backpack** now draws the five rarity beams as vertical bars in
  the ground-drop ramp, under `LOOT IS FOUND BY COLOUR`.

New component API (all usable from a ground-loot popup or a vendor screen, not
just this one screen), covered by four new automation tests
(`RiorsEdge.Items.Equipment.{RarityLimits,LimitDisplacement,AffixDeltas,BulkDiscardCount}`):

- `EquipLimitForRarity(Rarity)` — static; `INDEX_NONE` means uncapped.
- `CountEquippedOfRarity(Rarity)` / `CountBackpackBelowRarity(MinimumKept)`.
- `PreviewEquip(Candidate)` → `FBreakerEquipPreview`: the slot swap, the limit
  displacement, the rarity count and cap, and the affix deltas — the complete
  consequence of one click, answered before it. `PreviewEquipAgainst` is the
  pure form over any equipped set, so the cap rule is testable with no
  component, actor or world.
- `CompareAffixes(Candidate, Reference)` → `TArray<FBreakerAffixComparison>`,
  one row per candidate affix in order.

## Implementation status (2026-08-14, the first LOOKING pass)

**This screen has now been photographed at 1920x1080 and iterated against the
picture.** What looking found:

- **The slot filter chip row overflowed the viewport and collided with the
  legend.** `ALL / HELMET / BODY ARMOUR / …` ran off the right edge of a 1920
  screen and the input legend was drawn straight through it, producing
  `GLOVES / X DISCARD LMB NECKLACE`. Two bugs in one place, both fixed:
  - The chips sat in an `SHorizontalBox` `FillWidth` slot that also held the
    legend. An overflowing horizontal box does not wrap and does not shrink its
    child — it draws it at full width through whatever shares the row. The bar
    is now TWO rows: the count and the legend own the first outright, so the
    legend cannot be overdrawn again; the chips get the whole width of the
    second and wrap into as many rows as they need. The bar is auto-height with
    the spec's 64 as a FLOOR rather than a fixed height.
  - The wrap is arithmetic, not an `SWrapBox`. `MeasureChipWidth` measures TEXT
    against a FONT (`FSlateFontMeasure`) and `PackChipRows` fills rows against
    a width derived from the measured viewport. Nothing reads an allotted size,
    so there is no layout feedback loop — this is emphatically not the banned
    `SWrapBox`+`UseAllottedSize` pattern, which measures a widget against its
    own arrangement. A per-chip `ButtonChromeAllowance` of 32px covers the
    padding `SButton` draws under our own `ContentPadding`; it was MEASURED from
    a capture after the first attempt under-counted and the row still overran.
- **The screen sizes from the viewport now.** It was authored at a hard 1760
  panel with hard 560/400 columns, so it could only be correct at one window
  size — the exact lesson the skill matrix had already learned. Both wide
  screens now share `MeasureWideScreen()`. Below the point where the spec's
  fixed columns plus a usable backpack stop fitting, the two FIXED columns give
  ground (floored at 320/280) and the backpack keeps its room, because the
  backpack is where the cards are. Cards per row is arithmetic on the measured
  backpack zone (1–4) instead of a frozen 2.

Still unverified:

- **Hover has never been photographed.** The equip-limit outline, the per-affix
  deltas on a hovered card and the doomed-piece disclosure are all hover-driven
  and the capture harness cannot move a mouse.
- **The backpack was EMPTY in every capture**, so card anatomy, affix delta
  glyphs, the limit-tell footer line and the discard modal are all still
  unlooked-at. The empty-backpack state (five rarity beams under LOOT IS FOUND
  BY COLOUR) reads correctly and is verified.
- The ▲/▼ tofu question is therefore still open — see the delta-glyph note
  below.

### Superseded (2026-08-13)

Still not landed:

- **Nothing in this pass is visually verified either.** Build clean, 70/70
  automation tests pass; no one has looked at the screen. Layout and hover
  behaviour are unverified by definition. *(Superseded by the 2026-08-14
  looking pass above: layout is now verified, hover is not.)*
- The ▲/▼ glyphs are `BreakerUI::DeltaBetterGlyph` / `DeltaWorseGlyph`
  tokens. The shipping faces are not imported, and the fallback face's
  Geometric Shapes coverage is unverified — if they render as tofu, those two
  lines are the fix.
- The equipped card does not repeat its rarity tag under the item level.
- Exact zone arithmetic. The spec's 560 + 400 + 960 sums to a full-bleed 1920,
  which the 64px screen margin makes impossible; the panel is 1760 wide, the
  two fixed columns keep their spec widths and the backpack takes what is left
  (two cards across, not three). Below roughly 1500px of panel the columns
  will overflow rather than reflow — deliberate, since reflowing on allotted
  size is what caused the historical layout oscillation. *(Superseded
  2026-08-14: the panel and the columns derive from the measured viewport, and
  narrow windows shrink the two fixed columns rather than overflowing. Still
  not an allotted-size reflow — the viewport is read once per rebuild.)*
- Gear score in the meta line is the sum of equipped item levels — `O2
  PLACEHOLDER`, no shipping formula is authored.
