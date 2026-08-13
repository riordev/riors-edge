# FIELDPLATE — Rior's Edge UI specification v1.0

Owner-authored design canvas, transcribed from `Form decisions pending.zip`
(`Style Guide.dc.html`) on 2026-08-13. This is the visual authority for every
screen and for the combat HUD. Companion documents: `UI-HUD-Spec.md`,
`UI-Inventory-Spec.md`, `UI-Skill-Tree-Spec.md`, `UI-Ability-Icons-Spec.md`.
The implementation token header is `Source/RiorsEdge/UI/BreakerUIStyle.h`;
values there must match this file exactly.

> The Breaker interface is stamped sheet metal, not glass. Every panel reads as
> a plate bolted to a chassis that has been carried through wet ruin — square
> shoulders, hard edges, a single accent rail marking function the way paint
> marks equipment in a motor pool. Rift technology never softens that: it is
> quarantined into thin instrument lines and object-marking teal, so the
> ominous part of the screen is always the part that belongs to the anomaly,
> never the housing around it.

Base `#0A111C` · grid 8px · radius 2px · **no gradients, no blur**.

## 01 Colour system

Flat fills only. Depth comes from border value, never from gradient.

### Background ramp

| Token | Hex | Use |
| --- | --- | --- |
| `bg/void` | `#060B12` | screen field, modal scrim base |
| `bg/base` | `#0A111C` | default screen ground |
| `bg/raised` | `#0F1826` | zone separation, gutters |

### Panel ramp

| Token | Hex | Use |
| --- | --- | --- |
| `panel/00` | `#131E2E` | plate face |
| `panel/10` | `#18263A` | cards, list rows, slots |
| `panel/20` | `#1F3047` | hover, headers, dividers |

### Text hierarchy

| Token | Hex | Use |
| --- | --- | --- |
| `text/primary` | `#E6EDF5` | names, values, headers |
| `text/secondary` | `#9FB0C4` | body, affix lines |
| `text/muted` | `#63768C` | labels, keycaps, meta |
| `text/disabled` | `#3E4C5E` | disabled controls, cooled-down glyphs |

### Function accents

| Family | Accent | Deep (pressed / track fill) | Use |
| --- | --- | --- | --- |
| Player / System | `#4FD8F5` | — | shields, class resource, focus rings, selected state, headers. The only accent allowed on chrome. |
| Weapon / Heat | `#FF8A3D` | `#C25A1E` | magazine, reload, heat, weapon stat deltas, crit damage numbers |
| Reward / Weak point | `#FFC64A` | `#B98212` | currency, weak-point hits, unspent points, purchase confirm |
| Harm | `#FF4040` | `#C22A2A` | health loss, incoming threat, destructive actions, negative deltas. Shares hue with Aberrant rarity by design. |

Rift-element **damage** uses `#A8FBFF` — hotter, whiter, damage-only.
Ultimates carry violet `#B866FF`.

### Teal object law

Teal is a noun, never an adjective.

- **Permitted**: rift geometry and rift VFX; suppression hardware (charges,
  anchors, lances) and their readouts (`#08B8A8`); Anomalous-rarity item
  frames, beams, and name text (`#26F2D9`). Nothing else.
- **Forbidden**: damage numbers of any element; buttons, borders, rails, focus
  rings, tab underlines, progress tracks, tooltips, icons for non-rift
  abilities. If a teal pixel is not describing a thing that exists in the
  world, it is a bug.

### Rarity ramp

| Rarity | Hex | Note |
| --- | --- | --- |
| Standard | `#DCE4EE` | |
| Uncommon | `#408CFF` | |
| Exceptional | `#B866FF` | |
| Aberrant | `#FF4040` | max 3 equipped |
| Anomalous | `#26F2D9` | max 1 equipped |

Rarity colour appears on the card's left rail (3px), the item name text, and
the world-drop beam — nothing else. Card faces stay `panel/10` at every rarity
so a wall of loot does not become a wall of colour. Anomalous is the single
exception: it also gets a full 1px border, because it is the only tier that is
also a world object class.

### Interaction states

| State | Fill | Border | Text |
| --- | --- | --- | --- |
| Primary action | `panel/20` | 1px `#4FD8F5` | `#E6EDF5` |
| Secondary | transparent | 1px `#2A3E58` | `#9FB0C4` |
| Discard | transparent | 1px `#C22A2A` | `#FF4040` |
| Disabled | `#0F1826` | 1px `#1F3047` | `#3E4C5E` |

Hover raises the fill one panel step and shifts the accent one step brighter.
Pressed drops the fill one step below rest and holds the accent. Disabled keeps
geometry, drops text to `#3E4C5E`, strips the accent entirely — never lowers
opacity, which would reveal the plate seams behind it.

## 02 Typography

Three faces, all SIL Open Font License.

- **Display — Saira Condensed**, weights 600/700, always uppercase, tracking
  +0.04em. Screen titles, panel headers, item names, buttons.
- **Body — Barlow**, weights 400/500/600, sentence case. Affix lines, tooltips,
  descriptions, all prose.
- **Numeric — JetBrains Mono**, tabular by default. Every number that ticks:
  magazine, damage, stat values, timers, keycaps. Fixed advance means counters
  never reflow mid-fight.

| Token | Size / line height | Weight | Use |
| --- | --- | --- | --- |
| h1 | 34 / 36 | 700 | screen title, one per screen, top-left of header zone |
| h2 | 20 / 24 | 600 | panel headers, item names, tab labels |
| body | 14 / 20 | 400 | affix lines, node descriptions, tooltips |
| caption | 11 / 14 | 500 | labels, states, keycaps, meta. Uppercase, +0.16em tracking. Never below 11px. |
| number-large | 56 / 56 | 700 | magazine count, HUD hero numbers. Reserve at 22 / 400, text/muted. |

## 03 Shape and space

8px base grid. Nothing lands off it.

**Accent rail — the signature.** Left rail is identity: which system owns this
panel. Top rail is transient status, reserved for events and alerts. The rail
is 3px, full-bleed to the panel's edge, no inset, no radius on the rail itself.
One rail per panel — a panel with two rails has no meaning. Nested panels do
not repeat the parent's rail colour.

**Radii.** 0px for structural zones and grid cells. 2px default — panels,
cards, buttons, slots. 4px only on ability squares and squircle icon tiles.
Never above 4px.

**Borders.** 1px `#1F3047` at rest, 1px `#2A3E58` for emphasis, 2px accent for
selected, 3px for the rail. There is no 2px neutral border.

**Spacing scale**: 4 / 8 / 16 / 24 / 40 / 64. 12 and 32 are not tokens.

- 4 — icon to label, inside chips
- 8 — stacked text lines, grid gutter
- 16 — card padding, sibling cards
- 24 — panel padding, panel to panel
- 40 — zone separation
- 64 — screen margin at 1920

Minimum hit target 44px. HUD ability squares 64px (the HUD spec draws them at
56px inside the cluster; see `UI-HUD-Spec.md`).

## 04 Motion

- **Panel transition** — 180ms, `cubic-bezier(0.16,1,0.3,1)`. Panels slide 16px
  along their rail axis and fade 0→1. Out is 120ms linear with no travel:
  equipment closes faster than it opens. No scale, ever — plates do not grow.
- **Purchase confirm** — 90ms in, 260ms out, ease-out. The border snaps to gold
  on frame one, no ease in; the commit must feel mechanical. Rank pip fills,
  then gold decays back to the owned colour over 260ms. Total 350ms, never
  queued: spam-buying reads as a stutter of snaps.
- **Damage feedback** — 40ms pop, 520ms rise, ease-out. Numbers spawn at 115%
  for 40ms, settle to 100%, rise 40px and fade over the last 200ms. Crits spawn
  at 140% and hold 60ms. Health-loss bars use 0ms drain and 400ms ease-out chip
  recovery — harm is instant, relief is slow.

## 05 Texture and wear — OPTIONAL, POST V1

- **Scratches** — one tiling plate-wear texture, additive, 6% opacity max, only
  on `panel/00` faces. Never crosses a text bounding box: reserve 16px clear
  inside every panel. One diagonal for the whole game.
- **Grime** — corner-weighted darkening, 8% max, inside the panel border only.
  Nothing in the centre third of a panel.
- **Vegetation shadow** — leaf silhouette entering from one screen corner, 10%
  black, drifting 2–3px over 8s. Menu screens only, never the combat HUD.
- **Rift static** — reserved. Only on panels describing an open rift,
  suppression hardware, or an Anomalous item: 1px teal scanline jitter along
  the rail, 2-frame flicker every 3–5s.

Legibility gate for all four: text contrast stays above 7:1 against the
dirtiest pixel under it, and the whole wear layer is a single toggle that ships
off by default.

## Implementation status (2026-08-13)

Implemented in this pass:

- `BreakerUIStyle.h` carries the full palette, rarity ramp, spacing scale, rail
  and border widths, and the type scale as engine tokens.
- `ABreakerPlaytestHUD` is rebuilt against the HUD spec (see that document).
- `SBreakerMenu` uses the background/panel ramps, rails, the four interaction
  states, the type scale, and the rarity/node state language.

Not implemented, and deliberately so:

- **Fonts.** Saira Condensed, Barlow, and JetBrains Mono are not in the project
  yet. The canvas HUD draws with the engine's default face and the Slate
  screens use `FCoreStyle`; the type *scale* is honoured, the *faces* are not.
  Importing the three OFL families and pointing the tokens at them is a
  separate content task.
- **Motion.** The canvas HUD honours the damage-number timings; Slate panel
  transitions and the purchase-confirm snap are not animated.
- **Texture and wear.** Section 05 is post-V1 by the spec's own labelling.
- Full screen re-zoning for Inventory and the Skill Tree board (the 1920-wide
  three-column and path-field layouts) — the token/state pass landed, the
  layout re-architecture did not. See the two specs for what remains.
