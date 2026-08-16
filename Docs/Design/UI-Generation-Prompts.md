# UI Generation Prompts

> STATUS 2026-08-16: HISTORICAL — self-declared below: these prompts produced the mocks and are not a spec; the FIELDPLATE specs and the owner's canvases are the authority.

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Self-contained prompts for generating UI design concepts (Claude, or any
design/image tool). Each carries the full context so it can be pasted
alone. Shared canon is repeated deliberately.

**These prompts are HISTORICAL and are not a spec.** They produced the mocks
that FIELDPLATE was transcribed from. The canon repeated inside them was
current when they were written and has since moved — item level now runs to
120, the rarity ramp gained gated unlocks, Mana inverted, and the HUD gained a
minimap and an absorbed-damage read that no prompt here describes. **Do not
build from this file and do not update code to match it.** The visual authority
is `UI-Style-Guide-Fieldplate.md` and the four screen specs. Keep this file for
the record of how the mocks were produced; regenerate from the current specs if
more concepts are wanted.

---

## Shared context block (prepend to any prompt below)

> Rior's Edge is a first-person movement-driven ARPG looter shooter set on
> an overgrown Earth — nature reclaiming ruins, with weathered futuristic
> tech scattered through it. The player is a Breaker: elite militia who
> enter interdimensional rifts and close them. Tone: grounded, worn,
> quietly ominous — not neon cyberpunk, not high fantasy. HARD COLOR LAW:
> saturated teal (#08B8A8 family) is reserved exclusively for rift
> phenomena, suppression hardware, and Anomalous-rarity objects — it marks
> OBJECTS, never damage or generic UI chrome. Rift-element damage uses a
> hotter, whiter cyan. Established rarity colors: Standard = off-white,
> Uncommon = blue (#408CFF), Exceptional = purple (#B866FF), Aberrant =
> red (#FF4040), Anomalous = teal (#26F2D9). Established UI accents:
> cyan = player/system, orange = weapon/heat, gold = reward/weak-point,
> red = harm. The elements are Rift, Entropy, and Void. Current UI is
> asset-free dark slate panels (#0A111C base) with cyan headers.

---

## Prompt 1 — UI style guide + theme

"Using the shared context above: design a complete UI style guide for
Rior's Edge. Deliver: (1) a design language name and 3-sentence thesis
that reconciles 'overgrown Earth' with 'militia-grade rift tech' — the UI
is Breaker field equipment, so it should feel like ruggedized military
hardware with organic wear, not a glossy hologram; (2) a full color
system: background ramp (3 steps), panel ramp (3 steps), text hierarchy
(3 steps), the four accent roles (player/system, weapon, reward, harm)
with exact hex values, hover/pressed/disabled state rules, and the rarity
ramp integrated — respecting the teal object-law; (3) typography: a
display face for headers, a workhorse for body, a mono/condensed for
numbers, with sizes for h1/h2/body/caption/number-large; (4) shape
language: corner radii, border weights, panel edge treatment (we favor a
thin accent rail on one edge), spacing grid (8px base); (5) texture and
wear direction: how scratches, grime, vegetation shadow, or rift-static
can appear on panels WITHOUT reducing legibility; (6) motion rules:
durations and easing for panel transitions, purchase confirmations, and
damage feedback. Present as a spec another artist could implement."

## Prompt 2 — Ability icon set

"Using the shared context above: design an ability icon system for Rior's
Edge, then apply it. System first: silhouette-driven, single accent color
on dark circular-square (squircle) tiles, readable at 52x52px, with a
consistent stroke weight and a shared perspective (side-on, slight
upward energy). Then design these nine icons as descriptions an
illustrator or SVG generator could execute exactly: SWIFT class — SKIM
(instant momentum redirect: a velocity line snapping to a new vector),
LEAD (marking a distant target: a marked sightline or tagged diamond),
OVERDRIVE (ultimate; a power state of suspended decay and doubled flow:
a meter overflowing or a figure outrunning its own trail). CASTER class —
SPELLBLADE STRIKE (close-range mana edge), VOID LASH (Void-element reach),
OVERCAST (spending below zero: a bar dipping under its own baseline).
Plus three generic states: ULTIMATE READY (violet accent), ON COOLDOWN
(radial sweep convention), UNAFFORDABLE (cost glyph). For each: 1-2
sentence visual description, the accent color used, and what must remain
readable at minimum size."

## Prompt 3 — Skill tree screen mock

"Using the shared context above: design a skill tree screen mockup for
Rior's Edge (describe it precisely enough to build, or render it if you
can). Requirements: the player has TWO progressions — a class tree
(three branches, e.g. Swift: Kinetic/Marksman, tiers gated by points
invested in that tree) and a universal Core tree of five constellations
(Precision, Volley, Affliction, Bulwark, Kinesis) with a sixth (Elements:
Rift/Entropy/Void) arriving later. Node types: Minor (multi-rank), Notable
(1-rank), Convergence, and Keystone (build-defining, one per tree
active). The screen must communicate: unspent points for both currencies,
what each node DOES as a number on the card ('+12% slide speed / rank'),
rank progress, why a node is locked (tier gate / prerequisite / points),
what is purchasable RIGHT NOW at a glance, and the constellation
structure of the Core tree as an actual spatial layout (constellation =
cluster) rather than a list. Interaction: click to buy one rank, hover
for full detail, one-click respec per tree. Assume 1920x1080, dark theme
per the style guide, mouse-first. Deliver: layout description with zones
and dimensions, the node card anatomy at each state (locked, purchasable,
owned, maxed, keystone), and how the class/core switch works."

## Prompt 4 — Inventory / equipment screen mock

"Using the shared context above: design the inventory screen mockup for
Rior's Edge. It is a paper-doll layout: character presence on the left
(a full-body render slot — currently a silhouette placeholder), EIGHT
equipment slots arranged around/beside it (Helmet, Body Armour, Gloves,
Boots, Necklace, Waist, Primary weapon, Secondary weapon), aggregate
stats panel ('gear totals') the player actually reads (health, crit,
move/slide/air multipliers, weapon damage, drop chance), and a backpack
grid below or beside with rarity-colored cards showing item level +
affix list. Required interactions: one-click equip from backpack (shows
what it replaces), one-click unequip, right-click/X discard with a
confirm, bulk 'discard below rarity' with a two-step arm, slot filter
chips, and a comparison affordance (equipped vs candidate deltas).
Items on the ground in-world drop with rarity-colored beams — the screen
should feel continuous with that language. Rarity ramp per the shared
context; Aberrant (max 3 equipped) and Anomalous (max 1) need their
equip-limit state visible. Assume 1920x1080, dark theme, mouse-first.
Deliver: zone layout with dimensions, item card anatomy, the
equip-limit tells, and the empty-state design."

## Prompt 5 — HUD refinement

"Using the shared context above: refine the in-combat HUD. Current
anchor: a Destiny-2-inspired bottom-right cluster (weapon name, large
magazine count /reserve, two ability squares + ultimate with E/T/G key
hints and cooldown sweeps, class resource bar above with state text
SETTLED/RUNNING/REDLINE), bottom-left compact vitals (shield over
health, armor chip, status effect chips), center crosshair with hit
feedback, floating damage numbers (outlined; gold weak-points, larger
orange crits), enemy overhead health bars, wave banner top-center, and
world-space loot popups. Design pass goals: (1) tighten the cluster into
one visual unit with the style guide's shape language; (2) a resource bar
that makes the three Momentum states legible in peripheral vision; (3)
ability squares that communicate ready/window-active/cooldown/
unaffordable without reading text; (4) a damage-number style with more
game-feel (weight, kerning, crit pop) that stays readable in crowds; (5)
an ultimate-active screen treatment stronger than 4 thin edge lines but
short of a full vignette. Deliver precise descriptions + measurements."
