# FIELDPLATE — Combat HUD spec

**Last reconciled against: O32**

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

**Damage numbers ABBREVIATE past 10 000 (2026-08-14).** The owner reported them
too large for the second time, after they had already been cut ~35% once. What
changed between the two reports was not the type — it was **O29**: item level
now runs to 120 and affix values roughly doubled, so a figure that was three
digits when 26/40/52 were chosen is routinely five or six, and the thin-space
rule above makes a six-digit number *eight glyphs* wide. At the crit size that
is most of the target. The complaint is width, and width is a formatting
question, so the sizes are **held** and `BreakerUI::FormatDamage` holds every
number to at most five glyphs instead:

| Range | Form | Example |
| --- | --- | --- |
| < 10 000 | exact, thin space | `8 420` |
| 10 000 – 99 999 | one decimal | `12.4k` |
| 100 000 – 999 999 | whole | `148k` |
| ≥ 1 000 000 | two, then one, then no decimals | `1.24M`, `12.4M` |

The thin-space rule is unchanged for every readout in a **fixed column** — the
magazine, the reserve, the health and shield pools — where the space is
reserved whatever the value does and the exact figure is wanted.
`BreakerUI::FormatTicker` still serves those. `RiorsEdge.UI.DamageNumberFormat`
asserts the five-glyph budget across twelve magnitudes, so the next content
retune cannot silently re-break it.

### 4.1 Absorbed hits — AUTHORED HERE, not from the design canvas

**Authorship note.** Sections 1–5 are transcribed from the owner's `HUD.dc.html`
canvas. This subsection is not: it was authored on 2026-08-14 in response to an
owner playtest of the Severed Warden — *"Warden armor works but no indication
that you're hitting a shield — it just didn't take damage for a bit."* The
canvas has no absorbed state, so everything below is a proposal to be ruled on,
not a transcription.

The Warden has frontal armour and an unarmoured rear, and its whole design is
that the front is the wrong place to shoot (Encounter-Design §2.3: "the
POSITIONING is the multiplier, via armour geometry, not a stat"). Before this,
the only report of that was the health bar not moving, which reads as a broken
game rather than as a wrong angle — an enemy failing to teach its own mechanic.

**Signal source.** `FBreakerDamageResult` already determines it and needs no new
field: `RawDamage` is post-crit and pre-defence, `MitigatedDamage` is the same
number after armour, the incoming-damage multiplier and a block roll, and
`1 − Mitigated/Raw` is the share the target ate. Deliberately called
**mitigation**, not armour, so an Overcast debuff or a future damage-reduction
modifier reads through the same channel instead of the HUD lying about a cause.
Latched at the shot, never looked up later: the number outlives the hit and the
target's armour is facing-dependent.

**Threshold** `BreakerUI::DamageAbsorbedThreshold` = 0.20. Below it, ordinary
armour shaving, and saying so every frame is noise.

| Read | Where | Treatment |
| --- | --- | --- |
| Hit marker | crosshair | Third tick state. Ticks pull outward (6→13 inner, 14→23 outer) and close into four 9px corner brackets — the *opposite* motion to the weak-point tick, which opens. Weapon/heat `#FF8A3D`. |
| Damage number | at the impact | Recedes to `text/muted` whatever it would otherwise have been, with an `ABSORBED −47%` caption in `#FF8A3D` beneath it, positioned off the number's own measured glyph height. |

Two rules this obeys and the reason for each:

- **The three damage SIZES are untouched.** An absorbed crit is still a crit and
  still 52px. The sizes are the only thing separating a body shot from a weak
  point from a crit at a glance, and losing them here would delete that
  separation exactly when the player most needs it.
- **The marker is told by geometry, not by a third colour.** Two colour states
  at one 2px mark do not read in a fight, and gold is already spoken for.

**Rejected, and why, because it was tried and looked at:** the first pass drew
both the number and the tick in `OrangeDeep` `#C25A1E`. On screen an absorbed
crit in OrangeDeep sits one value step from an ordinary crit in Orange, so the
two states that most need separating read most alike; and at 2px over the world
the deep step is too dark to survive a dark background. Deep values are
FIELDPLATE 01's *pressed / track fill* register and do not belong on a
world-space mark.

## 5 · Ultimate treatment

A 3px violet (`#B866FF`) frame inset 8px from the screen edge, plus a 120px
violet band at 10% opacity along the top and bottom edges only — the sides stay
clear so peripheral threat reading is untouched. A 260px title plate sits at
132px from the top, clear of the wave banner band, for the first 1.2s, then
fades leaving the frame. On expiry the frame steps down 3px → 2px → 1px over
the final 3 seconds, so the ultimate ending is visible without a timer.

## 6 · Minimap — AUTHORED HERE, no design canvas exists

**Authorship note, stated plainly because the brief asked for it.** There is no
minimap on the owner's `HUD.dc.html` canvas and no line about one anywhere in
the FIELDPLATE corpus. The owner asked for the feature — *"need a minimap as
well"* — and **every decision in this section is mine**, derived from three
things that already exist and are authoritative: `UI-Style-Guide-Fieldplate.md`
for the visual system, `Docs/Design/Level-Design.md` §5 for the field's real
dimensions, and **O19** for the teal object law. Nothing below is transcribed.
It is a proposal, and the shape, size, scale and contents are all open to an
owner ruling.

### 6.1 The one decision everything else follows from: it is LANDSCAPE

Level-Design §5 gives the field a **25000 cm long axis** (−3000 … 22000 on the
spawn-forward axis) with every station strung along it — camp, breach, range,
dash markers, encounter pocket at 8500, jump-gap run, arena at 17000 — while
the *occupied* width is roughly 8000 cm. The field is a corridor, not a square.

A square minimap over a corridor spends most of its area on empty flank. So the
plate is **320 × 176** (1.82 : 1; both on the 8px grid at 40 and 22 cells) with
the field's forward axis running along its **long** side.

It follows that the map is **field-aligned, not rotating**: world +X is always
plate-right. A rotating map throws that alignment away on every turn, and the
one question this field actually raises — *how far along am I, and what is
ahead* — is exactly the one a rotating map answers worst. The cost is that
facing has to be carried by the player mark instead of by the map, which is
what the triangle is for.

### 6.2 Anchors and scale

| Property | Value | Derivation |
| --- | ---: | --- |
| Anchor | top-right, 48px | The last free corner. Top-left is the playtest legend, top-centre the wave banner, both bottom corners the vitals plate and the combat cluster. |
| Plate | 320 × 176 | §6.1. |
| Scale | **56 cm per spec pixel** | Puts the half-window at 8960 cm forward × 4928 cm lateral, chosen so the encounter pocket (8500 cm out) is *on* the map from the safe ring rather than one pixel off its edge. One 8px grid step is 448 cm. |
| Graticule | every **2000 cm** | The combat pocket radius (Level-Design G6), so a cell is a statement about fighting distance rather than an arbitrary ruler. World-anchored, so it slides past as the player moves — a grid pinned to the plate is decoration; a grid pinned to the world is what says you are travelling. |

### 6.3 Contents

| Mark | Treatment |
| --- | --- |
| Plate | Standard FIELDPLATE plate: flat `bg/base` face, 1px `#2A3E58` border, one 3px **cyan** `#4FD8F5` left rail. |
| Player | Cyan triangle at plate centre, 9px, rotating with actor forward. The only mark here that reports a direction, and since the map does not turn it is the entire facing readout. |
| Safe ring | Segmented cyan outline at 65% alpha, 40 segments, radius from `GetSafeZoneRadius()`. An outline and not a fill, because the ring is a boundary and a filled disc would compete with the blips on top of it. Segments outside the plate are simply not drawn. |
| Hostiles | Harm `#FF4040` squares at 5px. Squares, not dots: a square survives at 5px where a circle smudges, and the system has no radius above 4px anyway. |
| Elite / boss | Gold **edge** around the blip; boss also 1.8× size. Rank is never a fill — harm has to keep meaning "hostile" whatever the rank does, the same rule the enemy health bars already follow. |
| Off-map hostiles | Clamped to the rim at 0.6× size and 55% alpha rather than dropped. Direction of threat is the most useful thing a minimap reports, and a field this long puts most of a wave off the window. |
| Scale line | `GRID 20M` at 11px `text/muted`, bottom-left inside. A map with no unit is a picture. |

### 6.4 The teal question, answered explicitly

**No teal anywhere on this plate**, and that is deliberate rather than
incidental. FIELDPLATE 01 makes the player/system cyan "the only accent allowed
on chrome", and **O19** makes teal a *noun* — legal only on rift geometry,
suppression hardware and Anomalous items. A minimap is chrome. The payoff is
forward-looking: the day a rift or a suppression anchor gets a marker on this
map, it can be teal *precisely because* nothing else on the plate is.

### 6.5 Cost

It is drawn on the canvas HUD in `DrawHUD`, so it had to be free.

- **It iterates nothing.** `DrawEnemyHealthBars` already walked every
  `ABreakerEnemy` once per frame; it now fills `EnemyBlips` inside that same
  loop, *before* the health-bar distance culls (a bar is pointless past 50 m and
  a minimap is mostly useful beyond it). `DrawMinimap` reads the array. The
  ordering is a stated contract in both functions.
- **It allocates nothing per frame.** `EnemyBlips` is a member and is `Reset()`
  rather than emptied, so capacity survives and the array stops allocating after
  the first busy frame.
- Everything else is flat rects, four `DrawLine` fans and one triangle.

### 6.6 Not built

- No terrain, no station labels, no map for the field's geometry — the field is
  spawned procedurally by `BreakerGameMode` and nothing publishes a footprint a
  HUD could draw. The graticule is standing in for that.
- No loot, NPC, objective or rift markers.
- No zoom control, and no north/forward compass letter.
- Never playtested. It has been photographed at 1920×1080 and read; whether
  56 cm per pixel is the right window is a feel question a screenshot cannot
  answer.

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
- ~~**Overcast is not reachable in play yet.**~~ **SUPERSEDED 2026-08-14 — it is
  reachable.** `UBreakerAttributeSet` gained the replicated `ClassResourceFloor`
  attribute (spec D8), defaulting to **0** for every class so Swift's Momentum is
  bit-identical, and `PreAttributeChange` now clamps ClassResource to
  `[Floor, Max]` — which is what lets an ordinary GAS cost effect drive the bank
  negative. `UBreakerManaComponent` owns the floor and publishes it only while
  the permanent class is Caster. The debt half of the track is live.
- **The Mana row's SEMANTICS inverted underneath this section, and the drawing
  did not change.** Per an owner ruling on 2026-08-14, Mana no longer
  accumulates from zero — the bar **starts full, spends down, and regenerates**
  (`PassiveRegenPerSecond` 6.0/s), with conditional income from weapon hits
  demoted to an accelerator capped at par with it. The signed channel described
  above is still exactly right, because Overcast is still the only thing that
  crosses zero and length-reads-magnitude / side-reads-sign is indifferent to
  which direction the ordinary case travels. **What is NOT re-checked is the
  label.** `BANKED` was chosen for an accumulating bank and now names a pool
  that starts full; whether it should read `MANA` is a copy question this spec
  should answer and currently does not. **Recorded for the owner, not decided.**
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

## The fixed-offset audit (2026-08-14)

The owner's screenshot showed the wave banner printing `WAVE 01` and
`4 HOSTILE` on top of one another. It is the third instance of one defect and
the file has now been swept for the rest of them.

**The shape of the defect**, stated once so it is recognisable next time: *a
left-aligned string and a right-aligned string share a row, and nothing in the
code knows they share it.* The left one starts at a fixed inset and runs as far
as its content runs; the right one runs backwards from a fixed inset just as
far; they meet somewhere in the middle that no constant can predict, because
the content is generated. Widening the container or nudging the constant only
changes which string re-opens it. This is the canvas-HUD twin of the Slate
`SHorizontalBox` overflow recorded in `UI-Style-Guide-Fieldplate.md`.

| Where | Was | Now |
| --- | --- | --- |
| **Wave banner** *(reported)* | 260px plate, divider at a fixed 55% (143px), 28px title rendering wider than that gutter, count right-aligned. A two-digit count closed what slack was left. | Both strings measured; content laid out left-to-right from the measurements; divider X from the title's width; **plate sized from the content**, 260 as a minimum only. One shared baseline. |
| Cluster row 1, speed readout | Right-aligned to the plate edge, knowing nothing about the class label and state word beside it. Slack today with `MOMENTUM`/`SETTLED`; none of it guaranteed. | Yields — it is the least important of the three and is playtest chrome — rather than printing through the state word. |
| Cluster row 2, weapon name | Left-aligned at 18px against a right-aligned ammo pair, neither aware of the other. | Ammo pair measured **first** (it is a fixed-column readout by design); the name is fitted into what remains, stepping down the type scale by measurement. |
| Status chips | Ran rightward with **no bound at all**. Three stacked DoTs walked them out from under the vitals plate. | Bounded to the vitals width; overflow counted as a `+N` chip rather than silently dropped. |
| Loot popup | Fixed 300px panel; item names and affix lines are generated content drawn at their own widths inside it. | Sized from the widest measured line, 300 as a minimum. |

`ABreakerPlaytestHUD::FitSpecPixels` is the shared step-down helper. Like every
other measured fit in this codebase its input is the measurement of a
**different** string, never the widget's own arrangement, so it is a pure
function of inputs known before layout and cannot oscillate.

## Looking at it: `-BreakerCaptureHUD` (2026-08-14)

The wave banner and every damage number shipped broken, and that is not a
coincidence: they were the two things on this HUD a capture run could not
reach. `-BreakerAutoPlay` drops the player into the gym and then nothing
presses F4 to start a wave and nothing pulls a trigger, so neither readout had
ever appeared in a screenshot.

`-BreakerCaptureHUD` is a dev-only, command-line-gated switch on the HUD that
fabricates the **events** and nothing else: four damage numbers at worst-case
O29 magnitudes (body, weak point, crit, absorbed crit), a hit marker in the
absorbed state, and a wave banner cycling its three shapes — the owner's
`WAVE 01 / 4 HOSTILE`, the widest active case `WAVE 12 / 24 HOSTILE`, and
`CLEAR — F4`. Layout, measurement and colour all go through the identical
drawing paths, so what is photographed is the real thing.

Combine with `-BreakerAutoPlay -BreakerScreenshots=N`.

## The verified / unverified split for this HUD (2026-08-14)

Stated in one place, because "photographed" and "playtested" are different
claims and this spec makes both.

**VERIFIED BY LOOKING** — captured at 1920×1080 and read:

- The whole bottom-right combat cluster: plate, rail, class-resource track with
  its notches and state word, weapon/ammo row on its shared baseline, and the
  three ability squares in their resting state.
- The bottom-left vitals plate: shield over health, the fixed 84px value column,
  the armour chips, and the status chips **inside their new bound** (they
  previously ran rightward with no bound at all and walked off the plate).
- Damage numbers at every magnitude the abbreviation table produces, plus the
  weak-point and crit sizes and the absorbed treatment.
- The hit marker in all three tick states, including the absorbed corner
  brackets.
- The wave banner in all three shapes it can take.
- The minimap plate, graticule, safe ring, blips and player triangle.

The last three exist as captures **only because `-BreakerCaptureHUD` was
written**, and all three had shipped wrong before it existed.

**NOT VERIFIED, and the harness structurally cannot** — it has no mouse and
cannot pull a trigger or press a key:

- Every hover state anywhere on the front end. On this HUD that is limited (the
  canvas HUD is not hover-driven), but the loot popup's look-at behaviour is
  adjacent and has only ever been seen in its resting composition.
- The four **ability state overlays** — ready, window-active, cooldown wedge,
  unaffordable. `-BreakerCaptureHUD` fabricates damage and banner events; it
  does not yet fabricate ability states. See `UI-Ability-Icons-Spec.md`.
- The Overcast debt half of the resource track. It is reachable in play now, but
  reaching it requires a Caster overspending, which a headless run does not do.
  Proven by `RiorsEdge.UI.ClassResourceRow`, which is arithmetic, not a picture.

**NOT VERIFIED, and no capture ever will be** — these need a playtest:

- Whether 56 cm per pixel is the right minimap window.
- Whether the absorbed read is noticed mid-fight, which is the entire point of
  it.
- Whether the damage-number hierarchy survives at combat pace, and whether the
  abbreviation reads as information or as a number that got smaller.
- Whether the cluster's peripheral position works while aiming.

**A screenshot shows composition. It has never once shown whether something
feels right, and this spec should not be read as claiming otherwise.**
