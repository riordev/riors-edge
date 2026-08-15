# The Anchor — hub layout brief

**Scope:** an asset/level brief for building THE ANCHOR as an authored space.
**Last reconciled against:** O40.

Everything below is measured from the runtime hub the game builds today
(`Source/RiorsEdge/Game/BreakerHubBuilder.cpp`, `namespace BreakerHubLayout`),
not proposed from scratch. That runtime version stays as the fallback until an
authored map replaces it, so the two need to agree on scale and on where things
stand — a hand-built Anchor that is half the size will make every distance in
this document a lie.

**Units are centimetres**, because Unreal's are. 1 m = 100 uu. Z is up.

---

## 1. The one number everything else is measured against

A character is **180 cm tall** and occupies a capsule of **90 cm diameter**
(`ABreakerEnemy`'s `InitCapsuleSize(45, 90)`; the player uses the same chassis).
Sprint is **950–1100 cm/s**. So:

| Distance | Metres | Sprint time |
|---|---|---|
| 900 | 9 | ~0.9 s |
| 1800 | 18 | ~1.8 s |
| 3300 | 33 | ~3.3 s |
| 7000 | 70 | ~7 s |

**Crossing the whole plaza corner to corner is about 10 seconds at a sprint.**
That is the feel target: big enough to read as a place people gather, small
enough that reaching a vendor is never a chore.

---

## 2. Overall footprint

| Element | Value | Notes |
|---|---|---|
| Plaza | **7000 × 7000 cm** (70 × 70 m), flat | `PlazaHalfExtent = 3500` in each direction from origin |
| Plaza surface | z = **−16** (16 cm below origin), 32 cm thick slab | Characters stand at z ≈ 0 |
| Boundary ring | radius **3300 cm** (66 m across) | 16 pillars, evenly spaced at 22.5° |
| Boundary pillar | **80 × 80 × 520 cm**, centre at z = 130 | Markers, NOT walls — the edge must stay readable as open ground |
| Central landmark | **180 × 180 × 1040 cm** obelisk at origin, centre z = 260 | Plus a 320 cm-diameter moss disc at its base |

The boundary is **posts, not walls**, deliberately. The Anchor should read as a
clearing that people gather in, not a room they are contained by. If you want
walls, put them well outside the 3300 ring so the ring still reads as the social
space.

**Origin is the player's arrival point.** `TeleportPawnToHub` drops the player
at the hub origin + 120 cm of clearance, so whatever stands at 0,0 is the first
thing they see. Today that is the obelisk — if you move the arrival point, say
so and the code follows.

---

## 3. Where the NPCs stand

The frame is **Forward / Right / Up** relative to the hub's origin transform.
"Forward +1800" means 18 m in front of the arrival point.

| Who | Position (Fwd, Right, Up) | Facing | What is beside them |
|---|---|---|---|
| **Kess, the Forge Keeper** | **(+1720, −900, 100)** | faces **left-to-right** (−Right) | Forge block **160 × 160 × 220 cm** at (+1800, −900, 110), warm orange light |
| **The Quartermaster** | **(+1720, +900, 100)** | faces **right-to-left** (+Right) | Stall **240 × 120 × 120 cm** at (+1800, +900, 60); supply crate **110 cm cube** at (+1600, +900, 55), amber light |
| **Travel gate ("the way out")** | **(−1800, 0, 0)** | faces the plaza | Two flanking posts framing a gap |

So: **vendors 18 m ahead and 9 m to either side; the way out 18 m behind.** The
player arrives between them, facing the vendors, with the exit at their back.
That is intentional — arriving should show you the people, not the door.

**NPC interaction range matters for placement.** F talks to the nearest NPC
inside its own interaction radius, and the travel gate shares the F key with a
precedence rule (gate first, then NPCs, then loot). **Keep the gate at least
~600 cm from either vendor** or a player standing between them will trigger the
gate when they meant to talk. The current 3600 cm separation is comfortable.

---

## 4. Palette (ruling O24)

Overgrown Earth: vegetation over ruins, weathered tech scattered through.

| Use | Colour |
|---|---|
| Ground / plaza | Earth |
| Pillars, structure | Concrete |
| Landmark | Stone, with moss at the base |
| Forge | Rust, warm orange light |
| Quartermaster stall | Off-white |
| Supply crate | Hazard amber |

**Teal is reserved for rift objects and must not appear in the Anchor.** It is
the one colour in this project that carries a specific meaning, and spending it
on hub dressing spends the meaning.

---

## 5. What the space has to support

Not decoration — these are functional requirements:

1. **Arrival reads instantly.** The player teleports in; there is no loading
   screen or camera move to orient them. The vendors must be visible from the
   arrival point without turning.
2. **The way out is findable without a marker.** The gate is the only
   destination-shaped thing in the space.
3. **It is a social space (the MMO read).** Built for more people than are in
   it. Do not scale it down to one player.
4. **Nothing hostile, ever.** No enemies spawn here and none should be able to
   path in.
5. **Room to move.** Dash, slide and wall-ride are base kit and players will
   mess about while waiting. Flat open ground plus a few things to jump on
   beats a cluttered set.

---

## 6. What is deliberately NOT specified

- **Verticality.** The runtime version is flat. If the authored Anchor has
  levels or balconies, that is a straight improvement — but the arrival point
  must stay ground level and the vendors must stay visible from it.
- **The building shells.** The Forge and the stall are boxes today because they
  are placeholders. Their POSITIONS are load-bearing; their shapes are not.
- **Anything about the story beat.** The Quartermaster's existing dialogue is
  the main-quest start; that is code, not layout.

## 7. Open

- **Whether the authored map replaces the runtime builder or dresses it.** If
  the map ships with its own geometry, `UBreakerHubBuilder::BuildHub` should
  stop spawning plaza and props and only place the NPCs and the gate — that is
  a small code change, but somebody has to decide which is the source of truth.
- **Whether the Anchor is the only hub.** Everything above assumes one.
