# Content and modes

## What this system is for

To give the loop somewhere to happen, and to make each place a different
question rather than the same fight in a different room. The atomic loop is:
enter a rift, kill what holds it open, close it. A player performs it from
level one to the end of the game, so everything else is a frame around it.

It fails when a mode is a reward table with geometry attached. If a player
cannot say what a mode asks of them that another does not, the mode is content
volume rather than content, and the endgame becomes a spreadsheet with a
skybox.

## The rules

**Five canon locations.** The **Anchor** is the shared social hub and is not
instanced. **Local Rifts** are the core instanced loop; closing one erases the
timeline behind it. **The Breach** is the mid-campaign gate. **Erased Earths**
are semi-open shared zones. **Rior's Frontier** is the endgame's scaling
frontier.

**Four endgame content types.** **Anomalies** are the primary farm — relatively
open maps built from tilesets, mixed-rarity packs, a map boss at the end.
**Raids** are seven players, checkpoints, puzzles and a final boss. **Dungeons**
are smaller raids for four. **Conquest** is a warzone for up to nine matchmade
players — nine is the matchmaking ceiling, not a requirement, and the mode must
play at fewer.

**Raids are puzzles rewarded for team play, and no encounter may have a build
that cannot participate.** Builds may excel in some situations and be weak in
others; none may be locked out.

**Roughly twenty percent of first-playthrough hours are one-time, eighty
repeatable.** Every one-time piece hands over a permanent capability or a
currency, never a stat. Every repeatable piece runs in about ten minutes solo,
or it will not survive a hundred repetitions.

**Group content never gates progression points.** A solo player reaches the
full budget.

**The campaign is post-slice.** The slice proves the loop, not the story.

**Elites are modifier-driven, not stat-driven**, and every modifier passes
three tests: it is **readable in graybox** with zero art; it is **answerable by
base-kit movement**; and it is **not a stat**. A flat damage increase is the
chassis, not a modifier.

**Modifiers combine, but not freely.** Forbidden pairs are enumerated and never
generate. A two- or three-modifier elite draws from at least two different
pressure kinds, and never two from durability — that is how sponges are born.

**Density caps win over the budget curve.** Where the two disagree, the solver
reports the shortfall rather than silently picking a side.

**Party scaling is count-first**: substantially more enemies against slightly
more health, with a role-pressure ladder so each additional player adds a
*kind* of problem rather than a quantity. **Spawn pressure pauses during a
revive** — without that rule, reviving is strictly incorrect play in a game
where standing still loses.

**Content is never harder than the player can have prepared for.** A campaign
rift's enemy level is clamped to a small margin above the player's, so a tier
system and an enemy-level curve cannot compose into a difficulty cliff hidden
inside two innocuous rules.

**The death budget favours solo.** Solo carries a small allowance; a party's
scales with its size rather than being flat, because solo is the primary
balance target and it is cheaper to start generous and tighten.

**Procedural assembly requires a movement contract per tile.** Every module
publishes at minimum one mantle or vault line, one sliding descent, and
one route requiring neither. A tile that fails is rejected at cook time. In a
procedural system a movement pillar is an authoring requirement, not a level
designer's instinct.

**The gym is non-canon.** It sits nowhere in the location canon, binds no
continuity rule, and nothing that spawns there constrains the campaign.

## The model

### The spatial grammar

Every distance in the field is derived from the movement kit rather than
chosen. Two rows carry the rest.

**The dash refresh distance — sprint speed times dash cooldown — is the
load-bearing number.** A space whose longest axis is shorter than it can be
crossed on foot in less time than the dash takes to return, which makes the
dash *structurally incapable* of being a traversal choice there. It can only be
a dodge. That is a legitimate position for an arena and a bug for a field.

**Corridor width comes from peripheral flow.** A surface at lateral distance
*d* passing at speed *v* sweeps the eye at *v/d*. Sustained flow above roughly
2.2 radians per second smears: the wall stops being a surface and becomes a
strobe, and the read is "I am moving too fast". The same walk speed reads as
three different speeds in three different corridor widths, with nothing about
the character changed.

The rest of the grammar derives the same way: one-, two- and three-jump gaps
from the jump arc; mantle height from the movement window; cover pitch from
enemy wind-up plus closing time; and sightline depth from the ranged enemy's
engagement band.

### Enemy modifiers

Ten, in three weight classes, drawn without replacement. Each has a graybox
tell and a positional answer:

| Pressure kind | Shape |
|---|---|
| **Durability** | Shields, damage reduction auras, damage reflection |
| **Space denial** | Persistent hazards, death detonations, zone projection |
| **Mobility** | Speed and repositioning that closes distance thought safe |
| **Attrition** | Splitting, and immunity to being moved |

A rare modifier appears on a small minority of elites overall. Weight classes
are internal and never surface in the UI using rarity words — those belong to
items.

### The Anchor

**The one image the space has to sell:** a market street, mid-afternoon,
ordinary commerce, and a two-hundred-metre suppression pylon in frame that
nobody in the shot is looking at. A screenshot without both is framed wrong.

Lived-in rather than ruined. Density falls off with radius. The pylon is
visible from every exterior. No signage explaining the setting. Warmth inside,
cold outside. Verticality is functional.

**The loop is ordered by what the player does before and after each stop:**
arrive at the boundary, stash, Forge, quartermaster, command post, gate. The
player arrives full, so the stash is first or they carry loot the whole way and
come back. The Forge is a room rather than a counter because it is the one
place they spend real minutes. **The quartermaster signs out ability unlocks** —
one token a piece, never the crafting currency — and sits between the Forge and
the command post because deciding what a character can do comes before deciding
what to do with it. Command is by the gate because choosing what to run before
choosing the build to run it with is backwards.

**Forge and vendor are Anchor interactions and never appear in a pause menu.**
Respec being Forge-gated is a product decision, and a respec button in a menu
silently undoes it. Inventory, equipment and the trees are full-screen modals
reachable anywhere — they are not Anchor services.

An Anchor is the only ground where severance stops. The machine that keeps the
player alive is the machine that would keep an Altered lucid, and the militia's
standing order is to shoot Altered on sight. That contradiction is what the
building is, and the space should stage it physically rather than explain it.

## Boundaries

This spec owns places, modes, encounter composition and the spatial grammar.
It does not own:

- enemy behaviour, archetypes and the damage pipeline — **combat**;
- what drops and at what rate — **items and crafting**;
- what a world-content point costs to award — **progression and trees**;
- what a class contributes to a group encounter — **classes and abilities**;
- how a mode is presented on screen — **art and UI**.

## Asserted invariants

| Invariant | Test |
|---|---|
| Forbidden modifier pairs never generate, across a large roll sample | `Encounter.Modifiers.ForbiddenPairs` |
| A multi-modifier elite always draws from at least two pressure kinds | `Encounter.Modifiers.Diversity` |
| Every density cap holds, and budget shortfall is reported rather than absorbed | `Game.WaveBudget.Caps` |
| Campaign rift enemy level never exceeds its margin above the player | `Game.RiftTier.LevelClamp` |
| The shipped boot path reaches every map role, in the shipped configuration | `Game.BootFlow.ShippedConfiguration` |
| The cover field is legal: no piece inside an instrument, no sub-dash gate | `Game.CoverRegistry.IsLayoutLegal` |
| Every procedural tile publishes a satisfiable movement contract | `Game.Tiles.MovementContract` |
| Group content grants no progression point | `Progression.WorldPoints.SoloReachable` |
| Spawn pressure pauses while a revive is in progress | `Encounter.Revive.PressurePause` |

## Open

- Where Anomalies, Raids, Dungeons and Conquest sit relative to the five canon
  locations. The likely mapping — that Anomalies are the content shape of the
  endgame frontier — is deliberately not written.
- What the top pack-composition tier is called, now that its name collides
  with a canon location.
- Where the permanent class choice is made. It happens on a menu screen today;
  siting it in the world is a level-design call to make once the Anchor is
  authored.
- Whether endgame tiers are capped or unbounded.
- Whether one Anchor is the whole settlement layer, or a network of them.
- Whether the ending's premise contradicts the endgame — the campaign closes
  every rift, and the endgame runs rifts.
