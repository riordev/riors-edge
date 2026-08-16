# Player, Weapon and Gear Asset Brief — commissioning prompts for the player-facing set

> STATUS 2026-08-16: UNBUILT TREATMENT — commissioning prompts, current as of 2026-08-15; none of the commissioned assets are in the game yet.

**Scope:** slice (see `Docs/Vertical-Slice.md`).
**Last reconciled against: O40** (2026-08-15).

Owner: art direction. Status: **reference document for commissioning, not design.**
This document authors no balance value and rules nothing. It exists to hand the
owner ready-to-paste generation prompts for **one or two of everything the
player sees of themselves** — eight weapons, one pair of first-person arms, two
variants per armour slot, and the item icons that make gear read like gear.
Where a number below is quoted from code it carries a file:line citation;
everything else is qualitative, per **O2** (measure before authoring — no new
value is invented here).

Reads from: `Source/RiorsEdge/Characters/BreakerViewmodelRig.{h,cpp}` (the
first-person blockout, as data), `Source/RiorsEdge/Characters/BreakerCharacter.{h,cpp}`
(camera, capsule, arms), `Source/RiorsEdge/Weapons/BreakerWeaponComponent.cpp`
(the archetype prototype table), `Source/RiorsEdge/Weapons/BreakerWeaponDefinition.h`,
`Source/RiorsEdge/Weapons/BreakerWeaponFeel.h` (the recoil the model must
survive), `Source/RiorsEdge/Items/BreakerItemTypes.h` (slots, rarity),
`Source/RiorsEdge/Items/BreakerEquipmentComponent.h`, `Source/RiorsEdge/UI/BreakerUIStyle.h`
(the palette), `Docs/Design/Art-And-Modelling-Plan.md`, `Docs/Design/UI-Inventory-Spec.md`,
`Docs/Design/UI-Ability-Icons-Spec.md`, `Docs/Design/Decisions.md` ruling **O24**.

Sibling briefs, same shape and same voice: `Docs/Design/Enemy-Model-Asset-Brief.md`,
`Docs/Design/Anchor-Hub-Layout-Brief.md`.

---

## 1. The one measurement everything else is relative to

**The rifle is ~80 cm long, and its rig origin is the trigger group.**

Every other asset in this document is stated against that. The rifle is the
reference weapon in code (`BreakerViewmodelRig.cpp:250-270` is the `default:`
case, and every other row's comment describes itself against it) and in the art
plan (`Art-And-Modelling-Plan.md:689`). Its receiver is authored **34 × 5 × 7 cm**
(`BreakerViewmodelRig.cpp:256`). Model to that and nothing else needs a rescale
pass.

The second measurement, because the first-person frame is the only frame:

| Quantity | Value | Source |
|---|---|---|
| Player capsule | **34 cm radius, 88 cm half-height — a 176 cm figure** | `ABreakerCharacter` never calls `InitCapsuleSize`, so it inherits Unreal's `ACharacter` default. Verified by grepping all of `Source/RiorsEdge/` — the only capsule initialisers are the enemy's `45/90` (`BreakerEnemy.cpp:54`), the Altered's `60/75` (`BreakerAlteredEnemy.cpp:104-106`), the NPC's `34/88` (`BreakerNPC.cpp:12`), the target dummy's `42/90` (`BreakerTargetDummy.cpp:19`) and the travel point's `40/100` (`BreakerTravelPoint.cpp:14`). **None is the player.** |
| Camera | **(−10, 0, +64) cm from the capsule centre**, using pawn control rotation | `BreakerCharacter.cpp:59-60` |
| Eye height above the floor | **152 cm** (88 cm capsule centre + 64 cm camera) | derived from the two rows above |
| Field of view | **90°** default, player-configurable and persisted | `BreakerCharacter.h:308` (`BaseFieldOfView = 90.0f`) |
| Whole-rig uniform scale | **0.9** | `BreakerCharacter.h:153` (`ViewmodelScale`), O2 PLACEHOLDER |

> **Discrepancy, flagged not resolved.** `Art-And-Modelling-Plan.md:399` asks for
> a *separate* first-person FOV defaulting to 70–80. The code has one camera at
> 90 and no separate viewmodel FOV. **Frame the assets for 90° at 16:9**, because
> that is what the game renders, and expect them to be checked again if a
> separate FP FOV is ever added.

> **Second discrepancy, same treatment.** The Anchor hub brief states the player
> "uses the same chassis" as the 180 cm enemy capsule
> (`Anchor-Hub-Layout-Brief.md:18-20`). Measured, that is not true today — the
> player is 176 cm and the enemy is 180 cm. The four-centimetre gap changes
> nothing about any prompt here; it is recorded so nobody re-derives a player
> height from the wrong document.

---

## 2. What the current build actually is — read this before writing a prompt

Three separate honest statements, because the three subjects are at three very
different stages.

### 2.1 The weapons are composed engine primitives, and they are *good* blockouts

`Source/RiorsEdge/Characters/BreakerViewmodelRig.cpp` is a data table of
**boxes, cylinders and cones authored in real-world centimetres**, one row per
archetype, assembled at runtime from `/Engine/BasicShapes` into a pool of at most
twelve `UStaticMeshComponent`s (`BreakerViewmodel::MaxProxyParts = 12`,
`BreakerViewmodelRig.h:179`; the pool is allocated once at `BreakerCharacter.cpp:112-119`
and recycled, never respawned).

This matters more than it sounds. **The proportions in that table were read off
each weapon's real mechanical numbers, not invented** — the file says so at
`BreakerViewmodelRig.cpp:42-44`:

> "MAGAZINE SIZE, CADENCE and SWAP TIME are the three things the player already
> feels, so those are the three the silhouette states."

So an authored weapon is not replacing a placeholder. It is replacing a
**correct** placeholder, and its job is to keep every proportional claim the
blockout already makes while adding surface. If the authored rifle is shorter
than the authored sniper by less than 20%, or if the machinegun's drum stops
being the biggest single part in the game, a shipping automation test
(`RiorsEdge.Characters.ViewmodelSilhouetteOrder`, referenced at
`Art-And-Modelling-Plan.md:703-706`) will say so.

### 2.2 The player has forearms, gloves, and nothing else

There is **no player body mesh of any kind.** The character constructor creates
exactly four limb components — `LeftArmVisual`, `RightArmVisual`,
`LeftGloveVisual`, `RightGloveVisual` (`BreakerCharacter.cpp:145-148`) — each a
stretched `/Engine/BasicShapes` cube, parented to the weapon rig rather than to
the camera so the hands ride the recoil spring with the gun
(`BreakerCharacter.h:133-136`). A forearm is **6 cm across** and a glove is a
**9.5 cm** near-cube (`BreakerCharacter.h:162,164`). Both are `SetOnlyOwnerSee(true)`
and cast no world shadow (`BreakerCharacter.cpp:140-141`).

**The known defect, quoted rather than paraphrased** (`BreakerViewmodelRig.h:19-21`):

> "THE ARMS WERE OFF SCREEN. Measured, not guessed: at the shipped transforms
> both arm blocks project to y=1283 and y=1384 on a 1080-tall frame. The player
> has been holding an invisible gun with no hands."

That is fixed — the arms now hang off the rig at shoulder anchors of
`(30, −22, −29)` and `(10, 19, −33)` cm in camera space (`BreakerCharacter.h:158,160`)
and are clipped by the near plane at the shoulder end deliberately, so they read
as entering frame from the player's body. But it is worth knowing, because it is
exactly the failure an authored arms mesh can reintroduce: **if the shoulder end
of the authored forearm is modelled as a finished, capped stump, it will either
float or be clipped ugly. Model the arms so the near plane can eat the shoulder.**

### 2.3 Gear is completely invisible, and that is a code gap, not an art gap

`UBreakerEquipmentComponent` (`Source/RiorsEdge/Items/BreakerEquipmentComponent.h`)
is **pure stat aggregation**. It equips, unequips, previews, aggregates, salvages,
tempers, reforges and attunes — and it owns no mesh, no socket, no visual
component and no attachment of any kind. There is no third-person body to attach
gear to, and no paper-doll (the SceneCapture approach is designed in
`Art-And-Modelling-Plan.md:730-791` and built nowhere).

**Consequence for this brief, stated plainly:** the twelve armour pieces in §6
have **no consumer in the current build.** They are commissioned so that the
owner has them when the renderer exists, and because their *icons* (§7) can be
used long before their meshes can. Weapons and arms, by contrast, drop in the
moment they are imported.

---

## 3. Technical constraints — non-negotiable

| Constraint | Value | Source |
|---|---|---|
| Units | **Centimetres.** The whole viewmodel table is authored in real-world cm and divided by 100 at build time, because the engine's BasicShapes are 100 cm on their longest axis. | `BreakerViewmodelRig.cpp:276-303`, esp. the `constexpr float Unit = 100.0f` at :282 |
| Axes | **X forward, Y right, Z up.** | `BreakerViewmodelRig.h:77` ("X forward, Y right, Z up"); `Art-And-Modelling-Plan.md:323` |
| Weapon rig origin | **The firing hand / trigger group.** Stocks are negative X, barrels positive X. | `BreakerViewmodelRig.h:78-79` |
| Weapon mesh type | **STATIC, determined not guessed.** Every viewmodel part is a `UStaticMeshComponent` (`BreakerCharacter.cpp:99`, and `ViewmodelParts` is `TArray<TObjectPtr<UStaticMeshComponent>>` at `BreakerCharacter.h:131`). Nothing in `ABreakerCharacter` creates a `USkeletalMeshComponent`, references an anim blueprint, or reads a skeleton. **Do not commission a rigged weapon** — there is nothing skeletal for it to bind to. |
| Arms mesh type | **Static today, skeletal eventually.** The four limb components are `UStaticMeshComponent` (`BreakerCharacter.cpp:135-144`). `Art-And-Modelling-Plan.md:393-394` commits to a single shared skeletal FP arms mesh on one skeleton for Human and Effigy [O14] — but no code consumes one. **Ask for the arms as a skeletal mesh whose BIND POSE also exports cleanly as a static mesh**, so the static export drops in now and the rig is not thrown away. |
| Gear mesh type | **Undetermined — no consumer exists.** See §2.3 and §10. |
| Separable pieces, weapons | Body, barrel + muzzle device, optic/sight, magazine, working mechanism (pump / bolt / charging handle) — **five separate meshes per weapon, always**, even if the concept was modelled as one object. | `Art-And-Modelling-Plan.md:405-419`; matches the code's part pool, which poses each piece independently |
| Socket names | Author `Muzzle` on the barrel piece, `Sight` on the optic, `Personal` on the arms for the personal item, and wrist/knuckle sockets on the gloves. **Honesty note: the code reads NO sockets today.** The muzzle flash is a `UPointLightComponent` placed at the archetype's `MuzzleCm` in rig space by C++ (`BreakerCharacter.cpp:823-826`, values in `BreakerViewmodelRig.cpp`), not read off a mesh. The names above come from `Art-And-Modelling-Plan.md:413-420` and are what the code will look for when it stops hard-coding. Author them; expect them to be unused for a while. |
| Materials | **One master material, instanced. No unique unwraps — trim sheets.** The blockout uses a single dynamic instance per part with one `Color` vector parameter (`BreakerCharacter.cpp:815-818`). Anything authored must work with one to three flat materials and no baked texture. | `Art-And-Modelling-Plan.md:398`; `BreakerCharacter.cpp:815-818` |
| Shadows | FP arms and weapon cast **no** world shadow. Already set in code (`BreakerCharacter.cpp:105,141`). |
| Visibility | Owner-see only. Already set (`BreakerCharacter.cpp:93,102,140`). |
| Polycount | **No in-code ceiling exists** — the current build is engine primitives. `Art-And-Modelling-Plan.md:395-396` quotes **25k tris for both arms** and **15–20k tris per weapon LOD0** as *finished art* targets. This brief is a blockout-to-first-pass commission, so the recommendation below is this document's own and is explicitly not sourced: **~1,500–3,000 tris per weapon, ~4,000–6,000 for the arms-and-gloves pair, ~800–2,000 per armour piece.** Adjustable. |

### 3.1 The first-person framing constraint — what the camera actually sees

Six binding silhouette rules, from `Art-And-Modelling-Plan.md:288-312`, restated
because they are the ones an image generator will break:

1. **The support forearm is the largest single shape on screen and must be the
   quietest.** Value it darker than the receiver. If it is the brightest thing in
   frame it competes with the object carrying the information.
2. **The gun's outline must survive a 4-pixel blur.** The read is three shapes:
   one long horizontal mass, one break in the top line (the optic), one break in
   the bottom line (the magazine). Anything not contributing to those three is
   texture, not geometry.
3. **One dominant feature per archetype, and no archetype may borrow another's.**
   The list is fixed and per-weapon in §4.
4. **No hardpoint, hook, launcher, tether, or grapple mount anywhere** — weapon,
   gloves, sleeve, sling. There is no grapple in this game and teasing one is the
   cheapest possible lie.
5. **Nothing crosses the screen centre at rest.** The crosshair region stays clear
   at hip; the sight occupies it only when aiming.
6. **The gun's mass sits low-right, the muzzle points up-left.** The assembly
   occupies roughly the lower-right quarter at 90° / 16:9, and the barrel runs
   *toward* the centre, not across it.

Per-archetype framing numbers, all from `BreakerViewmodelRig.cpp` and all O2
PLACEHOLDER:

| Archetype | Hip offset from camera (fwd, right, down) cm | Sight height above rig origin cm | Muzzle in rig space, X cm | Source line |
|---|---|---|---|---|
| Rifle | 46, 14, −13 | **8.3** | **61.5** | :264-269 |
| SMG | 46, 14, −13 | 6.5 | 34.5 | :99-104 |
| Sniper | 44, 14, −13 | 12.0 | **73.0** (furthest) | :122-127 |
| Shotgun | 46, 15, −14 | 7.2 | 57.0 | :146-154 |
| Rocket | **72, 19, −22** (furthest out and lowest) | **15.0** (highest) | 56.0 at Z +6.0 | :171-176 |
| Burst Rifle | 46, 14, −13 | 12.4 | 69.5 | :194-199 |
| Machinegun | 48, 16, −15 | 9.1 | 62.5 | :219-224 |
| Sidearm | **40, 11, −11** (closest to the centre line) | **4.4** (lowest) | **18.5** (nearest) | :241-247 |

**The ADS pose is derived, never authored.** The aim rest is computed as
`(AdsForwardCm, 0, −SightHeightCm × ViewmodelScale)` (`BreakerCharacter.cpp:638`),
so an accurate sight height *is* the aim pose. Get the sight's height above the
trigger group right and aiming works; get it wrong and no amount of hand-placing
fixes it.

### 3.2 The model has to survive being kicked

The weapon is not a static prop in frame. Every shot drives a spring on the whole
assembly, and the arms ride it. From `BreakerWeaponFeel.h` (struct defaults, which
are the rifle's) and the per-archetype overrides in `BreakerWeaponComponent.cpp`:

| Quantity | Rifle default | Extremes | Source |
|---|---|---|---|
| Instant backward displacement per shot | **3.2 cm** | Rocket **10.0** (`:163`), Sniper 9.0 (`:92`), Shotgun 8.0 (`:128`); SMG 2.0 (`:57`), Sidearm 2.2 (`:287`) | `BreakerWeaponFeel.h:172` |
| Lateral component | 0.9 cm | Sniper 1.6, Machinegun 1.5 | `BreakerWeaponFeel.h:177` |
| Muzzle-up rotation per shot | **2.4°** | Rocket **6.8°** (`:165`), Sniper 6.5° (`:94`); SMG 1.6° (`:59`) | `BreakerWeaponFeel.h:181` |
| Hard ceilings | **9.0 cm and 7.0°**, so sustained fire cannot walk the mesh off screen | — | `BreakerWeaponFeel.h:186,189` |
| Spring | stiffness 260, damping 26; damping below `2·sqrt(stiffness)` overshoots slightly, which reads as snap | Sidearm stiffness **340** (fastest settle), Rocket **140** (slowest) | `BreakerWeaponFeel.h:195,198`; `BreakerWeaponComponent.cpp:290,166` |
| Kick while aiming | scaled to 0.45 — a sighted weapon must stay readable through its own sight | Sniper 0.5 (`:97`) | `BreakerWeaponFeel.h:203` |

**Two consequences for geometry.** First, the muzzle flash light hangs off the
*rig*, not the camera (`BreakerCharacter.cpp:121-129`) — so anything modelled at
the muzzle moves with the kick and must still read while moving up to 10 cm back
and 7° up. Second, a rocket launcher that displaces 10 cm per shot on a rig that
sits 72 cm forward is travelling a seventh of its own stand-off distance; **do not
put fine detail at the muzzle end of the heavy weapons**, it will smear.

---

## 4. The eight weapon archetypes — one ready-to-paste prompt each

Every block below is self-contained; no repo knowledge is needed to use it. The
mechanical facts in each prompt are quoted from the live prototype table in
`Source/RiorsEdge/Weapons/BreakerWeaponComponent.cpp` (function
`GetPrototypeDefinition`, lines 306-510) and the struct defaults in
`Source/RiorsEdge/Weapons/BreakerWeaponDefinition.h`. **All of them are O2
PLACEHOLDER.** They drive the silhouette; they are not authored here.

### 4.0 The numbers each silhouette states

| Archetype | Damage @ ilvl 1 | Cadence | Mag / reserve | Reload | Swap-in | Max range | Source |
|---|---|---|---|---|---|---|---|
| **Rifle** | 24.0 | 600 RPM, automatic | 30 / 120 | 1.8 s | 0.50 s | 12 000 cm | `BreakerWeaponDefinition.h:26,43-44,61-65`; the Rifle case is `default:` and overrides only the display name (`BreakerWeaponComponent.cpp:503-505`) |
| **SMG** | 13.0 | **900 RPM**, automatic | 35 / 175 | 1.5 s | 0.35 s | 6 000 cm | `BreakerWeaponComponent.cpp:329-350` |
| **Sniper** | **72.0** | 150 RPM, semi | **8** / 40 | 2.3 s | 0.70 s | **15 000 cm** | `:351-369` |
| **Shotgun** | 10.0 × **8 pellets** | 85 RPM, semi | 8 / 40 | 2.2 s | 0.50 s | **4 000 cm** | `:370-391` |
| **Rocket** | **90.0**, projectile at 3 200 cm/s, 350 cm blast | 55 RPM, semi | **4** / 16 | 2.8 s | 0.80 s | 12 000 cm | `:392-409` |
| **Burst Rifle** | 29.0 | 720 RPM **within a 3-round burst**, 0.34 s locked cycle gap | **27** (nine whole bursts) / 108 | 2.0 s | 0.55 s | 13 000 cm | `:416-445` |
| **Machinegun** | 11.0 | 700 RPM, automatic | **120** / 300 | **4.2 s** | **0.95 s** | 11 000 cm | `:446-475` |
| **Sidearm** | 21.0 | 420 RPM, semi | 14 / **210** | 1.1 s | **0.18 s** | 7 000 cm | `:476-502` |

> **Drift, flagged.** `BreakerViewmodelRig.cpp:45-46` and
> `Art-And-Modelling-Plan.md:689` both describe the rifle as *"35 rnd, 620 rpm"*.
> The live values are **30 rounds at 600 RPM** — the rifle takes the struct
> defaults (`BreakerWeaponDefinition.h:43,61`) because its `switch` case is
> `default:` and sets only a display name. The difference is cosmetic for a
> silhouette (a 30-round stick and a 35-round stick look the same) but the table
> above is the one that is true, and the two documents should be reconciled by
> whoever owns them.

### Rifle — the reference. Model this one first.

```
Basic first-person videogame weapon model: a standard-issue automatic assault
rifle, built for Unreal Engine import as static meshes (no rig, no bones, no
skeleton). Scale: modeled in real-world centimeters, approximately 80 cm
overall length, Z up, barrel pointing along +X. Origin of the model at the
TRIGGER GROUP / firing hand — the stock extends behind the origin into negative
X, the barrel extends in front into positive X.

Silhouette: long, straight, balanced, nothing exaggerated. Three shapes and no
more: one long horizontal receiver mass (about 34 cm long, 5 cm wide, 7 cm
tall), ONE break in the top line — a low boxy top-mounted optic sitting about
8 cm above the trigger group — and ONE break in the bottom line, a straight
30-round box magazine raked slightly forward. A fixed straight stock behind the
grip. A plain cylindrical barrel about 26 cm long running forward from the
receiver, ending in a slightly wider muzzle device.

Character: THE ONE GUN THAT LOOKS ISSUED. Uniform matte factory finish, zero
personalization, no tape, no paint, no hand-marking, no wear beyond even use.
It is the weapon every other gun in this armory will be compared against, so
it must be the most boring and the most correct.

Materials: one to three flat matte materials only, no textures, no decals.
Very dark desaturated gunmetal for the receiver and barrel, a slightly
different dark olive-toned polymer for the grip, stock and magazine. Nothing
saturated. Nothing teal or cyan anywhere — that color is reserved.

Deliver as FIVE SEPARATE MESHES in one file, not merged: body/receiver, barrel
with muzzle device, optic/sight, magazine, and the charging handle mechanism.
Low poly, blockout-to-first-pass quality, about 1,500-3,000 triangles total.
No environment, no hands, no arms, no base, single object, neutral orientation.
```

### SMG — 900 RPM, 35 rounds, and no stock

```
Basic first-person videogame weapon model: a cheap, mass-produced submachine
gun, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 40 cm overall — the SHORTEST long
gun in the set, half the length of a rifle. Z up, barrel along +X, origin at
the trigger group.

Silhouette: short, stubby, front-heavy. The single defining feature is that it
has NO STOCK AT ALL — nothing extends behind the grip. The second defining
feature is that the magazine is the LONGEST thing on the weapon: a 35-round
stick, raked slightly forward, that looks disproportionately large for a gun
this small. That contradiction is the whole read — this thing fires 900 rounds
per minute and it is carrying the ammunition to do it in a body that is barely
there.

Character: cheap and replaceable. Stamped sheet metal and visibly injection-
molded polymer, mold seams and sprue marks acceptable, a slightly LIGHTER and
more washed-out plastic than any other weapon in the set. This is the gun you
do not care about losing.

Materials: two flat matte materials, no textures. Pale-ish worn polymer for the
body, shell and magazine; very dark gunmetal for the short barrel and bolt.
Nothing saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel, sight, magazine, bolt. Low poly,
about 1,200-2,000 triangles. No environment, no hands, no base.
```

### Sniper — 8 rounds, 150 RPM, and the most personal object in the game

```
Basic first-person videogame weapon model: a long-range bolt-action sniper
rifle, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 96 cm overall — the LONGEST
weapon in the set by about 20% over a standard rifle. Z up, barrel along +X,
origin at the trigger group.

Silhouette: extremely long and notably THIN — long WITHOUT being bulky. A
narrow receiver (about 30 cm long but only 4.5 cm wide) and a very long thin
barrel (about 44 cm) that is thinner than a rifle's despite the gun being
longer. The single dominant feature, which no other weapon in this set is
allowed to have: an OVERSIZED CYLINDRICAL OPTIC that visibly breaks the top
line, roughly 26 cm long and 7 cm in diameter, sitting about 12 cm above the
trigger group — a scope that looks too big for the gun it is on.

Character: PERSONALLY MAINTAINED, the most individual object in the armory.
Add a small flat off-white taped card on the top of the stock at the cheek
rest — a hand-marked range card, roughly 7 x 5 cm, plainly stuck on by the
owner. Bedding compound at the action, tape at the barrel band, a stock that
has been shaved to fit somebody's face. Nothing factory about it.

Materials: three flat matte materials, no textures. Dark gunmetal receiver and
optic, darker still barrel, dark olive polymer stock and grip, and ONE small
off-white patch for the taped card — that patch is the only light value on the
whole weapon and it must read at a glance.

Deliver as FIVE SEPARATE MESHES: body, barrel, scope, magazine, bolt handle.
Low poly, about 1,500-2,500 triangles. No environment, no hands, no base.
```

### Shotgun — 8 pellets per trigger pull, and a mechanism you can watch

```
Basic first-person videogame weapon model: a pump-action combat shotgun, built
for Unreal Engine import as static meshes (no rig, no bones). Scale: real-world
centimeters, approximately 76 cm overall. Z up, barrel along +X, origin at the
trigger group.

Silhouette: the WIDEST and THICKEST weapon in the set — blunt, heavy, closer to
a tool than to a firearm. A thick receiver about 26 cm long, 8 cm wide, 9 cm
tall. A fat barrel about 5 cm in diameter.

TWO dominant features, and no other weapon may borrow either:
1. A TUBE MAGAZINE slung UNDERNEATH the barrel, running most of the barrel's
   length, clearly a separate parallel cylinder — the read that says "this
   holds shells, not a box magazine."
2. A PUMP on that tube, and it must be the BRIGHTEST VALUE on the whole
   weapon: a bare bright-steel sleeve, visibly a moving part, positioned where
   the support hand goes. The player watches this thing move when they reload,
   so it has to be obviously a mechanism and obviously separate from the body.

Character: industrial, blunt, unglamorous, honestly worn. Not tactical, not
sleek. Something taken out of a workshop.

Materials: three flat matte materials, no textures. Dark gunmetal receiver and
barrel, dark olive polymer stock and grip, and BRIGHT BARE STEEL for the pump
and the front bead sight — that value contrast is doing all the work. Nothing
saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel, sight, tube magazine, pump. Low
poly, about 1,500-2,500 triangles. No environment, no hands, no base.
```

### Rocket — the one that breaks the shoulder line, and the only hazard amber in the game

```
Basic first-person videogame weapon model: a shoulder-fired, field-fabricated
rocket launcher, built for Unreal Engine import as static meshes (no rig, no
bones). Scale: real-world centimeters, approximately 83 cm overall, and it is
the LARGEST SINGLE PART of any weapon in the set — one tube about 66 cm long
and 11 cm in diameter. Z up, tube along +X, origin at the firing grip.

Silhouette: a TUBE, mounted HIGH — about 6 cm above the grip line, high enough
that it visibly breaks the shoulder line rather than sitting in front of the
chest. It is the only weapon in this set that does that. Deliberately
ASYMMETRIC: a small simple box sight offset to the LEFT SIDE of the tube, not
on the centre line. A forward flare (a short cone widening toward the muzzle)
and a REAR BACK-BLAST CONE widening backward behind the grip. Two grips: a
firing grip at the origin and a forward grip about 27 cm out.

Character: FIELD-FABRICATED. Welded rather than machined, uneven seams,
hand-cut plate, and hand-painted markings. It should look like it might not be
entirely safe to fire. This is the only weapon in the game permitted a bright
color, and it gets exactly one: a HAZARD AMBER / warning-orange band wrapping
the tube roughly a third of the way along, plainly hand-painted rather than
printed.

Materials: three flat matte materials, no textures. Very dark gunmetal for the
tube and cones, dark olive polymer for both grips, and ONE hazard amber band.
No other saturated color. Nothing teal or cyan anywhere.

Deliver as FIVE SEPARATE MESHES: body/tube, forward muzzle flare, offset sight,
grip, rear vent cone. Low poly, about 1,200-2,000 triangles. No environment, no
hands, no base.
```

### Burst Rifle — three rounds, a gap you cannot shorten, and a ladder you memorise

```
Basic first-person videogame weapon model: a precision burst-fire marksman
rifle, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 89 cm overall — longer than a
standard rifle but visibly LIGHTER than one. Z up, barrel along +X, origin at
the trigger group.

Silhouette: rifle-shaped but built for discipline rather than volume. Three
differences from a standard assault rifle, and they must all be visible at a
glance:
1. A LONGER, THINNER barrel — about 34 cm and noticeably narrower than a
   standard rifle's, so the weapon reads as reaching further.
2. The dominant feature: a TALL DEDICATED OPTIC sitting on a VISIBLE RISER
   BLOCK, about 12.5 cm above the trigger group — clearly a separate mount
   lifting the scope well clear of the receiver, unlike a rifle's flat boxy
   optic sitting low. The riser is as much of the read as the scope is.
3. A STRAIGHT IN-LINE STOCK running level with the barrel line, no drop, no
   comb — the stock of something you shoot from a supported position.

Character: this weapon fires a fixed three-round burst and then WAITS, and it
cannot be hurried. Whoever carries it pre-aims. It should look considered and
slightly precious next to the standard rifle — better made, more deliberate,
less issued. No personalization or hand-marking (that belongs to the sniper);
this is quality from the factory, not care from the owner.

Materials: two to three flat matte materials, no textures. Dark gunmetal
receiver, riser and optic; darker thin barrel; dark olive polymer for the grip
and in-line stock. Nothing saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel, optic (with its riser as part of
the optic mesh), magazine, charging mechanism. Low poly, about 1,500-2,500
triangles. No environment, no hands, no base.
```

### Machinegun — 120 rounds, a 4.2 second reload, and "only while planted"

```
Basic first-person videogame weapon model: a belt-or-drum-fed light machine
gun, built for Unreal Engine import as static meshes (no rig, no bones).
Scale: real-world centimeters, approximately 83 cm overall, but it must read as
the HEAVIEST OBJECT IN THE GAME — more than twice the visual mass of a standard
rifle at similar length. Z up, barrel along +X, origin at the trigger group.

Silhouette: thick everywhere. A heavy receiver about 34 cm long, 8 cm wide,
10 cm tall. A SHROUDED barrel — a fat perforated or ribbed sleeve about 7 cm in
diameter, not a bare tube.

TWO dominant features, and they are the whole archetype:
1. A DRUM MAGAZINE, and it must be THE BIGGEST SINGLE PART ON ANY WEAPON IN
   THE SET — a cylinder roughly 17 cm across lying ON ITS SIDE (its axis
   pointing left-right, across the weapon, not along it), slung under the
   receiver. It holds 120 rounds and it has to look like it. Nobody should have
   to be told what it is.
2. A BIPOD, folded down and deployed, two thin legs splaying forward and
   outward from under the barrel about 14 cm long. This weapon's recoil is
   designed to become unmanageable if you keep firing while moving, so the gun
   itself must say "planted" before the player has fired a shot.

Character: heavy, agricultural, unsubtle. Something two people were meant to
carry.

Materials: three flat matte materials, no textures. Dark gunmetal receiver and
drum, darker shrouded barrel, dark olive polymer stock and grip, and BRIGHT
BARE STEEL for the bipod legs only — so the legs read as separate hardware
against the dark mass above them. Nothing saturated, nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body, barrel shroud, sight, drum, bipod. Low
poly, about 2,000-3,000 triangles given the drum and bipod. No environment, no
hands, no base.
```

### Sidearm — 0.18 seconds to bring up, and it must read as *nothing*

```
Basic first-person videogame weapon model: a compact semi-automatic pistol,
built for Unreal Engine import as static meshes (no rig, no bones). Scale:
real-world centimeters, approximately 22 cm overall — the SMALLEST weapon in
the set by a factor of three or more against any long gun, and roughly one
fourteenth the visual mass of the machine gun. Z up, barrel along +X, origin at
the trigger group.

Silhouette: almost nothing. A slide about 17 cm long, 3.4 cm wide, 5.4 cm tall,
sitting on a grip. NO STOCK. NO SEPARATE MAGAZINE PROTRUDING — the rounds live
inside the grip, so the bottom line of this weapon is unbroken, which is the
opposite of every other weapon in the set. A very short barrel, barely
protruding. Two tiny sights, front and rear, and they are the only fine detail
on the object.

Character: the whole point of this weapon is that it comes up in under a fifth
of a second. The read the instant it appears must be "there is almost nothing
in my hands." Do not add a rail, a light, a compensator, a threaded barrel, or
any accessory. Every gram you add to this silhouette destroys the archetype.

Materials: two flat matte materials, no textures. Dark gunmetal slide and
barrel, dark olive polymer grip, and BRIGHT BARE STEEL on the two tiny sights
only — small bright points on an otherwise dark object. Nothing saturated,
nothing teal or cyan.

Deliver as FIVE SEPARATE MESHES: body/frame, barrel, sights, magazine
(internal, still its own mesh for the reload), slide. Low poly, about 800-1,500
triangles. No environment, no hands, no base.
```

---

## 5. The player

### 5.1 What the first-person view actually shows

**Two forearms and two gloves, and nothing else.** No torso, no legs, no head,
no shadow, no reflection. The player character in first person *is* the arms.
That is not a simplification for this brief — it is the shipping state
(§2.2), and it is why §8 puts the arms above five of the eight weapons.

The support forearm crosses the frame diagonally from the lower-left corner to
the weapon's handguard; the firing forearm enters from the lower-right and is
mostly occluded by the weapon body. Both are clipped at the shoulder by the near
plane, deliberately. Both ride the recoil spring with the gun.

### 5.2 The prompt

```
Basic first-person videogame ARMS model: a matched pair of forearms and gloved
hands for a first-person shooter viewmodel, seen from the wearer's own eyes.
Built for Unreal Engine. Scale: modeled in real-world centimeters — a forearm
is about 6 cm across at the wrist and a gloved hand is about a 9.5 cm cube.
Z up, arms extending along +X away from the viewer. The arms are cut off at the
ELBOW or above, open-ended, NOT capped or finished at the shoulder end — that
end is clipped off screen and must never be seen, so do not model a shoulder,
a torso, a neck, or a head. Nothing above the elbow exists.

Pose: bind pose / T-pose forearms with the hands relaxed but shaped to grip —
one hand shaped to wrap a pistol grip, one shaped to wrap a horizontal
handguard. Do not model them holding a specific weapon; the weapon is a
separate object placed between them.

Character: MILITIA, NOT MILITARY. Equipped by an organization that has money
for the things that keep people alive and nothing left over. The read is
competent, funded unevenly, personally maintained. Layer grammar from the skin
outward: a plain dark bodysuit sleeve, soft armor at the forearm, one hard
plate strapped over the outside of the forearm, and a webbing strap or two.
Every hard edge is scuffed and every recess is clean — wear happens where a
body touches things. Add exactly ONE personal item: a wrist wrap, a taped ring,
or a small charm, plainly added by the wearer rather than issued. It is the
cheapest humanizing detail available and it is the only decorative element
permitted.

CRITICAL NEGATIVES: no hook, no grapple mount, no hardpoint, no tether, no
launcher, no wrist-mounted device of any kind — the game has no such verb and
this model must not promise one. No exposed skin above the glove. No unit
insignia large enough to read.

Composition: THE FOREARM IS THE LARGEST SHAPE ON SCREEN AND MUST BE THE
QUIETEST. Value it DARKER than the weapon it holds and give it less surface
detail than the weapon, because the weapon carries the information and the arm
must not compete with it.

Materials: two flat matte materials, no textures needed. Very dark olive for
the gloves, a slightly lighter dark slate for the sleeve and forearm armor.
Nothing saturated. NOTHING TEAL OR CYAN anywhere — that color is reserved for
rift objects and the player may never carry it.

Deliver GLOVES AND FOREARMS AS SEPARATE MESHES (gloves are a real equipment
slot and will be swapped independently), plus the personal item as its own
small mesh. Rig to a standard UE5-Manny-compatible arm hierarchy if rigging at
all, and ALSO export the bind pose as a static mesh — the game currently
consumes static meshes only. About 4,000-6,000 triangles for the pair. No
environment, no weapon, no base, black background.
```

### 5.3 What is NOT being asked for, and why

- **No third-person body.** Nothing renders one. See §10.
- **No animation set.** `Art-And-Modelling-Plan.md:426-437` lists the motion set
  the arms will eventually need (idle, sprint, slide enter/loop/exit, wall ride,
  dash, fall, air jump, and per-archetype fire/reload/aim/swap). None of it is
  consumable today — the arms are posed by C++ maths every frame
  (`BreakerCharacter.cpp:832-867`), not by an anim graph.
- **No dodge, block, or parry pose. Author none.** This is ruling **O1** and
  `Art-And-Modelling-Plan.md:490-502` is emphatic about it: dodge and block are
  passive chance layers with no input, no stance and no key, so a dodge-roll or
  shield-raise animation would lie to the player about the control scheme. It is
  named there as *"the single most likely art mistake in the project."*

---

## 6. Gear — one or two per slot

### 6.1 The eight slots, as the code defines them

`EBreakerEquipSlot` (`Source/RiorsEdge/Items/BreakerItemTypes.h:9-21`) — enum
order, which is the save-data order and therefore permanent:

`Helmet, BodyArmour, Gloves, Boots, Necklace, Waist, Primary, Secondary`

The UI presents them in **wear order**, which is different and which is the order
an artist should think in (`BreakerMenu.cpp:6585-6586`, labels at `:2353-2358`):

| # | Slot | UI label | Covered by |
|---|---|---|---|
| 1 | Helmet | `HELMET` | §6.3 |
| 2 | BodyArmour | `BODY ARMOUR` | §6.4 |
| 3 | Gloves | `GLOVES` | §6.5 — **shares authoring with the first-person gloves in §5** |
| 4 | Waist | `WAIST` | §6.6 |
| 5 | Boots | `BOOTS` | §6.7 |
| 6 | Necklace | `NECKLACE` | §6.8 |
| 7 | Primary | — | §4, the eight archetypes |
| 8 | Secondary | — | §4, the eight archetypes |

So **six armour slots × two variants = twelve pieces**, plus the eight weapons
already covered. That is "one or two of everything."

### 6.2 What makes a set read as a set

Six loose objects on six body parts do not read as equipment; they read as
clutter. Four rules, drawn from `Art-And-Modelling-Plan.md:270-275, 708-712`:

1. **One fastener grammar.** Pick one buckle, one strap width, one webbing
   weave, one plate-edge profile, and use those and only those on all twelve
   pieces. This is the single cheapest thing that makes twelve objects look like
   one kit, and it is the same rule the weapons follow ("the same fastener and
   rail vocabulary, so they read as one armoury").
2. **One layer order, visible at every edge.** Inside to out: bodysuit → soft
   armour vest → hard plate at chest and shins → webbing and pouches → outer
   shell garment. Every piece should show at least two of those layers meeting,
   so the eye can tell where a piece sits in the stack.
3. **One wear rule.** Scuffed at every hard edge, clean in every recess. Wear is
   where a body touches things — never uniform grime, never random damage.
4. **Light at the shoulders and hips.** The player wall-rides, slides and
   double-jumps. `Art-And-Modelling-Plan.md:272` makes this binding: *"A player
   who wall-rides and slides cannot look like a heavy infantryman."* Armour the
   torso and shins; keep the shoulders and hips unarmoured and mobile.

**The two variants per slot are not "better and worse."** They are the same kit
made by two different processes, which is what a militia looks like:

- **Variant A — ISSUED.** Factory-made, uniform, correct, slightly impersonal.
  Matches the rifle's character exactly. This is the baseline every other item is
  read against.
- **Variant B — SALVAGED.** The same function achieved with found material and
  hand tools: a plate cut from something else, a strap replaced with a different
  strap, one part obviously not original. Same silhouette family, visibly
  different provenance.

**Rarity is not the variant axis.** Rarity gets its own treatment ladder
(`Art-And-Modelling-Plan.md:454-463`): Standard is the base mesh, Uncommon a
material tint, Exceptional adds one small attachment, Aberrant adds an attachment
plus an emissive signature mark, and Anomalous gets a unique silhouette element
and is **the only rarity permitted rift chroma on the item itself.** That ladder
composes with A/B; it does not replace it. Commission A and B at Standard and let
the ladder ride on top.

### 6.3 Helmet ×2

```
Basic videogame armor model: TWO variants of a militia combat HELMET for a
first-person shooter's third-person character, built for Unreal Engine. Scale:
real-world centimeters, sized to a 176 cm humanoid — an adult head covering.
Z up, face along +X.

Both variants MUST LEAVE THE WEARER'S FACE VISIBLE. This is a hard constraint:
the game has a social hub where players see each other, and a helmet that hides
the face makes that space dead. Cover the skull, the temples and the jaw line;
leave the eyes, nose and mouth open. No full face plate, no mirrored visor, no
respirator covering the mouth.

VARIANT A — ISSUED: a clean factory-made shell, uniform matte, even edges, one
mounting rail on the left temple with nothing on it, a simple chin strap with
one buckle. Impersonal and correct.

VARIANT B — SALVAGED: the same shell profile achieved from cut and welded plate
— a visible seam where two pieces were joined, a mismatched replacement strap, a
patch of a different material riveted over what was presumably a hole. Same
silhouette, obviously repaired.

Shared grammar both variants must use: one strap width, one buckle type, one
plate-edge profile. Every hard edge scuffed, every recess clean.

Materials: two flat matte materials, no textures. Desaturated olive-slate shell,
darker webbing and straps. Nothing saturated. NOTHING TEAL OR CYAN — that color
is reserved and gear may not carry it.

Deliver the two variants as separate meshes in one file. About 800-1,500
triangles each. No environment, no head, no body, no base.
```

### 6.4 Body Armour ×2

```
Basic videogame armor model: TWO variants of a militia BODY ARMOUR torso shell
for a first-person shooter's third-person character, built for Unreal Engine.
Scale: real-world centimeters, sized to a 176 cm humanoid. Z up, chest along
+X.

BINDING SILHOUETTE RULE: armored at the CHEST and STOMACH, and deliberately
LIGHT AND MOBILE at the SHOULDERS AND HIPS. This character wall-rides, slides
and double-jumps, and cannot look like a heavy infantryman. No pauldrons, no
skirt plates, no bulk at the joints.

Layer grammar, visible where the edges meet: a plain bodysuit underneath, a soft
armor vest over it, ONE hard plate across the chest, then webbing and pouches on
top of that. The eye should be able to count those four layers at the vest's
hem and collar.

VARIANT A — ISSUED: a factory vest, even stitching, matched pouches in a
regular row, one clean rectangular hard plate, uniform color across every
component.

VARIANT B — SALVAGED: the same vest carrying a hard plate that was plainly cut
from something else — different material, different edge finish, held on with
straps rather than seated in a pocket. Pouches of three different origins. One
seam repaired by hand.

Shared grammar: one strap width, one buckle type, one plate-edge profile, one
webbing weave. Scuffed at every hard edge, clean in every recess.

Materials: two to three flat matte materials, no textures. Desaturated
olive-slate vest, darker webbing, a slightly different value for the hard
plate. Nothing saturated. NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file. About 1,500-2,000
triangles each. No environment, no body, no head, no arms, no base.
```

### 6.5 Gloves ×2

```
Basic videogame armor model: TWO variants of militia GLOVES, built for Unreal
Engine. Scale: real-world centimeters — a gloved hand is roughly a 9.5 cm cube,
sized to a 176 cm humanoid. Z up, fingers along +X.

IMPORTANT: these gloves are also seen in FIRST PERSON, filling the lower half of
the screen for the entire game. They get more scrutiny per triangle than any
other armor piece, and they must be authored SEPARATELY from the forearm so a
glove can be swapped without re-authoring the arm. Include a clean cut at the
wrist and a socket point at the wrist and at the knuckles.

VARIANT A — ISSUED: a clean tactical glove, uniform material, a simple knuckle
guard, one wrist strap with one buckle.

VARIANT B — SALVAGED: a work glove pressed into service — heavier, a mismatched
reinforcement patch stitched across the palm, the knuckle guard obviously a
separate piece added later and lashed on rather than seated.

Shared grammar with the rest of the kit: one strap width, one buckle type, one
plate-edge profile. Scuffed at the knuckles and fingertips, clean in the palm
creases — wear is where a hand touches things.

CRITICAL NEGATIVE: no wrist-mounted device, no hook, no grapple, no launcher,
no hardpoint of any kind. The game has no such verb.

Materials: two flat matte materials, no textures. Very dark olive glove body, a
slightly lighter dark slate for the knuckle guard and strap. Nothing saturated.
NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file. About 600-1,200
triangles each. No environment, no arms, no body, no base.
```

### 6.6 Waist ×2

```
Basic videogame armor model: TWO variants of a militia WAIST belt-and-pouch rig,
built for Unreal Engine. Scale: real-world centimeters, sized to a 176 cm
humanoid. Z up, front along +X.

This slot is cheap geometry with a high visual return, so it should carry more
readable detail per triangle than any other armor piece — the belt line is
roughly at eye level in a third-person view and it is where a character's
personality lives. It must stay LIGHT AT THE HIPS: no hanging skirt plates, no
thigh rigs that would foul a slide.

VARIANT A — ISSUED: a plain webbing belt with four matched pouches evenly
spaced, one buckle, one closed magazine carrier.

VARIANT B — SALVAGED: the same belt carrying five pouches of four different
origins — one canvas, one hard case, one plainly a repurposed tool pouch, one
lashed on with cord instead of clipped. Nothing matches and it clearly works.

Shared grammar: one strap width, one buckle type, one webbing weave. Scuffed at
every buckle and clasp, clean inside every pouch fold.

Materials: two flat matte materials, no textures. Desaturated olive-slate
webbing, darker pouch bodies. Nothing saturated. NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file. About 800-1,500
triangles each. No environment, no body, no base.
```

### 6.7 Boots ×2

```
Basic videogame armor model: TWO variants of militia BOOTS with shin protection,
built for Unreal Engine. Scale: real-world centimeters, sized to a 176 cm
humanoid — a boot and the shin above it. Z up, toe along +X.

This character's entire identity is movement — sliding, wall-riding,
double-jumping — and the boots are where that reads. They must look like they
are FOR that: a sole with visible edge grip, a reinforced toe, an ankle that is
plainly articulated rather than encased. HARD PLATE ON THE SHIN, nothing bulky
at the ankle or the hip end.

VARIANT A — ISSUED: a clean laced combat boot with a factory shin plate seated
in a fitted pocket, matched buckles, even sole.

VARIANT B — SALVAGED: the same boot with a shin plate strapped over the top
rather than seated, cut from a different material with a different edge, and a
sole that has plainly been re-glued at the toe.

Shared grammar: one strap width, one buckle type, one plate-edge profile.
Scuffed hard at the toe, the outer edge and the shin plate; clean in the lace
channel and behind the ankle.

Materials: two flat matte materials, no textures. Very dark boot body,
desaturated olive-slate shin plate. Nothing saturated. NOTHING TEAL OR CYAN.

Deliver the two variants as separate meshes in one file (model one boot each;
the pair mirrors). About 800-1,500 triangles each. No environment, no legs, no
body, no base.
```

### 6.8 Necklace ×2

```
Basic videogame accessory model: TWO variants of a small hanging NECK item worn
over a militia armor vest, built for Unreal Engine. Scale: real-world
centimeters, sized to a 176 cm humanoid — the hanging element is roughly 4-6 cm
across on a cord or chain about 45 cm long. Z up, front along +X.

This is the smallest and lowest-priority piece in the kit and it must not
pretend otherwise. It hangs at the sternum, over the vest, and its only job is
to be a legible small shape against a large flat one. Do not make it jewelry —
this is militia, not fantasy.

VARIANT A — ISSUED: a plain flat identification tag on a beaded chain, stamped
and unremarkable, exactly the thing an organization hands out.

VARIANT B — PERSONAL: something the wearer chose — a small worn object on a
leather cord, plainly not issued, plainly kept. A keepsake, not a trinket.

Shared grammar: the same cord and clasp hardware as the rest of the kit, at the
same scale.

Materials: one to two flat matte materials, no textures. Dull metal for the tag,
dark cord. Nothing saturated. NOTHING TEAL OR CYAN — that color is reserved for
rift objects and Anomalous-rarity items only, and this piece is neither.

Deliver the two variants as separate meshes in one file. About 200-500 triangles
each. No environment, no body, no base.
```

---

## 7. Icons — the Destiny read

### 7.1 The honest state, first

Three things are true of the current build and all three were verified by
reading, not assumed:

1. **There is no icon field on anything.** `FBreakerItemInstance`
   (`BreakerItemTypes.h:261-315`) carries an id, a definition id, a slot, a
   rarity, an item level, affixes, a save version, a weapon archetype, a rule and
   a legendary id. No icon, no texture, no brush, no soft object path. Grepping
   `Icon` across all of `Source/RiorsEdge/` returns eight hits and **every one is
   a comment**, all about *ability* icons in the HUD
   (`BreakerPlaytestHUD.cpp:2246,2280,2440,2549` and four more).
2. **No UI code in this project loads or draws an image, at all.**
   `FSlateBrush`, `FSlateImageBrush`, `FSlateDynamicImageBrush`, `UTexture2D`,
   `SImage` and `IImageWrapper` **do not appear anywhere in `Source/`.** Every
   surface in the menu is `FCoreStyle::Get().GetBrush("WhiteBrush")` — a solid
   1×1 white — tinted by `BorderBackgroundColor` (the ten call sites are
   `BreakerMenu.cpp:100,112,128,148,904,971,2600,3304,4611`), and every surface in
   the HUD is `AHUD::DrawRect` plus `FCanvasTextItem`. **Every item in this game
   today is text on a coloured rectangle.**
3. **The project cannot display a PNG file, and this is a build-level fact.**
   `Source/RiorsEdge/RiorsEdge.Build.cs` declares `Core, CoreUObject, Engine,
   InputCore, ApplicationCore, EnhancedInput, GameplayAbilities, GameplayTags,
   GameplayTasks, Slate, SlateCore` — **no `ImageWrapper`, no `ImageCore`, no
   `RenderCore`.** Runtime PNG decode is not linked into the module. **An icon
   must be imported in the Unreal Editor as a `T_` texture `.uasset`, and new UI
   code must be written to draw it.** That is greenfield in all three layers —
   the asset, a field on `FBreakerItemInstance`, and a draw path in both
   `BreakerMenu.cpp` (Slate) and `BreakerPlaytestHUD.cpp` (canvas). It is outside
   this document's territory and it is the gate on the whole icon set.

**The import path itself is proven, which is the good news.** `Content/` holds
302 `.uasset` files, including a real authored-mesh set — 52 static meshes and 9
materials at `Content/Breaker/Meshes/anchor_hub/` — plus **import scripts at
`Content/Python/`** that took them in. So the pipeline from external file to
working `.uasset` exists and has been used in this repo.

**What does not exist:** there are exactly **15 texture assets in the whole
project**, and every one is a UE-template character skin (`T_Manny_*`,
`T_Quinn_*`) or a dev grid checker (`T_GridChecker_A`). **There is no UI texture,
no icon atlas, and no `Content/UI/` directory at all.** The icon set below would
be the first UI texture the project has ever had.

(Note: the anchor-hub meshes are named lowercase-snake, not the `SM_` / `T_`
convention `Art-And-Modelling-Plan.md:526-529` asks for. Use the convention for
anything new.)

**And the specific thing the owner asked for — "see your item's image while it's
equipped" — has no home yet.** The HUD draws the equipped weapon as its
**uppercase archetype name in text**, auto-shrunk from 18 px to a floor of 11 px
(`BreakerPlaytestHUD.cpp:651-653`), and **equipped armour is not on the HUD at
all** (grepping `Equipment` / `GetEquippedItem` in `BreakerPlaytestHUD.cpp`
returns nothing). Neither is there a reserved square region on any item card or
equipment slot to put one in — every card is `[3 px rail][1 px ring][text rows]`
and nothing else. So the icons below are commissioned against a layout that does
not exist yet, and adding a 64 px thumbnail will reflow the equipment slot card
and the backpack card. That is a small, contained UI change, but it is a change.

### 7.2 The spec

| Property | Value | Where it comes from |
|---|---|---|
| Shape | **Square.** | Every slot and card region in the UI that could hold one is square or full-width; a non-square icon has nowhere to go. |
| Display size | **64 × 64 px** at 1920×1080 | **Measured off the shipping widgets, not chosen.** 64 is already a live number in this UI: the loadout screen's weapon-archetype tiles are `HeightOverride(64.0f)` (`BreakerMenu.cpp:2270`). It fits everywhere an item is drawn — an equipment slot card is `MinDesiredHeight(72.0f)` with 8 px of vertical content padding (`BreakerMenu.cpp:2631,2607`), so a 64 px square is the largest that fits its row without growing it; a backpack card is `WidthOverride(300.0f)` (`BreakerMenu.cpp:2766,2827`) with 16 px horizontal padding, leaving ~205 px for the name and affix lines beside a 64 px thumbnail. It also clears the two floors the style guide sets: the ability square at **56 px** (`BreakerUIStyle.h:165`) and the minimum hit target at **44 px** (`BreakerUIStyle.h:138`). 64 is the smallest power of two above both. |
| Authored size | **256 × 256 px**, power of two | 4× the display size. Covers 2× DPI, covers the hover/tooltip draw, and covers the paper-doll panel (`Art-And-Modelling-Plan.md:757` sizes its render target at 1024) without a second export. Downsampling to 64 is free; upsampling is not. |
| Background | **Fully transparent.** RGBA, straight or premultiplied alpha, no baked plate. | The plate, the border and every state overlay are drawn by UI code, exactly as with the ability icons — `UI-Ability-Icons-Spec.md:94-96` is explicit: *"no tile background baked in — the plate, border, and state overlays are drawn by UI code so that one glyph serves all four states."* An icon with its own baked background can never take a rarity treatment. |
| Optical box | The subject occupies a **44 × 44** box centred in the 64, i.e. **10 px clear on all sides** at display size (176 × 176 in a 256 authored) | Same proportion as the ability system's 36-in-52 with 8 px clear (`UI-Ability-Icons-Spec.md:16-18`), scaled to 64. The clearance exists so a 2 px rarity border and a 3 px rail can sit around the icon without touching it. |
| Colour | **Value, not hue.** One accent maximum, and it is the rarity colour, applied by code. | §7.4 |
| Format | 32-bit PNG for delivery; imported as a `T_` texture `.uasset` in-editor. Suggested paths, following `Art-And-Modelling-Plan.md:526-529`: `Content/ProjectBreaker/UI/Icons/T_Icon_Weapon_Rifle`, `T_Icon_Gear_Helmet_A`, etc. |

### 7.3 The prompt pattern

The set's legibility argument is that **every icon is the same drawing done to a
different object.** Use one block, substitute the subject line, and change nothing
else — consistency across the set matters far more than any individual icon.

```
A single item icon for a dark sci-fi looter-shooter inventory screen, in the
style of a technical equipment catalogue rather than an illustration.

SUBJECT: <<< one line, from the list below >>>

Composition: the object is shown in ORTHOGRAPHIC THREE-QUARTER VIEW from
slightly above and slightly to the left — the same camera for every icon in the
set, never straight-on and never dramatic. The object is centred and fills a
square optical box occupying about 70% of the frame, with clear empty space on
all four sides. It is a product shot, not a scene.

Rendering: flat, matte, near-unlit. Value contrast carries the entire read.
Three tones only — a dark body, one mid tone for the secondary material, and
one bright edge or highlight marking the object's single most identifying
feature. No gradients, no gloss, no rim lighting, no bloom, no lens effects, no
drop shadow, no ground shadow, no reflection.

Palette: desaturated militia hardware only — dark gunmetal, dark olive polymer,
one bare-steel bright. NOTHING SATURATED. Absolutely NO TEAL, CYAN, OR
TURQUOISE anywhere in the image; that color band is reserved and will be
applied separately by the interface.

Background: FULLY TRANSPARENT. No plate, no tile, no frame, no border, no
vignette, no backdrop of any kind — the interface draws its own plate behind
this and a baked background will destroy it.

Output: 256 x 256 pixels, square, 32-bit PNG with alpha. It must remain
identifiable when scaled down to 64 x 64 pixels, so no detail smaller than
about 4 pixels at that size, and no text.
```

**The eight weapon subject lines**, each stating the one feature that must
survive the 64 px downscale (they are the same dominant features as §4, because
an icon that disagrees with the viewmodel is worse than no icon):

| Icon | Subject line |
|---|---|
| Rifle | `a plain issued automatic assault rifle, seen side-on-ish; the read is one long balanced horizontal mass with a low boxy optic on top and a straight box magazine below` |
| SMG | `a stubby stockless submachine gun; the read is that it has NO STOCK and that its magazine is the longest thing on it` |
| Sniper | `a very long thin bolt-action sniper rifle; the read is an oversized cylindrical scope breaking the top line, plus one small off-white taped card on the stock — that pale patch is the only bright value` |
| Shotgun | `a thick blunt pump-action shotgun; the read is a tube magazine slung under the barrel and a bright bare-steel pump on it` |
| Rocket | `a field-welded shoulder-fired rocket tube; the read is one fat tube with a forward flare, a rear back-blast cone, and one hand-painted hazard-amber warning band — the only saturated color permitted in the entire icon set` |
| Burst Rifle | `a marksman's burst rifle; the read is a tall scope on a visible riser block lifting it well clear of the receiver, on a long thin barrel with a straight in-line stock` |
| Machinegun | `a heavy belt-fed machine gun; the read is an enormous side-mounted drum magazine and a deployed bipod` |
| Sidearm | `a small compact pistol; the read is that there is almost nothing to it — a slide, a grip with no protruding magazine, two tiny bright sights, and no accessories whatsoever` |

**The twelve gear subject lines** follow the same pattern: `a militia combat
helmet, issued variant — clean factory shell, face opening visible, one empty
temple rail`, and so on for each of the twelve pieces in §6. State the ONE
feature that separates variant A from variant B and nothing else; at 64 px that
is all that will survive.

### 7.4 How rarity reads

**The icon never carries its own rarity.** The interface does, and it already has
the palette (`BreakerUIStyle.h:71-95`, exact values, sRGB hex):

| Rarity | Colour | Token |
|---|---|---|
| Standard | `#DCE4EE` (off-white) | `RarityStandard` |
| Uncommon | `#408CFF` (blue) | `RarityUncommon` |
| Exceptional | `#B866FF` (violet) | `RarityExceptional` |
| Aberrant | `#FF4040` (red) | `RarityAberrant` |
| Anomalous | `#26F2D9` (teal) | `RarityAnomalous` |

Three rules that already exist in code and that any icon treatment must obey:

1. **Rarity is exactly three things and no others**, verified by reading every
   `RarityColor()` call site: a **3 px left rail** (`RailThickness`,
   `BreakerUIStyle.h:135`; drawn at `BreakerMenu.cpp:2327,2332` and
   `BreakerPlaytestHUD.cpp:1347`), **the text colour** of the item title
   (`BreakerMenu.cpp:2872`), and — **for Anomalous only** — a **1 px full ring**
   (`BreakerUIStyle.h:92-95` `RarityGetsFullBorder`; `BreakerMenu.cpp:2328-2334`),
   because it is the only rarity that is also a world object class.
2. **There is no rarity fill, deliberately.** Card faces stay `Panel10`
   (`#18263A`) at every tier. The reason is stated in the code at
   `BreakerMenu.cpp:2211-2212`: *"Card face stays panel/10 at every rarity so a
   wall of loot does not become a wall of colour."* An icon that bakes in a
   coloured background defeats that directly.
3. **Teal is reserved.** `#08B8A8` and `#26F2D9` are legal on rift geometry,
   suppression hardware, and Anomalous items — and *"never on chrome: buttons,
   rails, focus rings, tracks, tooltips"* (`BreakerUIStyle.h:65-69`). Ruling
   **O24** and `Art-And-Modelling-Plan.md:355-360` extend the same law into the
   world: the player's body, arms, gloves and weapons may never carry it. **This
   is why every prompt in this document bans teal explicitly.** An icon
   generator handed "sci-fi equipment" will reach for teal on the first try. It
   must not get away with it, because the moment a common rifle icon is teal, the
   colour has stopped meaning "Anomalous."

So the whole rarity read is: **the icon is monochrome militia hardware; the plate
around it is the rarity.** That is also what makes twelve gear icons and eight
weapon icons cost twenty drawings instead of a hundred.

---

## 8. Priority — the five assets that unblock the most

The owner wants variation testable soon. In order, with the reason each one is
above the next:

1. **The Rifle.** It is the default equipped weapon (`SlotOneArchetype` defaults
   to `Rifle`, `BreakerSaveGame.h:24`), it is the reference every other weapon in
   the armoury is described against, and it is the asset that establishes the
   fastener and rail vocabulary the other seven inherit. Nothing else can be
   judged until it exists. `Art-And-Modelling-Plan.md:672` rates it **P0** and
   says replacing the weapon mesh alone *"makes the game look 60% finished."*

2. **The first-person arms and gloves.** They are on screen behind all eight
   weapons, 100% of playtime, and they are the only part of the player character
   that exists at all (§2.2). One asset improves every weapon in the game
   simultaneously, which no weapon can do. `Art-And-Modelling-Plan.md:483` rates
   them **P0**, second only to the weapon.

3. **The Shotgun.** The other **P0** weapon and one of the three the vertical
   slice actually closes on (`Art-And-Modelling-Plan.md:664`). Against the rifle
   it is the first real variation test — widest against baseline, and it is the
   only weapon carrying a visible mechanism the player watches move. If rifle and
   shotgun do not read as different guns at a glance, nothing further should be
   commissioned until they do.

4. **The Sidearm.** The cheapest asset in the set (~800-1,500 tris, five small
   pieces) and the most different from the rifle — 22 cm against 80 cm, a
   fourteenth of the machinegun's mass, and the only weapon with an unbroken
   bottom line. It buys the *entire length axis*, end to end, for less work than
   any other weapon. With items 1, 3 and 4 the owner has three guns spanning the
   full silhouette range, which is enough to answer "does variation read" for
   real.

5. **The eight weapon icons, as one batch.** They are one afternoon of
   generation from a single prompt pattern (§7.3), they make Primary and
   Secondary look like a looter's slots instead of a stat sheet, and they are the
   first thing the owner asked for by name. **Conditional, and stated plainly:
   they cannot be seen until somebody writes the UI code to draw a texture** —
   there is none today (§7.1). If that code is not coming soon, promote the
   **Burst Rifle** into this slot instead, since it is the archetype most easily
   confused with the rifle and therefore the one whose silhouette most needs
   proving.

**Deliberately not in the five:** the twelve gear pieces. Nothing in the build
can render them (§2.3), so they would sit unused while three weapons and a pair
of arms would be on screen the day they import. Commission them second.

---

## 9. If the first result is unusable

Five failure modes, in descending order of probability.

1. **The generator returns a beauty render instead of a game asset** — dramatic
   rim lighting, a studio backdrop, depth of field, a ground shadow, reflective
   floor. This is by far the most likely failure for any "weapon" prompt, because
   the training data is overwhelmingly product photography and concept art.
   **Corrective phrasing:** *"This is a 3D game asset for real-time rendering,
   not a rendered illustration. Flat matte materials, uniform neutral lighting,
   no rim light, no depth of field, no reflections, no background, no ground
   plane, no shadow. If the image has a mood, it is wrong."* Adding the word
   *blockout* or *graybox* helps more than adding *low poly*.

2. **Teal appears anyway.** "Sci-fi weapon" pulls teal and cyan out of every
   generator on the market, and this project has one hard colour law
   (`BreakerUIStyle.h:65-69`, ruling **O24**). **Corrective phrasing:** *"There
   must be ZERO teal, cyan, turquoise, aqua, or any blue-green anywhere in this
   image — not on emissive panels, not on screens, not on indicator lights, not
   as a lighting tint. This palette is desaturated grey, olive and steel only.
   If you added a glowing blue-green element, remove it entirely rather than
   dimming it."* Naming the specific failure ("glowing panels") works better than
   restating the palette.

3. **The generator merges the model into one object, or rigs it.** Both break the
   pipeline: the code poses five pieces independently (§3) and consumes static
   meshes only. **Corrective phrasing:** *"Export as separate static meshes in one
   file — body, barrel, sight, magazine, mechanism — with no bones, no skeleton,
   no rig, and no skinning. Each named piece must be its own object, not a
   material group on a merged mesh."* If the tool insists on rigging, take the
   bind pose and strip the skeleton on import.

4. **Two archetypes come back looking the same.** The most likely pairs are
   rifle / burst rifle and rifle / machinegun. The fix is never "add more
   detail" — detail does not survive the 4-pixel blur (§3.1 rule 2).
   **Corrective phrasing:** *"These two must be distinguishable from silhouette
   alone, in solid black, at thumbnail size. Exaggerate the ONE difference: make
   the scope riser taller and the barrel thinner"* (burst rifle), or *"make the
   drum bigger than any part on any other weapon and deploy the bipod"*
   (machinegun). Exaggerating one feature beats adding five.

5. **The arms come back with a shoulder, a torso, or a face.** Prompts containing
   "arms" reliably summon a whole person. **Corrective phrasing:** *"Model ONLY
   from the elbow to the fingertips. There is no shoulder, no upper arm, no
   torso, no neck, no head, no body. The arms are cut off open-ended at the elbow
   — do not cap or finish that end."* If it still returns a figure, ask for
   "a pair of disembodied gloved forearms" explicitly.

A sixth, lower-probability one worth knowing: some generators normalise
everything to the same size, so the sidearm comes back as long as the sniper.
**Restate the absolute centimetre length, not a relative one** — "approximately
22 cm overall" corrects this reliably where "small pistol" does not. This is the
same fix the enemy brief found for kaiju-scaled creatures
(`Enemy-Model-Asset-Brief.md:222`).

---

## 10. Explicitly undetermined — do not infer beyond this document

- **Whether gear should be skeletal or static is unanswerable from the repo,
  because gear has no renderer at all.** `UBreakerEquipmentComponent` owns no
  visual component of any kind (§2.3), there is no third-person body, and the
  paper-doll is designed but unbuilt (`Art-And-Modelling-Plan.md:730-791`). The
  twelve pieces in §6 are commissioned against the *plan's* modular-slot scheme
  (`:443-452`), not against code. If that plan changes, they change.

- **Whether the player is Human or Effigy for the purposes of the first arms
  asset.** Ruling **O14** makes the Effigy a real second player model with Human
  shipping first (`Art-And-Modelling-Plan.md:277`), and commits to one shared
  skeleton for both before any arms work begins. Nothing in code expresses a
  player species. §5's prompt assumes Human, per O14's ordering.

- **The first-person FOV.** The code renders at 90° from one camera
  (`BreakerCharacter.h:308`); the art plan asks for a separate viewmodel FOV at
  70–80 (`Art-And-Modelling-Plan.md:399`). This document frames for 90 because
  that is what ships. Which is correct is not stated anywhere and is not resolved
  here.

- **Whether the authored arms parent to the camera or to the weapon rig.**
  `Art-And-Modelling-Plan.md:400` says arms to camera and weapon to arms; the
  code does the opposite deliberately, hanging both arms off the rig so the hands
  ride the recoil spring with the gun (`BreakerCharacter.h:133-136`). Both are
  defensible; the code's version is the one that has been looked at on a screen.
  A commissioned mesh does not care, but whoever writes the attachment code will.

- **Whether an item icon is per-definition, per-archetype, or per-slot.** There
  is no icon field on `FBreakerItemInstance` and no `UBreakerItemDefinition`
  asset class at all — items are rolled procedurally from a `DefinitionId`
  `FName` (`BreakerLootLibrary.cpp:47-59`). §7 assumes **one icon per weapon
  archetype and one per gear variant**, which is 20 drawings, because that is the
  only granularity the data model can currently key off. If item definitions ever
  become authored assets, this becomes a per-definition question.

- **No in-code polycount ceiling exists for anything.** The figures in §3 are
  this document's own recommendation for a blockout-to-first-pass commission,
  explicitly not sourced from a source-of-truth number, and adjustable. The
  finished-art figures (25k arms, 15–20k per weapon) are at
  `Art-And-Modelling-Plan.md:395-396` and are a later target, not this one.

- **Every number in the viewmodel table, the recoil table and the archetype
  prototype table is O2 PLACEHOLDER and none of it has been playtested.**
  Automation proves the silhouette *ordering* and the palette law; a screenshot
  pass proved the framing; nothing has proved how any of it feels. Model to these
  proportions and expect a pass.
