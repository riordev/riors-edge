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
  `N/1` on the teal rail *and* a full teal border. Both counts are derived in
  the UI by walking `UBreakerEquipmentComponent::GetEquipped()` —
  the component exposes no rarity-count accessor, and adding one was out of
  scope for this pass. The count turns to its rail colour at the limit.
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

Not landed:

- Per-affix deltas against the equipped piece (▲/▼/=) and the equip-limit
  disclosure footer (`LIMIT FULL 3/3` + hover outline on the ejected piece).
  Both need equipment-side queries that do not exist yet.
- The bulk-discard modal; the arm still commits on the second click.
- The empty backpack shows its line of copy but not the five rarity beams.
- Exact zone arithmetic. The spec's 560 + 400 + 960 sums to a full-bleed 1920,
  which the 64px screen margin makes impossible; the panel is 1760 wide, the
  two fixed columns keep their spec widths and the backpack takes what is left
  (two cards across, not three). Below roughly 1500px of panel the columns
  will overflow rather than reflow — deliberate, since reflowing on allotted
  size is what caused the historical layout oscillation.
- Gear score in the meta line is the sum of equipped item levels — `O2
  PLACEHOLDER`, no shipping formula is authored.
- **Nothing here is visually verified.** The build compiles and the automation
  suite passes; no one has looked at the screen.
