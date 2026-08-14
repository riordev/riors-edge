# FIELDPLATE — Combat HUD spec

Owner-authored design canvas, transcribed from `HUD.dc.html` on 2026-08-13.
Reads on top of `UI-Style-Guide-Fieldplate.md`; every colour token below is
defined there. Reference resolution 1920×1080; the implementation scales all
geometry by `ViewportHeight / 1080`.

## Anchors

- Safe margin **48px** on all sides.
- **Vitals** bottom-left, 420 wide: status chips above the plate, then shield
  (11px) over health (8px) with values right-aligned in a fixed 84px column so
  they never shift, armour as three 18×5 chips. Cyan left rail.
- **Combat cluster** bottom-right, see below.
- **Wave banner** centred at 48px from the top.
- **Crosshair** 80×80 centred; hit ticks gold at 45°.
- **Enemy bars** 180×8 with the name at 11px beneath.
- **Loot popups** sit in world space with the rarity rail and the take key
  inline.

## 1 · One cluster, one plate

The bottom-right group is a single **440×184** panel at 48px from both edges:
one 1px `#2A3E58` border, one 3px orange (`#FF8A3D`) rail on the left edge,
12px interior padding, 10px between rows. Momentum bar on top, weapon name and
magazine as one baseline-aligned row, three ability squares as the base.
Grenades are not a Breaker resource, so that readout is gone and the row ends
at the ultimate. Nothing floats loose: the cluster is one piece of equipment
with three readouts, and the orange rail states that the whole unit belongs to
the weapon.

## 2 · Momentum in peripheral vision

The three states are separated by colour, fill height, and segment count, not
by reading the word. 12px track, two 2px notches at 33% and 66%. The bar stays
short on purpose: every class drops its own resource into this slot, so the row
has to hold a different label and range without regrowing.

| State | Colour | Track |
| --- | --- | --- |
| Settled | cyan `#4FD8F5` | low fill, single continuous bar |
| Running | gold `#FFC64A` | fill crosses the first notch; the bar splits into 8px chevron-cut blocks so the texture itself changes |
| Redline | orange `#FF8A3D` | past the second notch, blocks widen to 14px and the track border goes orange 2px |

The state word is a confirmation for the centre of vision, never the carrier.

## 3 · Ability squares without text

56×56, 4px radius, 12px gaps.

| State | Treatment |
| --- | --- |
| Ready | 2px accent border, glyph at full accent |
| Window active | border stays 2px, plate fill lifts to `#1F3047`, two 8px corner ticks at top-left and bottom-right — geometry, not brightness |
| Cooldown | 1px neutral border, glyph to `#3E4C5E`, hard dark wedge sweeping clockwise plus the timer in mono |
| Unaffordable | deep red border, struck hex chip lower-centre, no sweep — nothing is filling, so waiting will not fix it |

Key hints sit in the bottom-right corner at 11px and inherit the state colour,
so a glance at the letter also reports the state.

## 4 · Damage numbers

Mono 700 with −0.02em tracking and a 2px outline in a near-black tinted toward
the number's own hue, so the outline never reads as grey mud.

| Kind | Size / weight | Colour |
| --- | --- | --- |
| Body | 40 / 500 | `#DCE4EE` |
| Crit | 80 / 700, spawns at 140% for 60ms | `#FF8A3D` |
| Weak point | 64 | `#FFC64A` |
| Rift element | body size | `#A8FBFF` — never teal |

Thousands get a thin space, not a comma — the space survives at 40px where a
comma turns into a dot. In crowds, numbers within 60px of one another stack
vertically at 8px offsets instead of overlapping, and a fourth simultaneous
number is dropped rather than drawn.

## 5 · Ultimate treatment

A 3px violet (`#B866FF`) frame inset 8px from the screen edge, plus a 120px
violet band at 10% opacity along the top and bottom edges only — the sides stay
clear so peripheral threat reading is untouched. A 260px title plate sits at
132px from the top, clear of the wave banner band, for the first 1.2s, then
fades leaving the frame. On expiry the frame steps down 3px → 2px → 1px over
the final 3 seconds, so the ultimate ending is visible without a timer.

## Implementation status (2026-08-13)

`ABreakerPlaytestHUD` implements all five sections and the anchor table, at
`ViewportHeight/1080` scale, using `BreakerUIStyle.h` tokens.

**The class-resource row is generic (2026-08-13).** §2's slot no longer reads
Swift's component: `ABreakerPlaytestHUD::ResolveResourceRow` returns a
`BreakerHUD::FResourceRow` (label, signed fraction, state word, state colour,
track treatment, track border) from `UI/BreakerHUDResourceRow.h`, and
`DrawResourceTrack` paints that. Two classes are wired — Swift's Momentum
(Settled/Running/Redline, unchanged) and Caster's Mana (BANKED/OVERCAST). A
third class is one `Resolve*` function, not a new drawing path. The 12px track
and the two notches at 33%/66% are fixed for every class.

**Overcast is drawn as a signed channel.** Mana can go negative, which a
left-anchored fill cannot express. Stealing track width for a debt zone would
move the two notches the spec fixes, so the axis that inverts is the vertical
one: a 1px grey zero baseline across the middle of the track, credit growing
rightward in the upper half in `#4FD8F5`, debt growing rightward in the LOWER
half in harm `#FF4040`, with the track border widening to 2px harm exactly as
Redline widens to 2px orange. Length reads magnitude, side of the baseline
reads sign. This is what the Overcast *icon* already does ("the same channel,
half above zero and half beneath"), so the HUD and the icon set agree. Debt is
measured against the Overcast floor, not against maximum Mana: while negative
the bar answers "how much rope is left", and the rope is the floor.

Deviations, all deliberate:

- **Typeface** is the engine default; the canvas HUD has no Saira/Barlow/
  JetBrains asset to draw with yet. Sizes are converted from the spec's pixel
  values through one calibration constant, so importing the fonts later is a
  token change rather than a layout change.
- **Ultimate window source**: the frame is driven by
  `Window.Swift.Overdrive`, the only ultimate window that exists. The step-down
  needs a known total duration; `UBreakerAbilityStateComponent` exposes only
  remaining time, so the step-down keys off the final 3 seconds directly, which
  is what the spec describes anyway.
- **Overcast is not reachable in play yet.** `UBreakerAttributeSet::
  PreAttributeChange` clamps ClassResource to `[0, Max]`, so nothing drives the
  bank negative until the `ClassResourceFloor` attribute (spec D8) lands. The
  debt half of the track is correct the moment it does; it is verified by the
  `RiorsEdge.UI.ClassResourceRow` automation test, not by a playtest.
- **Playtest chrome** (F1/F2/F3 key legend, diagnostics panel, report-copied
  confirmation) is not in the design canvas. It is kept, restyled onto the
  tokens, at `text/muted` in the top-left — it is instrumentation, not shipping
  UI, and it leaves with the gym.


## Type-size corrections from looking at it (2026-08-14)

The spec's cluster type sizes were authored on a design canvas and never seen
in-engine. Once the screenshot harness existed the owner looked at the rendered
HUD and asked for two of them down: *"the settled font and the gun ammo size
seem a little too big and disjointed on both ends."*

| Readout | Spec | Shipped | Why |
|---|---:|---:|---|
| Class-resource state word | 17 | **13** | It is a CONFIRMATION of what the track already says in colour, fill height and block texture. At 17 it competed with the track for the row instead of annotating it. |
| Magazine | 44 | **32** | At 44 it was taller than the weapon name and state line stacked together, pulling the eye into the corner — the opposite of what a subordinate readout should do. |
| Reserve | 18 | **15** | Held in proportion to the magazine. |

All three are now tokens in `BreakerUIStyle.h` (`HudResourceStatePixels`,
`HudMagazinePixels`, `HudReservePixels`) rather than literals at the call site,
so the next size request is a one-line edit in one place.

**"Disjointed" was a real geometry bug, not only a size complaint.** Both pairs
were positioned with hand-tuned vertical offsets — `Y + Pad - 3` for the state
word, `+2` and `+22` for the two ammo numbers — that only coincidentally lined
up at the sizes they were tuned against, and drifted apart the moment either
size moved. Each pair now shares one **baseline computed from the measured
glyph heights**, so the smaller number sits on the larger one's bottom edge at
every size and every UI scale. That is the same class of fix as the
`MOMENTUMSETTLED` collision: a magic number replaced by a measurement.
