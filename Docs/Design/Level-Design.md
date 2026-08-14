# Level Design — the spatial grammar

Last reconciled against: `Docs/Design/Decisions.md` O1, O2, O24, O25, O26 (2026-08-14).

Owner: level and space. Status: the grammar is derived here; the runtime field
in `Source/RiorsEdge/Game/BreakerGameMode.cpp` is built against it. Every
dimension in this document is a placeholder under **O2** and none of it has
been playtested — a screenshot shows composition, not feel.

---

## 0. Why this document exists

The owner has reported the same thing twice, most recently as:

> "walk speed feels weird but i think its a map scope issue."

Movement has been retuned **three times** since that class of complaint first
appeared (the weight pass, then gravity eased 1.60 → 1.45, then → 1.38). Three
retunes against one complaint is evidence the complaint is not about the thing
being retuned. **O26** now says movement is not the centre of the design and
gets no further dedicated passes, which closes that door anyway.

So this document does the opposite of what the last three passes did. It takes
the movement constants as **fixed inputs** and derives what the space has to
measure for them to feel like anything. Nothing here changes a movement value.
Where the arithmetic says a movement value looks wrong, §7 records it as a
recommendation with the working shown, and the code stays untouched.

**The whole method in one line:** a movement verb has a distance and a
duration; a space either contains that distance or deletes the verb.

---

## 1. The inputs

Read out of `Source/RiorsEdge/Movement/BreakerCharacterMovementComponent.{h,cpp}`
on 2026-08-14. World gravity is UE's default −980 cm/s², so `GravityScale` 1.38
gives a base acceleration of **1352.4 cm/s²**.

| Constant | Value | Constant | Value |
|---|---:|---|---:|
| `WalkSpeed` | 700 cm/s | `DashSpeedFloor` + `DashSpeedBonus` | 1700 cm/s |
| `SprintSpeed` | 1100 cm/s | `DashCooldown` | 4.0 s |
| `JumpZVelocity` | 700 cm/s | `SlideEntrySpeed` / `SlideMaxDuration` | 550 / 1.0 s |
| `GravityScale` | 1.38 | `WallRideMaxDuration` | 0.85 s |
| `ApexGravityMultiplier` / band | 1.50 / 220 | `WallRideMinimumSpeed` | 450 cm/s |
| `FallGravityMultiplier` | 1.80 | `WallRideJumpAwaySpeed` / `UpSpeed` | 650 / 650 |
| `MaxAcceleration` | 4200 cm/s² | `WallRideTraceDistance` | 85 cm |
| `BrakingDecelerationWalking` | 2400 cm/s² | Capsule half-height / mantle ceiling | 88 / 150 cm |

Enemy-side inputs, from `ABreakerRangedEnemy` (LATTICE):
`MinEngagementDistance` 900, `MaxEngagementDistance` 1900, `WindupSeconds` 0.85,
`ProjectileSpeed` 1100.

---

## 2. The jump arc, worked

Everything about gaps and ledges follows from one arc, so it is derived once
here in full and cited afterwards.

**Rise, phase 1** — free rise from 700 cm/s down to the top of the apex band at
220 cm/s, gravity multiplier 1.0:

```
t1 = (700 − 220) / 1352.4                       = 0.355 s
h1 = (700² − 220²) / (2 × 1352.4)               = 163.3 cm
```

**Rise, phase 2** — through the apex band, multiplier lerping 1.0 → 1.50, mean
≈ 1.25, so a ≈ 1690.5 cm/s²:

```
t2 = 220 / 1690.5                               = 0.130 s
h2 = 220² / (2 × 1690.5)                        =  14.3 cm
```

**Apex height = 163.3 + 14.3 = 178 cm. Rise time = 0.485 s.**

**Fall, phase 1** — through the band downward, multiplier 1.50 → 1.80, mean
1.65, a ≈ 2231.5: `t3 = 0.099 s`, `h3 = 10.8 cm`.

**Fall, phase 2** — the remaining 167 cm at 1.80 × 1352.4 = 2434.3:

```
v  = √(220² + 2 × 2434.3 × 166.8)               = 928 cm/s
t4 = (928 − 220) / 2434.3                       = 0.291 s
```

**Fall time = 0.389 s. Total airtime = 0.874 s. Landing speed 928 cm/s** — just
under `LandingHeavyFallSpeed` 950, which is why an ordinary jump costs nothing
on arrival. That is a consistency check on the movement layer's own comment and
it passes.

---

## 3. The derived grammar

Every row is a number the field is built from. All are `EditAnywhere` on
`ABreakerGameMode` under **Playtest | Field**, with the same derivation written
beside them in the header, so the space can be retuned as fast as movement was.

| # | Quantity | Derivation | Value |
|---|---|---|---:|
| G1 | **Dash refresh distance** — the unit of traversal | `SprintSpeed` × `DashCooldown` = 1100 × 4.0 | **4400 cm** |
| G2 | Sprint corridor width | 2 × (1100 ÷ 2.2 rad/s comfort ceiling) = 2 × 500 | **1100 cm** |
| G3 | Dash corridor width | 2 × (1700 ÷ 2.2) = 2 × 773 | **1600 cm** |
| G4 | Turn radius at sprint | v²/`MaxAcceleration` = 1100² ÷ 4200 | 288 cm |
| G5 | Turn radius at dash speed | 1700² ÷ 4200 | 688 cm |
| G6 | **Combat pocket radius** | 2 × G5 + 600 clearance = 1376 + 600 | **2000 cm** |
| G7 | Stopping distance from sprint | 1100² ÷ (2 × 2400), friction makes it less | ≤ 252 cm |
| G8 | One-jump gap, maximum | `SprintSpeed` × airtime = 1100 × 0.874 | 961 cm |
| G9 | One-jump gap, authored | 0.73 × G8, margin for a bad take-off | **700 cm** |
| G10 | Two-jump gap, comfortable (2nd at apex) | 1100 × (0.485 + 0.485 + 0.548) | 1670 cm |
| G11 | Two-jump gap, maximum (2nd taken late) | 1100 × (2 × 0.874) | 1923 cm |
| G12 | Two-jump gap, authored | between G8 and G10 | **1400 cm** |
| G13 | Swift three-jump, comfortable | 1100 × (3 × 0.485 + 0.669) | 2336 cm |
| G14 | Swift-only gap, authored | between G10 and G13 | **2100 cm** |
| G15 | Ledge step, mantle-able | movement doc's 35–150 cm window | **145 cm** |
| G16 | Wall-ride length, one full ride | `SprintSpeed` × `WallRideMaxDuration` = 1100 × 0.85 | 935 cm |
| G17 | Wall-ride wall, authored | 3 × G16, so a ride can start in the first two thirds | **2800 cm** |
| G18 | Wall-ride corridor width | `WallRideJumpAwaySpeed` × usable air ≈ 650 × 0.85 | **550 cm** |
| G19 | Wall minimum height | apex 178 + capsule centre 88 + headroom | **500 cm** |
| G20 | Minimum ceiling | apex 178 + standing height 176 | **≥ 400 cm** |
| G21 | Slide length | ≈ `SprintSpeed` × `SlideMaxDuration`, less friction | ≈ 1000 cm |
| G22 | Ranged sightline depth | Lattice band 1900 + 900 | **2800 cm** |
| G23 | **Cover pitch, maximum** | 1100 × (`WindupSeconds` 0.85 + 900/1100 = 1.67 s) | **1700 cm** (limit 1837) |
| G24 | Air-dash gap extension | 1700 vs 1100 over the 0.389 s after an apex dash | +235 cm (961 → 1195) |

### 3.1 The two rows that matter most

**G1, the dash refresh distance, is the load-bearing number in this document.**
It says: a space whose longest axis is under 4400 cm can be crossed on foot in
less time than the dash takes to come back. Inside such a space the dash is
*structurally incapable* of being a traversal choice — it can only ever be a
dodge. That is a legitimate design position for an arena, and it is a bug for a
field. Everything about how far apart the stations sit is G1.

**G2, the sprint corridor width, is the direct answer to the owner's sentence.**
A surface at lateral distance *d* passing at speed *v* sweeps the retina at
*v/d* rad/s. Sustained peripheral flow above roughly **2.2 rad/s (126 °/s)**
smears — the wall stops being a surface and becomes a strobe, and the read is
"I am moving too fast", which is exactly the complaint. At sprint:

| Wall distance | Flow | Reads as |
|---:|---:|---|
| 200 cm | 5.50 rad/s (315 °/s) | unusable; nauseating |
| 350 cm | 3.14 rad/s (180 °/s) | "walk speed feels weird" |
| 500 cm | 2.20 rad/s (126 °/s) | the threshold |
| 800 cm | 1.38 rad/s (79 °/s) | comfortable |
| 1500 cm | 0.73 rad/s (42 °/s) | open ground |

The point is that **the same walk speed reads as three different speeds in three
different corridor widths.** Nothing about the character changed.

---

## 4. What makes movement feel BAD

The reverse of the grammar, stated as prohibitions, because the owner is
currently living in most of these.

| Anti-pattern | Dimension | What actually happens |
|---|---|---|
| **The sub-dash room** | long axis < G1 (4400 cm) | Dash is a dodge and never a route. The verb is present and unusable. |
| **The strobe corridor** | width < G2 (1100 cm) | Peripheral flow > 2.2 rad/s. Diagnosed as "speed feels wrong"; it is width. |
| **The coin-flip gap** | 900–1050 cm | Straddles the 961 cm single-jump reach. Clears on a good take-off and fails on a bad one, with no way for the player to tell which. **Never author a gap in this band.** |
| **The invisible ceiling** | roof < 360 cm | Jump apex 178 + standing 176. The jump silently stops existing. Under 266 cm the wall-ride entry trace also stops finding wall at apex. |
| **The stub wall** | wall < 600 cm | Under two thirds of one ride (G16). The verb ends before the player has registered it started. |
| **The uncrossable wall pair** | gap > 600 cm | A wall jump leaves at 650 cm/s with ≈ 0.85 s of usable air = 552 cm. A 700 cm pair — *which is what the shipped field had* — cannot be chained on the game's own numbers. |
| **The contact-range room** | long axis < 3300 cm (3 s at sprint) | Every fight resolves at contact regardless of build, because there is nowhere to be that is not near. |
| **The exposed crossing** | cover pitch > 1837 cm (G23) | Ground where a LATTICE telegraph cannot be answered by moving. Under O1 movement is the *only* active defence, so this is unanswerable damage, not difficulty. |
| **The rail orbit** | pocket radius ≈ G4/G5 | An orbit at exactly the minimum turn radius is the only line that exists. Circling stops being a choice. |
| **The featureless plane** | any size | Optical flow needs texture. 200 m of one colour reads as a smaller space than 60 m with something to pass. |
| **The sealed room** | — | See §6. A room with no mouth is not a small level; it is a level with one room. |

---

## 5. The stations — what the field is now

Positions are along the spawn forward axis, on the **real ground plane**
(see §6.1). Names match the labels in `BreakerGameMode`.

| Station | Forward | Grammar it is built on |
|---|---:|---|
| **Anchor camp** (template courtyard) | −1800 … +1800 | Safe ring at 1800 = the courtyard half-extent, so "safe" and "inside the compound" are the same statement. Forge, Quartermaster, supply crate, plaza. |
| **The Breach** | 900 … 4100 | Ascent 900→2100 rising to 520 cm (23.4°, inside the 44.76° walkable limit), crest landing, 2000 cm descent at 14.6° which doubles as the downhill slide lane. Width G2. |
| **The rubble stair** | 900 … 3400, left | Four G15 risers to 580 cm. The conventional route out — climbable with no jump at all, per master sheet 5.4. |
| **Target range** | 4600 + {1200, 2400, 4500, 2100} | Unchanged ranges relative to a firing line, so every falloff reading taken before this pass still compares. Watchtower perches at ±3000 with G15 stacks up. |
| **Dash reach markers** | 3800 … 7800, left | Nine posts at 500 cm. Nine posts ≈ one G1, so the lane *is* "how much ground one dash window buys". |
| **Flat slide lane** | 3100 … 4100, left | G21 long with a stripe at the midpoint, so the player sees where their slide actually ended. |
| **Wall-ride corridor** | 4300 … 14700, right | Two pairs, G17 long, G18 apart, G19 tall, separated by one G1 so getting between them is a dash decision. |
| **Encounter pocket** | 8500 | Radius G6. ≈ 2 × G1 from the camp: the approach is a route with two dash decisions in it, not a four-second sprint. Melee pack in the near half, elite at the back, two LATTICE on the rim at ±G6. |
| **Jump-gap run** | 10700 … 13000 | Three lanes at G9 / G12 / G14 across one 220 cm trench, pips counting 1 / 2 / 3 jumps, platforms G3 wide. The trench has a floor and a ramp out — a failed jump costs the climb back and nothing else. |
| **Second and third pockets** | 12900 (+right), 9000 (−left) | Radius G6, both > G1 from pocket one. |
| **Sniper lane** | 3100 … 13100, far left | Width G3, markers at 30 / 60 / 90 m from the firing line, kerbs low enough to never block a shot. One hard-cover piece with G22 of clear ground behind it, which is the minimum a LATTICE needs to use its whole band. |
| **Elite arena** | 17000 | Radius G6 — twice G5 doubled, and exactly Encounter-Design §3.3's 4000 × 4000. Twelve ring markers. |
| **Field extents** | −3000 … 22000, ±11000 | 5 × G1 forward, 2.5 × G1 each side. 25000 cm of long axis = **22.7 s at sprint**, and five dashes to beat that. |

### 5.1 The independent corroboration worth noticing

`CombatPocketRadius` was derived from `MaxAcceleration` and the dash speed with
no reference to any design document: 2 × 688 + 600 = 1976 → 2000. Doubling it
gives 4000 × 4000, which is **exactly** the boss-arena footprint
Encounter-Design §3.3 specifies. Two independent routes to the same number is
the strongest evidence in this document that the method is sound.

---

## 6. What was actually wrong

Measured, not inferred. `ABreakerGameMode::LogGymSummary` now prints all of it
on every launch, so it stays checkable.

### 6.1 The field was built 212 cm above the floor

Every spawn function took the ground plane as *pawn location minus the 88 cm
capsule half-height*. That is only the ground if the pawn spawned on it.
`Lvl_FirstPerson`'s PlayerStart sits at z 300 on the template's central plinth,
whose top is at z **210** — so the whole runtime field, apron included, was
built on a phantom plane **212 cm above** the real floor at z 0.

Fixed by probing: `ResolveGroundZ` traces down on a ring of eight probes at
1500 cm (outside the 350 cm plinth, inside the 1800 cm wall) and takes the
lowest hit. It logs what it found, including how much plinth it corrected for.

### 6.2 The playable room was 40 m × 40 m and sealed

The template is a **4000 × 4000 cm courtyard** (`Floor`, −2000…2000, top at
z 0) with a **continuous 400 cm parapet on all four sides and no doorway**:

- lower course `SM_Cube2/3/4/5`, z 0–200, full span each side;
- upper course `SM_Cube17/18/19/20`, z 200–400, inset by 100 cm.

Against G1 that room is a diagnosis on its own:

```
crossing at sprint : 4000 / 1100 = 3.64 s
crossing at dash   : 4000 / 1700 = 2.35 s
dash cooldown      : 4.00 s
```

**The player could cross the entire world they had access to, twice, between
two dashes.** Everything the game spawned — targets, enemies, lanes, pockets —
was on the far side of a wall that two base-kit jumps (355 cm) do not clear in
one go. The only way out was to discover a two-stage climb: 200 cm onto the
inner ledge, then 200 cm more onto the parapet.

The runtime answer is **The Breach** — a collapsed embankment over the +X wall,
which is also the correct read for O24's overgrown Earth. The proper answer is
deleting the wall, and that is editor work (§8).

### 6.3 The additions were islands

The previous field spawned three pockets, a lane and a wall pair scattered
across 180 m of flat, unmarked apron with nothing between them. Big ground is
not the same as a big space: with no stations, no route and no texture, the
field read as a small box next to a car park. The apron is now four slabs (down
from 81 tiles) plus 200 scrub-sized tint patches, and the spine between the
breach and the arena carries shoulder ruins at G1 × 0.45 intervals.

### 6.4 Bugs found by applying the grammar to the existing field

| Found | Was | Now |
|---|---|---|
| Wall-ride pair could not be chained | 700 cm gap vs 552 cm of wall-jump reach | G18 = 550 cm |
| Arena ring described an un-runnable circle | 1400 cm markers vs G5 688 + clearance | G6 = 2000 cm |
| Target range was inside the template wall | first dummy at 1200 cm, behind template ramps | past the breach at 4600 + range |
| Encounter stood where the breach descent lands | 3500 cm out | pocket at 8500 |
| Camp had a redundant back wall 200 cm inside the template parapet | two walls, 40 m room | removed |
| Watchtowers were inside the courtyard | ±2100 lateral vs 1800 wall | on the range shoulders |

### 6.5 The ground was coplanar with itself (owner: "a lot of the textures on the ground were tearing")

Photographed before and after, from a new grazing-angle capture vantage added
for the purpose — this defect is invisible from a plan view and invisible from
head height facing a wall, which is why every existing vantage missed it.

The apron's top face sits at exactly the probed ground plane. Three separate
things were then authored at, or effectively at, that same height:

| Source | What it was | Why it tears |
|---|---|---|
| Tint patches vs **each other** | 200 plates placed by rejection-free random sampling, all at one fixed height | ~18% area coverage means dozens of overlapping pairs, and two overlapping plates whose top faces share a z are coplanar. The depth test has no winner. |
| Tint patch **lips** | each plate was a 4 cm-thick cube that also cast a shadow | At 150–200 m both the cast shadow and the lip's own shaded side face are sub-pixel and alias into a **stippled dashed line tracing every patch outline** — the dark dotted seams in the before-shot. This was the visible majority of the report. |
| Jump-gap **trench floor** | `SpawnFieldSlab(..., TopZ = 0.0f, ...)`, lying on the apron | Its top face was at exactly the apron's top face, over the whole trench. |

Fixes, all in `SpawnExpandedField` / `SpawnJumpGapRun`:

- Patch placements are **rejected when their rotated footprints overlap**, so
  no two patches ever share a surface. 196 placed from 420 attempts, so the
  density is unchanged and the double-tinted blotches are gone as a bonus.
- Patches are **planes, not cubes**, cast **no shadow**, and are lifted clear of
  the apron. A plane has no lip to shade and none to alias.
- The trench floor sits on the new `GroundOverlayLift` (6 cm, `EditAnywhere`,
  O2 placeholder). Set it to 0 to reproduce the bug for an A/B.

`GroundOverlayLift` is the rule going forward: **anything laid ON the apron is
separated from it.** Six centimetres is far under `MantleStepHeight` and far
under the engine's step-up, so nothing trips on it and no seam reads as a
ledge. The template `Floor` is *not* affected — the apron is authored as the
rectangle around it and abuts without overlapping, which the runtime log
confirms (frame origin (0,0), forward (1,0), `Floor` −2000…2000).

---

## 7. Movement recommendations — NOT applied (O26)

The arithmetic produced four observations about movement values. Under **O26**
none of them is acted on; they are recorded with the working so a future ruling
has the numbers.

1. **`DashCooldown` 4.0 s sets the minimum size of a traversal space at 4400 cm
   (G1), which is larger than the project's own authored arena.**
   Encounter-Design §3.3 specifies a 4000 × 4000 boss arena, so inside the
   slice's flagship room the dash can never be traversal. Either that is the
   intent — the dash is a combat verb and traversal belongs to jump, slide and
   wall ride — or `DashCooldown` wants to be ≈ 2.2 s (refresh distance 2420 cm,
   which fits inside a 4000 cm arena with room to choose). **This should be a
   ruling, not an accident.** The field is currently built on the first reading.

2. **`WallRideJumpAwaySpeed` 650 only crosses 552 cm of gap.** If wall-ride
   corridors are meant to be ≈ 700 cm — which is what the shipped field assumed
   — the away speed needs ≈ 850 cm/s. The field moved to 550 instead, which is
   the O26-compliant fix.

3. **`SlideMaxDuration` 1.0 s buys ≈ 1000 cm, which is less than one second of
   sprinting (1100 cm).** A traversal verb that covers less ground than not
   using it is a stance change. If the slide is meant to read as traversal
   rather than as a profile change, 1.4–1.6 s is the band. If it is meant to be
   a duck-and-shoot, 1.0 s is right and the doc should say so.

4. **Walk 700 to sprint 1100 is only a 1.57× ratio**, and in a 40 m room that
   difference is 1.6 s across the whole world. Part of "walk speed feels weird"
   may simply be that walk and sprint were indistinguishable in a space small
   enough that both arrived immediately. This is an observation about the space
   and needs no movement change; re-read it after a playtest in the new field.

---

## 8. Editor work — what to delete from `Lvl_FirstPerson` and why

Cannot be done headlessly; `.umap` must not be hand-edited. All names below are
the actual actor labels, printed by `LogGymSummary` as `[BreakerGymTemplate]`
lines on every launch, with their measured bounds.

### 8.1 Delete — the seal

| Actors | Bounds | Why |
|---|---|---|
| `SM_Cube2`, `SM_Cube3`, `SM_Cube4`, `SM_Cube5` | the four walls, z 0–200 | They seal the courtyard. There is no doorway anywhere in the map. |
| `SM_Cube17`, `SM_Cube18`, `SM_Cube19`, `SM_Cube20` | the same four, z 200–400 | The upper course. Together with the above they put the crest at 400 cm against a 355 cm double jump. |

Deleting these eight actors is the single highest-value edit in the project's
level layer. **When they go, set `bSpawnBreachRamp` to false** — or keep the
embankment as authored ruin, which is the O24-consistent choice, and delete only
the wall it climbs.

**Replace with:** one compound wall carrying **one mouth** on the +X face at
`SprintCorridorWidth` (1100 cm) or wider, height ≥ 400 so it still reads as an
Anchor perimeter. A camp should have a wall; it should not have a lid.

### 8.2 Delete — the crowding

| Actors | Count | Why |
|---|---:|---|
| `SM_Cylinder2` … `SM_Cylinder9` | 8 | 250 cm × 400 cm pillars embedded in the wall midpoints, protruding 150 cm into the room. They narrow every approach to the wall to well under G2. |
| `SM_Cube`, `SM_Cube6` … `SM_Cube12`, `SM_QuarterCylinder`, `SM_QuarterCylinder2/3/4` | 12 | 300 × 300 × 200 corner fillets. They exist to make a 40 m template room feel navigable; in a 250 m field they are noise. |
| `SM_Ramp` … `SM_Ramp8` | 8 | 400 × 300 × 200 wall ramps. These are the yellow shapes that fill the entire before-shot. Nothing in the gym uses them. |

### 8.3 Keep, or make a deliberate decision about

| Actors | Recommendation |
|---|---|
| `Floor` (−2000…2000, top z 0) | **Keep.** It is the camp floor and the runtime apron is authored to abut it exactly, coplanar with no seam and no overlap. |
| The central plinth: `SM_Cube13/14/15/16`, `SM_QuarterCylinder5`…`12`, `SM_Cylinder` (disc, z 200–210) | **Owner decision.** Keeping it makes the spawn a 210 cm dais, which is a fine place to start and which the field frame now handles correctly. Deleting it means moving the PlayerStart down to ≈ z 100; the ground probe still resolves correctly either way. |
| `SM_Ramp9`…`SM_Ramp12` (plinth access ramps) | **Keep `SM_Ramp11` specifically** if the plinth stays: the breach ascent is authored to start at X 900, where that ramp's foot reaches the floor, so the two meet flush. |

### 8.4 Other editor-only items

- **There is no kill volume and no `KillZ`.** `Docs/Playtest-Gym-v1.md` records
  that a 40 m fall triggers a playtest reset *because* the template level has
  none. Set World Settings → Kill Z to spawn Z − 4000 so the engine-level and
  gameplay-level answers agree.
- **The template floor material is a tiled checker at courtyard scale.** Over a
  250 m field it reads as graph paper. Replace with a flat value; the runtime
  tint patches then carry the optical flow.
- **Lighting is the template default** and every runtime actor is `Movable`, so
  nothing bakes. Correct for graybox; revisit when the field is authored.

---

## 9. Acceptance checks

Falsifiable, and deliberately so — the point of deriving numbers is that they
can be wrong out loud.

- [ ] The G9 gap (700 cm) clears with **one** jump, every time, from a sprint.
- [ ] The G12 gap (1400 cm) **fails** on one jump and clears on two.
- [ ] The G14 gap (2100 cm) fails on two. It should stay uncrossable until
      Swift's third jump is reachable — with `SwiftThirdJumpUnlockLevel` at its
      O2 placeholder of 20 and nothing raising `CharacterLevel`, it is
      uncrossable in the gym today **by design**. It is the one piece of
      geometry that will visibly change the day that ruling lands.
- [ ] A full wall ride fits on a G17 wall, and a wall jump crosses a G18
      corridor to the opposite wall with air to spare.
- [ ] A slide entered at sprint ends at or near the G21 stripe.
- [ ] Sprinting the G2 breach corridor does not read as "too fast".
- [ ] Crossing the encounter pocket does not leave the player more than G23
      from cover at any point.
- [ ] The dash is used for **travel** at least once between two stations. If it
      is only ever a dodge, G1 is wrong or `DashCooldown` is (§7.1).

Nothing above has been playtested. Automation proves that the field spawns and
that 151 tests still pass; whether 22.7 seconds of long axis feels like a world
or like a walk is exactly what it cannot see.

---

## 10. Capture harness

`-BreakerCaptureTour` alongside `-BreakerScreenshots=N` points the capture at
authored vantage cameras derived from the same station constants — plan view,
the route from behind the camp, the breach crest, an oblique over the encounter
pocket, down the wall-ride corridor, and along the sniper lane — instead of at
the player's eyes. A spawn view is one composition; a layout is not visible from
inside it, and this pass changes the layout.

**Known harness quirk, recorded rather than hidden:** the screenshot request is
serviced at the end of the frame, after the view-target change in the same
tick, so shot *N* renders vantage *N* and there is currently no spawn-eye frame
in a tour run. Run without `-BreakerCaptureTour` for the spawn view.
