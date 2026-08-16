# The Anchor Hub — the settlement, the loop through it, and the boundary it stands on

> STATUS 2026-08-16: UNBUILT TREATMENT — the settlement designed here does not exist; today's Anchor is the runtime blockout hub (Game/BreakerHubBuilder.cpp) on Lvl_Anchor, which this document predates, and whether an authored map replaces or dresses that hub (A7) is an open owner question.

**Scope:** post-slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

**Nothing in O29–O32 touches this document's subject.** They rule gear depth,
the Core-tree axes, content shape and legendary cadence; this document authors a
*place*. Two second-order notes, both recorded rather than acted on:

- **O31** ("no encounter may have a build that cannot participate") applies to
  the hub only in that the hub is where a player *changes* build. It strengthens
  §7's case for the Forge being reachable and legible, since respec is the
  mechanism a player uses to answer an encounter they cannot participate in.
- **The field around the Anchor gained a cover-anchor registry** (21 anchors) and
  a real spatial grammar in `Docs/Design/Level-Design.md`. The hub's own spaces
  were authored before that grammar existed and have **not** been checked against
  it. Whether the camp's 14 m plaza and its 3 m interaction range read correctly
  against G-numbers derived from the movement constants is an open question this
  document does not answer.

Domain: the main Anchor as a *place* — what the player does there, where each
function physically sits and why, the suppression radius as the organising idea,
how the Altered are staged inside a settlement whose standing order is engage on
sight, what makes the space read as inhabited, and a build plan that is honest
about who can author what.

**Authority.** `Docs/Design/Decisions.md` is law and supersedes everything here.
Then `CONTEXT.md`'s next-actions list, then `Design-Overview.md` (map, not law),
then the per-domain docs (O28). `Docs/Design/Story-Source.md` is authoritative for
STORY and ENEMY ARCHITECTURE and historical for systems — this document consumes
its story material and cites it, and does not cite it for anything mechanical.
This document authors nothing that contradicts a ruling.

**Every number in this document is `O2 PLACEHOLDER`.** Distances, radii, heights,
counts and durations are *shape*, not values. O2 freezes value authoring. Where a
number is load-bearing for a design argument the argument is stated, so that
changing the number changes the argument visibly rather than silently.

**Ownership.** This document owns: the Anchor's spatial layout, the placement of
each hub function, the suppression-radius rules as a level-design idea, the
physical staging of the Altered inside the settlement, and the hub build plan. It
does **not** own: the mission list or the class-choice placement
(`Campaign-And-Story.md`), rift structure and tiering (`Game-Modes.md`), the stash
and save boundaries (`Save-Architecture.md`), the Forge's crafting operations
(`Item-Foundation.md`, Story-Source §4.7), colour and asset direction
(`Art-And-Modelling-Plan.md`), or any UI screen (the five `UI-*.md` files are the
FIELDPLATE visual authority and this document does not touch them). Where it names
one of those, it is consuming it.

---

## 0. How to read this document

Every substantive claim carries one of three labels. The owner needs to know which
parts they are free to overrule without disturbing anything else.

| Label | Meaning |
|---|---|
| **TRANSCRIBED** | Already stated in the corpus or readable in the code. Recorded here so the hub can be read in one place. The source is cited. Not new. |
| **AUTHORED** | New in this document. Owner-reviewable and **deletable** — if it is wrong, delete it and nothing else breaks. |
| **RECONCILED** | Two existing sources said adjacent things; this states the reading that satisfies both without changing either. |

Anything needing an owner ruling is in **§9 OPEN QUESTIONS**, ranked by how much
it blocks, with options and costs. Nothing is silently decided.

### 0.1 Two citation corrections, made once so they are not repeated

**RECONCILED.** Two attributions circulate in the project in a form that does not
match the ledger, and this document uses the corrected form throughout.

- **The object-chroma law is O19, not O24.** O19 rules *"saturated teal is a
  property of objects, not of damage"* and the reservation lives in
  `Art-And-Modelling-Plan.md` §2b. **O24 is a different ruling** — *world
  aesthetic: overgrown Earth*. The gym code comment and the feedback log both say
  "O24 palette" when they mean the O24 dressing pass *plus* the O19 chroma law.
  This document cites **O19 / Art §2b** for chroma and **O24** for overgrowth.
- **`Item-Foundation.md` lives at `Docs/Item-Foundation.md`**, not under
  `Docs/Design/`, and it has **named prose sections, not numbered ones** — there is
  no "Item-Foundation §N" to cite. Its crafting content is one line under *"Not
  built / open"*; the Forge's actual five operations are in Story-Source §4.7 and
  the station's screen is `UI-UX-Spec.md` §7.1.

### 0.2 Units and scale, because no document states them

**RECONCILED.** No design document in the corpus states a scale convention — not
human height, not door size, not a grid. Art §7.1 asks for *"correct scale"*
without defining it. The code does define it, so this document records the
convention rather than inventing one:

- **Everything is centimetres.** The engine is UE-native cm and every doc that
  gives a unit gives cm. A `/Engine/BasicShapes` cube is 100 cm, so a scale value
  reads directly as metres.
- **The player is ~176 cm tall** — capsule half-height 88, read from
  `ABreakerCharacter` and confirmed by the ground-plane offset the game mode uses
  (`Origin = PawnLocation - FVector(0,0,88)`).
- **The mantle staircase is the only authored human-scale reference in the
  project**: steps at 50 / 100 / 145 cm. Anything in the Anchor a person steps
  onto, sits on, or leans against should agree with those.
- **Movement, TRANSCRIBED from `Docs/Movement-Design.md`** (the authority; the
  gym doc's 950 is stale): **walk 700 cm/s, sprint 1100 cm/s**, jump impulse
  700 cm/s, dash floor 1500 cm/s. Every walking-time figure in §3 is computed from
  700 and 1100.

**Not cited for dimensions:** `Docs/Playtest-Gym-v1.md`. It documents the v1 gym
only and predates the camp, the apron and the O24 dressing entirely; its sprint
figure disagrees with `Movement-Design.md`. The code is the source for what the
gym currently is, and §1.3 reads it directly.

**Naming discipline observed here** (TRANSCRIBED, Story-Source §1.2 NAMING
CONSTRAINTS): **"Bastion" is not used** for any settlement, district, structure or
route in this document — it is the Tank branch name. **"Aberrant" and "Anomalous"
are not used** for anything but the rarity tiers. Every proper noun this document
coins is listed in §1.4 so it can be checked against future rulings in one place.

---

## 1. What an Anchor is, and what exists today

### 1.1 The fiction, transcribed

**TRANSCRIBED** (Story-Source §1.2, §1.3, §1.5, §8.3, §10.1):

- An **Anchor** is a settlement built on rift-suppression hardware. Humanity
  clusters inside the suppression radius. **Anchors are the only safe ground.**
- Anchors are **small — cities, not nations.** Infrastructure exists inside the
  suppression radius and degrades rapidly outside it.
- **Ordinary life continues inside an Anchor.** Markets, work, families. *"The
  suppression hardware is visible from everywhere in the city and nobody looks at
  it anymore."*
- The stated design consequence, verbatim: *"the player returns to a functioning
  place. Vendors, Forge, and social space are diegetically justified without
  pretending the world is fine."*
- **Severance** — the degradation suffered by Altered refugees cut off from an
  erased timeline — is *"REVERSIBLE inside a functioning Anchor. Suppression
  hardware halts the decay."*
- The Anchor is the **social hub** and is **not instanced** (§8.3, §10.1). It is
  the MMO layer.

The second-to-last of those is the one that matters most and is the easiest to
under-read. An Anchor is not a safe room. It is the only place in the setting
where **a specific kind of dying stops**. The same machine that keeps the player
alive is the machine that would keep an Altered lucid, and the militia's standing
order is to shoot Altered on sight. That contradiction is not a subplot; it is
what the building is. §5 stages it physically.

### 1.2 The one image the Anchor has to sell

**TRANSCRIBED** (`Art-And-Modelling-Plan.md` §4.1): *"A market street,
mid-afternoon, ordinary commerce, and a two-hundred-metre suppression pylon in
frame that nobody in the shot is looking at. That contrast is the entire
environment brief. If a screenshot of the Anchor does not contain both the mundane
and the pylon, it is framed wrong."*

Art §4.2's direction rules, transcribed because the layout in §3 is built to
satisfy them by geometry rather than by discipline: **lived-in, not ruined**;
**density falls off with radius**; **the pylon is the sun** and is visible from
every exterior; **no signage explaining the setting**; **warmth inside, cold
outside**; **verticality is functional** — fire escapes, scaffolds, service
catwalks, never floating platforms.

And the acceptance criterion that a layout can pass or fail on its own
(Art §4.4): *"Rift chroma appears nowhere in the Anchor except on the suppression
hardware itself."*

### 1.3 What exists in code today — the delta, read from the source

**TRANSCRIBED** from `Source/RiorsEdge/Game/BreakerGameMode.cpp`
(`SpawnAnchorCamp`, `SpawnSafeZone`), `Source/RiorsEdge/Interaction/BreakerNPC.h`,
and `Source/RiorsEdge/Classes/BreakerMomentumComponent.cpp`. All primitives are
`/Engine/BasicShapes` cubes and cylinders coloured by a dynamic material instance;
the base cube is 100 cm, so a scale value reads directly as metres.

**Note on ownership:** at the time of writing, `Game/BreakerGameMode.cpp` was
owned by another agent and `Combat/` by a third. **Those lanes have since
merged**, so the ownership note is historical — but the substantive part still
holds: nothing in this document edits `Source/`. The inventory below is
read-only observation and the build plan in §7 is a description of work, **not a
claim that it was done**.

**The inventory below has NOT been re-verified since the enemy-integration and
field passes landed**, and `BreakerGameMode.cpp` changed substantially in them.
Treat every number in the table as *last confirmed at transcription time* rather
than as current. In particular the camp now shares a game mode with a cover
registry, a wave budget solver and a boss spawner, and the five deltas below were
derived against the file as it stood before any of that. Re-transcribe before
acting on a specific value.

| Thing | What it actually is | Where |
|---|---|---|
| Camp plaza | 14 × 14 m slab, 15 cm thick, earth-coloured | 14 m behind the spawn point |
| Camp back wall | 14 m × 0.3 m × 3.6 m concrete | 7 m behind camp centre |
| Forge | a 1.6 × 1.6 × 2.2 m rust-coloured cube with a warm point light (intensity 900, radius 700) | 5 m behind camp centre, 4.5 m left |
| Supply crates | 2.4 × 1.2 × 1.2 m off-white cube | 5 m behind centre, 4.8 m right |
| Ammo resupply crate | 1.1 m amber cube, a distance trigger evaluated in `Tick` — no interaction plumbing | 3 m behind centre, 4.8 m right |
| **Kess**, Forge Keeper | `ABreakerNPC`, 4 dialogue nodes, id-linked, quest flags persisted to save | 4.2 m behind centre, 4.5 m left |
| **Quartermaster** | `ABreakerNPC`, 3 dialogue nodes | 4.2 m behind centre, 4.8 m right |
| NPC interaction range | **3 m** (`InteractionRange = 300`) | — |
| Safe zone | **radius 9 m** (`SafeZoneRadius = 900`), centred on the **spawn point**, not the camp | — |
| Safe pad | teal disc, **4 m across** | safe-zone centre |
| Boundary posts | 12 teal cylinders, 80 cm tall, at 9 m, every 30° | safe-zone radius |
| **Suppression pylon** | **2.6 m tall**, 36 cm across, teal, with a teal point light | safe-zone centre |
| Watchtowers | two 3 × 3 m platforms at 2.4 m and 3.4 m | flanking the route out |
| Arena posts | six markers at 14 m radius | around the elite's ground |

Five deltas follow from that table, and they are ranked by how much difference
closing them makes per unit of work.

**1. The pylon is 2.6 metres tall. The brief says two hundred.** That is a factor
of roughly seventy-seven, and it is the single largest gap between what exists and
what an Anchor is supposed to be. It is also the cheapest gap in the project to
close: it is one scale value in one `SpawnShape` call. §7 argues this is the
smallest change available that would make the biggest difference, and it argues it
for the whole document, not just for this section.

**2. The camp is mostly outside the suppression radius.** The safe zone is a 9 m
circle centred on the *spawn point*; the camp centre is 14 m behind it and both
NPCs stand at 18–19 m. So Kess and the Quartermaster — the two people the fiction
puts inside the field — are outside it, and the Forge with them. In fiction that
is exactly backwards. It is also one number.

**3. The visible boundary and the real boundary disagree by a factor of two.** The
teal pad is 4 m across; the zone is 9 m. A player standing 6 m out is inside
suppression and looks like they are outside it. The twelve 80 cm posts at 9 m are
the real edge and are easy to miss entirely. §4 argues the boundary must be legible
from anywhere in the hub; today it is barely legible from on top of it.

**4. `IsInSafeZone` does three different jobs at once.** It is the Momentum
anti-farm gate, the enemy-free zone, and — by implication in the design — the
fiction boundary of the Anchor. Those are three concepts with three different
correct radii. §4.3 recommends splitting them, and that split is a prerequisite for
an Anchor larger than nine metres.

**5. Nothing in the camp transacts.** Kess and the Quartermaster *talk*. There is
no stash, no buy/sell, no crafting operation, no rift selection, no induction
range, and no containment. Every hub function in §2 is currently either a menu
reached with a key or does not exist.

**RECONCILED — and this is the framing the rest of the document uses.**
`Campaign-And-Story.md` §1.4 already ruled on what the existing camp is: *"The
Anchor camp that exists in the gym today is not the Anchor. It is a forward
staging camp: a slab, a back wall, a Forge, a supply crate, two watchtowers, a
pylon, and two people. That is the smallest viable unit of Anchor presence and the
campaign uses it as such — Act I's field camps are this, and the Anchor proper is
a city."*

So the camp is not a bad Anchor. It is a **good forward camp** and the Anchor does
not exist yet. The five deltas above are therefore not bugs in the camp; four of
them are things the camp should keep doing correctly *as a camp* (the pylon should
be small at a forward camp — a field suppressor, not a city pylon), and the fifth
is a genuine structural knot worth untying before anything larger is built.

### 1.4 Proper nouns coined in this document

**AUTHORED.** All are placeholder names, all are deletable, and all are listed
here so they can be checked against Story-Source §1.2's naming constraints in one
place. None of them is "Bastion", a rarity word, or an existing branch name.

| Name | What it is | Section |
|---|---|---|
| **The Ring Walk** | the loop of street that carries every hub function | §3.2 |
| **The Threshold Plaza** | the shared arrival/departure plaza at the boundary | §3.3 |
| **The Landing** | where the closing ritual deposits a returning player | §3.3 |
| **The Gate** | where the player departs for content | §3.3 |
| **The Skirt** | the pylon's base structure and the plaza against it | §4.1 |
| **The Cell** | the containment room inside the Skirt | §5.2 |
| **The Yard** | the militia spur behind the Command post | §3.4 |

---

## 2. What the hub is FOR, as a loop

A hub whose functions are scattered without reason is a menu with walking. The
test this section applies to every function is: **why is it here and not somewhere
else, and what does the player do immediately before and immediately after?**

### 2.1 The functions the game already needs a home for

| Function | Owner doc | State today | Where it sits (§3) | Why there |
|---|---|---|---|---|
| **Arrival from content** | `Game-Modes.md` §3.5 step 5, *"Return to Anchor"* | the closing ritual is design-only | **The Landing**, at the boundary | Every rift run in the game ends here. It is the most-entered point in the hub and it must be at the edge — see §2.3. |
| **Stash** (O17, account-wide) | `Save-Architecture.md` §2.3 | does not exist | 15 m from the Landing | The player arrives full. Backpack is 60 slots and overflow forces an Anchor return; transfer is Anchor-gated so a rift run cannot mutate account state. If the stash is not the *first* thing on the walk, the player carries loot the whole way and comes back. |
| **The Forge** (Kess) — respec, add affix, reroll value, upgrade tier, exalt/corrupt, divine order | Story-Source §4.7, §13.2; `UI-UX-Spec.md` §7.1 (the screen); `Docs/Item-Foundation.md` "Tier scale" | Kess talks; no operations exist | the loop's centre of gravity, **as an interior** | *"Highest interaction count of any NPC in the game"* (§13.2). Respec is Forge-gated by locked decision and T-1 exists *only* through exalt/corrupt, so this is where build changes and the top of the item chase both live. It is the one function the player spends real minutes at, which is why it is a room and not a counter — see §2.5. |
| **Quartermaster** — gear, ammunition, consumables | Story-Source §13.7; `UI-UX-Spec.md` §676 | talks; no transactions | a **stall on the street**, 20 m past the Forge | Deliberately plain: *"a flat two-column buy/sell list, no ceremony, no rarity fireworks."* The contrast with the Forge is the design — one is a room you enter, one is a counter you pass. |
| **Mission / rift selection** — contracts, act gates | `Game-Modes.md` §3.7; **no doc specifies the selection surface** | a dialogue node (see §2.4) | **Command post**, 20 m from the Gate | Command *"gates acts… owns mission structure, difficulty tiers"* (§13.3). You take a contract and leave. Putting Command beside the Forge would make the player choose what to run before choosing the build to run it with, which is backwards. |
| **Frontier modifier re-roll** | `Game-Modes.md` §4.4 — the interaction is specified, **its location is not** | does not exist | Command post, endgame only | *"Modifiers are rolled onto the Frontier instance before entry, visible before entry, and the player may re-roll at a material cost."* It is the clearest un-homed interaction in the corpus and it belongs with the person who owns difficulty. |
| **Class induction** — the permanent choice at ~level 3 | `Campaign-And-Story.md` §4.2 (mission A1-1) | the pre-play menu screen | **The Yard**, a spur behind Command | A one-time event. A permanent five-kit range on the main street is a thing the player walks past for forty hours and uses once. §3.4. |
| **Fragment reconstruction** (the Researcher) | Story-Source §1.7, §13.4; campaign A2-1 onward | does not exist | a **shuttered frontage on the loop** that opens in Act II | She must not exist in Act I. A closed door on the main street that opens is the cheapest act-progression signal available and it costs one door state. |
| **Refugee intake** (the Order) | Story-Source §13.7; campaign A2-6 | does not exist | the **outer city**, at the boundary, opposite the Gate | *"a loading dock, a manifest, and a gap in the suppression field"* (A2-6). Deliberately off the loop and a walk away: you have to go looking for it, which is the point of it. |
| **Severance containment** | Story-Source §1.5; campaign A3-5 | does not exist | **The Cell**, inside the pylon Skirt | §5. |
| **Ammunition resupply** | exists in code as a distance trigger | an amber crate in the camp | folded into the Quartermaster | One less object; the crate stays at forward camps where there is no Quartermaster. |

**Not in this table, deliberately:** inventory, equipment, and the skill trees.
Those are full-screen modals reached from anywhere (`UI-UX-Spec.md` §19), and
`UI-UX-Spec.md` §801 already rules the boundary: *"Forge and Vendor are never in
the pause menu. They are Anchor interactions."* Respec being Forge-gated is a
locked product decision and a respec button in a pause menu would silently undo
it. This document adds nothing to that line and does not move it.

### 2.2 The constraint that stops the Command post becoming a mission menu

**TRANSCRIBED, and it is the most important thing anyone building a hub
mission-select screen needs to know.** Two rules in `Game-Modes.md` push hard
against the obvious design:

- **§3.7 / Campaign §2.2 (O27):** *"The rift tier available to a player is capped
  at their level, so tiering is invisible during levelling."* Tier 1–15 is the
  campaign; tier 16–30 is the endgame band, available at 50. **So for the whole
  campaign there is no difficulty selector, because there is nothing to select.**
  A tier picker only becomes meaningful at level 50.
- **§3.2 and §4.2:** the tier and modifier readout *"is displayed diegetically on
  the threshold so the player sees what they signed up for **inside** the mode
  rather than on a menu."*

**AUTHORED consequence.** A war-table with a tier slider and a modifier list is
therefore the wrong object twice over: for forty hours it would have nothing to
show, and when it finally did, it would duplicate a readout the corpus has
deliberately placed somewhere else.

What the Command post is instead: **a person who gives you the next thing to do.**
During the campaign that is a conversation and an act gate. At 50 it acquires the
Frontier's token spend and modifier re-roll, which are genuine choices with a
material cost — and even then the *readout* of what was rolled still happens at the
ingress, not here. The hub's selection surface grows a function at 50 rather than
starting as a menu that is empty until then.

This is the reason §3.2 puts Command 20 m from the Gate rather than merging them,
and §9 question 7 records what is still un-ruled.

### 2.3 Why arrival is at the edge and not the centre

**AUTHORED.** The closing ritual ends with *"Return to Anchor"* and the erasure
sequence immediately before it is the geometry of a rift unloading around the
player, *"geometry-first, from the far walls inward"*, with audio dropped to a
single tone (`Game-Modes.md` §3.5). The player has just watched a world end, in
silence, and nobody comments on it.

The next thing they see is the first frame of the Anchor. Put the Landing at the
boundary and that frame is: a street, at eye level, with the pylon two hundred
metres tall at the far end of it and people in the way. Put the Landing at the
centre and the frame is the base of the machine with the city facing away.

The first is Art §4.1's pillar image delivered automatically, several hundred
times per playthrough, at the exact moment the fiction is loudest. The second is a
loading bay. This is the single highest-leverage placement decision in the layout
and it costs nothing.

### 2.4 What the hub cannot do yet, read from the code

**TRANSCRIBED.** The gap between §2.1's table and what a player can actually do is
larger than "the geometry is missing", and it is worth stating precisely because
three of these are small and one is not.

- **There is no stash object and no stash system.** Inventory opens with `I` from
  anywhere; `Save-Architecture.md` §2.3 gates *stash transfer* to the Anchor, but
  the stash itself does not exist in any form.
- **There is no vendor.** The Quartermaster's `Vendor` dialogue node is an explicit
  stub — *"Storefront's not built yet, Breaker."* Making it real needs three things
  that do not exist: a currency on the equipment component, a vendor screen in
  `EBreakerMenuScreen`, and a dialogue-choice action other than `SetsQuestFlag`
  (today a choice can set a flag and nothing else, so no dialogue node can open a
  screen).
- **There is no quest log, objective tracker, or waypoint.** The HUD's only
  interaction affordance is `F  TALK — <NAME>`. The Quartermaster already hands out
  a real contract that nothing tracks, nothing reports back, and nothing rewards.
- **There is no Forge operation.** Kess talks. Add affix, reroll, upgrade tier,
  exalt/corrupt and divine order are all unbuilt, and O12's crafting currencies are
  ruled to be scalars but *"the currencies themselves (count, names, tiers, yields)
  are owner-authored and not designed here."* **This document must not name a
  currency, and does not.**

None of that blocks the layout — walking the blockout (§7 Stage 1) needs none of
it — and all of it blocks the hub being *useful*. The order matters: build the
space, learn whether it is the right size, then fill it.

### 2.5 Why the Forge is the only interior on the loop

**RECONCILED** (Art §4.3 P0 row; Story-Source §13.2). Art already asks for the
Forge to be *"warm, cramped, tool-dense, personal,"* with *"Kess's Effigy hands…
the focal point of the composition"*, and marks it the one Anchor space to
*"author with care"* because it is where the player spends real time. Story-Source
makes it the highest-interaction NPC in the game and hangs the campaign's longest
withheld answer on it.

A function with the highest interaction count and a slow-burn character thread
cannot be performed standing in a street. So: the player steps out of the crowd,
through a door, into a small hot room, and the street noise drops. That transition
is free — it is a door and an audio state — and it is the only thing in the hub
that separates *doing something* from *walking past something*.

It also does structural work. It is the only place on the Ring Walk where the
crowd is not, which means it is the only place a quiet conversation can happen
without a systems change, which is what a slow-burn thread needs.

---

## 3. The spatial design

This game's pillar is movement, and the player crosses the hub many times per
session. A hub the player crosses many times is either a pleasure or a tax. The
whole of this section is an argument that the difference is decided by two
numbers: **how far the two most-used things are apart**, and **how far it is from
the arrival point back to the departure point when the player wants to do nothing
at all.**

Every dimension below is `O2 PLACEHOLDER`. Movement reference values per §0.2:
**walk 700 cm/s, sprint 1100 cm/s.**

### 3.1 The failure mode this layout is built to avoid

**AUTHORED.** The obvious hub shape is a wheel: a landmark in the middle, the
functions on the rim, the player at the centre choosing a spoke. It is the wrong
shape here, for one reason: **every trip becomes a there-and-back.** Visit three
functions on a wheel and you walk six radii. The wheel doubles the walking and
puts the player at the middle where nothing happens, which is also where the
population is not.

The alternative failure is the corridor: one line, functions strung along it in
order. It halves the walking for a player doing everything and it is terrible for
a player doing nothing, because the arrival point and the departure point are at
opposite ends of it.

The shape that answers both is a **loop with a short chord across it.**

### 3.2 The Ring Walk

The functional hub is a single loop of street encircling the pylon's base, with
arrival and departure adjacent to each other on it. One circuit passes every
function exactly once, in the order the player needs them, with no backtracking.
The chord between arrival and departure is short enough that a player who wants to
turn straight around pays almost nothing.

```
                       ( outer city — 240 m suppression radius )

                         ~~ Researcher's frontage (opens Act II)
                        /                                    \
              Forge ---+          [ THE SKIRT ]                +--- Quartermaster
             (interior)|      pylon, 200 m, and The Cell       |
                       |            (§4, §5)                   |
              Stash ---+                                       +--- Market
                        \                                     /
                    Landing =20m= Gate               Command post --- The Yard
                         \_________ THE THRESHOLD PLAZA _________/
                                          |
                                   ( to content )
```

| Element | Dimension | Reason |
|---|---|---|
| **Suppression radius** | **240 m** | The fiction boundary and the city extent. At sprint a radial run from the Skirt to the edge is ~22 s — far enough that the edge is a destination rather than scenery, near enough that it is not a commute. For scale: the gym's existing ground apron is already **180 × 180 m**, so this is the same order of magnitude as something the project already spawns. |
| **Ring Walk circumference** | **~200 m** (~32 m radius from the pylon axis) | A full circuit is ~29 s at walk, ~18 s at sprint. That is the budget for "I did everything", once per 6–15 minute run. |
| **Plaza width** — Skirt face to building faces | **20 m** | Wide enough to hold a crowd, narrow enough to read as a street rather than a square. A square reads as a lobby. |
| **Landing → Gate (the chord)** | **20 m** | **The most important number in this document.** A player who wants only to re-run pays ~2.9 s at a walk. This is what decides whether the hub is a pleasure or a tax, and it is the number to attack first if playtest says the hub drags. |
| Landing → Stash | 15 m | You arrive full; the stash is the first thing you touch. |
| **Stash → Forge** | **25 m** | The two highest-interaction functions, ~3.6 s apart at walk. If any pair of numbers gets defended in a retune, it is this one. |
| Forge → Quartermaster | 20 m | |
| Quartermaster → Command | **~55 m, through the market** | The only long leg, and it is long on purpose: it is the leg that carries the population. See §6. |
| Command → Gate | 20 m | You take a contract and leave. |
| Building height on the loop | **12–20 m** | ≥12 m makes the rooftops a genuine second level and satisfies the ≥6 m wall-ride surface requirement (`Game-Modes.md` §2) with room to spare. |
| Rooftop gap spacing | **4–7 m** | Clearable by the base two-jump kit (O25), comfortable with a dash, trivial for Swift. Never required. |
| NPC interaction range | 3 m | TRANSCRIBED from code. Sets the minimum standing clearance in front of every stall. |

### 3.3 The Threshold Plaza

**AUTHORED.** Arrival and departure share one plaza at the boundary, twenty metres
apart, facing each other across it. Three things follow.

**It makes the re-run free.** The farming loop — land, turn, leave — is under
three seconds and it never passes a single function. That is the correct default,
because the endgame is where players live (`Game-Modes.md` §2, ~80% repeatable)
and a hub that taxes the repeatable loop taxes the game.

**It makes the full loop opt-in.** A player who wants the stash and the Forge
turns the other way and walks 200 m through the city — about 29 seconds. Nothing
forces them. The hub's content is available, not administered.

**It puts the two most emotionally loaded moments in the game in the same twenty
metres.** The Landing is where the player stands, several hundred times, having
just watched a timeline unload in silence. The Gate is where they choose to do it
again. Nobody says anything about either. That is free, it is exactly the delivery
discipline Story-Source §8.5 asks for — *"the player should have assembled the
argument themselves before anyone states it"* — and it costs one plaza.

### 3.4 The Yard, and where one-time functions go

**AUTHORED.** The class induction (`Campaign-And-Story.md` §4.2: three dummy
targets, five loaner kits, at roughly level 3) is a one-time event with a permanent
physical footprint. Put it on the Ring Walk and the player walks past a range they
used once for the next forty hours.

So it goes on a **short spur off the loop behind the Command post** — the Yard,
the militia's own ground. Ten seconds off the main route, signposted by an NPC in
A1-1 and by nothing afterwards.

That spur then earns its keep three more times: it is where the guard roster for
the Cell is posted (§5.3), it is the natural staging for the Order friction in A2-6
without putting the Order inside the militia's space, and it is the only place in
the hub where the player can see the militia being an *organisation* rather than
one tired man behind a desk. One spur, four uses, and it never intrudes on the
loop.

### 3.5 Movement inside the suppression radius

**AUTHORED, and this is the section the movement pillar lives or dies in.**

**Every movement verb is available everywhere inside the Anchor, with no
restriction.** Walk, sprint, both jumps (O25), dash, slide, wall ride, wall jump,
and Swift's third jump. No hub-only slowdown, no sheathe-your-weapon walk state, no
disabled dash.

Three reasons, in order of weight:

1. **The Anchor is where the player is between fights.** It is the one place they
   cannot die. Removing verbs in the safest place in the game teaches the player
   that the pillar is conditional — that movement is a combat tool the game lends
   them rather than a thing their character *is*. That is a worse cost than any
   problem a restriction would solve.
2. **Art §4.2 already committed to it:** *"The player has wall ride and air jump.
   The Anchor should be traversable, but traversal routes must be justified — fire
   escapes, scaffolds, service catwalks — never floating platforms."*
3. **O26 dropped movement's priority but did not narrow its scope.** Movement gets
   no further dedicated passes; it does not get quietly removed from a new space.

**The rooftop route is the payoff, and it is nearly free.** The Ring Walk's
buildings are 12–20 m tall and continuous, so the same 200 m loop exists on the
roofs: wall rides between faces, 4–7 m gaps, and drops at each of the five function
points. It is never faster in a way that matters (both routes are ~18–29 s) and it
is never required. What it is, is *better* — no crowd, real geometry, the pylon
uninterrupted. A movement player gets a hub they want to move through instead of
one they tolerate, and the cost is authoring the tops of buildings that have to
exist anyway.

This also satisfies the corpus's standing movement guardrail — level design offers
movement opportunities without punishing conventional routes (`Game-Modes.md` §2,
Art §4.2) — by construction rather than by review.

### 3.6 Momentum in the hub, and what a Swift player actually feels

**TRANSCRIBED**, read from `BreakerMomentumComponent::AdvanceLoop`. The safe-zone
behaviour is more specific than "generation is gated", and the specifics change the
design answer:

```
if (IsInSafeZone())
{
    PendingGrants = 0.0f;
    RefreshState();
    return;          // returns BEFORE generation AND before decay
}
```

So inside the safe zone Momentum **neither generates nor decays**. It is frozen,
not drained. A Swift player who walks in at Redline walks out at Redline. Queued
one-shot grants are discarded, and the per-second rate never accrues.

Three further details, all read from the same file and all relevant to how the hub
feels rather than to how it is specified:

- **A dash inside the zone burns its grant for nothing.** The dash-credit
  accounting runs *before* the safe-zone check, so the dash advances the
  observed-dash timestamp and queues `DashGrant`, and the gate then zeroes it. The
  same shape applies to the airborne and wall-ride credit counters. So a Swift
  player who dashes across the plaza has spent a charge, spent the credit, and
  received nothing — and there is no feedback saying so.
- **Phantom Step still fires inside the zone**, because its call sits above the
  gate. So one Swift mechanic works in the hub and the rest do not, which is
  exactly the kind of inconsistency that reads as a bug.
- **The zone is a 2D column with no vertical bound.** Being above it is being in
  it, at any height. That is correct for a suppression field and it matters for
  §3.5's rooftop route: the roofs over the gated area are gated too.

The first two are behaviours to be aware of when the gate is rescoped (§4.3), not
defects this document asks anyone to fix.

**AUTHORED — the feel problem, stated honestly.** The Swift player is fast
everywhere in this game, and this is a hub they will cross at speed by reflex. They
will slide the market leg and wall-ride the Skirt because that is what the kit is
for. And the resource bar will not move. Not because they are doing it wrong —
because the bar is switched off, and nothing tells them that. The most likely
player reading is *"my class is broken in town"*, and it will be reported as a bug
before it is reported as a design decision.

Three answers, with costs:

| Answer | Shape | Cost |
|---|---|---|
| **Do nothing** | The bar freezes; the state pip reads Settled. | Free. The class's core feedback loop appears dead in the one place the player idles most. Reported as a bug. |
| **Show it as suppressed** *(recommended)* | The HUD Momentum track gets one additional visual state — suppressed, not empty — and it is on exactly while the player is inside the gate. | One HUD state (`UI-HUD-Spec.md` owns the visual; this document does not author it). Turns the dead bar into a worldbuilding beat: the field that stops severance is the field the player can feel on their own body. Nearly free, and the strongest available answer. |
| **Generate, capped low** | Allow generation inside the hub but clamp it at the Settled band. | Reopens exactly the farm the gate exists to close, and does it in the one place the player can stand safely forever. Rejected here; listed for completeness. |

**And the recommendation that matters more than the HUD state: scope the gate to
the functional core, not to the suppression radius.** §4.3 makes the argument in
full. In one line: if the whole 240 m disc freezes Momentum, then the outer city —
the largest and most traversable part of the Anchor — is Momentum-dead, and a Swift
player has no reason to ever go there. Freeze the ~64 m Ring Walk area; leave the
outer city live. The anti-farm rule is preserved (the player cannot idle-farm next
to the vendors) and the outer city becomes the part of the hub the movement player
actually wants.

**Flagged, not decided:** there is no fiction on record for *why* suppression stops
Momentum, and Campaign §1.3 argues explicitly that *"a Breaker is not
rift-powered."* If suppression stopping Momentum is canon, it says something about
Momentum that no class document has ruled. If it is a bare anti-farm rule wearing a
coat, that is fine and it should be known to be that. §9 question 4.

### 3.7 The outer city

**AUTHORED.** The 240 m disc minus the ~64 m functional core is roughly nine tenths
of the Anchor's area and contains no hub functions at all. That is a deliberate
choice and it needs its reason stated, because the obvious failure mode is
building a city the player never enters.

It is not dead space; it is the four things the loop cannot hold:

- **The density gradient.** Art §4.2 makes repair standard, light quality and
  building density fall off with radius, which *"gives the level a free legible
  compass."* A gradient needs distance to be a gradient. Nine metres cannot carry
  it; two hundred can.
- **The Order's dock** (A2-6), at the boundary, opposite the Gate. Unauthorised
  intake needs to be somewhere the militia is not, and that has to be a real walk
  or it is not somewhere the militia is not.
- **Swift's ground.** Per §3.6, this is where Momentum is live, and it is the only
  large open traversable space in the hub.
- **Later content.** Whether Anchor defense is a content type is explicitly open
  (Story-Source §10.3), and if it ever ships it needs ground between the boundary
  and the things worth defending. Authoring that ground now costs a radius value;
  retrofitting it later costs the layout.

The honest risk: this is ~9/10ths of the hub's art budget serving four purposes,
two of which are speculative. §7 handles it by making the outer city the **last**
stage, and §9 question 2 makes the radius an explicit ruling rather than a number
this document smuggles in.

---

## 4. The suppression radius as the organising idea

### 4.1 It is a hard physical boundary with fiction attached

**TRANSCRIBED.** Story-Source §1.2: an Anchor is *built on* rift-suppression
hardware, and humanity clusters *inside the suppression radius*. §1.3:
infrastructure *"degrades rapidly outside it."* §1.5: severance is halted inside a
*functioning* Anchor. §13.3, Command's argument: *"Anchors have finite suppression
capacity. Admitting more refugees than the field can hold does not save them — it
collapses the Anchor and loses everyone inside."*

That last line is the one that makes the radius a level-design object rather than a
lore note. **The boundary is finite, it is measurable, and the game's central moral
argument is an argument about a number.** Command is right about the arithmetic
(§13.3), and the player should be able to see the arithmetic. A radius the player
can walk to, stand on, and look across is that argument made physical, and it does
the work with no dialogue at all.

**AUTHORED — the boundary is a real edge, not a fade.** At 240 m the ground
treatment, the light, and the repair standard stop. Outside it: the overgrown Earth
that O24 describes, immediately, with no transition band. Inside it: patched
concrete, permanent scaffolding, cabling run late and never tidied. The sharpness is
the point — a field has an edge, and a fade would read as a rendering budget rather
than as physics.

**The Skirt.** The pylon's base structure: a 24 m footprint at the centre of the
Ring Walk, 200 m tall. It is not enterable except at one door (§5). The 20 m plaza
between the Skirt and the building faces is the market street of Art §4.1, and
because the loop encircles the Skirt, **every exterior composition on the Ring Walk
has the pylon in it by geometry.** Art's acceptance criterion — *"An Anchor exterior
screenshot contains the pylon and at least one person not looking at it"* — becomes
difficult to fail rather than a thing a level designer must remember.

### 4.2 It must be legible from anywhere, and the O19 chroma law is how

**TRANSCRIBED** (O19; Art §2 Pillar 3, §2b, §4.4; `BreakerUIStyle.h`). Saturated
teal — roughly `#3FD8C8` → `#0E5F5C` — is reserved, narrowly, for rift portals,
Vestige emissive, severance progression, **suppression hardware**, and the
Anomalous rarity. O19 fixed the rule verbatim: *"saturated teal is a property of
objects, not of damage."* Rift-*element* damage gets a distinct hotter, whiter cyan
so routine damage never wears the reserved band. Art §4.4's acceptance criterion:
*"Rift chroma appears nowhere in the Anchor except on the suppression hardware
itself."*

**AUTHORED — the consequence, and it is a deliberate authored moment.** In a city
built to Art §4.2's rules — desaturated concrete, warm tungsten and sodium
interiors, cool exteriors, no signage — the only saturated colour anywhere is the
pylon. **The suppression hardware is the single most teal thing in the game**, it is
two hundred metres tall, and it is at the centre of every composition. That is not a
coincidence of the palette; it is the palette pointed at one object.

The design payoff, which the game must never state (Campaign §1.3 already makes the
argument): the pylon and the rift portal are the *same colour*, and the player works
out on their own that the only reason humanity is holding is that it is standing on
a piece of the problem. The colour law delivers that with no writing, and it only
delivers it if the reservation is absolute. **One decorative teal object anywhere in
the Anchor and the whole read collapses.**

Practical legibility rules that follow, all AUTHORED:

- The pylon is the **primary directional light modifier** for every exterior (Art
  §4.2, *"the pylon is the sun"*). Its light is cool; every interior and market
  light is warm. So a player anywhere in the hub can tell which way the centre is
  from the colour temperature on the wall in front of them, without a compass.
- **The boundary carries the same chroma at a lower intensity.** The edge of the
  radius is marked by suppression hardware — the field has to be held up by
  something — so the ring is legible from the Skirt and the Skirt is legible from
  the ring. The code already does exactly this at gym scale with twelve posts at
  the safe radius; the pattern is right and only the scale is wrong.
- **Nothing else in the Anchor is teal.** Not a sign, not a light, not a Breaker's
  kit, not a weapon, not a UI element that renders in world space.

### 4.3 The three boundaries that are currently one function

**RECONCILED**, and this is the structural knot from §1.3 delta 4.
`ABreakerGameMode::IsInSafeZone` is asked to be three things whose correct radii
differ by a factor of thirty.

| Concept | What it governs | Correct scale | Consumer today |
|---|---|---|---|
| **Suppression** | the fiction boundary; no hostiles; account state writable; severance halted; the Anchor *is* this | 240 m | nothing — the concept does not exist in code |
| **Momentum gate** | the anti-farm rule: no generation while standing in the hub's function area | ~64 m (the Ring Walk area) | `BreakerMomentumComponent::AdvanceLoop` |
| **Spawn safety** | the gym's enemy-free pad | 9 m | the gym encounter's spawn offsets |

Collapsing them was correct when the whole world was a 14 m camp. It stops being
correct at the first Anchor larger than the momentum gate, and every consequence of
the collapse shows up as a bug rather than as a design conversation: an Anchor-sized
safe zone makes the entire city Momentum-dead, and a Momentum-sized safe zone puts
the city's population outside suppression.

**Recommendation:** split them into three predicates before any Anchor geometry is
authored. It is a small, mechanical change, it has no gameplay effect at gym scale
(all three predicates return the same answer for a 9 m camp), and it is the
difference between the Anchor being buildable and being a series of retrofits.

**It is not this document's change to make.** `Game/BreakerGameMode.cpp` is owned by
another agent. This is a recommendation with a stated reason, handed over.

---

## 5. The Altered in the hub

This is the strongest dramatic material the setting has and it is the section most
likely to be softened by accident. It is not softened here.

### 5.1 The facts, transcribed and placed side by side

**TRANSCRIBED** (Story-Source §1.5, §13.3, §13.5, §13.7):

- The Altered are refugees. *"They are not inherently hostile. What makes them
  hostile is SEVERANCE."*
- Severance is *"REVERSIBLE inside a functioning Anchor. Suppression hardware halts
  the decay."*
- **Militia policy:** *"Stage cannot be reliably identified during a firefight. The
  standing order is therefore engage on sight. This is a defensible policy and a
  horrifying one. It is the specific thing the Order attacks."*
- **What the militia believes:** *"the Altered are caused by an unknown force. From
  where the militia is standing this is true — no lucid Altered has ever survived
  long enough inside an Anchor to explain otherwise."*
- **The player is the first Breaker to meet one in time.**
- Command *"should know severance is reversible, and should have known for years."*
- The Order *"run unauthorized refugee intake — moving lucid Altered through gaps in
  the suppression field."*
- Campaign A3-5, *Bring Them In*: *"Get the Survivor inside a functioning Anchor
  before the clock runs out. Command signs the exception personally and says
  nothing about it afterwards."*

Put those in one place and the shape is unmistakable. The cure exists. It is a
building. Everyone lives inside it. Getting a person through the door requires a
signature from a man who has known for years that the door is a cure, and who is
right about the arithmetic that keeps it shut.

### 5.2 Where a lucid Altered is kept — the Cell

**AUTHORED.** The containment is **inside the Skirt: a room at the base of the
pylon**, where suppression is strongest.

That placement is the whole staging, and it is chosen for the contradiction it
produces rather than in spite of it. Suppression halts severance, so the correct
place to keep someone you want to stay lucid is as close to the hardware as
physically possible — which is also the most valuable real estate in the city.
**The militia's answer to "where do we put the one thing that proves the standing
order is wrong" is: in the best room in the Anchor, behind a locked door, under
guard.** Both facts are true at once. Neither is stated.

Concretely:

- **One door in the Skirt, on the Ring Walk**, in the twenty metres of plaza the
  player crosses on every full circuit. It is not down an alley and it is not
  hidden. It is on the main street, and it is unremarkable, which per Art §4.2 —
  *no signage explaining the setting* — means it carries no label at all.
- **It is old.** The door is not new construction and the frame has been repaired.
  The room has been used before. Nothing says what happened to who was in it, and
  nothing ever should.
- **The player walks past it from level 3.** Several hundred times, for forty hours,
  as a closed door in a wall.
- **It opens once**, in A3-5, when the player walks the Survivor through it. That is
  the payoff and the entire cost of building it was one door state.

### 5.3 How the standing order looks inside a settlement

**AUTHORED.** *Engage on sight* is a field order. Inside a city it becomes something
uglier and more ordinary: a duty roster.

- **Two militia at the door**, always, and they are not jailers. They are a firing
  party with a standing order and a person on the other side of a wall.
- **The roster is posted in the Yard** (§3.4) and it rotates fast — visibly faster
  than any other posting on the board. No one comments on why. It is the only detail
  in the hub that states what the duty costs, and it states it as paperwork.
- **The guards are not hostile to the player and will not discuss the door.** Not
  because they are under orders to refuse; because there is nothing they have been
  told. The militia's official position is that the Altered are caused by an unknown
  force (§1.5), and the guards believe it. They are not in on anything.
- **Nobody in the Anchor reacts to the door.** Ordinary life continues (§1.3). A
  market runs twenty metres from it.

**The line this document does not cross:** no NPC in the Anchor ever explains the
policy to the player, defends it, or apologises for it. Command's argument exists
and it is good (§13.3), and it belongs to the campaign's dialogue at the moment the
campaign chooses — not to hub ambience. A hub that argues its own morality has
already lost the argument.

### 5.4 The Act I constraint, and why this staging survives it

**RECONCILED.** `Campaign-And-Story.md` §3.4 rule 1 is absolute: *"No Altered asset
appears anywhere before A2-1 — not in optional content, not in Anchor lore, not in a
prop… Everything before the turn is Vestige."* Art §2.2 requires that to be verified
by asset-reference search rather than by memory.

The staging above satisfies it without a special case, because **for the whole of
Act I the Cell is a door.** No asset, no lore, no prop, no dialogue node, no codex
entry, no audio. Two guards who are guards. A player who asks about it gets nothing,
because there is nothing.

This is also the reason the Cell is *not* introduced later as new geometry. A room
that appears in Act III is a room the player knows is a plot device. A door they
have walked past four hundred times is the same room doing enormously more work, and
the difference in build cost is zero.

### 5.5 What the Order does with this, and where

**AUTHORED, consuming Story-Source §13.7 and campaign A2-6.** The Order are
logistics, not preachers. Their claim is *"that the militia is running a smaller
version of the same program and has stopped counting."*

The Cell is what makes that claim land, and it lands geometrically. The Order's dock
is at the boundary of the outer city (§3.7) — a loading dock, a manifest, and a gap
in the suppression field. The militia's containment is at the *centre*, inside the
hardware. Both are moving lucid Altered into suppression. One does it at the edge,
in the dark, with a forged manifest; the other does it in the best room in the city
with a signature.

The player walks between the two of them. Nobody draws the line for them.

**Deferred, TRANSCRIBED** (Story-Source §13.7): whether the Order holds a suppression
source of their own is explicitly undecided, and it determines whether they are a
faction with territory and a second hub or *"desperate and the player is their only
leverage."* This document assumes the second — one dock, no second hub — because it
is the cheaper assumption and because reversing it adds a hub rather than
invalidating this one. §9 question 12.

---

## 6. Population and life

Humanity clusters inside the radius. An Anchor is crowded. The failure mode is
precise and it is worth naming before the solution: **a lobby with three
quest-givers.** The Anchor has exactly three functioning named NPCs today (Kess,
the Quartermaster, and Command when it exists), which is the definition of that
failure, and no amount of good layout fixes it.

### 6.1 What the player should see

**AUTHORED.** Standing anywhere on the Ring Walk, the player should be able to see
**roughly 30 bodies** without turning, of which 3–4 are named and the rest are
ambient and busy.

Thirty is not a richness target; it is a *counting* threshold. Below roughly twenty,
a player counts the NPCs — not deliberately, but they arrive at a number, and once
they have a number the space is a set. Above it they stop counting and start reading
the street as a crowd. The exact threshold is a playtest question and the number is
`O2 PLACEHOLDER`; the mechanism is not.

They should also see, per Art §4.2 and costing no character art at all: laundry,
cabling run late and never tidied, awnings, permanent scaffolding, patched concrete,
warm windows. **A street with thirty lit windows reads as more inhabited than a
street with thirty idle bodies**, and light is the cheapest population in the game.
Both, ideally, in that order of priority if only one can be afforded.

### 6.2 What is achievable as code-spawned primitives now

**AUTHORED**, against what the code already does.

`ABreakerNPC` already spawns a talkable primitive humanoid with dialogue, id links
and quest flags, and `ABreakerEnemy` already builds a primitive humanoid from
torso/head/limb primitives and ground-snaps it every tick. The two together mean a
**non-talkable ambient civilian is a small piece of code and no art**: the same
primitive body, a walk cycle between two authored points, an idle at each end,
and no interaction.

Thirty of those, in the palette's desaturated earth and concrete tones, on the
market leg, is achievable today by the same means as everything else in the gym. They
will be capsules with limbs and they will look like blockout, which is correct —
they exist to answer the density question, and the density question is exactly the
one that cannot be answered on paper.

What is nearly free and worth doing in the same pass:

- **Warm window lights.** `AttachPropLight` already exists and is already used for
  the Forge glow and the supply crate. A building face with six warm point lights is
  six lines.
- **Market clutter.** Stall canopies, crates, and cable runs are cubes and thin
  cylinders. The O24 dressing pass in `SpawnWorldDressing` already establishes the
  seeded-prop pattern for exactly this.
- **The crowd is a sound problem more than a visual one**, and there is no audio at
  all in the project (`CONTEXT.md` next action 2 ranks audio first among asset
  blockers). A market with no sound will read as dead at any body count. This is
  worth knowing before the density playtest, so the result is not misread as "we
  need more NPCs."

**One existing constraint the crowd inherits**, TRANSCRIBED (Art §3.3): the HELM
slot *"must never fully hide the player's face in the Anchor — social space needs
faces."* That is an equipment rule written for the player, and it states the
Anchor's purpose more plainly than any environment line in the corpus: it is the
one place in the game where people are people. The ambient crowd is the other half
of that rule and there is currently no art budget for it at all — Art's §7.2 bucket
table and its 14.5-week schedule contain **no line item for civilians**, and the
only acknowledgement anywhere is Art's open question 7, which calls Effigy
personhood *"a real environment-population decision."* This is the largest single
hole in the corpus for a hub, and §9 question 11 records it as such.

### 6.3 What needs an artist

- **One civilian body** with 3–4 material variants and two silhouette variants.
  Per Art §5's blockout-first discipline this is the last step, not the first.
- **The market kit** (Art §4.3 marks the vendor stall and market street as *kitbash
  from market kit*, P1). One kit serves the stalls, the awnings, the scaffolding and
  the clutter.
- **Kess's hands** (Art §4.3, P0) — the Forge interior's focal point.
- **The pylon as a real asset** (Art §4.3, P2, *"one large asset, seen mostly at
  distance"*).

### 6.4 The minimum for the space to read as inhabited

**AUTHORED**, stated as a bar rather than a wish, because it is the thing that will
get cut:

1. **Thirty ambient bodies visible from the Ring Walk**, moving, ignoring the
   player.
2. **Warm interior light behind at least a dozen windows** on the loop.
3. **At least one visible activity that is not commerce** — someone repairing
   something, someone carrying something heavy, someone stopped and talking.
   Commerce alone reads as a vendor hub; work reads as a city.
4. **Nobody looks at the pylon.** Art's acceptance criterion, restated as a crowd
   behaviour: the idle set must not contain a look-up.
5. **Nobody looks at the Cell door**, for the same reason and to a different end.

Points 4 and 5 are the same rule and it is the cheapest characterisation in the
project: *what the crowd ignores is the setting.*

---

## 7. Build plan, in stages

The binding constraint, TRANSCRIBED from `CONTEXT.md`: **nobody can author `.uasset`
or `.umap` files outside the editor**, and the zero-setup convention is that a clean
clone plays with no content. So everything below is sorted by *who can do it*, not
by how much it improves the hub.

### Stage 0 — what exists (baseline)

The forward staging camp of §1.3. **It is not the Anchor and it should not become
one.** Act I needs field camps and this is a good one. The only thing worth changing
in it is the safe-zone/camp-centre mismatch (delta 2), which is a bug in the camp on
its own terms — the people are outside the field.

### Stage 1 — code-spawned blockout (no editor, no artist)

Everything in this stage uses helpers that already exist in `BreakerGameMode.cpp`:
`SpawnShape`, `SpawnGymBlock`, `AttachPropLight`, `ABreakerNPC::Spawn*`, and the
seeded-prop pattern from the O24 dressing pass. It is one function in one file.

In priority order — and the order is by *difference per unit of work*, not by
importance:

1. **Raise the pylon to 200 m.** One scale value. It changes what the space
   *means* from anywhere in the level, it costs nothing, it needs no artist, no
   editor, and no ruling. **This is the smallest change that would make the biggest
   difference, in the whole document.** Everything else in Stage 1 is larger and
   most of it matters less.
2. **Split the three boundary predicates** (§4.3). No gameplay effect at gym scale;
   a prerequisite for everything after it.
3. **The Ring Walk as masses.** ~14 building blocks at 12–20 m, a ground disc, the
   20 m plaza, the 24 m Skirt. Cubes and a cylinder.
4. **The Threshold Plaza**, with the Landing and Gate as marked primitives 20 m
   apart. Even without a rift to travel to, standing in it answers §3.3's question.
5. **Function markers**, one per hub function, each an `ABreakerNPC` or a distance
   trigger at the §3.2 spacing. They do not need to *do* anything yet — the layout
   question is about walking, not transacting.
6. **The Cell door and two guards.** A dark cube in the Skirt and two NPCs with no
   dialogue about it.
7. **The rooftop route.** Gap spacing at 4–7 m, which is a placement rule rather
   than new geometry.
8. **Thirty ambient civilians and warm window light** (§6.2).

**What Stage 1 buys, and it is the only reason to do it:** the ability to *walk the
layout*. Whether 200 m of Ring Walk is a pleasure or a tax, whether 20 m makes the
re-run feel free, whether 30 bodies stops the counting, and whether a 200 m pylon
reads as awe or as a wall — none of those can be answered on paper and all of them
can be answered in an afternoon in a grey blockout. Every number in §3 is a
hypothesis and this stage is the instrument.

### Stage 2 — owner, in the editor

- **A real `L_Anchor` map.** The camp today is runtime-spawned into the First Person
  template, and `CONTEXT.md` already records the owner reporting twice that the
  space reads wrong and that it is a map-scope issue. An Anchor at 240 m cannot be
  spawned into template geometry.
- **Lighting.** Warm tungsten and sodium interiors, cool exteriors, the pylon as the
  primary directional modifier. This is most of what makes the blockout stop looking
  like a blockout, and it is entirely editor work.
- **The Forge as a real interior** — the P0 space, the only one Art asks to be
  authored with care.
- **Navigation, collision, and the transition from the Gate to content.**

### Stage 3 — artist

The market kit, the civilian body, Kess's hands, the pylon asset, and the material
pass that turns coloured primitives into patched concrete. Per Art §5's phase
discipline, none of this starts before the blockout has been walked and the layout
has stopped moving.

### What each stage cannot answer

- Stage 1 cannot tell you whether the Anchor is *beautiful*. It can tell you whether
  it is the right size, which is the question that is expensive to get wrong.
- Stage 2 cannot tell you whether the crowd works. Bodies are Stage 1 and audio is
  unbuilt.
- Nothing before a networking position (O22; `Design-Overview.md` S3) can tell you
  whether the crowd is NPCs or players, and Story-Source §8.3 says the Anchor is
  non-instanced. §9 question 10.

---

## 8. Risks

1. **The hub is 200 m of walking in a game about going fast.** §3.3's 20 m chord is
   the mitigation and it is a single number. If playtest says the hub drags, attack
   that number and the Quartermaster→Command leg first; do not shrink the Ring Walk,
   because shrinking it takes the population out and the population is the reason
   the hub exists.
2. **Nine tenths of the Anchor's area serves four purposes and two are speculative**
   (§3.7). Staging it last is the mitigation; a smaller radius is the fallback and
   costs only the density gradient.
3. **The Cell can be softened into nothing by one line of dialogue.** The staging in
   §5 works because nobody explains it. The first NPC who justifies the standing
   order in ambient dialogue destroys it, and that is the kind of line that gets
   added late by someone trying to be helpful.
4. **The Act I asset constraint is easy to violate accidentally** (§5.4). Art §2.2
   already requires verification by asset-reference search rather than memory. A
   containment cell is exactly the kind of geometry someone dresses with a
   thematically appropriate prop.
5. **The teal reservation is one decorative object away from collapsing** (§4.2).
   In a hub with market signage, hazard markings and vendor stalls, teal is the
   colour someone will reach for. This is a review rule, not a design rule, and it
   needs to be on a checklist.
6. **Momentum suppression will be reported as a bug** before it is understood as a
   rule (§3.6), and the fix is a HUD state this document does not own.
7. **The hub has no audio and a market is a sound problem.** A density playtest run
   in silence will under-report the population by an unknown amount and the result
   will be misread as needing more bodies.

---

## 9. OPEN QUESTIONS

Ranked by how much each one blocks. Nothing below is decided in this pass.

### 1. One primary Anchor, or a network? — BLOCKS the entire layout

**TRANSCRIBED as open** (Story-Source §1.8): *"One primary Anchor, or a network?
Network is truer to the fiction and worse for MMO population density. Recommend one
primary with the rest as lore and fast-travel destinations."*

Everything in this document assumes one primary Anchor. A network changes the answer
to almost every section: the hub becomes a fast-travel node, the population argument
in §6 cannot be paid for several times over, and the Ring Walk's 200 m is
indefensible if the player is choosing between four of them.

| Option | Shape | Cost |
|---|---|---|
| **One primary Anchor** *(the source's own recommendation)* | Other Anchors exist in lore and as fast-travel destinations with no interior. | The fiction says humanity clusters in *Anchors*, plural, and the player only ever sees one. Cheapest by a wide margin. |
| Two hubs, one primary | The second is the Order's, if §9.9 resolves that way. | Doubles the population and dressing budget for the second one. |
| A real network | Several walkable Anchors. | Multiplies §6 and §7 Stage 3 by N and thins the crowd in every one of them. Not affordable. |

### 2. What is the suppression radius in metres, and does all of it freeze Momentum? — BLOCKS the code split and the Swift feel

§3.2 authors 240 m and §4.3 recommends splitting suppression from the Momentum gate.
Both are placeholders standing in for a ruling.

| Option | Shape | Cost |
|---|---|---|
| **Split, gate scoped to the core** *(recommended)* | Suppression 240 m; Momentum gate ~64 m; spawn safety unchanged. | A small mechanical change in owned code. Gives the outer city a reason to exist for a movement player. |
| Split, gate = suppression | Both 240 m. | The outer city is Momentum-dead and Swift has no reason to enter it. §3.7's third purpose disappears. |
| Do not split | One radius for everything. | Forces the Anchor to be Momentum-gate-sized, i.e. tens of metres. The Anchor stops being a city. |

Sub-question either way: **what is the number?** 240 m is authored on the reasoning
in §3.2 and nothing else.

### 3. Is the Cell canon, and does it exist from level 3? — BLOCKS the Altered staging

§5 is the most AUTHORED section in this document. The facts it composes are all
transcribed, but the room, its placement inside the Skirt, and the four-hundred-hour
closed door are new.

| Option | Shape | Cost |
|---|---|---|
| **Cell in the Skirt from level 3** *(recommended)* | A door on the main street, opened once in A3-5. | One door state and two guard NPCs. The payoff is proportional to how long it was closed, so deciding late costs the whole effect. |
| Cell introduced in Act III | The room appears when it is needed. | Reads as a plot device. Same build cost, far less effect. |
| No containment; A3-5 is handled in dialogue | The Survivor gets "inside an Anchor" abstractly. | Free. Gives up the strongest physical staging the setting offers, and makes the cure a line of text. |

### 4. Does suppression stopping Momentum have a fiction, or is it a bare anti-farm rule? — BLOCKS nothing; decays if unasked

The gate exists in code and its reason is anti-farm. §3.6 recommends *presenting* it
as suppression, which is nearly free and good, and which quietly asserts that
Momentum is suppressible by rift hardware. Campaign §1.3 argues the opposite of the
adjacent claim — *"a Breaker is not rift-powered"*. If the suppression read is canon,
it says something about Momentum that no class document has ruled. If it is a
mechanical rule wearing a coat, that is fine and should be known.

### 5. Does the Anchor exist in the vertical slice at all, or only the Forge? — BLOCKS how much of §7 is scheduled

**TRANSCRIBED as open** (`Art-And-Modelling-Plan.md` open question 6, and the §4.3
slice compromise): *"the Anchor is not required for the vertical slice, which is
one graybox biome or arena. Build the **Forge only** as a functional room.
Everything else is P2."* Art's plan builds Forge-only and asks for confirmation
before its Phase F.

This document is a design for the whole Anchor and does not overrule that. The
question is whether the owner wants the **Stage 1 blockout** (§7) — which is a day
of code and answers the layout question permanently — or wants the Anchor deferred
entirely behind the slice. Those are compatible: the blockout is cheap precisely
because it is not the slice.

### 6. Is the Anchor a separate map, and when? — BLOCKS Stage 2

Everything today is runtime-spawned into the First Person template and the owner has
twice reported the space reads wrong at map scope. A 240 m Anchor cannot live there.
The question is whether the Anchor map arrives before or after the real gym map that
`CONTEXT.md` next-action 5 already wants.

### 7. What is the rift/mission selection surface, and how does the player physically enter a rift? — BLOCKS the Command post and the Gate together

**No document in the corpus specifies either.** `Game-Modes.md` authors tiers 1–30
and the campaign authors 26 missions; nothing says how the player picks one, and
nothing says how they get from the Anchor to a rift threshold — §3.2 starts *at*
the threshold. The only implemented affordance in the game is an NPC dialogue node.

§2.2 argues the surface should stay a conversation rather than becoming a war
table, because tier is invisible during levelling and the tier readout is
deliberately diegetic at the ingress. That is an argument, not a ruling.

| Option | Shape | Cost |
|---|---|---|
| **Conversation + a Gate you walk through** *(this document's assumption)* | Command gives the contract; the Gate is the transition. | Cheapest. Needs a dialogue-choice action that is not a quest flag (§2.4). Command and the Gate stay 20 m apart. |
| A physical portal that is also the selector | One object does both. | Command and the Gate merge and §3.2's Command leg is wrong. Fights §3.3's shared Threshold Plaza. |
| A contract board / map table | A world object with a full-screen modal. | Empty for forty hours (§2.2). Real UI work. |

### 8. Where do the un-homed interactions live? — BLOCKS nothing individually, blocks the hub's completeness together

Three interactions are specified somewhere in the corpus with no location at all,
and a hub is where things without a location end up by default. Each needs a
deliberate answer rather than accumulating at the Command post:

- **Party formation and matchmaking** for Dungeon (4), Raid (7) and Conquest (9
  matchmade, join-in-progress). No doc says from where.
- **The score / results screen.** `Game-Modes.md` §3.5 step 4 says *"No score screen
  yet"* — the *yet* implies one follows the erasure, and nothing says whether it
  appears on return, in the hub, or as a modal.
- **Salvage.** O12 makes materials scalar currencies with no container, and
  `UI-UX-Spec.md` notes the Forge and salvage screens need *"a currency header, not
  a materials container"* — but salvage has no station.

### 9. "Anchor" the settlement versus "anchor point" the rift holder — BLOCKS nothing, costs more later

`Game-Modes.md` §3.2 names the thing holding a rift open the **anchor point**, and
the campaign's rift skeleton is *threshold / body / anchor point / closing ritual*.
Story-Source §1.2 locks **Anchor** as the settlement. Two load-bearing nouns one word
apart, in the same sentence often enough to matter. A rename now is a search and
replace; a rename after voice recording is not.

### 10. Is the Anchor non-instanced for the slice? — BLOCKS whether §6's crowd is people

Story-Source §8.3 and §10.1 both say the Anchor is not instanced and is *"the MMO
layer"*. There is no replication position (O22, still pending; `Design-Overview.md`
S3). If the crowd is other players, §6's ambient-civilian work is temporary
scaffolding. If it is NPCs, it is the real thing. Both are defensible; building
without knowing means building one of them twice.

### 11. Who authors the ambient population, and against what budget? — BLOCKS §6 entirely

**There is no civilian art budget anywhere in the corpus.** Art's §7.2 bucket table
and its 14.5-week phase schedule contain no line item for civilians; there is no
crowd density target, no shared civilian base body, no LOD or imposter plan. Seven
named NPC roles exist and no ambient population.

§6 authors a target (~30 visible bodies) and a Stage-1 route to it (primitive
ambient NPCs, achievable today). What is not decided is whether the *shipping*
crowd is a scheduled art deliverable or is permanently blockout-quality background.
Both are defensible for a one-developer project; only one of them should be
discovered late.

### 12. Does the Order hold a suppression source of their own? — BLOCKS whether there is a second hub

**TRANSCRIBED as deferred** (Story-Source §13.7): *"If yes they are a faction with
territory and a second hub. If no they are desperate and the player is their only
leverage."* §5.5 assumes no, because it is cheaper and because reversing it adds a
hub rather than invalidating this one.

### 13. Do Effigies have legal personhood inside an Anchor? — BLOCKS one crowd rule

**TRANSCRIBED as open** (Story-Source §1.8; restated as Art open question 7, which
calls it *"a real environment-population decision"*). It decides whether Effigies
appear in the ambient population as civilians, appear only as militia, or do not
appear at all outside Kess. It is one line in a spawn table today and a much larger
question the moment the civilian body is authored (§6.3), because it decides
whether that body needs a non-human variant — and Art §7.4's standing warning is
that committing the rig to human proportions before the Effigy exists costs a
re-rig later.

---

## 10. Acceptance criteria

Checkable, and written so a failure is visible rather than arguable.

- [ ] The pylon is visible from every exterior point inside the suppression radius.
- [ ] Any exterior screenshot taken on the Ring Walk contains the pylon and at
      least one person not looking at it (Art §4.4).
- [ ] Zero saturated-teal objects in the Anchor other than suppression hardware
      (O19, Art §4.4). Verified by an object search, not by memory.
- [ ] The Anchor contains no signage explaining rifts, Altered, or severance
      (Art §4.4).
- [ ] Landing to Gate is walkable in under 5 seconds without sprinting.
- [ ] Stash to Forge is walkable in under 5 seconds without sprinting.
- [ ] A full Ring Walk circuit passes every hub function exactly once with no
      backtracking.
- [ ] Every movement verb functions everywhere inside the suppression radius, and
      the rooftop route is completable and never required.
- [ ] The Ring Walk offers at least three wall-ride surfaces of ≥6 m
      (`Game-Modes.md` §2 movement contract).
- [ ] No Altered asset, prop, dialogue node, or codex entry exists anywhere in the
      Anchor before A2-1 (Campaign §3.4 rule 1; verified by asset-reference search
      per Art §2.2).
- [ ] The Cell door exists and is unlabelled from the player's first visit.
- [ ] No ambient NPC in the Anchor states, defends, or apologises for the standing
      order.
- [ ] No idle animation in the ambient set contains a look toward the pylon or the
      Cell door.
- [ ] At least 30 bodies are visible from any point on the Ring Walk.
- [ ] Momentum inside the gated area is visibly *suppressed* rather than visibly
      *empty*.
