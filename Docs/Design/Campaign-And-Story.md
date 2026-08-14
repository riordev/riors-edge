# Campaign and Story — the spine, the missions, and the handoff

**Last reconciled against: O28**

Domain: the premise as a *working* fiction, the three-act spine expressed as a
ladder of area levels, the story mission list with what each one teaches and
gives, where the permanent class choice sits, how the campaign hands the player
to the endgame, and what the campaign needs from the existing quest/flag code.

**Authority.** `Docs/Design/Decisions.md` is law and supersedes everything here.
Then `Design-Overview.md` (map, not law), then the per-domain docs (O28). This
document is subordinate to all of them and authors nothing that contradicts a
ruling. Class permanence, the level cap of 50, the element set (Rift / Entropy /
Void, O5/O19), and every O-number are LOCKED and are treated as fixed inputs.

**Every number in this document is `O2 PLACEHOLDER`.** Level bands, area levels,
mission counts and durations are shape, not values. O2 freezes value authoring;
nothing here is a balance sheet.

---

## 0. How to read this document

The project has deep systems documentation and, until now, no campaign. That gap
was not an absence of *fiction* — the fiction is dense and good — it was an
absence of anything saying **what the player does, why, and in what order**.
Recovering the fiction and authoring the order are two different jobs, so every
substantive claim below carries one of three labels:

| Label | Meaning |
|---|---|
| **TRANSCRIBED** | Already stated in the corpus. Recorded here so the campaign can be read in one place. The source is cited. Not new, not up for debate in this pass. |
| **AUTHORED** | New in this document. Owner-reviewable. If it is wrong, it can be deleted without touching anything else. |
| **RECONCILED** | Two existing documents said adjacent things; this states the reading that satisfies both without changing either. |

**Sources this document transcribes from.** `Master-Sheet-Import.txt` §1, §8 and
§13 carry the entire premise, the act structure, the erasure fiction, the final
choice, and the NPC roster. O28 stripped that file of *authority* — it is
"historical source material, not law" — but it did not delete its *content*, and
nothing has superseded the story material in it. So this document **re-asserts**
that content as live design, which puts it under owner review for the first
time since O28. Where it is transcribed, it is labelled and cited; where this
document goes past it, it is labelled AUTHORED.

**Ownership.** This document owns: the mission list, the act-to-area-level
ladder, the teaching order, the class-choice placement, and the campaign side of
the endgame handoff. It does **not** own: the XP curve or the ~15 world Core
Points (`XP-And-Pacing.md`, CANON per O7), rift structure and objective
archetypes (`Game-Modes.md`), enemy archetypes/modifiers/bosses
(`Encounter-Design.md`), area-level maths (`Power-Curve.md`), or art
(`Art-And-Modelling-Plan.md`). Where it names one of those, it is consuming it.

---

## 1. Premise and setting

### 1.1 The premise, as the mechanics already assert it

**TRANSCRIBED** (Master §1.1–1.7, §8; Art-And-Modelling-Plan §1–4; O24, O19).

An alternate Earth, roughly one century forward. Interdimensional rifts have
opened across the planet and what comes through is hostile. The player is a
**Breaker**: a member of an elite militia that fights what comes out of rifts and
closes them by entering and clearing what holds them open.

The rifts were opened by one person, **Rior** — scientist, activist, not in this
timeline. He built rift technology as salvation: reach into an uninhabited
timeline, take what a dying biosphere needs, come back, nobody dies. Humanity
used it exactly as designed and then used it more, until the dead worlds being
stripped were not dead. He concluded the problem was never scarcity — he had
handed appetite a larger plate — and that humanity cannot be permitted to reach
further than one world. He is now erasing humanity from timelines, sequentially,
working outward. **This timeline is next. That is what "Rior's Edge" means: the
leading edge of his advance.**

He cannot be reached from inside this timeline. Rifts are the only direction he
can be attacked from. **That is why Breakers enter them**, and it is the reason
the game's atomic loop exists at all.

Two families come through. **Vestiges** are rift-native, genuinely alien, with no
design intent and no readable anatomy — Rior opened the doors, he did not build
what came through them. **The Altered** are refugees from timelines he has
already finished, who came through ahead of him. They are not inherently
hostile; **severance** — being cut off from a timeline that no longer exists —
degrades them until nothing is left but the shape. Severance is *reversible*
inside a functioning Anchor. Militia policy is engage on sight, because stage
cannot be identified in a firefight. That policy is defensible and horrifying,
and it is the specific thing the **Order** attacks.

**An Anchor** is a settlement built on rift-suppression hardware. Humanity
clusters inside the suppression radius; Anchors are the only safe ground.
Ordinary life continues inside one. The suppression hardware is visible from
everywhere and nobody looks at it any more.

And the fact the player does not learn at the start: **closing a rift does not
seal a door. It closes a gap in time. The timeline behind it, and everyone in
it, never existed.** Retroactivity is bounded — what already happened in *this*
timeline stays happened — because unbounded erasure is a paradox engine that
eats the fiction.

### 1.2 What the mechanics are, in fiction

**AUTHORED.** The brief for this section was to build the premise so that the
mechanics that already exist are *explained* by it. This is that mapping. Each
row is a system that shipped or is designed; the right column is what it is in
the world. Nothing here changes a mechanic.

| Existing mechanic | What it is in fiction |
|---|---|
| Entering a rift and killing the thing holding it open | The only offensive direction that exists. Rior cannot be reached laterally. |
| The closing ritual (`Game-Modes.md` §3.5) | A world ending, performed by the player, four hundred times, without comment. |
| Area level, rising forever past the character cap | How deep along Rior's advance the content sits. Deeper is closer to him and further from anything this timeline can protect. |
| The Anchor safe zone and its suppression pylon | The only place severance stops. Mechanically it is a radius; diegetically it is the entire reason a settlement exists where it does. |
| The Forge, and respec being Forge-gated | Rior built the Effigies to inherit the planet after humanity. The player reforges their build inside the thing he built to replace them. |
| Permanent class selection | A Breaker is inducted once. The militia does not retrain people it expects to lose. |
| Gear as the whole endgame; level cap 50 | A person stops improving. Equipment does not. |
| The single Anomalous equip slot | The one piece of rift matter a Breaker is permitted to carry. See §1.3. |
| Two enemy families with two silhouette languages | One is what came through. The other is who came through. |
| Overgrown Earth (O24) | See §1.4. |

### 1.3 The teal law, and what it says about the world

**TRANSCRIBED** (Art-And-Modelling-Plan §1 Pillar 3, §2b; O19; `BreakerUIStyle.h`
"Teal object law"). Saturated teal is reserved, narrowly, for rift portals,
Vestige emissive, severance progression, **suppression hardware**, and the
**Anomalous** rarity. O19 fixed the rule verbatim: *"saturated teal is a property
of objects, not of damage."* Rift-*element* damage gets a different, hotter,
whiter cyan so routine damage never wears the reserved band. The code already
holds the line: the gym's entire teal budget is three objects — the safe-zone
pad, its ring of boundary posts, and the suppression pylon.

**AUTHORED — three things this implies, and the campaign is built on all three.**

**1. The rift is a substance, not an energy.** A colour that belongs to objects
and never to damage describes something that can be *present* or *absent* and
cannot be *emitted*. You cannot be hit by rift. That is why closing one is a
geometric and temporal operation — closing a gap in time — rather than an
explosion, and it is why suppression is a field held up by a physical pylon
rather than a shield anyone wears. It also explains, without a line of dialogue,
why the player's own kit never glows teal: a Breaker is not rift-powered. They
are a person with a rifle standing in the doorway.

**2. Suppression hardware is made of the same thing as the rifts.** The
reservation puts the pylon and the portal in one colour band. The only reason
humanity is holding is that it is standing on a piece of the problem. Nobody in
an Anchor remarks on this because nobody looks at the pylon any more — which is
already the environment brief (Art §4.1). The campaign never states it. The
player works it out the first time they see a rift portal and recognise the
colour from the thing over the market.

**3. The Anomalous slot is the one place a Breaker carries rift matter, and the
limit is one.** Anomalous items are rule-rewriters and the top of the endgame
chase, and they are the only rarity permitted the reserved chroma (Art §3.3).
So the chase at the far end of the game is a chase for pieces of him, and the
game already caps it at one equipped. That cap is a locked item rule that now
also reads as a statement.

### 1.4 Overgrown Earth, and why it is the strongest dressing available

**TRANSCRIBED** (O24; implemented as the gym dressing pass in
`BreakerGameMode.cpp`): nature has reclaimed the ground — vegetation over ruins —
with slight sci-fi styling and weathered, out-of-place tech scattered through it.

**AUTHORED — this is not set dressing, it is the argument.** Master §1.3 already
says infrastructure exists inside the suppression radius and degrades rapidly
outside it. So the green is not post-apocalyptic decay in the usual sense: it is
what happens when humanity voluntarily withdraws into a handful of radii and
stops holding the rest. And Rior built the Effigies as **caretakers, to inherit
the planet after humanity** (Master §1.2). **The overgrown Earth is what his plan
looks like when it works.** Every exterior in the game is a picture of him being
right, and the game never says so.

Two consequences the campaign takes as binding:

- **Density and repair standard fall off with distance from a pylon**
  (Art §4.2). This gives every level a free legible compass and makes "how far
  out are we" readable without a UI element.
- **The Anchor camp that exists in the gym today is not the Anchor.** It is a
  forward staging camp: a slab, a back wall, a Forge, a supply crate, two
  watchtowers, a pylon, and two people. That is the smallest viable unit of
  Anchor presence and the campaign uses it as such — Act I's field camps are
  this, and the Anchor proper is a city.

### 1.5 The NPC roster the campaign uses

**TRANSCRIBED** (Master §13). Every named NPC owns a system; an NPC without a
mechanical function is a lore dispenser and players skip lore dispensers.

| NPC | Owns (mechanical) | The thread |
|---|---|---|
| **Kess**, Forge Keeper (Effigy) | Respec, crafting, tier upgrades, the T-1 exalt/corrupt path. Highest interaction count in the game. | The only character who has met Rior. Will not say what he was like. Does not resolve before Act III. |
| **Command** (placeholder: Commander Aluko) | Contract giver. Gates acts. Owns mission structure and difficulty tiers. | Argues for SEAL, honestly, and is right about the arithmetic. Knows severance is reversible and has for years. |
| **The Researcher** (placeholder: Dr. Imani Vosk) | Reconstructs Rior fragments; therefore owns every capability they unlock. | Rior's former colleague. Was present, understood, did not stop it. Her guilt is visible in what she refuses to reconstruct. |
| **The Survivor** | The emotional load of the final choice; the proof severance is survivable. | Lucid because the player found them in time. On a clock. Never accuses the player of anything, which is worse. |
| **The Quartermaster** | Gear, ammunition, consumables. | Deliberately the most ordinary person in the Anchor. No opinion about rifts at all. Exists to ground the hub. |
| **The Order** | Unauthorised refugee intake. | Not preachers — logistics. Their claim is not that Rior was right; it is that the militia runs a smaller version of the same program and has stopped counting. |
| **Rior** | Nothing. Never present. | Delivered entirely through recovered fragments and the state of the worlds he has finished. Reads as working notes, never confessions. Never witty — wit invites the player to enjoy him. |

**Already in code**, verbatim: Kess (`KESS — FORGE KEEPER`) and the Quartermaster
are spawned in the gym camp with four and three dialogue nodes respectively, and
Kess's `Rior` node already plays the refusal — *"(A long pause. The Forge hums.)
...Bring me something worth heating, Breaker."* That is the campaign's
slow-burn thread, shipped, and it is currently the only story content in the
build.

---

## 2. The campaign spine

### 2.1 The act structure

**TRANSCRIBED** (Master §8.1; `XP-And-Pacing.md` §3–4). The act breaks are
dictated by the progression schedule, not chosen: Class Points run 1–30, Core
Points run 1–50. The mechanical and narrative arcs complete on the same beat,
three times.

| Act | Levels | Player question | Location | Mechanical arc | Hours (O2 PLACEHOLDER) |
|---|---|---|---|---|---|
| **I** | 1 → 15 | What am I? | One Anchor and its local rifts | Class kit, resource loop, first branch commitment | ~6.8 |
| **II** | 16 → 30 | What are the rifts, really? | Wider region; The Breach | Branch completion. Class Points exhausted at 30. | ~12.0 |
| **III** | 31 → 50 | Who is Rior, and what do I owe him? | Inside erased timelines | Pure Core Tree specialisation | ~20.8 |
| **Endgame** | 50 (capped) | — | Rior's Frontier | Gear only | unbounded |

Two curve facts the mission list is built around, both transcribed from
`XP-And-Pacing.md`:

- **The level 15 gate must be reachable on main-path content alone.** Optional
  content should push the player to 16–17 by the gate, which is pleasant. An act
  gate that requires optional content is the fastest way to make an ARPG a
  chore.
- **The 28–31 seam relief exists** because level 30 is the last Class Point and
  must not be the most expensive level in the act. The campaign must not stack a
  hard story gate on top of it; the seam is already carrying four simultaneous
  changes.

### 2.2 Area level is a property of content — the ladder

**TRANSCRIBED** (O27; `Power-Curve.md` §1; `XP-And-Pacing.md` §8). Area level is
the single input describing how hard a piece of content is. It drives monster
health, monster damage, and the item level of drops. It is **never** derived from
the player's level, gear, or build. `ZoneLevel` is authored per zone on a Data
Asset; `EnemyLevel = ZoneLevel + PackModifier` where PackModifier is in [−1, +3];
enemies never scale to the player in campaign zones. Rift tier available to a
player is capped at their level, so tiering is invisible during levelling.

**AUTHORED — the campaign is the ladder of area levels the player climbs.** That
sentence is the entire structural thesis of the campaign and it is worth being
explicit about what it forbids: there is no such thing as a story mission that
is hard "because it is the story". A mission is hard because its area level is
higher, and area level is a number a designer typed.

The ladder rule, in one line:

```
Campaign area level for a mission = the level the player is expected to be
at when they reach it, with the act climaxes authored +1 to +2 above it.
No campaign content is authored above area level 50.
```

`O2 PLACEHOLDER` for every number below.

| Act | Area level band | Notes |
|---|---|---|
| I | 1 → 15 | Prologue at 1. Act I boss at 15, i.e. on-level with a player who reached the gate on main path alone. |
| II | 16 → 30 | The Breach opens the band. Act II boss at 30. |
| III | 31 → 50 | One Earth per sub-band: ~31–36, ~37–41, ~45–48. Final mission at 50. |
| Endgame | 50 → ∞ | Frontier. Outside this document; see §5. |

**Why the act climaxes are authored +1 to +2 and not more.** The XP falloff table
(`XP-And-Pacing.md` §5.2) pays 1.15× at +3 to +4 and caps at 1.25× at +5. Anything
past +2 turns an act boss into an XP-efficiency incentive and invites players to
under-level deliberately. The lead should be felt as difficulty, not farmed as a
bonus.

**The one campaign-wide guardrail on area level.** `Power-Curve.md` predicts that
with the weapon growth rate `w` equal to the monster health growth rate `g`, a
*baseline* build holds a roughly constant TTK across the whole game, and all
felt progression comes from the build multiplier band. **If that prediction is
false, this ladder is wrong and the fix is the two growth constants, not the
mission list.** The measuring run is already named as the project's highest-value
playtest (`CONTEXT.md` next action 1). Do not retune the campaign to work around
an unmeasured curve.

### 2.3 The one-time / repeatable split

**TRANSCRIBED** (`Game-Modes.md` §2): target roughly **20% one-time, 80%
repeatable**, measured in first-playthrough hours. The campaign is the one-time
content. It must be good enough to sell the game and short enough that the
endgame is where players live. Every one-time piece must hand over a permanent
*capability or currency*, never a stat.

**AUTHORED — what that means for the mission list below.** A story mission is a
frame around repeatable content, not a replacement for it. Each mission in §3
authors: an area level, one or two objective archetypes drawn from
`Game-Modes.md` §3.4, a beat, and a grant. It does **not** author bespoke
geometry per mission — the rift skeleton (threshold / body / anchor point /
closing ritual) is the same skeleton every time, which is exactly what makes 20
missions affordable for one developer.

---

## 3. Story missions

### 3.1 The teaching contract

**AUTHORED.** A mission that teaches nothing and gives nothing is a corridor.
Every entry in the table below must satisfy both halves:

- **TEACHES** — one system the player did not have to understand before, taught
  by a situation rather than a tooltip. The order is dictated by what exists in
  code, not by fiction convenience.
- **GIVES** — a permanent capability, an NPC, a Core Point, an unlock, or an
  archetype first-clear. Never a stat.

**Exactly one mission in the campaign is exempt from the TEACHES half** — the Act
II turn (A2-1). That exemption is deliberate and is defended where it appears.

### 3.2 The teaching order, derived

The order below is derived from what the code actually contains today, so the
campaign teaches things that exist rather than things that are planned.

| # | System | Where it lives in code | Taught at |
|---|---|---|---|
| 1 | Fire, reload, ADS-vs-hip trade, weak points | `Weapons/` | A1-P |
| 2 | The rift skeleton and the closing ritual | `Game-Modes.md` §3.2/§3.5 (unbuilt) | A1-P |
| 3 | Movement verbs: dash, slide, wall ride, wall jump, two jumps | `Movement/` | A1-2 |
| 4 | The second weapon slot and swap tempo | `UBreakerWeaponComponent` | A1-2 |
| 5 | Loot, item level, affixes, the one-additive-bucket rule | `Items/` | A1-3 |
| 6 | The Forge and respec | `RespecAtForge` | A1-3 |
| 7 | Class resource loop, 2 abilities + 1 ultimate | `Classes/`, `Abilities/` | A1-1 → A1-4 |
| 8 | Skill trees; two separate point currencies | `Progression/` | A1-4 |
| 9 | Ranged enemies that punish holding a lane | `ABreakerRangedEnemy` (LATTICE) | A1-5 |
| 10 | The passive dodge/block layer | `UBreakerCombatComponent` | A1-6 |
| 11 | Status and DoT; snapshotting | `UBreakerStatusComponent` | A1-7 |
| 12 | Elites: rank, modifiers, reading a modifier before contact | `BreakerMonsterChassis`, Encounter §1 | A1-8 |
| 13 | Facing-dependent armour; position as a damage stat | Warden (designed, unbuilt) | A2-5 |
| 14 | Priority targets | Silence objective | A2-7 |
| 15 | The endgame chase: Aberrant limits, Anomalous, T-1 | `Items/` | A2-10, A3-4 |
| 16 | Frontier access and tiering | `Game-Modes.md` §4 | A3-8 |

**Note the ordering choice at #9 and #10.** Ranged enemies come *before* the
passive defensive layer is taught. That is deliberate: LATTICE punishes holding a
straight line, and the correct answer to it is movement, not defence. Teaching
dodge/block first would invite the player to read the defensive layer as
something they operate, which is the single most likely misunderstanding in the
game (O1; Art §3.5 calls it "the single most likely art mistake in the project").
The player must have already learned that the answer is *where you are* before
they are told there is a layer that sometimes saves them.

### 3.3 The mission list

**AUTHORED**, except where a row cites `XP §7` — those Core Point grants are CANON
per O7 and are consumed, not authored. All levels and area levels are
`O2 PLACEHOLDER`.

| ID | Mission | Lvl | AL | What happens | Teaches | Gives |
|---|---|---|---|---|---|---|
| **A1-P** | **Spill in Sector Four** | 1→3 | 1 | A rift opens on the perimeter. The player is handed a rifle and a line to hold, then sent in. One rift, three beats, closed. | Fire, reload, weak points, sprint/jump. The rift skeleton. **The closing ritual, in full, unskippable, once.** | Core Point #1 (XP §7 #1). A weapon. The 3,000 XP prologue bolus (XP §2). |
| **A1-1** | **Induction** | 3 | — | Return to the camp. Command signs the player in. The induction range: five loaner kits, thirty seconds each, three dummies. Then the choice, and it is permanent. | Class resource loop; two abilities and one ultimate; the shape of a class. | **The permanent class** (see §4). Three banked Class Points, spent on the spot. |
| **A1-2** | **Ground Under Foot** | 3→5 | 4 | A pack keeps regrouping in the ruin field past the pad. Skitters only. The route matters more than the fight. | Dash, slide, wall ride, wall jump, the second jump. The traversal segment as a pacing valve. | The **second weapon slot** and the swap tempo. The Quartermaster. |
| **A1-3** | **The Forge Is Cold** | 5→6 | 5 | Kess. The first gear worth equipping drops on the way in, and she explains what to do with it — and what she will and will not answer. | Loot, item level, affixes, the additive bucket. Respec. | Core Point #2 (XP §7 #2). Forge access. Kess's refusal thread opens. |
| **A1-4** | **What You Spend** | 6→7 | 6 | A short rift with a wide-open second room, run twice with two different node loadouts because Command wants a comparison. | Skill trees. **Two currencies, deliberately not interchangeable.** | Archetype first-clear: **Clear**. |
| **A1-5** | **Lanes** | 7→9 | 8 | A rift built around a covered ground route. Lattices hold it. The obvious path is the one that gets you hit. | Ranged enemies. Holding a straight sprint line is punished; any direction change beats the lead. | Archetype first-clear: **Sever**. Core Point band (XP §7 #3). |
| **A1-6** | **Nothing You Can Press** | 9→10 | 9 | Density. Enough incoming that the dodge and block popups fire repeatedly and visibly. Command's briefing describes it as luck, because it is. | **The passive defensive layer.** No input, no window, no timing. | Archetype first-clear: **Hold**. |
| **A1-7** | **Slow Work** | 10→12 | 11 | An anchor point with far too much health to burst. Bleed and Poison are the answer and the numbers keep ticking after the player disengages. | Status, DoT, snapshotting; DoTs crit; DoTs ignore the defensive layer. | Archetype first-clear: **Hunt**. |
| **A1-8** | **The Marked** | 12→13 | 12 | Three Veterans, one modifier each, in three rooms — one per pressure kind, so each is read cleanly before they ever combine. | Rank and modifiers. **Reading a modifier from 20m before contact.** | Archetype first-clear: **Escort**. |
| **A1-B** | **ACT I BOSS — The Holdfast** | 14→15 | 15 | The largest Vestige mass yet seen; it is not holding the rift open so much as it *is* the thing the rift is attached to. No orders, no tactics, no author. | That a boss is an arena problem, not a health bar. | **Core Point #4** (XP §7 #4). **Rior fragment #1** → Core Point #5 (XP §7 #5). Act II gate opens. |
| **A2-0** | **The Breach** | 16 | 16 | The first stable large rift. It does not close. It has weather. | Rift tiering; that some rifts are infrastructure. | **Core Point #6** (XP §7 #6). Breach access. |
| **A2-1** | **Field Repair** | 17→18 | 17 | **THE TURN.** Something comes through wearing a uniform. Not the player's uniform, but close enough to read. Rank markings. A field repair on the shoulder. Badly wounded, ordinary, not a boss. Command does not have an explanation ready. | **Nothing. The only exemption in the campaign.** | **The Researcher.** The fragment reconstruction system. |
| **A2-2** | **What She Won't Rebuild** | 18→19 | 18 | Vosk reconstructs fragment #1 and stops partway through the second thing in the box without saying why. | The fragment system: recovery → reconstruction → capability. | A fragment capability (deeper rift access). |
| **A2-3** | **Above the Line** | 19→21 | 20 | A Sever rift at scale — tethers at height, conventional route 40% slower and openly available. | Vertical routes as an optimisation, never a gate. | Archetype first-clear: **Carry**. *(Candidate home for Swift's third jump unlock — see §7.)* |
| **A2-4** | **Someone Else's Tempo** | 21→22 | 21 | Escort a suppression drone at its pace, then Carry charges with the Secondary slot disabled. | Fighting at a tempo you do not set. The swap system as a cost. | Archetype first-clear: **Collapse**. |
| **A2-5** | **What Is Left** | 22→23 | 22 | The first Severed Warden. Frontally armoured, rear unarmoured. Trading with it head-on loses; circling it wins by a factor of three. | **Facing-dependent armour. Position is a damage stat.** | **Rior fragment #2** → Core Point #8 (XP §7 #8). |
| **A2-6** | **Intake** | 24→25 | 24 | First contact with the Order. Not preachers — a loading dock, a manifest, and a gap in the suppression field. | That there is a second reading of the standing order. | An NPC standing axis (see §6.4). |
| **A2-7** | **Silence** | 25→26 | 25 | A broadcasting Vestige buffs everything in the room continuously. Killing it first is the whole fight. | Priority targeting — the exact skill the Frontier boss will demand. | Archetype first-clear: **Silence**. Set complete → Core Point #7 (XP §7 #7). |
| **A2-8** | **The Field Marshal** | 26→27 | 26 | The Altered commander. The first humanoid that demonstrably gives orders. Every phase mechanic is an order given to adds. | That the rifts are **directed**. | **Core Point #9** (XP §7 #9). The campaign's first T-1 source. |
| **A2-B** | **ACT II BOSS — The Gardener** | 29→30 | 30 | Deep inside the Breach: an Effigy caretaker that was never captured, still doing its original job on an Earth that has no people left to inherit it. It fights because the player is damaging what it tends. Killing it collapses the Breach — and the player watches a whole inhabited timeline unload at horizon scale. Nobody says anything. | That something on the far side was **built**, not born. | **Core Point #10** (XP §7 #10) — lands on Class Point exhaustion. Act III gate. |
| **A3-0** | **Threshold** | 31→32 | 31 | First step onto an erased Earth. Same continents, same gravity, unrecognisable outcome. | The semi-open zone loop: discovery, landmarks, traversal challenges, world events. | Fast travel. Discovery XP opens. |
| **A3-1** | **EARTH 1 — the one that never industrialised** | 32→36 | 32→36 | An Earth that never built anything, and was taken by Vestiges anyway. It undercuts the simple reading of Rior's argument, which is why it is first. | Open-zone play — Frontier-shaped, without the modifier stack. | **Core Point #11** (XP §7 #11). |
| **A3-2** | **The Survivor** | 33→34 | 33 | The first friendly face inside an erased Earth. Lucid, because the player found them in time. They are fine with the Altered the player has killed, which is worse than an accusation. | Nothing new. **Introduces the clock.** | The Survivor as a persistent, degrading NPC. |
| **A3-3** | **EARTH 2 — the one that solved everything** | 37→41 | 37→41 | An Earth that solved everything and was strip-mined by other timelines using his technology. This is the one that proves him right. | Nothing new — by design. Act III's job is argument, not tuition. | **Core Point #12** (XP §7 #12). |
| **A3-4** | **The Box She Closed** | 42 | 42 | Fragment #3. Vosk reconstructs it and refuses the rest, still without saying why. | The top of the gear chase: Aberrant equip limits, Anomalous, the T-1 path. | **Core Point #13** (XP §7 #13). A fragment capability. |
| **A3-5** | **Bring Them In** | 44 | 44 | Get the Survivor inside a functioning Anchor before the clock runs out. Command signs the exception personally and says nothing about it afterwards. | Nothing new. | **Core Point #14** (XP §7 #14). Proof that severance is survivable. |
| **A3-6** | **EARTH 3 — the Earth where Rior lost** | 45→48 | 45→48 | A world that won. The player meets a version of themselves — not evidence of them, the actual person, alive. Somewhere in this zone, once, briefly, and not as a threat, **Rior notices the player**. | Nothing new. | **Core Point #15** (XP §7 #15). |
| **A3-F** | **The Edge** | 49→50 | 50 | The assembled technology presents one decision. **SEAL** — close every rift permanently; this timeline is safe forever and every timeline behind them never existed, including every refugee not yet inside an Anchor. **HOLD** — keep the doors open; the refugees can still be reached, and Rior's method remains intact and usable by the people who misused it the first time. | Nothing. The player assembled the argument themselves twenty hours ago. | The epilogue. The XP bar is removed and replaced by the build-completion readout. **Frontier access.** |

**26 entries: 10 in Act I, 10 in Act II, 6 in Act III.** The distribution is
deliberately front-loaded and Act III is deliberately thin, which is a problem
this document does not solve — see §7 risk 2.

### 3.4 Six mission-level rules that are not negotiable

**AUTHORED**, each derived from a locked constraint.

1. **No Altered asset appears anywhere before A2-1** — not in optional content,
   not in Anchor lore, not in a prop (Master §8.2; Encounter §0; Art §2.2
   acceptance criteria require verifying this by asset reference search, not by
   memory). Everything before the turn is Vestige. This is why the Act I boss is
   a Vestige mass and not a commander.
2. **Every mission is completable with base kit only** — walk, sprint, jump,
   crouch, dash, slide, wall ride, wall jump, and a weapon. No mission may
   require parry (tree-granted), Swift's third jump (class-innate), or any
   ability. Conventional routes exist everywhere and are never more than 40%
   slower.
3. **No mission requires a timed defensive input.** Dodge and block are passive
   (O1). This is the single easiest constraint in the corpus to violate by
   accident and the violation will not show up in a test.
4. **No mission is missable and no Core Point can be lost.** A permanently missed
   Core Point on a character with a permanent class is unrecoverable and
   unacceptable (XP §7 rule 3).
5. **No mission requires party content** (XP §7 rule 4; solo is the balance
   target).
6. **The closing ritual is identical at level 3 and level 50** and never gets
   flashier with tier. It is not a reward. It is skippable after the first three
   clears, and the skip prompt is deliberately small — because players who skip
   it will re-watch it once, deliberately, after Act III.

### 3.5 The three beats the whole campaign is built to deliver

**AUTHORED** framing over **TRANSCRIBED** beats.

- **A2-1, the turn.** The reason it is exempt from the teaching contract is that
  its content *is* the absence of content: an ordinary wounded person in a
  near-miss uniform, no fight, no reward, and a Command officer with nothing
  prepared. Giving it a mechanic would give the player something to do instead of
  looking. It should run under two minutes and it should be the most carefully
  authored asset in the project (Art §2.2 — more detail budget than the boss).
- **A2-B, erasure at scale.** The player has closed perhaps sixty rifts by now
  without being told what closure does. The Breach is the first one large enough
  and inhabited enough that the unloading horizon is legible as a *place ending*.
  Still nobody speaks. The Act III+ addition to the ritual — a single silhouette
  in the unloading geometry, ~2% of the time, never acknowledged, no codex entry
  (`Game-Modes.md` §3.5) — begins after this.
- **A3-F, the choice.** Neither option is safe. SEAL is not "abandon them", it is
  "unmake them". HOLD is the merciful choice with an unacceptable risk attached.
  Implementation is narrative epilogue only: it changes Anchor dialogue, NPC
  standing and epilogue text, and gates no content — because a choice that
  removes endgame content punishes the player for the reading of the story they
  arrived at.

**And the thing nobody in the game ever says** (Master §1.6): Rior erases
timelines to protect one world. So does the militia. He is simply the only one
who says so. No NPC states this. No codex entry states this. If it is stated, it
stops being the player's.

---

## 4. The class-choice moment

### 4.1 The problem, stated precisely

**Locked:** class selection is permanent per character. **Current state:** it
happens on the BREAKER CLASS menu screen before the player has fired a shot, and
the only escape is a DEV MODE checkbox that is not shipping behaviour.

That is a permanent, irreversible decision made with zero information, and it is
the worst-structured moment in the game. Three separate constraints press on it:

- `Class-Kits.md` §0: *"class identity must be legible in the first hour."* So it
  cannot be deferred deep into Act I.
- One Class Point is granted per level from 1 to 30. Deferring the choice banks
  points; it does not destroy them, but it does mean the player spends the
  deferred window with no class kit at all.
- Each class starts with two abilities free at level 1 (`Class-Kits.md` §0.2), so
  a classless player is a player with an empty ability bar.
- O4 and the 40-hour target exist partly *because* the class is permanent — the
  campaign is short enough that a second character is plausible. That is a
  mitigation, not a fix.

### 4.2 RECOMMENDATION — the choice sits at the end of the prologue, at the first camp return, at roughly level 3

**AUTHORED.** Mission **A1-1, Induction**, immediately after the prologue rift
and before the first field mission.

**What the player must have experienced first — the minimum, and it is small:**

1. Fired a weapon, reloaded, hit a weak point, and killed something.
2. Moved: sprinted, jumped twice, and been made to move sideways by a Skitter
   lunge that cannot track.
3. Entered a rift, cleared it, killed the thing holding it open, and **performed
   the closing ritual once, in full**.
4. Walked back into a camp that is still standing and met the two people in it.

That is six to eight minutes of play and it is enough. The player now knows what
the *game* is. They do not yet know what a Momentum loop is, and no amount of
prologue would teach them that.

**What the Induction adds, and this is the actual recommendation:** an **induction
range** inside the camp — three dummy targets and five loaner kits. The player
can hold each class's two starter abilities for as long as they like, in a
non-combat space, before choosing. This is the difference between a blind
permanent decision and an informed one, and it is close to free: the gym already
has recycling diagnostic targets, `DevForceClass` already swaps a class kit at
runtime, and the two starter abilities per class already resolve through
`UBreakerAbilityDefinition::DefaultAbilityIdForSlot`. The shipping work is a
scoped, non-permanent version of a mechanism that exists for debugging.

**Then the lock is explicit.** The confirmation uses the word *permanent*, states
that respec at the Forge moves points and never class, and requires a second
input. A permanent decision deserves a confirmation that reads like one.

**And the reward for waiting:** the player arrives at the choice holding **three
banked Class Points** and spends all three the moment they choose. The first
class moment is three purchases at once instead of an empty tree, which is a
better first five minutes of a class than level 1 has ever been.

### 4.3 The options that were rejected, and why

| Placement | Cost |
|---|---|
| **Menu, pre-play** (current) | Zero information. The permanence is the game's most punishing rule and it is applied before the player can spell any of the five class names. Ships today; should not ship at launch. |
| **Level 1, in-fiction, first thirty seconds** | Same information problem wearing a costume. Moving a bad decision into the world does not improve it. |
| **~Level 5, after the Forge (A1-3)** | Defensible. The player would additionally understand gear and respec, which is genuinely relevant. Cost: two more missions played with no ability bar and no resource loop, in a game whose combat identity lives in those. Rejected as a near-miss, not a bad idea — if the induction range proves too expensive, this is the fallback. |
| **Level 15, at the act gate** | Rejected outright. Fifteen levels with no class identity contradicts `Class-Kits.md` §0 directly, banks half the Class Point budget, and makes Act I a different game from the rest. |

### 4.4 What this does not solve

The choice is still permanent and the player still makes it having played one
class for zero minutes and touched four others for thirty seconds each. The
induction range narrows the gap; it does not close it. The real mitigations are
elsewhere and are already ruled: the 40-hour campaign, Veteran's Path cutting a
second character to ~26 hours (XP §6), and O17's account-wide stash meaning a
second character is a second *build* rather than a fresh grind. The campaign's
job is to make sure the player knows those exist — **A1-1's dialogue should say,
plainly, that a Breaker is inducted once and that people do run more than one.**

---

## 5. Handing off to the endgame

### 5.1 What the campaign owes the endgame

**AUTHORED.** The campaign is the tutorial for the endgame, not the product
(XP §3). At the moment the player finishes A3-F, the campaign has succeeded if
and only if all six of these are true:

1. **Character level 50, hard stop.** XP bar removed from the HUD, no overflow,
   no hidden accumulation, no paragon substitute. The screen space becomes the
   build-completion readout: Aberrant (n/3), Anomalous (n/1), count of T0+
   affixes equipped (XP §4).
2. **Identity complete, and it has been for twenty-five levels.** O4's testable
   criterion is one Core keystone, one class keystone and one equipped Aberrant
   **by level 25** (XP §10.1). If a player cannot name their build at 25, the
   campaign failed, not the endgame.
3. **All 65 Core Points obtainable**, 50 from levels plus the 15 world points,
   none endgame-gated, none missable (O7).
4. **The loop is muscle memory.** Threshold, body, anchor point, closing ritual —
   performed several hundred times. The Frontier is the same loop with the
   objective list removed and the risk decision added.
5. **Frontier access in hand**, and the player understands tokens and tiering
   because A3-8-equivalent content taught them inside the campaign rather than
   after it.
6. **The ritual has been recontextualised.** This is the campaign's only
   non-mechanical deliverable and it is the reason the ritual is authored the way
   it is. A player who re-watches it once, deliberately, after Act III, is the
   design working.

### 5.2 The narrative handoff, and the problem in it

**AUTHORED — flagging a collision nobody has recorded.**

Master §8.6 says the final choice is *narrative epilogue only* and *does not gate
content*. Master §8.3 and O8 say the endgame is **Rior's Frontier** — "timelines
Rior is currently working on". Put those together and there is a contradiction
that will be visible to every player on their first post-campaign run:

> **SEAL says every rift is closed permanently. The player then opens one and
> runs a Frontier.**

This is a real fiction failure and it needs a ruling. Options and costs are in
§8. The **recommended** resolution is the cheapest and the most consistent with
what is already ruled: Master §1.6 states that erasure is *bounded* and that
Rior's method is *surgical, not clean*. Extend the same discipline to SEAL —
sealing this timeline's doors closes what reaches *here*, and does not reach the
timelines already in progress on the far side. The Frontier remains exactly what
it was: the war that did not end because the door on this side shut. HOLD needs
no reconciliation at all.

### 5.3 The mechanical handoff — written to survive the open ruling

**This section is deliberately written so that it stays true whichever way the
owner rules the endgame item-level clamp.**

The open problem, **TRANSCRIBED** from `Power-Curve.md`'s closing section
("OPEN: the endgame item-level clamp"): across the levelling game the two curves
compose exactly — baseline TTK is identical at area level 1, 10, 25 and 50. Past
the character cap it breaks. `GetDropItemLevel` clamps to 50 because affix tiers
are only authored that far, while the monster chassis climbs to area level 100.
Measured, baseline TTK is **1.00× at area level 50 and 74× at area level 100**,
and nothing in the game answers that. The build variance band is 8.7×, which is
the *price of entry* at 50, not progression past it. Five options are tabulated
at the end of `Power-Curve.md`; two collide with the locked "no post-cap
character power" rule and would need an O-ledger amendment.

**The campaign-side contract, stated so it does not depend on that ruling:**

> The campaign's last authored area level is **50**. Everything above 50 is
> endgame content and is owned by `Game-Modes.md` and `Power-Curve.md`. The
> campaign delivers a level-50 character with an identity-complete build, all 65
> Core Points reachable, and Frontier access, into content authored at area
> level 50. **That handoff is the same under every option on the table.**

Why it is genuinely ruling-independent: every one of the five options changes
what happens *above* area level 50 — extending the tier table, adding a second
item-level track, adding rarities above Anomalous, adding a post-cap multiplier,
or capping area level at 50. None of them changes what a level-50 character
looks like at area level 50, and that is the only thing the campaign produces.

**The one exception, named so it is not a surprise.** If the owner rules **"cap
area level at 50"**, the endgame tier ladder disappears and with it the reason to
climb. The campaign is not broken by that ruling, but its *last act acquires a
new job*: the three erased Earths must become genuinely re-runnable with real
variance, because they would become a much larger share of what a capped player
does. That is a content-volume decision, not a campaign-structure one, and it is
recorded here so it is costed at the same time the ruling is made rather than
discovered afterwards.

### 5.4 The last level

**AUTHORED.** XP §4 requires 49→50 to be the largest single level in the game and
says explicitly not to soften it — it is the only place in the design where a
wall is the correct feeling. That collides with putting a story gate at 50.

Resolution: **A3-F unlocks at 49, not 50, and the player crosses 50 during it.**
The final mission's own payouts carry the last level. The XP bar therefore
disappears inside the story climax, at the same moment the build-completion
readout replaces it and the game hands the player to gear. The wall is still
there and it is still the largest level; it is simply the last thing the campaign
is doing rather than a gate in front of it.

---

## 6. Quest and flag architecture

### 6.1 What exists today — read from the code, not assumed

**TRANSCRIBED** from `Source/RiorsEdge/Interaction/BreakerNPC.{h,cpp}`,
`Source/RiorsEdge/Save/BreakerSaveGame.h`,
`Source/RiorsEdge/Characters/BreakerCharacter.{h,cpp}`, and
`Source/RiorsEdge/UI/BreakerMenu.cpp`.

**The dialogue system.** `ABreakerNPC` is an `AActor` (not a pawn) carrying a flat
list of `FBreakerDialogueNode`, each with an `FName NodeId`, an `FString
SpeakerLine`, and an array of `FBreakerDialogueChoice`. A choice carries `FString
Text`, `FName NextNodeId` (`NAME_None` ends the conversation), and `FName
SetsQuestFlag`. `StartNodeId` defaults to `TEXT("Start")`. `ValidateDialogue`
checks that the start node resolves, that every node has at least one choice, and
that every non-none link resolves. Content is **hardcoded C++** — two static
spawners, `SpawnForgeKeeper` and `SpawnQuartermaster`, with a comment stating the
intent that it *"can later move into Data Assets without changing the runtime."*

**The flag system.** A flag is an `FName`. Runtime storage is
`TArray<FName> QuestFlags` — a private, non-`UPROPERTY`, non-replicated member on
`ABreakerCharacter`. The full API is three functions: `AddQuestFlag` (whose entire
body is `if (Flag != NAME_None) QuestFlags.AddUnique(Flag);`), `HasQuestFlag`, and
`GetQuestFlags`. Persistence is `TArray<FName> QuestFlags` on `UBreakerSaveGame`
(slot `BreakerSave0`, user index 0), copied wholesale in both directions.

**The save.** `UBreakerSaveGame` holds `FBreakerProgressionState Progression`,
`EquippedItems`, `BackpackItems`, two weapon-slot archetypes, `QuestFlags`, and
`int32 SaveVersion = 1`.

**Five flag literals exist in the shipped game**, all set from dialogue and none
read anywhere: `Quest.MetForgeKeeper`, `Quest.AskedKessAboutRior`,
`Quest.CheckedVendor`, `Quest.AcceptedFirstContract`, plus `Quest.Test` in
automation.

### 6.2 The honest summary

**The flag system is write-only.** `HasQuestFlag` has zero callers in the entire
codebase. `AddQuestFlag` has exactly one, in the dialogue screen's choice lambda.
Nothing in the game reads a flag: not dialogue, not NPC spawning, not enemy
spawning, not loot, not the HUD.

The most legible symptom is already shipped. The Quartermaster's `Job` node hands
out a real contract — *"The spill out past the pad keeps regrouping. Thin it out,
and put that elite down while you're at it. I count what comes back — that's the
job."* — and accepting it sets `Quest.AcceptedFirstContract`. The encounter it
describes exists. **Nothing tracks kills against it, nothing reports back, there
is no turn-in node and no reward.** That is the entire campaign layer, in
miniature: the fiction is there, the objective is there, and there is no arc
between them.

This is not a criticism of the code. The header says exactly what it is —
*"groundwork for vendors and quest states"* — and it is good groundwork. It is
simply not yet a system a campaign can hang off.

### 6.3 What the campaign needs from it

**AUTHORED.** Ordered by how much each one blocks. Nothing below is a parallel
system: every item extends the structures named in §6.1.

| # | Need | Shape | Blocks |
|---|---|---|---|
| **1** | **Flags must be readable as gates.** | `HasQuestFlag` already exists and is correct. What is missing is callers and, above all, **condition fields on dialogue**: `TArray<FName> RequiredFlags` and `TArray<FName> BlockedByFlags` on `FBreakerDialogueChoice` and on `FBreakerDialogueNode`, filtered where `BuildDialogueScreen` currently iterates `Node.Choices` unconditionally. | Everything. Without this there is no campaign state at all. |
| **2** | **Per-NPC entry state.** | `ShowDialogue` always sets `DialogueNodeId = NPC->GetStartNodeId()`, so Kess greets the player identically forever. Needs an ordered list of `(RequiredFlags → StartNodeId)` overrides evaluated first-match on `ABreakerNPC`. | Every returning-NPC beat. Kess's refusal thread cannot progress without it. |
| **3** | **A quest object.** | There is none — "quest" is a flag-name prefix. Needs an id, an ordered objective list, a state (`NotOffered / Offered / Active / ReadyToTurnIn / Complete`), and a reward hook. **Objective state can be expressed entirely as flags**, so this is a layer over §6.1, not a replacement for it. | The mission list in §3. All of it. |
| **4** | **Non-dialogue flag sources.** | Today the only writer is a dialogue button. The campaign needs kills, rift completion, archetype first-clears, zone entry, item pickup, and area discovery to set flags. Every one of these already has an event or a delegate; none of them writes a flag. | Every objective that is not "talk to someone". |
| **5** | **Save on flag change.** | `SaveGameState()` is called from `EndPlay` and from menu commit points — **not** when a flag is set. A flag earned in conversation reaches disk only if the session later ends cleanly. A crash after the Act II turn loses the Act II turn. | Trust. This is a data-loss bug wearing a design gap's clothing. |
| **6** | **A flag registry.** | Flags are free-form `FName` literals typed at the call site. They look like GameplayTags (`Quest.MetForgeKeeper`) and are not — they are not registered in `BreakerAbilityTags.h` and cannot be matched with tag queries. A typo is silent and permanent. Either register them as real GameplayTags or add a validated central list. | Authoring 26 missions' worth of flags without a silent typo. |
| **7** | **`SaveVersion` must be read.** | It is declared in three structs (`UBreakerSaveGame`, `FBreakerProgressionState`, `FBreakerItemInstance`) and **read by nothing**. There is no migration branch anywhere. The campaign will add fields to the save repeatedly; the first time it does, every existing save either breaks or silently loses state. | Every subsequent change to the save format. Cheapest item on this list, and it gets more expensive every week. |
| **8** | **Objective presentation.** | There is no quest log, no objective tracker, and no waypoint. `UI-UX-Spec.md` contains no quest UI at all. The HUD's only interaction affordance is `F  TALK — <NAME>`. | The player knowing what they are doing. Owned by the UI docs, recorded here as a dependency. |
| **9** | **Per-character vs account scope.** | `Save-Architecture.md` §2.1/§2.2 already rules the split: fragment/story-collectible unlocks and NPC standing and the epilogue flag are **account-wide**; campaign act/quest progress is **per-character**. Today there is one flat `TArray<FName>` on one character in one slot, so the split does not exist. | Alts. A second character re-earning fragment capabilities is pure repetition with no build expression. |

### 6.4 Three properties the flag design should keep

**AUTHORED.**

- **Presence-only, monotonic, never removed.** `AddUnique` with no remove is a
  good default and the campaign should not need more. A flag that can un-fire is
  a state machine hiding in a set. Where a counter is genuinely needed (kills
  against an objective), it belongs on the quest object from §6.3 #3, not in the
  flag array.
- **Namespaced by act and mission id.** `Campaign.A2-1.Turn.Seen`, not
  `SawTheAltered`. Twenty-six missions produce a hundred-plus flags and the
  namespace is the only thing that will keep them legible.
- **NPC standing is a flag axis, not a number.** Save-Architecture already lists
  NPC standing as account-wide state; the Order thread (A2-6) and the final
  choice both want it. A small set of named flags is enough and it avoids
  inventing a reputation system the game does not otherwise need.

---

## 7. Risks

1. **The teaching order assumes systems that are designed and not built.**
   Facing-dependent armour (A2-5) does not exist in the damage pipeline; the
   Warden, the Field Marshal, the rift skeleton and the closing ritual are all
   design-only. The mission list is written against the designed corpus, which is
   the correct target, but nothing in §3 should be read as a schedule.
2. **Act III is thin and this document makes that visible rather than solving
   it.** Act III is 53% of the campaign's hours and gets 6 of 26 missions. That is
   the same problem Master §8.8 and XP §11 risk 2 already record as known and
   accepted; putting it in a table does not fix it. Either the three Earths are
   much larger than currently implied, or repeatable rift content explicitly
   carries ~40% of Act III's XP. That is a content-plan decision.
3. **Twenty-six missions is a lot for one developer.** The mitigation is
   structural and it is already in the design: every mission is the same rift
   skeleton with a different area level, objective pair, and beat. If any mission
   starts wanting bespoke geometry, it should be cut or merged rather than built.
4. **The Effigy antagonist (A2-B) is the most speculative thing in this
   document.** It is a genuinely new commitment: it makes Rior's caretakers a
   combatant faction and it pulls Kess's thread taut early. It is flagged in §8
   and it can be deleted without disturbing anything else in the list.
5. **Swift's third jump has no home in the campaign yet.** O25 rules it
   class-innate and the mechanism is built with an `O2 PLACEHOLDER` threshold of
   character level 20, still an open ruling. A2-3 (Above the Line, levels 19–21,
   the vertical-route mission) is the obvious campaign-side home and it is the
   only campaign input to that ruling this document has. Recorded, not decided.
6. **The class-choice recommendation adds a shipping feature** (the induction
   range) to solve a design problem. If it is not built, the choice must not
   quietly revert to the pre-play menu screen — the fallback is §4.3's level-5
   placement, which costs nothing to build.

---

## 8. OPEN QUESTIONS

Ranked by how much each one blocks. Nothing below is decided in this pass.

### 1. Does the SEAL ending contradict the existence of the endgame? — BLOCKS the ending and the endgame framing together

SEAL closes every rift permanently. The player then runs Frontiers, which are
rifts. Master §8.6 says the choice gates no content, which is correct as a
product decision and produces this collision as a side effect. Nobody has
recorded it.

| Option | Shape | Cost |
|---|---|---|
| **Bounded seal** *(recommended)* | SEAL closes what reaches *here*; the timelines already in progress on the far side are untouched. Consistent with Master §1.6's "surgical, not clean". | Slightly weakens SEAL, which is supposed to be terrible. One line of epilogue text. |
| Endgame is diegetically pre-choice | The Frontier loop is the war you are still fighting; the epilogue is the last thing you do and is replayable. | The ending stops being final. |
| Choice is stated, not executed | The player declares intent; execution is deferred. | Reads as a non-ending. Players will say so. |
| Frontier is a record, not a place | Reframe the endgame as replayed history. | Kills the stakes of the whole endgame. Rejected here, listed for completeness. |

### 2. Where does the permanent class choice sit? — BLOCKS the first ten minutes and the shipping class screen

§4 recommends the end of the prologue at ~level 3, with a five-kit induction
range. Alternatives and costs are tabulated in §4.3. Sub-question the owner must
answer either way: **is the induction range built, or does the choice sit at
level 5 after the Forge with no trial?** The range is the recommendation; the
level-5 fallback is free.

### 3. Is the Act II boss an Effigy caretaker? — BLOCKS the Act II climax and Kess's thread

**AUTHORED and speculative.** "The Gardener" makes Rior's caretakers a combatant
presence, connects Kess to the plot mechanically rather than only through a
withheld answer, and turns the overgrown-Earth dressing into evidence. It also
commits an art asset that does not exist and makes the Effigy question ("do they
have legal personhood inside an Anchor", Master §1.8) load-bearing earlier than
planned. Alternative: the Act II boss is a second, larger Vestige anchor and the
Effigy thread stays with Kess alone. Cheaper, and much less interesting.

### 4. Does Act III get more content, or does the level curve move? — BLOCKS Act III construction

Act III is 20 levels and ~21 hours across three zones and 6 missions. Three
levers: build much larger zones; add a fourth Earth; or rule explicitly that
repeatable rift content carries ~40% of Act III's XP and design the zones around
that. XP §11 risk 2 and Master §8.8 both already flag this; it is now blocking
the mission list rather than the curve.

### 5. What is the quest-object shape, and who builds it? — BLOCKS every mission

§6.3 #3. The recommendation is a thin layer whose objective state is expressed as
flags, so the existing flag array remains the single source of truth and there is
no parallel system. The alternative — a full quest state machine with its own
serialised state — is more capable and duplicates §6.1. Also unanswered: does the
quest object live in C++ with a data-asset content layer (matching every other
content system in the project's zero-setup convention), or purely in data?

### 6. Does Rior ever notice the player, and where? — BLOCKS one scene in Act III

Master §13.6 leaves it open and recommends **yes, once, late in Act III, briefly,
and not as a threat**, on the grounds that if he never does he is scenery. §3.3
places it in EARTH 3 (A3-6) as the recommended reading. Needs a yes.

### 7. Does Kess's withheld answer resolve, and does the player earn it? — BLOCKS the campaign's longest thread

Master §13.8 lists this open. The thread is already live in shipped dialogue
(`Quest.AskedKessAboutRior`), so it is accumulating weight in the build right now
with no authored destination. Options: it resolves in Act III as a reward for
asking repeatedly across the campaign; it resolves unconditionally at a story
beat; or it never resolves. The first is the most expensive and the best.

### 8. Is the campaign act/quest state per-character or account-wide, in detail? — BLOCKS alts

`Save-Architecture.md` §2.1/§2.2 already rules the coarse split. What is not ruled
is the boundary case that matters: fragment capabilities are account-wide, and
fragments gate deeper rift access, so **an alt skips a campaign gate**. That is
either intended (characters are builds, O17) or a hole. Needs an explicit call
before the fragment system is built.

### 9. Does the ~2% silhouette in the post-Act-III closing ritual ship? — BLOCKS nothing, decays if unasked

`Game-Modes.md` §9.12 already asks this. It is deliberately never acknowledged and
has no codex entry, which means it is exactly the kind of thing that gets closed
as a bug in QA. It needs to be wanted on purpose, in writing, before it is built.

### 10. What does a mission look like when the player out-levels it? — BLOCKS replay and completionism

Area level is authored on content and never scales to the player (O27), and the
XP falloff table already handles the reward side. What is undecided is whether a
finished story mission is *replayable at all* — the closing ritual implies the
rift is gone. Recommendation: story rifts are one-time and the repeatable layer
is separate local rifts in the same zone, which keeps the fiction honest. Needs
a call before the zone content plan.
