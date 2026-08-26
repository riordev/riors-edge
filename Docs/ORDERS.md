# ORDERS

Every lane reads this at the start of a cycle. It replaces the per-lane messages.
When it changes, it changes here and you get it on your next rebase — so rebase
before you plan, not after you build.

Written by the design seat, not by a lane. **No lane edits this file.** If you
disagree with something in it, say so in your report and it changes here.

---

# PART ONE — THE OWNER HAS RULED

Six decisions came back. Four are settled, one needs a shape it does not have,
one is answered but not in the form the question was asked.

## 1. Swift's starters — NOT Slipcut. A new dash passive, plus Skim

Owner, verbatim: *"it doesnt really matter to me we can start with an enhanced
dash passive which we create + skim."*

So **O176 as written is dead.** The starter pair is not Slipcut + Skim. It is a
new enhanced-dash passive plus Skim, and Slipcut joins Lead, Cadence Break, Hard
Stop and Sightline as unlockables.

**The problem: this game has no passive ability slot.** `EBreakerAbilitySlot` is
`ClassAbilityOne`, `ClassAbilityTwo`, `Ultimate` — all active. Passives in this
project are **tree nodes**: `EBreakerNodeStatTarget::DashCooldown` exists,
`RecentlyDashed` is a build condition, and `Core.*.Afterburn` already keys off it.

Two readings, and I am ruling the cheap one unless the owner overturns it:

- **(a) RULED.** The passive is a **tree node granted at level one**, not a slot
  occupant. Swift starts with Skim in `ClassAbilityOne`, an always-on dash node
  from the tree, and `ClassAbilityTwo` **empty until the first unlock**. That
  slot sitting visibly empty is a feature — it is the first thing the
  quartermaster fills.
- **(b) NOT RULED.** Passives become slottable. That is a system addition — a
  fourth slot kind, a picker that mixes actives and passives, save migration.
  Do not build it.

If the empty second slot reads badly in play, that is the owner's call after
seeing it, not a reason to build (b) now.

**LEDGER owns the node. KIT owns the loadout flip.** Coordinate the landing;
KIT's three enumerated reds clear on it.

## 2. Ability sound — the owner will make assets, so do not wait for them

Owner: *"ill make assets for those sounds eventually."*

That answers the fifth-verb question sideways. He is willing to author per-ability
content, so the answer is not "one cue for all twenty-five" forever — but
"eventually" means KIT cannot block on it and the abilities cannot stay silent
until then.

**RULED: the fifth verb ships with a single default cue for every ability, and a
per-ability override that falls back to the default when empty.** One verb, one
default, twenty-five optional slots. The owner fills slots as he makes assets and
nothing is silent in the meantime, nothing is re-plumbed when the assets land.

GLASS owns the verb — `ABreakerSoundDirector`'s fifth. KIT owns nothing here:
`OnAbilityActivated` is already broadcast and already bound by GLASS's HUD, so
the cue is GLASS calling `GetSoundDirector()` in a handler that already runs.

## 3. Fernhall — see Part Two. It is the whole world now

## 4. Density — optimise, do not lower the target

Owner: *"we just need to optimize they should have enemy variety ill expand on
this later."*

**RULED: the 50–100 concurrent target stands.** 100 engaged primitives at 34.16 ms
is a problem to solve, not a number to design around. Enemy variety is a
requirement, not a nice-to-have — the owner will expand on what variety means.

This makes the sweep urgent rather than interesting: engaged at N = 25 / 50 / 75 /
100 gives slope and intercept, and the intercept is what tells us whether the
cost is per-body or fixed. Optimising without it is guessing.

## 5. The at-cap band — tune content UP

Owner: *"we will tune power up."*

**RULED: the authored 8–10× stands and content rises to meet it.** Measured 6.54×.
Do not widen the assertion, do not move the band down. The pin stays until the
measurement reaches the band.

## 6. Prolific — the breach is accepted

Owner: *"this is fine i dont mind over the mark damage most of the time."*

**RULED: 22.64× against the authored 12–20× endgame band is accepted.** Prolific's
magnitude does not come down. The band's upper edge is a target, not a ceiling,
and a rewrite stacking past it is the intended feel.

The pin closes. Record in the ruling that the band is now described as
*where most builds land*, not *where every build must land* — because an
assertion that fires on the intended outcome is an assertion pointed the wrong
way.

---

# PART TWO — FERNHALL IS THE WORLD

Owner: *"fernhall should just be an area the player can roam with the rifts and
some missions inside of it just the 1 area we have."*

This is the largest simplification the project has made and it deletes more than
it adds. Read it before planning anything.

## What it replaces

The multi-zone structure — several planet-like maps, travel between them, a rift
interior generator per zone — is **off**. There is one place. Everything the
campaign was going to be happens in it.

## The three spaces, and there are only three

```
  ANCHOR      social. No combat, no HUD ammo, weapon holstered. Vendors,
              travel, the place you return to.
  FERNHALL    the world. Roamed, fought in, missions taken and done here,
              rift doors standing in it.
  RIFTS       instanced. Entered from a door in Fernhall, generated, exited
              back to where you stood.
  (the gym stays as a test bench and is not part of the loop)
```

The loop is: **Anchor → Fernhall → a rift inside Fernhall → back out to
Fernhall → Anchor.** That is the whole game's shape. It is small enough to
finish.

## What Fernhall has to become

The yard is 100 × 50 m and about a minute end to end. That is a corridor, not a
place you roam. **It has to grow, and the composer is why that is affordable** —
`compose_fernhall.py` builds from a Kenney kit with the grammar validating
placement, so growth is authoring, not modelling.

Target shape, all `O2 PLACEHOLDER` and all owner-tunable once he walks it:

- **Four to six connected yards**, not one large field. Each the size of the
  current one or a little larger, joined by lanes and gates. A field of 500 × 300
  m is empty; six rooms of 100 × 50 read as a place.
- **Each yard has a reason to exist** — a rift door, a mission giver, a fight
  that only happens there, a route to two others. A yard with none of those is a
  corridor with grass.
- **The existing yard is the entry.** It already has the travel gate back to the
  Anchor, the rift pad, and `marker_npc_contract`. It becomes the plaza the
  player arrives in, and the rest extends from it.
- **Enemies live in it**, not in waves. Density and variety per yard, tied to
  ruling 4 — this is where the 50–100 target has to actually hold, and where
  variety stops being a word.

## Rifts, under the rulings that already exist

O122 stands: **a campaign rift is entered freely, an endgame rift is
consumable.** Both kinds stand in Fernhall as doors. The free ones are how the
campaign is played; the consumable ones are the endgame, and they use the same
doors with a different key.

O82 stands: campaign respawn is unlimited from the start of the tileset, and the
endgame death budget stays **parked** until consumable rifts exist — a limit on a
free instance kicks the player out of a door they walk straight back through.

The rift *interior* generator is still not started and is still gated on the
owner having stood in the yard. What is unblocked is the **door**: a marked site
in Fernhall that writes a `PendingRift` and travels. The interior can be the gym
for one landing — the gym is a real space with real spawning, and a rift whose
interior is the gym is a complete loop with a placeholder room, which is worth
far more than a perfect room with no loop.

## Missions

The smallest thing that is a mission: **an NPC in a yard gives a task, the task
happens in Fernhall, the NPC pays it.** `marker_npc_contract` is already placed
and measured. The First Contract — the fourteen dead Core Points — is the first
one and it is what turns those points on.

Do not build a quest system. Build **one contract**, end to end, and see what it
needed. A quest system authored before a single quest exists is a guess about a
shape nobody has held.

## What this does to the wave budget

The gym's rising-wave curve was always the mechanism for a **wave mode**. Fernhall
is not waves — it is a populated area. The solver still applies: a yard's
population is a composition, and `SolveWave` prices archetypes against a budget.
What changes is that the integer driving it is not a wave index but a yard's
difficulty. O134's space input is now the live question rather than a rift
question, because a yard's shape decides its population.

---

# PART THREE — WHAT EACH LANE DOES NEXT

## GROUND — `lane/ground`

**You are unblocked. Fernhall growing is yours and it is now the project's spine.**

1. **Read Part Two and report back before building.** Say what the composer can
   and cannot express about six connected yards, what the grammar validates
   across a gate, and whether the cover rules hold at a junction. Report first;
   the yard shape is a design decision I will make against what you find.
2. **The rift door.** A marked site that writes a `PendingRift` and travels, with
   the **gym as the interior** for the first landing. O122's free-entry rule.
3. **O134 is now live** — a yard's shape decides its population. Your held design
   report on space inputs stops being speculative.

Do not build the rift interior generator. Do not build a quest system.

## FIELD — `lane/field`

**Density is a requirement now, not a finding. Optimise.**

1. **The sweep first** — engaged at N = 25 / 50 / 75 / 100. Slope and intercept.
   Without the intercept every optimisation is unmeasured. GROUND owns the probe
   if the flag needs changing.
2. **Then optimise against what the sweep says.** The cost is game-thread and the
   target is unchanged, so the question is what an engaged body does per tick and
   which parts of it can be amortised, staggered or skipped at distance.
3. **Enemy variety** is coming as a requirement — the owner will expand. Do not
   design for it yet; do not make optimisations that assume one archetype.
4. Still open from before: the colour ramp's terminal hue (every Boss dies
   magenta, not one family), and the fracture-mask material query.

## LEDGER — `lane/ledger`

1. **The enhanced-dash passive** — ruling 1. A tree node granted at level one.
   Report the shape before authoring the magnitude: which node target, whether it
   is a Core or a Swift node, and what "enhanced dash" means in the vocabulary
   that exists (`DashCooldown` is real; charges are not).
2. **Swift's partition** — the two lines KIT's three reds wait on, with Slipcut
   as an **unlockable** and not a starter. Land it with KIT's loadout flip.
3. **Ruling 6 closes the Prolific pin.** Rewrite the band's description: it is
   where most builds land, not a ceiling.
4. **Ruling 5 keeps the at-cap pin open** and points it at content, not the
   assertion.

## KIT — `lane/kit`

1. **O176 is dead as written.** Slipcut is an unlockable. Your three enumerated
   reds still clear on LEDGER's two lines — the contents change, the shape does
   not.
2. **Do not build the dash passive.** It is a tree node and LEDGER owns it. Your
   half is the loadout flip when it lands.
3. **The six abilities that still draw nothing** — Rend, Provoke, Breach Charge
   and three more. Apply the Swift template: cast-moment flash through the pooled
   renderer, palette role by verb, windows stay HUD bars. That is unblocked now.
4. **Sound is GLASS's.** You need no interface and should not wait.

## GLASS — `lane/glass`

1. **The fifth verb** — ruling 2. One default cue, per-ability override slots
   falling back to it. Nothing silent, nothing re-plumbed when the owner's assets
   arrive.
2. **`BreakerEffectRenderer` is PUBLISHED** — see below. Name the consumers at
   its declaration and treat a public-surface change as a declared crossing.
3. Still open: the boss phase readout coordination with FIELD's bar.

---

# PART FOUR — A THIRD KIND OF SHARED PATH

`UI/BreakerEffectRenderer.*` is GLASS-owned and called from four lanes —
`Abilities/`, `Combat/`, `Game/`, `UI/`. The map has no vocabulary for that:
`Tests/` is shared because every lane writes its own, `Docs/` because every lane
appends, and both have **no owner**.

**A published path has an owner and named consumers.** The owner changes the
implementation freely and **declares** a change to the public surface. Same rule
as a header, with the consumers named in advance instead of discovered.

Published paths today:

```
  UI/BreakerEffectRenderer.*      GLASS  ->  KIT, FIELD, GROUND
  UI/BreakerEffectMath.h          GLASS  ->  KIT, FIELD, GROUND
  Attributes/BreakerHealthBands.h LEDGER ->  FIELD, GLASS
```

The `EnemyBlips` producer/consumer contract between FIELD's TU and GLASS's
minimap is the same shape wearing a member variable instead of an API, and O155
already rules it a declared crossing.
