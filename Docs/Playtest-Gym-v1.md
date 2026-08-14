# Playtest Gym v1

**Last reconciled against: O32**

The existing First Person map now becomes a zero-setup combat and movement test whenever `BreakerGameMode` is active.

## THE FIELD WAS RE-LAID-OUT — read `Docs/Design/Level-Design.md` first

The gym's spatial layout is no longer a set of hand-picked offsets. It is built
from a table of dimensions **derived from the movement constants**, all
`EditAnywhere` on `ABreakerGameMode` under **Playtest | Field**, each carrying
its derivation in a comment. Three things changed that invalidate the distances
quoted further down this page:

- **The field is built on the real floor.** It used to be built 212 cm above
  it, because the ground plane was taken as "spawn minus a capsule" and the
  PlayerStart sits on the template's 210 cm plinth.
- **THE COURTYARD IS NO LONGER SEALED.** It used to be a 4000 × 4000 cm room
  with a 400 cm wall and no doorway anywhere in the map, which is why a runtime
  **breach ramp** and a **rubble stair** were built to climb out of it. On
  2026-08-14 the owner deleted all eight parapet cubes in the editor, so the
  wall is gone. **`bSpawnBreachRamp` still defaults `true` and nothing in the
  code detects the seal**, so the embankment is still built every session
  against nothing. Keeping it is a legitimate O24 choice — an authored ruin —
  but it is currently the default rather than a decision. See Level-Design §8.1.
- **Everything moved out past the breach and onto stations**, each at least one
  dash-refresh distance (1100 x 4.0 = 4400 cm) from the last: camp, breach,
  target range, encounter pocket, jump-gap run, two more pockets, sniper lane,
  wall-ride corridor, elite arena. Field long axis is 25000 cm — 22.7 s at
  sprint.

New in the field: a **jump-gap run** with three lanes at 700 / 1400 / 2100 cm,
pipped 1 / 2 / 3, sized so each gap needs exactly that many jumps. **The 3-jump
lane is now crossable as Swift** — it was described here as "uncrossable until
Swift's third jump is reachable", and as of 2026-08-14 it is, because the unlock
gate was keyed to a `CharacterLevel` nothing writes and now defaults to 1. There
is also a **flat slide lane** with a stripe where a sprint-entered slide should
end.

## Capture switches

The gym can photograph itself. Verified against the parse sites, because two of
these are commonly mis-stated.

| Switch | Form | What it does |
|---|---|---|
| `-BreakerAutoPlay` | flag | Skips the title menu into the gym. **Required for `-BreakerCaptureMenu`**, which is parsed inside its branch. |
| `-BreakerScreenshots=N` | int, clamped 1–60 | Takes N frames and exits. First at 6.0 s, then every 2.0 s. |
| `-BreakerCaptureMenu=<SCREEN>` | string | Opens the front end on a named screen first. Accepts `INVENTORY`, `SKILLTREES` (or `SKILLS`), `LOADOUT`, `SETTINGS`, `CLASS` (or `CLASSSELECT`), `PAUSE`. Anything else **silently** falls back to the main screen. |
| `-BreakerCaptureBoard=<BOARD>` | **string, not a flag** | Picks a skill board: `CORE`, `COMPARE`, or `BRANCH<n>`. |
| `-BreakerCaptureTour` | flag | Points at eight authored field vantages instead of the player's eyes. |
| `-BreakerCaptureHUD` | flag | Fabricates the HUD **events** a headless run cannot reach — damage numbers at worst-case O29 magnitudes, an absorbed-state hit marker, and the wave banner cycling its three shapes. Layout goes through the identical drawing paths. |
| `-BreakerCycleWeapons=<seconds>` | float, > 0 | Walks the viewmodel through every archetype. |
| `-BreakerBossOnStart` | flag | Spawns the Field Marshal during the gym build so a headless run can photograph it. |

Frames land in `Saved/Screenshots/breaker_NN.png`; the process exits ~2.5 s
after the last one. Capture runs on a **core ticker, not a world timer**,
because opening a menu pauses the world — the first version photographed
nothing while logging success.

`-BreakerCaptureHUD` exists for a specific reason worth keeping in front of
people: the wave banner and every damage number **shipped broken**, and they
were exactly the two readouts a capture run could not reach. Nothing presses F4
and nothing pulls a trigger. That is not a coincidence.

## What appears at runtime

- Four recycling diagnostic targets: health, shield, armour, and lateral movement.
- Three recycling chase/attack enemies using the same GAS damage pipeline as the player and targets.
- Two LATTICE ranged enemies (`ABreakerRangedEnemy`), spawned wide on either
  flank of the melee pack so their fire lanes cross the ground route the
  chasers push you down. See "The ranged archetype" below.
- **One elite carrying rolled modifiers, one SEVERED WARDEN at the front of the
  pocket, and one SEVERED SKIRMISHER placed against a real cover anchor.** These
  are new; the full account is in "The enemy content now reaches a player" at
  the end of this page, which is the section to read for what a fight in the
  pocket actually contains.
- **21 registered cover anchors** across the field. Not decoration — the
  Skirmisher archetype does not exist without them.
- Runtime movement facilities: mantle steps, dash markers, gap platforms, parallel wall-ride lanes, a flat slide lane, and a downhill slide lane.
- A code-driven crosshair and debug HUD showing movement state, horizontal speed, health, shields, ammunition, and reload state.
- Red body-hit feedback, gold weak-point feedback, and applied damage numbers.
- A deliberately simple first-person placeholder weapon block with a procedural kick and muzzle flash. It exists only to communicate viewmodel position and firing state until proper art and animation replace it.

The targets use engine basic-shape meshes, so this setup does not depend on new binary assets or machine-specific editor work.

## Controls

- Move: WASD
- Look: Mouse
- Sprint toggle: Left Shift
- Dash: Q
- Slide: C or Left Ctrl
- Jump / wall jump: Space
- Fire: Left mouse
- Aim: Right mouse
- Reload: R
- Equip primary / secondary weapon: 1 / 2. The current gym loadout is rifle / scattergun.
- Escape: open the pause menu; Escape backs out of Settings or Loadout and resumes from the pause root.

The game opens on a title menu rather than immediately dropping the player into the gym. Title and pause menus expose the two-slot loadout and saved sensitivity, FOV, and invert-look settings.

The combat HUD is the FIELDPLATE layout, not the original bottom-left strip:
vitals bottom-**left**, a single 440×184 combat cluster bottom-**right** carrying
the class-resource track, weapon/ammo and three ability squares, a wave banner
top-centre, and a minimap top-right. **The ability squares are live**, not
placeholders — they show ready / window / cooldown-wedge / unaffordable states
for the abilities actually bound to E/T/G. Full spec in
`Docs/Design/UI-HUD-Spec.md`.
- Reset player, ammunition, targets, and session stats: F1
- Copy a structured session report to the clipboard: F2
- Toggle diagnostics: F3
- **Start the next wave: F4**
- **Spawn the Field Marshal: F5** (or the console command `Breaker.Boss`)
- **Abilities: E / T / G** — two class abilities and the ultimate
- **Inventory: I** (EQUIPMENT | SKILL TREES tabs). **Talk to the nearest NPC / pick up loot: F**
- FOV: left/right bracket
- Mouse sensitivity: fixed equal X/Y baseline for this pass

F5 is bound onto the **PlayerController's** input component rather than the
character's, because F1–F4 live on `ABreakerCharacter` in `Characters/` and the
lane that added the boss did not own that file. If the controller has no input
component it logs a warning and `Breaker.Boss` is the fallback.

FOV persists locally between launches. Diagnostics show frame rate, session accuracy, weak-point rate, damage, reload count, target types/distances, and a brief impact marker. The copied report is formatted for pasting into the Notion Signals database.

## The ranged archetype — LATTICE

Implements Encounter-Design §2.2. Until this landed, every enemy in the gym
was a melee chaser whose three gears all ended in contact, so nothing asked
the player to take cover, strafe, or care about distance, and nothing had an
attack that was visible in flight.

**How you meet it.** No new key. It is already in the world:

- **On spawn / after F1 reset** — two of them, mossy grey-green humanoids with
  a violet emitter at chest height, standing ~32m out on the left and right
  flanks of the melee pack, roughly 15m to either side of the pack's line.
  Walk toward the encounter and they will already be holding station.
- **In wave mode (F4)** — from wave 2 onward, `Wave/2` of them capped at 3
  (Encounter-Design §5.3's hard cap on converging projectile sources), placed
  a ring further out than the melee packs. They come **out of** the melee
  budget, not on top of it, so pack density is unchanged.
- Its state label prints over its head like every other enemy:
  `CLOSING` / `HOLDING` / `FALLING BACK` / `AIMING` / `REPOSITION`.

**What it does.** It holds a preferred band (900–1900cm) instead of closing:
advances when you are too far, backs off *faster than it advances* when you
crowd it, and strafes — reversing direction on a cadence — while it fires. It
never has a contact attack; the base melee path is disabled on it.

**The shot.** A real replicated projectile actor (`ABreakerEnemyProjectile`),
not a hitscan with a cosmetic streak. A ~42cm violet-magenta orb at 1100 cm/s
carrying its own point light, flying in a straight line with no gravity and no
splash. Player sprint is 950 cm/s, so it crosses the band in 0.8–1.7s: long
enough to watch it come and sidestep it. Violet, not teal — the object-chroma
law reserves saturated teal for rift objects and suppression hardware.

**The telegraph.** A 0.85s wind-up before every shot. The chest emitter grows
from 0.16 to 0.38 scale, ramps from near-black violet to hot violet, and its
point light blooms on a squared curve so the last third of the wind-up is
where it really lights. The enemy also drops to 30% move speed while
committing, so the tell is readable from its feet as well as its emitter. Per
Encounter-Design §0, this window is tuned for **passive** defence (O1) and is
not to be shortened — the answer is repositioning, not a button.

**Leading — the deliberate choice.** It uses a **partial lead**
(`LeadFraction` 0.35). Encounter-Design §2.2 specifies zero lead, which makes
a moving player literally unhittable from the front and reads as harmless; a
full lead means only a direction change beats it. 0.35 aims at where a player
holding a straight sprint line would be, so keeping a lane gets punished and
**any** change of direction — strafe, dash, slide, jump — beats it outright.
Set `LeadFraction` to 0 to recover the document's pure Lattice.

**Tuning knobs**, all `EditAnywhere` on `ABreakerRangedEnemy` and all
**O2 PLACEHOLDER**:

| Category | Knobs |
|---|---|
| `Enemy\|Ranged\|Band` | `MinEngagementDistance` 900, `MaxEngagementDistance` 1900, `BandHysteresis` 150, `AdvanceSpeedMultiplier` 1.25, `RetreatSpeedMultiplier` 1.35, `StrafeSpeedMultiplier` 0.90, `StrafeReverseSeconds` 2.1 |
| `Enemy\|Ranged\|Fire` | `WindupSeconds` 0.85, `WindupMoveScale` 0.30, `ShotCooldown` 2.4, `ProjectileSpeed` 1100, `LeadFraction` 0.35, `ProjectilesPerVolley` 1, `VolleySpreadDegrees` 7, `ProjectileClass` |
| `Enemy\|Ranged\|Telegraph` | `TelegraphIdleColor`, `TelegraphHotColor`, `TelegraphLightIntensity` 2600, `TelegraphIdleScale` 0.16, `TelegraphHotScale` 0.38 |
| Inherited chassis | `DetectionRange` 3200, `MoveSpeed` 320, `AttackDamage` 16 (the projectile's damage), health inherited at 220 |
| `ABreakerEnemyProjectile` | `MaximumLifetime` 6, `OrbColor`, `GlowIntensity` 2400, `GlowRadius` 700, and the collision sphere radius / visual scale in the constructor |

**Health is deliberately the base 220, not §2.2's 1.6x.** Trash and elite
health are mid-re-anchor and awaiting an owner ruling; adding a 1.6x enemy now
would push the measured TTK further past the O18 target. Apply the 1.6x ratio
when the re-anchor lands.

**Known gap:** the playtest report's TTK sampling does not separate ranged
kills from melee kills — both feed the same trash bucket. `Playtest/` is not
owned by this change; adding an archetype dimension is a follow-up.

## First feedback pass

Test each item for several minutes and record **too weak / good / too strong**, plus one sentence describing why:

1. Walk acceleration and stopping precision.
2. Sprint speed relative to arena scale.
3. Dash force, direction control, momentum retention, collision/stop cancellation, and four-second cooldown.
4. Slide entry reliability, duration, slope response, and exit control.
5. Wall-ride activation consistency, 0.85-second maximum, gravity, and wall jump.
6. Hip-fire spread versus aimed spread.
7. Rifle damage cadence, reload length, and magazine size.
8. Weak-point readability and hit feedback.
9. Damage falloff between the near, middle, and far targets.
10. Any motion discomfort, camera obstruction, collision snag, or input failure.

Do not tune from a single run. First verify the controls and target behavior, restart once, then perform the real feedback pass.

---

## The enemy content now reaches a player (integration pass)

Before this pass, ten modifiers, three archetypes and a boss existed in
`Combat/` and spawned nowhere. `Game/` now calls into all of them.

### What the standing encounter spawns

Everything at the encounter pocket (8500 cm forward, radius 2000):

| Archetype | Count | Where, and why there |
|---|---:|---|
| Skitter (melee) | 3 | Near half of the pocket, unchanged. |
| Elite anchor | 1 | Back of the pack, **now carrying rolled modifiers** — O27's difficulty axis, reachable from a controller for the first time. |
| LATTICE | 2 | On the pocket rim at ±2000, fire lanes crossing the ground route. |
| **SEVERED WARDEN** | 1 | **Front** of the pocket. §2.4's axis is "Wardens punish approaching from the front", so the player meets it and has to decide to go around. §5.3 caps these at 1 per player. |
| **SEVERED SKIRMISHER** | 1 | **At a cover anchor**, 260 cm behind the block on the side away from the player's approach. Not a preference — see below. |

One Skirmisher and not two, because two alongside the two Lattices is four
converging projectile sources against §5.3's cap of three. The cap check in
`LogGymSummary` is what caught it.

### The cover registry, and why the Skirmisher needs it

`ABreakerSkirmisherEnemy` searches a 1400 cm ring around **itself** and keeps
only candidates whose line from the threat is blocked. Where it spawns
therefore decides whether the archetype exists at all: in the open it is a
plain shooter with a longer telegraph than a Lattice, which is strictly worse
than a Lattice.

The field already built hard cover and recorded none of it.
`RegisterCoverAnchor` now records every piece as it is spawned — each pocket's
four cover blocks (on a `CoverPitchMax` ring, Level-Design G23), each pocket
pillar, and the sniper lane's hard-cover piece. **21 anchors** in the shipped
field. `SpawnSkirmisherNearCover` places against the nearest one and **logs a
warning** when there is none in range rather than spawning a silent
open-ground shooter.

### THE FIELD MARSHAL

| Reach it by | Notes |
|---|---|
| **F5** | Bound onto the *PlayerController's* input component from `HandleStartingNewPlayer`, because the playtest keys F1–F4 live on `ABreakerCharacter` in `Characters/`, which this lane does not own. |
| `Breaker.Boss` | Console fallback, unregistered on EndPlay. |
| Wave 12 | §4.2's boss wave. |
| `-BreakerBossOnStart` | Spawns it during the gym build so the capture harness can photograph it. |

It spawns at the **elite arena** (17000 cm forward), facing back down the
field so a player arriving from camp meets its armoured front. `OnBossDefeated`
refills ammo and logs; its TTK sample lands in the new boss bucket.

**Clearance, measured:** its galleries reach ±1900 cm and its alcoves ±1700
against the arena pocket's 2000 cm radius. It fits — and only just. The
pocket's broken wall arc sits at 1800–2200 cm from centre, so a gallery can
land inside a ruin segment. `SpawnBossTest` warns if the radius is ever tuned
under the clearance.

### Wave mode is solved, not ramped

`Game/BreakerWaveBudget.h` is Encounter-Design §4.2/§4.3/§5.3 as pure
world-free maths. `Budget(n) = 6 + 4n` capped at 90; Skitter 1 / Lattice 3 /
Skirmisher 3 / Warden 6 / +4 per elite modifier; rest waves every 6 at half
budget with no elites; **wave 12 is the Field Marshal alone**; loot only on
rest and boss waves; the 70% single-archetype variety rule; and every §5.3 cap
enforced. `WaveBudget` on the game mode is EditAnywhere, so the whole shape
retunes without a recompile, and `GetWaveComposition(N)` reads any wave out
without playing to it.

**Two findings, both pinned by test:**

1. **§4.2's budget curve and §5.3's density caps contradict each other from
   about wave 8.** Under 12 live enemies, 3 ranged sources, 1 Warden and 1
   elite, a solo wave cannot spend past the mid thirties while the curve climbs
   to 90. The caps win — they are the ones with reasons written beside them —
   and the composition reports the shortfall for the owner rather than
   resolving the contradiction silently.
2. **12 is a multiple of both the rest and boss intervals.** Checking rest
   first deletes the boss wave entirely. The order is load-bearing and tested.

### TTK buckets

The report splits **melee trash / ranged trash / elite / modifier-bearing /
boss**. Before this, a Champion and the boss both landed in **melee trash** —
not, as the handover assumed, in elite: `IsElite()` is `rank == Elite`, and a
Champion's rank is `ModifierBearing` while the boss's is `Boss`. One boss kill
at 25x health moved the sub-1s trash average that O18's re-anchor reads.

Precedence is by **rank**, because rank is what multiplies the chassis. An
elite that also carries modifiers is an elite sample.

### Most kills no longer drop anything, and that is the fix

Expect the gym to feel much less generous than it did, on purpose. Until
2026-08-14 every death rolled loot unconditionally, so **kill count was item
count**, and the rarity table was flat — a trash mob at area level 1 had exactly
the same Aberrant odds as a boss at 50. Now there is a **drop-chance step by
rank** (trash 0.10, elite 0.75, modifier-bearing 0.90, boss 1.0) and a **rarity
gate** before the weighted roll: a rarity can only appear when the drop's item
level is at or above its unlock *and* the monster's rank is at or above its
minimum.

What that means at the controller: ~134 items/hour instead of ~692, and
Aberrants are **structurally impossible** below area level 25 and off trash at
any level. If you are measuring loot feel, `GymAreaLevel` is the dial — the
whole rarity ladder is gated on it. Numbers and the full table are in
`Docs/Item-Foundation.md`; all of them are `EditAnywhere` and O2 PLACEHOLDER.

In wave mode, loot is separately restricted to **rest waves and the boss wave**
by the budget solver, so a standard wave drops nothing at all regardless of the
chance step.

### The `[BreakerGym]` summary line

Now counts by class as well as telemetry bucket — a Warden and a Skitter were
both "melee" — and asserts §5.3's caps **per encounter** rather than per world,
since a Warden at 8500 and the boss at 17000 are two fights and not one
illegal one.
