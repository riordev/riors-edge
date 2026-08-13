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

## Implementation status (2026-08-13)

Landed in `SBreakerMenu::BuildInventoryScreen`:

- Fieldplate palette throughout, rarity as a 3px left rail plus name colour on
  a `panel/10` face, Anomalous additionally bordered.
- Card anatomy lines 1, 2 and 3, the type scale, and the mono caption style.
- Empty slots as dashed-look plates that keep their geometry and slot name.
- The two-step bulk-discard arm (`CONFIRM` in gold), already present, now on
  the spec's colours.

Not landed:

- The 1920 three-column re-zoning. The screen is still a centred panel with a
  vertical flow; header counters, the render slot, and the pinned gear-totals
  block are not built.
- Per-affix deltas against the equipped piece (▲/▼/=) and the equip-limit
  disclosure footer (`LIMIT FULL 3/3` + hover outline on the ejected piece).
  Both need equipment-side queries that do not exist yet.
- The bulk-discard modal; the arm currently commits on the second click.
