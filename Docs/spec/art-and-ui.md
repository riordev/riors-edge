# Art and UI

## What this system is for

To make the game legible at speed. The player is airborne, sliding and
strafing; every asset is judged at fifteen to forty metres, in motion, in a
sixtieth of a second. Silhouette and value contrast are the entire budget.

The interface has a harder job than the world: gear is the whole endgame, so
the item card and the comparison view are the most important screens in the
game, not the HUD. A beautiful enemy the player cannot parse while wall-riding
is a failed asset, and a tooltip that hides which of two items is better is a
failed screen.

## The rules

### Art direction

Four pillars, ordered. When two conflict the higher one wins.

**1. Readability beats fidelity, always.** Every enemy and every weapon must
survive the **flat-grey test**: rendered at half saturation, uniform mid-grey,
at thirty metres, the family, the threat class and the severance stage must
still be identifiable. If they are not, the model is not done.

**2. Grounded near-future, degraded outward.** Materials are recognisable —
steel, concrete, polymer, ballistic nylon, cast aluminium. Technology is
industrial, repaired, and slightly heavier than it needs to be. The reference
axis is field-repaired military hardware, not science-fiction couture. Nothing
floats without a reason. Nature has reclaimed the ground: vegetation grows over
and through the ruin, and the two read together — the pillar says what things
are made of, the reclamation says what has happened to the ground they sit on.

**3. The rift is a colour, and only objects get it.** One narrow chroma band is
reserved for rift-origin phenomena. **Teal is a noun, never an adjective.**

- **Permitted:** rift geometry and rift effects, suppression hardware and its
  readouts, top-rarity item frames, beams and name text.
- **Forbidden:** damage numbers of any element, buttons, borders, rails, focus
  rings, tab underlines, progress tracks, tooltips, and icons for non-rift
  abilities.

If a teal pixel is not describing a thing that exists in the world, it is a
bug. Rift-element *damage* uses a hotter, whiter cyan, deliberately outside the
reserved band, because routine damage is high-frequency and the reservation's
whole value is that it is rare.

**4. Wrongness is compositional, not gory.** Vestiges are not monsters with too
many teeth; they are objects assembled by something that did not know what a
body is for. Wrongness comes from symmetry violations, repeated modules at the
wrong scale, and motion that does not match mass. Gore and viscera read as
Earth biology, and Vestiges are not from an Earth.

**Blockout, then playtest, then author.** No bespoke art is made for a system
that has not been played.

### Interface

**The single readability rule.** If a player cannot answer *am I about to
die*, *is my ability up*, and *did that shot connect* from peripheral vision
while airborne, the HUD has failed. Everything else is allowed to be slower.

**The centre belongs to combat.** Nothing persistent lives in the middle of the
screen except the crosshair and the hit marker.

**Enemy state is read off the enemy**, in the world, not off a bar at the top
of the screen.

**Anything requiring a steady eye is a full-screen modal.** Comparison, tree
navigation and crafting are not done while moving. There is no in-combat
inventory management.

**No item score, anywhere, ever.** A single number telling the player which
item is better deletes the decision the whole endgame is made of.

**The Core tree and the doctrine board are tabs, never a merged view.** The
reason is no longer two budgets — there is one pool of sixty-five and the
boards spend it together — it is that they are two different reading models:
the Core tree is tier bands where a node's row is its rank, and the board is an
adjacency lattice where size is the type. Merged, the player has to hold both
grammars at once on one surface.

**Tooltip order is signatures, then prefixes, then suffixes, never
interleaved**, with tier badges in a fixed column so the eye can scan one axis.

**Equip-limit counters are load-bearing.** When an equip would exceed a limit,
the counter changes state and the action becomes an explicit swap picker.
Equipping is never silently blocked and never silently removes something the
player did not choose.

**Rule-rewrite nodes render as wide prose cards; percentage nodes render as
compact one-liners.** This makes the tree screen a design-smell detector: if a
board fills with one-liners, the tree has drifted into the affix layer and the
UI says so without anyone filing a bug.

**Damage numbers are on by default**, world-space at the impact point, and
**semantically typed** — colour carries the kind, size carries the weight.
Per-target aggregation inside a short window is **mandatory, not a polish
item**: multishot, pierce and pellets otherwise produce a wall of digits that
communicates nothing. Damage-over-time ticks aggregate on a longer window and
must never out-shout direct hits. **Absorbed and zero damage are shown, never
suppressed** — a grey zero teaches the player about armour caps and immunity
phases, where silence teaches them the game is broken. A hard simultaneous cap
culls oldest-first.

**No damage-per-second meter, ever.** It converts a build-crafting game into a
spreadsheet-optimisation game and pushes the community toward a single correct
answer.

**Dodge and block produce reactive feedback with zero world-position delta and
no triggered animation.** They are passive layers; animating them as inputs
teaches a lie about what the player did.

**Never signal state by colour alone. Never reflow the HUD between fights.**
Positions are fixed; only content changes.

**Disabled controls are painted, never faded.** Opacity on a subtree reveals
the plate seams behind it. Refuse the click in the handler instead.

### Banned interface patterns

Each has a shipped, owner-visible bug behind it.

- **Never let anything read its own arrangement.** This is the general rule the
  next four are instances of.
- Never wrap in a container sized from its allotted space inside a scroll box.
  The two negotiate forever and the layout oscillates.
- Never poll input or rebuild widgets from a per-frame attribute.
- Never auto-wrap text where the width matters. Compute the width before
  layout.
- Never size a fixed box to its shortest label. This is the recurring "text is
  cut off or overdraws its neighbour" class.
- On any text that could clip, fill the space and justify inside it rather than
  aligning to an edge. A non-filling child is arranged at its measured width
  and then clipped to that same box, and measure and rasterise round
  independently.

## The model

### The token system

Flat fills only. **Depth comes from border value, never from gradient.**

Three background steps, three panel steps, four text weights. Four function
accent families — player and system, weapon and heat, reward and weak point,
harm — and one accent is permitted on chrome.

**Rarity colour appears once per item per view, and never as the only carrier
of the fact.** A second carrier is a mark, a number, or a word — never a second
tint of the same hue. A key is not an instance: a legend swatch, a filter chip
and the colour helper's own definition name the ramp rather than wearing it.
Where a card carries the rail, the name goes to text/primary; where a beam
carries the colour, its height and profile carry the tier as well. Card faces
stay neutral at every rarity so a wall of loot does not become a wall of
colour, and the top rarity keeps its full border as the single exception,
because it is the only tier that is also a class of world object.

This replaces an enumeration of sites, and the replacement is the point: a list
had already lost sixteen sites to three without anyone deciding to breach it,
and a screen invented next quarter needed an amendment before it could be
judged. A principle counts its own sites.

**Three faces, all open-licensed:** a condensed display face, always uppercase,
for titles and names; a body face for prose and affix lines; and a **monospaced
numeric face, tabular by default, for every number that ticks** — fixed advance
means counters never reflow mid-fight.

**Every non-alphanumeric mark is drawn geometry, never a glyph.** Pips, arrows,
triangles, chevrons, tally cells, state dots: a rectangle or a path, not a
character in a string. This is a measured constraint rather than a preference —
the engine's fallback face carries 878 codepoints and no Geometric Shapes
block, so a comparison triangle renders as tofu on the most-scanned line of the
screen. It binds every mark this interface still owes: the rarity cue, locked
ability indicators, equip-limit state, refusal warnings, comparison arrows.
Geometry also survives the shipping faces arriving, where a glyph has to be
revisited; the one place a character is worth restoring afterwards says so at
its own site.

**An eight-pixel grid, and the scale is 4/8/12/16/24/40/64.** 12 is a token —
8 to 16 is too coarse for a dense row gap, and a scale the declaring file
violates four times over is not a scale. 32 is not a token. Radii of zero for
structural zones, two for panels and cards, four for icon tiles, never more.
The 44px minimum hit target is a stated exception to the grid, not a value on
the scale: a control the thumb has to find is sized by the thumb.

**The left accent rail is identity** — which system owns this panel. The top
rail is transient status. One rail per panel: a panel with two rails means
nothing, and a nested panel does not repeat its parent's.

### Motion

Panels slide along their rail axis and fade. **They never scale — plates do not
grow.** Out is faster than in: equipment closes faster than it opens.

**A purchase confirm snaps on the first frame with no ease in**, because a
commit must feel mechanical, then decays back over a longer tail. It is never
queued; spam-buying should read as a stutter of snaps rather than an animation
backlog.

**Harm is instant and relief is slow.** A health bar drains immediately and
recovers its chip over time.

Damage numbers pop, settle, rise and fade. Criticals spawn larger and hold
fractionally longer.

## Boundaries

This spec owns the visual system, the screens' laws and the art direction. It
does not own:

- what a stat means or what it is worth — **power and scaling**;
- the rarity ladder itself — **items and crafting**;
- what a node does — **progression and trees**;
- what an ability does — **classes and abilities**;
- what an enemy does — **combat**;
- where a space is and how big — **content and modes**.

## Asserted invariants

| Invariant | Test |
|---|---|
| No fixed-width control is smaller than its worst-case content | `UI.Layout.NoOverflow` |
| No widget reads its own allotted size | `UI.Layout.NoSelfMeasurement` |
| Column wrap widths account for interior padding | `UI.Inventory.WrapWidth` |
| Damage numbers aggregate per target inside the window, under a maximal multi-hit build | `UI.Damage.Aggregation` |
| Simultaneous damage numbers never exceed the cap | `UI.Damage.Cap` |
| Teal appears on no interface element outside the permitted list | `UI.Teal.ObjectLaw` |
| Every rarity is distinguishable without colour | `UI.Rarity.NonColourCue` |
| Every enemy and weapon passes the flat-grey test at range | `Art.FlatGreyTest` |
| Equip-limit refusal offers a swap rather than blocking | `UI.EquipLimit.SwapPicker` |
| No screen prints an aggregate item score | `UI.NoItemScore` |

**Automation cannot see a layout.** These tests prove arithmetic and
composition; they cannot say whether a screen reads. The capture harness is the
second instrument and it has a permanent hole — it cannot move a mouse, so
every hover state, tooltip and zoom gesture in the project is unverifiable by
it. Those are checked by a person or not at all.

**The surfaces no instrument reaches**, named rather than described, because a
hole nobody enumerates is a hole nobody checks. Every one of these needs a
pointer to exist at all, so a screenshot cannot reach them and neither can a
test:

- **Item comparison tooltips** — the equipped-versus-hovered panel, and the
  tooltip ordering rule above. A tooltip that hides which of two items is
  better is the failure this spec opens with, and it is only observable here.
- **Skill-tree node detail panels** — the card that appears on selection,
  carrying the node's description, its per-rank effects and the before/after
  projection. Node descriptions are the one place authoring notes reach the
  player verbatim: a bucket name, a section reference or a bare multiplier in
  that text is a defect no automated check will raise.
- **Tree pan and zoom** — whether a constellation is legible at the zoom the
  player actually uses, and whether the reachable area is reachable by drag.
- **The equip-limit swap picker** — `UI.EquipLimit.SwapPicker` asserts a swap
  is OFFERED; whether the picker is usable, and whether it names the item it
  would displace, is a person's judgement.
- **Every hover state on every screen** — the entire feedback layer that says a
  control is live before it is pressed.

A person looking at these is not a supplement to the instruments. For these
five it is the only instrument there is.

## Open

- Who owns audio. It is the largest unowned domain in the project: every enemy
  telegraph, the closing ritual, the dodge and block feedback model and the
  ultimate-ready cue all assume a channel with no owner, no palette and no
  budget. Telegraph tuning cannot be validated without it.
- The specific shade of the rift-element damage cyan, and the two remaining
  element colours.
- Whether the inventory grid gets the frame width its three-across layout
  needs, which changes the frame policy for every other full-screen tab.
- Whether the nameplate policy becomes a per-mode asset before the densest
  mode exists, or after it breaks.
- How much bespoke geometry elite modifiers need. The tells were specified
  against a budget that assumed none.
