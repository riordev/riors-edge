# Enemy Model Asset Brief — commissioning prompts for AI asset generation

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40** (2026-08-14).

Owner: art / character direction. Status: **reference document for commissioning, not design.** This document authors no balance value and rules nothing; it exists to hand the owner ready-to-paste generation prompts for basic blockout-grade enemy meshes that can drop into the existing `Source/RiorsEdge/Combat/` enemy hierarchy without a code change. Where a number below is quoted from code it carries a file:line citation; everything else is qualitative, per **O2** (measure before authoring — no new value is invented here) and the project rule that dimensions not already in code are described, not guessed.

Reads from: `Docs/Design/Encounter-Design.md` (archetype identity, read-at-distance), `Source/RiorsEdge/Combat/BreakerEnemy.cpp` (current body construction), `Source/RiorsEdge/Combat/BreakerWardenEnemy.cpp`, `Source/RiorsEdge/Combat/BreakerSkirmisherEnemy.cpp`, `Source/RiorsEdge/Combat/BreakerBossEnemy.cpp`, `Source/RiorsEdge/Weapons/BreakerWeaponComponent.{h,cpp}` (weak-point hit test), `Docs/Design/Art-And-Modelling-Plan.md` (palette, direction pillars), `Docs/Design/Decisions.md` ruling **O24** (world aesthetic).

---

## 1. What the current build actually is — read this before writing a prompt

Every enemy in the shipping slice (`ABreakerEnemy` and its subclasses `ABreakerWardenEnemy`, `ABreakerSkirmisherEnemy`, `ABreakerBossEnemy`) is built from **`/Engine/BasicShapes` primitives — cubes, spheres, a cylinder — each an `UStaticMeshComponent` with `NoCollision`**, tinted by a dynamic instance of `BasicShapeMaterial` (`BreakerEnemy.cpp:33-44`, `60-134`). The comment at the top of the constructor states the load-bearing fact directly:

> "Humanoid silhouette from basic shapes: torso, head, two arms, two legs. Purely cosmetic — every piece is NoCollision and the capsule, hit box and weak point keep doing all the collision work." (`BreakerEnemy.cpp:56-59`)

This has two consequences for anything commissioned to replace these primitives:

1. **The mesh is decoration. All hit detection runs against separate, invisible primitive shapes that never move with the mesh's triangles.** A generated model's silhouette must line up with those primitives; its topology never has to carry collision.
2. **The three normal archetypes (Skitter, Lattice, Warden) and the Skirmisher and boss all share one body hierarchy** — same capsule root, same six cosmetic parts, same weak-point sphere — with archetype identity added as extra attached primitives (a shield for the Warden, an insignia plate and muzzle light for the Skirmisher, a command apparatus for the boss) and a material colour swap. A commissioned model is a **drop-in replacement for that shared hierarchy**, not a bespoke skeleton per archetype.

---

## 2. Technical constraints — non-negotiable

| Constraint | Value | Source |
|---|---|---|
| Mesh type | **Static mesh required, not skeletal.** Every visual part is `UStaticMeshComponent`; there is no skeleton, no bones, and nothing in the enemy actors reads animation. Do not commission a rigged/skinned mesh — it will not attach to anything skeletal because nothing skeletal exists here. | `BreakerEnemy.cpp:64,72,80,89,98,106,129` (all `UStaticMeshComponent`) |
| Up axis / forward axis | **Z up, X forward, Y right** — standard Unreal convention, and the convention the codebase's own modelling notes already assume for the player rig. | `Docs/Design/Art-And-Modelling-Plan.md` §3.2.2 ("Axes: X forward, Y right, Z up. Matches Unreal, matches the code.") |
| Scale / units | **Centimetres.** The root collision capsule is `InitCapsuleSize(45.0f, 90.0f)` — 45cm radius, 90cm half-height, i.e. a **180cm-tall capsule**. All cosmetic parts are authored as offsets from that capsule's origin in centimetres. Model to this scale; do not scale on import. | `BreakerEnemy.cpp:53` |
| Body proportions already committed in code | Torso (cube) scaled `(0.55, 0.36, 0.78)` at relative Z=22; head (sphere) scaled `0.34` uniform at relative Z=78; each arm (cube) scaled `(0.18, 0.18, 0.62)` at Y=±34, Z=24, tilted 12°; each leg (cube) scaled `(0.22, 0.22, 0.80)` at Y=±14, Z=-50. These are all relative to a 100cm default `/Engine/BasicShapes` cube/sphere, so they translate directly to real-world centimetre dimensions (e.g. torso ≈ 55×36×78cm). A commissioned mesh does not have to hit these exactly, but should read as the same rough mass distribution — low legs, compact torso, small head — since encounter geometry (lunge ranges, sight lines) was tuned against this silhouette. | `BreakerEnemy.cpp:64-112` |
| Material / texture | **Single flat-colour material, no baked textures.** Every part uses one dynamic instance of the engine's `BasicShapeMaterial` with a single `Color` vector param (e.g. the current Vestige-ish body tint is `FLinearColor(0.35, 0.32, 0.42)`, a muted grey-violet — `BreakerEnemy.cpp:41`). Commission (or export) a mesh that works fully untextured, one to three flat materials at most. | `BreakerEnemy.cpp:33-44` |
| Polycount ceiling | **Not sourced from code** — the current build is literally zero-authored-poly engine primitives, so there is no in-code ceiling to quote. Recommendation only, flagged as such: this is a blockout pass, so treat **under ~2,000–3,000 tris per normal enemy** as a sane ceiling for something meant to look like a graybox humanoid, not a shipping asset. `Art-And-Modelling-Plan.md` §2.1 quotes 10-16k tris for the eventual *finished* Vestige models — that is a post-slice target for real art, not this brief's basic blockout. | Recommendation only; finished-art figures at `Art-And-Modelling-Plan.md` §2.1 |
| Named parts / sockets the code expects | The code does not read named sockets off an imported mesh at all — everything is a separate `UStaticMeshComponent` attached in C++. So there is nothing to name inside the commissioned mesh file itself. What matters is that the **silhouette** supplies a torso mass, a head mass, two arms, two legs, and (for the Warden) a shield-shaped protrusion on the front, (for the Skirmisher) a chest plate and a muzzle point, and (for the boss) a back-mounted apparatus — because those are the shapes the collision primitives and telegraph reads are built against. | `BreakerEnemy.cpp`, `BreakerWardenEnemy.cpp:80-94`, `BreakerSkirmisherEnemy.cpp:47-65`, `BreakerBossEnemy.cpp:70-81` |
| Skeletal vs static — determined, not guessed | **Static.** Confirmed by reading the constructors of all four enemy classes; none creates a `USkeletalMeshComponent`, an `AnimBlueprint`, or a skeleton asset. | As above |

---

## 3. The weak-point system — what it actually is (important: it changes what geometry must provide)

**The weak point is a separate, invisible collision primitive — a `USphereComponent` tagged `"WeakPoint"` — attached to the root capsule. It is not a bone, not a UV island, and not read off the mesh's geometry at all.**

- Base enemy: `WeakPoint = CreateDefaultSubobject<USphereComponent>(...)`, radius **20cm**, at relative location `(0, 0, 78)` — roughly head height — tagged `"WeakPoint"`, colliding only on the weapon trace channel (`BreakerEnemy.cpp:121-127`).
- A small visual sphere (`WeakPointVisual`, scale 0.4, i.e. ~20cm sphere) sits inside/around it purely for legibility, and the code swells it during telegraph windows (e.g. the Skitter's lunge wind-up scales it up by `LungeWeakPointSwell = 2.1x`, `BreakerEnemy.h:297`).
- The weapon's hit-scan gives the player a forgiveness margin: `WeakPointToleranceCm = 14.0f` is added to the sphere's effective radius when resolving a shot (`BreakerWeaponComponent.h:326`, used at `BreakerWeaponComponent.cpp:1351-1368`), so the practical target is closer to a **34cm-radius sphere** even though the authored collision is 20cm.
- **The weak point is positioned by hand per archetype, not derived from the mesh.** It stays at the base humanoid's head position for Skitter/Lattice; the boss (`ABreakerBossEnemy`) never repositions it even though its narrative weak point is a back-mounted "command apparatus" mesh at `(-38, 0, 46)` (`BreakerBossEnemy.cpp:70-75`) — the collision sphere is inherited unmoved from the base class. **This is a code-side inconsistency, not something this document can fix**, and it means the *visual* apparatus and the *collision* weak point do not currently coincide for the boss. Flagging this explicitly rather than silently designing around it, per the brief's own instruction.

**What this means for the prompts below:** because collision is a separate sphere the artist/generator never has to model precisely, the ask on any generated mesh is a **visual tell only** — a distinct, legible protrusion, node, seam, or aperture at (or near) the stated location, sized roughly to a 20–35cm-radius feature on a 180cm-tall figure, that reads as "the thing to shoot" from 15–40m per `Art-And-Modelling-Plan.md` Pillar 1. It does not need pierceable geometry, a hollow, or separate UV space — it needs to be visually obvious and separately colourable/emissive so a future material pass can highlight it.

---

## 4. Palette — conform to O24, do not invent a style

**Ruling O24** (`Docs/Design/Decisions.md`): *world aesthetic is overgrown Earth — nature reclaiming ruins, slight sci-fi styling, functional/weathered/out-of-place tech.* `Art-And-Modelling-Plan.md` composes this with its own pillars, and the enemy-specific rules that matter for a prompt are:

- **Readability beats fidelity** (Pillar 1): every model must survive a flat-grey, 30m, untextured render — silhouette and value contrast are the entire budget.
- **Wrongness is compositional, not gory** (Pillar 4): Vestiges are wrong through asymmetry and repetition-at-the-wrong-scale, never through teeth or viscera. Gore is explicitly out of direction.
- **Teal is reserved** for rift-origin objects and Anomalous rarity only (`#3FD8C8`→`#0E5F5C` band) — never put it on a normal enemy's outward surface.
- **Vestige colour rule**: near-black, value-only masses, with rift chroma only visible through interior gaps, never on outward surfaces.
- **Altered colour rule**: desaturated olive-slate (`#4A5049`), warm off-white insignia (`#D8CFBA`), hazard amber (`#D89A2E`) — a militia palette, not sci-fi.
- **The code's own current placeholder tint** for the shared humanoid body is `FLinearColor(0.35, 0.32, 0.42)` — muted grey-violet (`BreakerEnemy.cpp:41`). Treat this as the working Vestige base tone until an art pass overrides it.

---

## 5. Family and severance — do the models need to express it?

Enemies carry two gameplay fields, `EBreakerEnemyFamily` (`Vestige` / `Altered`) and `EBreakerSeveranceStage` (`NotApplicable` / `Early` / `Mid` / `Late`), read every frame to gate cover-use and flinch behaviour (`BreakerEnemyFamily.cpp:31-45`). Roster placement, confirmed in code:

| Archetype | Family | Stage |
|---|---|---|
| Skitter, Lattice | Vestige | N/A |
| Severed Warden | Altered | **Mid** (`BreakerWardenEnemy.cpp:61-62`) |
| Severed Skirmisher | Altered | **Early** (`BreakerSkirmisherEnemy.cpp:23-24`) |
| Field Marshal (boss) | Altered | **Early** (`BreakerBossEnemy.cpp:47-48`) |

`Art-And-Modelling-Plan.md` §2.2 fixes what stage should look like (insignia completeness, kit shed, posture, rift chroma at joints) as a **post-slice, full-art** system with a three-stage ladder — that system is not built into these blockout meshes and the prompts below do not attempt it; they only need to land the single fixed stage each archetype is actually spawned at. **GAP, flagged rather than invented:** whether the owner wants blockout meshes to already gesture at stage (e.g. a torn insignia decal on the Warden) is not answered in the repo one way or the other — the prompts below include the stage-appropriate read only where the source archetype file explicitly builds a corresponding visual (e.g. the Warden's shield, the Skirmisher's insignia plate), and skip any stage detail not already in code.

---

## 6. Ready-to-paste prompts

Each block below is self-contained — no repo knowledge required to use it.

### Skitter (Vestige — melee pressure)

```
Basic low-poly blockout of a hostile alien creature for a video game, built for
Unreal Engine import as a single static mesh (no rig, no bones). Scale: roughly
150-180cm tall, modeled in real-world centimeters, Z-up, facing +X. Silhouette:
low, wide, dragging mass — wider than it is tall, hugging the ground. Give it
FIVE uneven limbs of visibly different lengths and thicknesses making ground
contact (never 2 or 4 — an even limb count reads as an animal or a person,
which this must not). No face, no eyes, no head as a distinct shape. No hands,
no held objects, no weapon. The body must be asymmetric on at least one major
axis — no mirror symmetry anywhere. On its upper/dorsal surface, model one
clearly distinct raised node or knuckle of geometry, about 30-40cm across,
that stands out from the rest of the body as an obvious "hit this" feature —
this is a weak point and must read instantly even in flat grey, untextured
lighting from 20-30 meters away. Keep the whole model to a single flat, matte,
unlit-looking material, near-black to dark muted grey-violet, no textures, no
surface detail like panel lines or rivets — one uninterpretable dark substance.
Low poly, blockout/graybox quality, under about 2,500 triangles. No environment,
no base, single centered object, neutral pose (not mid-attack).
```

### Lattice (Vestige — ranged/zone denial)

```
Basic low-poly blockout of a hostile alien creature for a video game, built for
Unreal Engine import as a single static mesh (no rig, no bones). Scale: roughly
180-220cm tall, modeled in real-world centimeters, Z-up, facing +X. Silhouette:
tall, thin, top-heavy, leaning slightly off-vertical as if unbalanced — reads
as fragile and unstable rather than sturdy. Give it THREE limbs total touching
or near the ground, one of them visibly vestigial/nonfunctional (thinner,
shorter, oddly placed). No face, no eyes, no head as a distinct shape. No
hands, no held weapon — this creature fires from its own body, not a gun. Must
be asymmetric on at least one major axis, no mirror symmetry. At the base of
the model, near where it meets the ground, model one clearly distinct bulbous
or nodal mass, about 30-40cm across, set apart from the rest of the silhouette
as an obvious "hit this" feature — a weak point that must read instantly in
flat grey, untextured lighting from 20-30 meters. Single flat matte material,
near-black to dark muted grey-violet, no textures, no surface detail. Low poly,
blockout/graybox quality, under about 2,000 triangles. No environment, no base,
single centered object, neutral standing pose.
```

### Severed Warden (Altered, mid-severance — anchor/pressure)

```
Basic low-poly blockout of a humanoid soldier-like enemy for a video game,
built for Unreal Engine import as a single static mesh (no rig, no bones).
Scale: roughly 180cm tall, modeled in real-world centimeters, Z-up, facing +X.
Silhouette: a heavy, upright humanoid — torso, head, two arms, two legs, in
roughly human proportion — advancing posture, weight forward, slightly
hunched, one shoulder dropped. It is carrying/wearing a large rectangular
shield-like slab across its front torso and forward arm, flat and plain, no
handle detail needed, roughly as tall as the torso — this shield is the single
biggest silhouette read and must be unmistakable at a distance. The FRONT of
the body (the shield side) should look armored, blocky, and featureless. The
BACK of the body — visible only from behind — should have an obvious exposed
area distinct from the front: a cluster of visible joints, seams, or an
unarmored gap in the plating, about 30-40cm across, positioned upper-back
between the shoulder blades, that reads clearly as a "hit this from behind"
weak point in flat grey lighting from 20-30 meters. One small rectangular
plate or patch on the front chest suggesting a worn rank/unit marking (no
readable text needed, just a distinct rectangular shape breaking the
silhouette) — this is a person who used to wear a uniform. Single or two flat
matte materials only: desaturated olive-slate for the body/kit, a slightly
different flat tone for the shield. No textures, no fine surface detail, no
face detail beyond a basic head shape. Low poly, blockout/graybox quality,
under about 3,000 triangles. No environment, no base, single centered object,
neutral standing pose facing forward.
```

### Severed Skirmisher (Altered, early-severance — cover user; not in the original three-archetype roster but shipped in code)

```
Basic low-poly blockout of a humanoid soldier enemy for a video game, built for
Unreal Engine import as a single static mesh (no rig, no bones). Scale:
roughly 180cm tall, modeled in real-world centimeters, Z-up, facing +X.
Silhouette: a leaner, more upright humanoid than a heavy trooper — torso,
head, two arms, two legs in human proportion, alert crouching-ready stance,
like a trained soldier using cover, not a shambling creature. On the front
chest, model one clearly distinct rectangular plate or patch, small and
sharply defined, breaking the silhouette — a worn insignia, the clearest
"this used to be a person, and recently" read in the whole enemy set. At one
shoulder/arm, suggest a small forward-projecting nub or point roughly where a
rifle muzzle would sit, without modeling a full separate weapon prop. Slight
asymmetry is fine (uneven kit, one shoulder heavier) but this is still
clearly a human silhouette, not an alien one. Single flat matte material in
desaturated olive-slate for the body, with the chest insignia patch in a
contrasting flat off-white or pale tone. No textures, no fine surface detail,
basic head shape only, no readable face detail required. Low poly,
blockout/graybox quality, under about 2,500 triangles. No environment, no
base, single centered object, neutral alert standing pose.
```

### Field Marshal (boss — Altered commander)

```
Basic low-poly blockout of a large humanoid commander enemy for a video game,
built for Unreal Engine import as a single static mesh (no rig, no bones).
Scale: roughly 300-320cm tall (the base humanoid frame scaled up by about
1.75x), modeled in real-world centimeters, Z-up, facing +X. Silhouette: the
same heavy armored soldier shape as a "Warden" archetype — upright, torso,
head, two arms, two legs, carrying a large rectangular shield-slab across the
front torso — but visibly more authoritative: add ONE additional silhouette
element no other unit in the set has, such as a stiff shoulder mantle, short
cape, or raised collar piece, to make it instantly distinguishable as a
commander before it does anything. Add more insignia than any other model:
multiple distinct rectangular patches/plates across the chest and collar area
(not just one shoulder), all breaking the silhouette clearly, all readable
as "rank" even without text. On the BACK, model one obvious raised
box-like or antenna-like apparatus mounted between the shoulder blades,
roughly 60-90cm tall, clearly a separate mechanical/technological attachment
rather than part of the body — this is a command device and must be
unmistakable in silhouette from behind, and should look like it could
visibly raise or tilt (model it in a neutral lowered/resting position). Front
of the body reads armored and blocky like the Warden; the back apparatus is
the standout unique feature. Two to three flat matte materials only:
desaturated olive-slate body, a warmer/richer or higher-contrast material for
the insignia and mantle to read as "higher rank," neutral dark tone for the
back apparatus. No textures, no fine surface detail. Low poly,
blockout/graybox quality, under about 4,000 triangles given the added
apparatus and insignia detail. No environment, no base, single centered
object, neutral standing pose facing forward.
```

---

## 7. If the first result is unusable

Three failure modes are the most likely, in order of probability:

1. **The generator gives the Vestiges a face, eyes, or bilateral symmetry.** This is the single most common failure for "alien creature" prompts, because most training data for creatures is animal- or humanoid-referenced and defaults to a face and left/right symmetry. **Corrective phrasing:** add explicitly "this creature has NO face, NO eyes, and is intentionally NOT bilaterally symmetric — if you gave it a face or made both sides mirror each other, that is wrong, try again with one side visibly different from the other." Repeating the negative instruction after showing a first attempt back to the tool usually works better than adding more positive description.

2. **The generator returns a rigged/skinned mesh, or a mesh split into dozens of disconnected pieces (fingers, individual armor plates, etc.) instead of one importable static mesh.** **Corrective phrasing:** add "export as a single merged static mesh with no bones, no rig, no skeleton, and no separate moving parts — treat this as one solid prop, not a character." If the tool insists on rigging, ask for the T-pose/bind-pose mesh only and plan to strip the skeleton on import (Unreal can still import it as a static mesh if the skin is removed, but confirm before committing budget to it).

3. **The weak-point feature does not read at distance, or gets lost in surface detail.** Because there is no code-side coupling between the mesh and the collision sphere (§3 above), a weak point that is merely implied by a texture seam or a subtle bump will not function as a *readable* tell even though it will still work mechanically wherever the invisible sphere is placed in the editor. **Corrective phrasing:** add "the weak point feature must be the single most visually distinct shape on the model — larger, brighter in value, or more geometrically separate than anything else on the body, because it needs to be identifiable from 20-30 meters in flat, untextured lighting." If it is still subtle, ask for it explicitly "protruding outward" or "a different, contrasting shape (e.g. a sphere or crystal node) embedded in an otherwise blocky body" rather than a flush panel.

A fourth, lower-probability failure worth knowing about: some generators default to armor or scale that reads as a large monster/kaiju rather than a roughly person-sized enemy. If the returned model is wildly larger or smaller than "about as tall as a person, maybe half again as tall for the boss," restate the explicit centimetre height from the prompt rather than a relative description ("человек-sized," "boss-sized") — absolute units correct this reliably where relative language does not.

---

## 8. Explicitly undetermined — do not infer beyond this document

- **The boss's visual apparatus and its collision weak-point sphere do not coincide in the current code** (§3). Whether that is an intended simplification or an oversight to fix is not stated anywhere in the repo. This document does not resolve it and the boss prompt above targets the back apparatus location as the *narratively* correct spot; if the owner wants the collision to actually move there, that is a code change outside this document's territory.
- **Whether blockout meshes should gesture at severance-stage detail (torn insignia, kit shed, etc.) at all** is not answered by any file read for this brief — `Art-And-Modelling-Plan.md`'s stage ladder (§2.2) is explicitly a post-slice, full-art system, not scoped to this pass. The prompts above stay conservative and only include the single insignia/shield detail each archetype already builds in code.
- **No in-code polycount ceiling exists for enemies at all** — the current build uses primitive shapes with no stated triangle budget. The ceilings suggested in §2 and used in the prompts are this document's own recommendation for a blockout pass, explicitly not derived from any source-of-truth number, and should be treated as adjustable.
- **Elite/modifier visual treatment (halos, tint) is a separate, already-built system** (`BreakerModifierComponent.cpp` — coloured sphere + point light, sized by modifier count) that composes with whatever base mesh ships here at runtime; it needs no mesh-level accommodation and is out of scope for these prompts.
