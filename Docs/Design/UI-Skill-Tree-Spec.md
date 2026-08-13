# FIELDPLATE — Skill matrix spec

Owner-authored design canvas, transcribed from `Skill Tree v2.dc.html` on
2026-08-13 (`Skill Tree.dc.html` is the superseded first pass). Reads on top of
`UI-Style-Guide-Fieldplate.md`.

## Zones (1920×1080)

- **Header** 1920×88 at `bg/raised`: title `Skill matrix` (h1), meta line
  `BREAKER · SWIFT · LV 42`, the two-tab switch (`Class · Swift` / `Core`),
  both point counters as separate railed chips — class points cyan rail, core
  points gold rail, each showing unspent large and `/ N spent` beneath — and
  respec at the far right in the discard style.
- **Board** 1500×936 fills the rest, with a 60px branch header strip above the
  path field.
- **Detail rail** is a fixed 420px column so node cards never reflow when it
  populates.
- **Footer** 1920×56 carries the input legend (`LMB BUY 1 RANK · HOVER FULL
  DETAIL · SHIFT+LMB BUY TO MAX · TAB SWITCH TREE`) and the live purchasable
  count in gold.
- Screen margin 40px, panel gutters 24px.

## Node states

Every card carries the effect as a number in mono on line two, so the tree is
readable without hovering anything.

| State | Treatment |
| --- | --- |
| Owned | cyan rail, cyan pips, `MAXED` rather than `5/5` alone when full |
| Purchasable | gold 2px border and a gold action line (`1 PT → RANK 4`). Gold is the only border colour that means "spend now", which is what makes scanning work. |
| Available, unowned | neutral 1px `#2A3E58` |
| Locked | dashed border, muted text, and the reason stated literally — tier gate with a progress bar (`TIER GATE 6 / 8`), or the prerequisite by name (`NEEDS WALL CARRY`) |

Keystones and sealed clusters also read dashed.

## Class ↔ Core

One tab pair, not a mode toggle: the board swaps, the header and detail rail
persist.

**Class** is three branches drawn as paths: one 2px trunk per branch running
the full height, cyan where the route is already owned and `#1F3047` where it
is not, with 2px diagonals dropping to each node. Tier gates are dashed
hairlines across the column, labelled once in a dedicated 76px gutter at the
left edge of the field — two short lines, tier then gate cost — so a label can
never land on node copy. Minor and Notable nodes label beneath the marker in a
180px block; Convergence and Keystone label to the right of the marker so the
trunk never runs through their text. Nodes are the markers themselves — 48px
square for multi-rank Minors with the rank inside, 44px diamond for Notables,
64px square for Convergences, 60px diamond for Keystones — and their name,
number, and state sit as plain text beneath, not inside a card.

**Core** is spatial: five clusters positioned around Kinesis as the hub, linked
by convergence lines, each cluster showing its node grid at a glance with a
purchasable count. Elements sits sealed below centre in suppression teal
(`#08B8A8`), the one place teal is legal here because it is a rift object, not
chrome.

Respec is per-tree and its button label states which tree it will clear.

## Implementation status (2026-08-13)

Landed in `SBreakerMenu::BuildSkillTreesScreen`:

- Fieldplate palette, the header/board/detail proportions in spirit (tree
  selector column, node grid, status line), the type scale.
- The four node states as the spec's colour language: owned cyan rail and pips,
  purchasable gold 2px border with a gold action line, available neutral,
  locked muted with the failure reason printed literally.
- `MAXED` in place of `5/5`, and the effect line in mono on line two.
- The per-tree respec button in the discard style, labelled with its tree.

Not landed:

- The path board: trunks, diagonals, tier-gate hairlines, and the marker
  geometry (48px square / 44px diamond / 64px square / 60px diamond). The grid
  of node cards remains.
- The Core constellation map (five clusters around Kinesis, convergence lines,
  sealed Elements cluster) — core nodes still enumerate as cards.
- The 420px hover-detail rail and the `SHIFT+LMB buy to max` input.
