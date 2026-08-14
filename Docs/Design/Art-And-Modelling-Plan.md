# Art and Modelling Plan — Rior's Edge (Project Breaker)

**Scope:** post-slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Owner: Art / Character direction
Status: Design pass. No assets authored yet.
Scope: Vertical slice only, with production hooks for Act I–II.
Reads from: `CONTEXT.md`, `Docs/Layer-Ownership.md`, `Docs/Character-Progression-Architecture.md`, and — **for history only** — `Docs/Design/Master-Sheet-Import.txt`.

**AUTHORITY CORRECTION (O28).** This document was written treating the master
sheet's LOCKED decisions as law. **They are not.** `Master-Sheet-Import.txt` is
superseded historical source material and is not to be cited as authority again.
Every "master sheet N.N" citation below should be read as *"this is where the
idea came from"*, never as *"this is settled"* — several of them (the Anomalous
equip limit of 1, the rarity colour table, the unique-weapons question at 12.5)
have no corresponding entry in `Decisions.md` and are therefore **not ruled at
all**. Check the ledger before building against any of them.

**Rulings applied to this document** (`Docs/Design/Decisions.md` is law and supersedes any conflicting text here): **O1** (§0, §3.5, §7.4 Phase A — block/dodge passive, no stamina, Parry the only defensive input), **O5** (§2b — three elements, and the teal collision it creates), **O19** (§2b — the teal collision is RULED and closed; the elements are **Rift / Entropy / Void**, "Time" is renamed), **O9** (§2.1 — elites carry Modifiers), **O14** (§3.1 and the note before Phase A — two player models, shared rig). **O8** sweep: the content-type word "Anomaly" does not occur in this document, so no rename was needed; every "Anomalous" here is the **rarity tier**, which keeps its name.

This document decides what the game looks like and, more importantly, what gets built in what order by one developer with AI tooling. Every section ends with acceptance criteria that can be checked in Play In Editor, not argued about.

---

## 0. Reading the constraints before drawing anything

The master sheet already fixes most of the art brief. Restated as production law:

| Locked fact | Art consequence |
|---|---|
| Two enemy families: Vestiges (unreadable, non-humanoid, wrong) and Altered (severance stage readable from the model) | Two entirely separate silhouette languages and two separate rigs. Do not share a skeleton. |
| No Altered appears anywhere before the Act II turn | Slice content must be able to ship Vestige-only. Altered assets are gated content, not launch-day filler. |
| Altered early stage wears insignia, uses cover, flinches; late stage does none of these | Stage must be legible at silhouette range before any animation plays. Insignia is a *mesh-level* read, not a decal detail. |
| Anchors are functioning cities on visible suppression hardware nobody looks at anymore | Anchor is lived-in and mundane. Not a bunker, not a ruin, not neon. |
| Gear is the entire endgame; level cap 50 hard stop | Item visuals must carry rarity and slot identity clearly, because gear is the only long-term reward channel. |
| Solo is the primary balance target | Enemy readability from one player's single viewpoint is the priority, not group-legibility rules. |
| Accuracy While Airborne is the marquee affix ("make it visually obvious on the item") | Boots and gloves need a dedicated visual tell for airborne-build gear. |
| Crit is the only multiplier of its kind | One hit-feedback vocabulary for crit. Do not invent a second gold-numbers effect for anything else. |
| No grapple | No hardpoint, hook, or launcher geometry anywhere on the arms, gear, or level kit. Do not tease a verb that does not exist. |
| Dodge/block are passive chance layers, not inputs (**RULED [O1]**; stamina pool removed; Parry is the only defensive input, on its own cooldown) | **They must not have a triggered player animation that reads as a dodge roll.** See §3.5. This is the single most likely art mistake in the project. |

CONFLICT — the vertical slice as scoped in `CONTEXT.md` names three weapon archetypes, while **eight** now exist in code (Rifle, SMG, Sniper, Shotgun, Rocket, Burst Rifle, Machinegun, Sidearm — the last three added by the O27 breadth pass). This document plans art for all eight because the code shipped them, but marks five as **P1/P2 art** so the slice can still close on three finished weapons. §5 is written for the original five; **§5.1 is the live eight-archetype contract** and supersedes it where they disagree. All eight already have a distinct code blockout — see §3.0.

---

## 1. Global art direction pillars

Four pillars, ordered. When two conflict, the higher one wins.

### Pillar 1 — Readability beats fidelity, always
This is a fast first-person game where the player is airborne, sliding, and strafing. Every asset is judged at 15–40 metres, in motion, at 1/60th of a second. Silhouette and value contrast are the entire budget. A beautiful enemy the player cannot parse while wall-riding is a failed asset.

Practical rule: every enemy and every weapon must survive the **flat-grey test** — render at 0.5 saturation, uniform mid-grey material, 30m distance. If the family, the threat class, and (for Altered) the severance stage are not identifiable, the model is not done.

### Pillar 2 — Grounded near-future, one century forward, degraded outward
Alternate Earth, ~2125. Materials are recognisable: steel, concrete, polymer, ballistic nylon, cast aluminium, printed composite. Technology is *industrial*, repaired, and slightly heavier than it needs to be. Nothing floats without a reason. No holographic UI on world objects except where suppression hardware justifies it.

The reference axis is **field-repaired military hardware**, not sci-fi couture.

### Pillar 3 — The rift is a colour, and only the rift gets it
Reserve one narrow chroma band for rift-origin phenomena: rift portals, Vestige emissive, severance progression, Anomalous rarity. Everything else in the world sits in desaturated earth/steel/concrete. This makes rift material read as *foreign* without any writing.

Proposed reservation:
- **Rift chroma:** teal-cyan, roughly `#3FD8C8` → `#0E5F5C`, high-value, low-area. Also the Anomalous rarity colour, which is intentional: Anomalous items are rule-rewriters and should feel rift-adjacent.
- **Human/militia:** desaturated olive-slate `#4A5049`, warm off-white insignia `#D8CFBA`, hazard amber `#D89A2E`.
- **Vestige:** value-only. Near-black masses with rift chroma in the wrong places.

EXTENDS — the master sheet fixes Anomalous as Teal in the rarity table but does not reserve the colour globally. This document reserves it. If that is rejected, Vestige emissive must move to a different band and the rest of this plan is unaffected.

#### 2b collision — the Rift element wants the reserved teal

**RULED [O5] — three elements, not four**, with per-element resistances applied after armour and before shields. **RULED [O19] — the elements are RIFT, ENTROPY, and VOID** ("Time" is renamed to **Entropy**; every element reference in this document uses Rift / Entropy / Void). That ruling created a collision with Pillar 3, which **O19 also closes** (see below):

- **Pillar 3 reserves teal** (`#3FD8C8` → `#0E5F5C`) for **rift phenomena** — rift portals, Vestige emissive, severance progression, Anomalous rarity, suppression hardware. The reservation's entire value is that it is *narrow and rare*: teal means "foreign, rift-origin, uncommon."
- **The new Rift ELEMENT wants teal for routine damage VFX** — hits, ailment glyphs, DoT ticks, resistance feedback. That is high-frequency, high-area, and constant.

If both hold, teal appears on every Rift-element trigger pull and stops meaning anything. The rarest colour in the game becomes the most common one, and Pillar 3's "the rift is a colour, and only the rift gets it" is dead on contact with a Rift-damage build.

**RULED [O19] — CLOSED. Option 1 wins.** The former [OWNER-CHOICE] between "distinct stated shade" and "narrow the reservation" is decided:

- **Rift-element damage gets a distinct shade — a hotter, whiter cyan** — separate from the reserved teal band, so routine element damage and rift-origin phenomena never read as the same thing.
- **The Pillar 3 teal reservation stays intact exactly as written**: rift portals, Vestige emissive, severance progression, Anomalous rarity, suppression hardware. It is not narrowed.
- The rule, verbatim: **"saturated teal is a property of objects, not of damage."**

**No value is authored here.** "Hotter/whiter cyan" stays verbal — no shade, hex code, or band is written under the O2 value freeze. The specific shade is a separate art-authoring step.

Consequences, now binding:

- Rift-element damage VFX may be *directed* against the rule above, but **the shade itself is unauthored**. **GAP — the specific hotter/whiter cyan is not authored; that is an art-authoring step, not a design ruling.**
- `UI-UX-Spec.md` §4.5's element glyph colours are unblocked *in rule* and still **unauthored in value**: Rift damage follows the hotter/whiter cyan rule; Entropy and Void colours remain undesigned. **GAP.**
- The Vestige "zero rift-chroma on outward surfaces" rule and the Anomalous-only rift-chroma gear rule (§3.3) are unaffected — both are object chroma, and objects are exactly what saturated teal belongs to.

### Pillar 4 — Wrongness is compositional, not gory
Vestiges are not monsters with too many teeth. They are objects assembled by something that did not know what a body is for. Wrongness comes from symmetry violations, repeated modules at the wrong scale, and motion that does not match mass. Gore, viscera, and body-horror flesh are **out of direction** — they read as Earth biology and Vestiges are not from an Earth.

---

## 2. Enemy family art direction

### 2.1 Vestiges — the unreadable family

Design brief: *no design intent, no readable anatomy, no tactics that resemble a military* (master sheet 1.5). The art must actively refuse the player's pattern matching.

**Rules — binding on every Vestige model:**

1. **No bilateral symmetry.** Every Vestige is asymmetric on at least one major axis. This is the fastest way to defeat "it's a guy."
2. **No face, no head, no eyes.** If the player can find a face they will assume a mind. If a weak point is needed, it is a *structural* weak point (an exposed junction, a load-bearing node), never an eye or a head.
3. **No hands, no held objects.** Vestiges do not carry weapons. Attacks emerge from the body.
4. **Modularity at the wrong scale.** Repeat one geometric module 3–20 times at inconsistent scale across the body. Repetition reads as non-authored growth rather than design.
5. **Ground contact is wrong.** Limbs contact the floor at counts that are not 2 or 4 — 3, 5, 7, or a continuous dragged mass. Silhouette contact points are what the player's peripheral vision reads first.
6. **Emissive only at the interior.** Rift chroma appears in gaps and between plates, never as a surface glow. Suggests the thing is lit from a place that is not here.
7. **Materials are non-Earth.** No cloth, no metal panel lines, no rivets, no wear-and-tear storytelling. Surfaces are one uninterpretable substance: matte, slightly translucent at the edges, subsurface-dark.

**Slice roster — three normals plus one elite modifier (matches `CONTEXT.md` scope):**

| ID | Silhouette read | Threat role | Ground contact | Weak point | Poly budget (tris, LOD0) |
|---|---|---|---|---|---|
| VES_Crawler | Low, wide, dragging mass; wider than tall | Melee pressure, closes distance, punishes standing still | 5 uneven limbs | Dorsal junction, exposed on lunge windup | 12k |
| VES_Spitter | Tall, thin, top-heavy, leans wrong | Ranged arcing projectile, forces movement | 3 limbs, one vestigial | Base node at the ground contact | 10k |
| VES_Bulwark-mass | Blocky, near-cubic, near-immobile | Area denial / cover-destroyer, rewards flanking | Continuous, no discernible limbs | Rear seam, only visible from behind | 16k |
| Elite modifier | +50% scale, additional module repetition, rift-chroma bloom in the interior gaps | Existing `ConfigureElite` (1.5x scale, 3x health, 2x damage) | inherits | inherits | Material + scalar + **the shared modifier-VFX library** (see below), **no new mesh** |

**REVISED — elites are not free.** The elite modifier still costs **zero mesh authoring**, and that part holds: it is a scale multiplier plus a material parameter and one extra emissive mask, and no elite gets bespoke geometry. But the previous claim that elites cost *nothing* was wrong. Per the enemy taxonomy (Archetype / Rank / Modifiers), a Champion or Veteran carries modifiers that must be **read before the player is in trouble**, and a scale-and-tint pass cannot carry that read on its own.

**The real cost is ONE shared modifier-VFX library**, authored once and reused across every modifier that will ever exist. Roughly **six reusable effects**, each a parameterised Niagara/material asset, not a per-modifier bespoke effect:

| Reusable effect | Typical read |
|---|---|
| Capsule | A volume around the enemy body |
| Decal ring | A footprint on the ground at a stated radius |
| Tether | A line between two actors |
| Trail | A path the enemy has taken or will take |
| Polygon | An area boundary that is not a circle |
| Streak | A fast directional event |

Rules for the library:

- **One library, N modifiers.** Every modifier composes from these primitives plus colour and scale parameters. A modifier that cannot be expressed as a composition of them is a design problem to send back, not a licence to author a seventh effect.
- **No per-modifier meshes.** This is unchanged and remains binding.
- The library is authored **once**, in Phase C, and its cost is real and must be scheduled. Treat it as a first-class asset, not as polish.
- The library is not Vestige-specific — Altered elites reuse it identically.

**GAP — the modifier list itself is still undesigned**, so the mapping from modifier to effect composition cannot be written here. What is fixed is the budget shape: one shared library, ~six primitives, zero new meshes.

**Naming constraint honoured:** these are Vestiges (formal) / Spill (field slang). Do not name any of these "Aberrant" or "Anomalous" — those are rarity tiers (master sheet 1.2).

**Acceptance criteria — Vestiges:**
- [ ] Flat-grey test at 30m: a naive tester correctly names which of the three is on screen, 9/10 trials.
- [ ] No tester, when asked "what part of it is the head," gives the same answer twice.
- [ ] Each model has zero bilateral symmetry planes through the torso mass.
- [ ] Zero rift-chroma pixels on any outward-facing surface; all chroma occluded except through gaps.
- [ ] Silhouette contact-point count differs between all three.

### 2.2 The Altered — severance stage readable from the model

Design brief: *how long an Altered has been severed should be readable from the model. Early stage still wears insignia, still uses cover, still flinches. Late stage does none of these* (master sheet 1.5).

This is the highest-value art direction statement in the whole master sheet, because it makes the game's central moral problem a **visual mechanic**. The militia's engage-on-sight order exists because stage cannot be identified in a firefight. The art must therefore make stage identifiable *slowly* — legible if the player looks, invisible if the player is panicking. That tension is the point. Do not make stage a coloured health bar.

**The three-stage ladder.** One base body, three mesh states. Stage is a property of the spawned enemy, not a separate creature.

| | **Stage 1 — Lucid-adjacent** | **Stage 2 — Degrading** | **Stage 3 — Shape only** |
|---|---|---|---|
| Insignia | Full: rank patch, unit flash, shoulder repair, name tape | Partial: patch torn, one flash remains, name tape illegible | None. Insignia surface has been overgrown/absorbed |
| Kit | Complete: webbing, pouches, helmet, boots laced | Kit shed asymmetrically — one pouch, helmet gone or hanging | Bare. Fabric fused into the body silhouette |
| Posture (bind pose) | Upright, weight even, shoulders squared | Weight forward, head low, one shoulder dropped ~15° | Head below shoulder line, spine forward, arms hanging past knee |
| Silhouette | Human. Reads as a soldier. | Human, but wrong weight distribution | Humanoid outline, non-human proportion. Limbs +15% length |
| Rift chroma | None visible | Thin lines at joints and neck, low intensity | Interior chroma through fissures; the same read as a Vestige |
| Face | Present, intact, visible | Present, obscured — hair, dirt, partial occlusion | Present but not usable. Never removed. |
| Behaviour hook | Uses cover, flinches on hit, retreats when suppressed | Uses cover inconsistently, flinch is delayed | No cover, no flinch, no retreat |

**Two non-negotiables:**

1. **The face is never removed, at any stage.** Stage 3 must remain recognisably a person who has stopped being one. Removing the face turns the Altered into a monster and destroys the entire moral premise — "it is what is left after the person is gone," not a creature. Obscure it. Never delete it.
2. **Stage 1 must be uncomfortable to shoot.** If killing a Stage 1 feels good, the militia policy has no weight. Stage 1 gets the best-fitting kit, the cleanest silhouette, and the most human idle in the game.

**The Act II turn asset — highest-priority single model in the project.** The master sheet is explicit: *something comes through wearing a uniform. Not the player's uniform, but close enough to read. Rank markings. A field repair on the shoulder.* This is one Stage-1 Altered, badly wounded, non-boss. It should be the single most carefully authored enemy in the slice — more detail budget than the boss.

Requirements for the beat, as art:
- The uniform is a *near-miss* of the Breaker uniform: same cut logic, same pouch grammar, different rank system, different flash shape, different colourway (recommend: warmer, browner olive against the player's slate).
- The field repair on the shoulder must be visible from the front at 20m. It is the detail that says "someone did this for them." Make it a large, hand-stitched, wrong-coloured patch, not a subtle one.
- Model it wounded: the damage is authored into the mesh, not a decal.

**Slice roster — Altered.**
| ID | Stage | Role |
|---|---|---|
| ALT_First | 1 | The Act II turn. Scripted, single, wounded. Non-combat or trivially weak. |
| ALT_Rifleman | 1 and 3 variants | Standard ranged. Ships both stages to prove the ladder reads. |
| ALT_Commander | 1 | Slice boss. See below. |

**Slice boss — the Altered commander.** The master sheet recommends *an Act II Altered commander — the first humanoid that demonstrably gives orders* (8.7). Art requirements:
- Stage 1 or early Stage 2. **Not Stage 3.** The point is that it gives orders, which requires it to still be someone.
- Rank must be readable: more insignia than any other model in the game, higher on the body (collar and chest, not just shoulder), plus one authority silhouette element — a shoulder mantle or a coat that no other Altered has.
- Must be visually distinguishable from all other Altered at the moment of first sight, before it speaks or gestures.
- Budget: 45k tris LOD0, one unique face, one unique kit set. Reuse the ALT base skeleton and body — spend everything on the outer layer.

**Acceptance criteria — Altered:**
- [ ] Flat-grey test at 30m: testers correctly rank two Altered by stage, 8/10 trials.
- [ ] A tester who has *not* been told the stage system exists spontaneously reports that "some of them look further gone." (This is the real test. If nobody notices unprompted, the ladder is too subtle.)
- [ ] Stage 3 silhouette is distinguishable from any Vestige silhouette at 40m — the Altered are humanoid and the Vestiges are not, and that difference must never blur.
- [ ] Insignia on Stage 1 is legible as *insignia* (not as noise) at 20m.
- [ ] The Act II turn model's shoulder repair is identifiable from the front at 20m.
- [ ] No Altered asset is referenced by any pre-Act-II level, spawner, or lore prop. **Verify by asset reference search, not by memory.**

---

## 3. The Breaker — player silhouette and first-person presentation

### 3.0 STATUS — what exists, what needs an artist, what the owner has to do

Read this before anything else in §3. It is the honest division of labour, and
it is the reason the rest of §3 is written as a commission brief rather than as
a plan.

**What is CODE BLOCKOUT and already in the build (no assets, works on a clean
clone).** `Source/RiorsEdge/Characters/BreakerViewmodelRig.{h,cpp}` is a data
table of composed engine primitives — `/Engine/BasicShapes` cube, cylinder and
cone painted with dynamic instances of `BasicShapeMaterial`, the same asset-free
technique the gym dressing uses. `ABreakerCharacter` allocates a pool of twelve
mesh components plus four limb components once in its constructor and re-poses
them whenever the equipped archetype changes. It delivers:

- a distinct proxy for **all eight** archetypes, with proportions read off each
  weapon's real cadence, magazine and swap time (see the table in §5.1);
- first-person arms that are actually **in frame** — two forearms and two
  gloves, stretched from a shoulder anchor to per-archetype hand positions;
- correct attachment: every part and both arms hang off the one transform the
  recoil spring drives, so the viewmodel kick, the ADS transition and the dash
  camera work all read on the whole assembly;
- an ADS pose **derived** from each weapon's own sight height, so each archetype
  puts its own sight on the crosshair;
- the muzzle flash light at the actual muzzle, so it moves with the recoil.

Covered by six automation tests (`RiorsEdge.Characters.Viewmodel*`) that pin the
ORDERING and the palette law, never the values (O2). Verified by reading
screenshots from the capture harness — see the honest assessment in §3.6.

**What NEEDS AN ARTIST.** Everything in §3.2 and §3.6. The blockout is a
stand-in whose only job is "the player can name the gun in their hands and can
see the recoil". It is not art, it does not have a hand, it does not have a
silhouette anyone would recognise at 20 m, and it will not survive contact with
a real level. It exists so the commission below can be written against something
concrete instead of against nothing.

**What the OWNER personally has to do, in the editor, and nobody else can.**
The project rule is absolute: `.uasset` and `.umap` are never hand-edited, so
none of this can be done from code or by an agent.

1. Import the authored FBX meshes and skeleton and set up the material
   instances. Everything in §3.6's asset list is editor work.
2. Create `BP_BreakerViewmodel` (or extend `BP_BreakerCharacter`) to swap the
   authored `SK_` arms in for the code pool, keeping the code pool as the
   fallback per the architecture rule.
3. Author the animation assets and the anim blueprint. Retargeting a bought
   pack onto a Manny-compatible rig is editor work.
4. Confirm the FP FOV and the near-clip behaviour on the authored geometry;
   the blockout's proportions were tuned against FOV 90 at 16:9.
5. Verify LFS coverage for any new binary extension before the first import.

Current state per `CONTEXT.md`: `BP_BreakerCharacter` is the active pawn and
remains a child of the C++ `ABreakerCharacter`; its first-person geometry is
explicitly replaceable presentation. Recommended next action #2 in `CONTEXT.md`
names character and weapon meshes as a binding constraint on feel.

### 3.1 What a Breaker is, visually

Militia, not military. Equipped by an organisation that has money for the things that keep people alive and nothing left over. The read is **competent, funded unevenly, personally maintained.**

- Silhouette: human, armoured at the torso and shins, deliberately *light* at the shoulders and hips so the movement kit is believable. A player who wall-rides and slides cannot look like a heavy infantryman.
- Layer grammar, inside to out: bodysuit → soft armour vest → hard plate at chest and shins → webbing/pouches → outer shell garment.
- Every hard surface is scuffed at the edges and clean in the recesses. Wear is where a body touches things.
- One personal item per character, always visible in first person: a wrist wrap, a taped ring, a charm on the sling. This is the single cheapest humanising detail available and it costs one 200-tri asset.

**Effigy variant (machine Breaker).** The master sheet establishes Effigies as machine Breakers, originally caretakers, repurposed. **RULED [O14] — the Effigy is a real second player model, not a cosmetic afterthought; Human ships first, Effigy follows.** **DEFER the assets for the slice** — an Effigy player option doubles every arms asset and every gear-fit problem — but **do not defer the rig decisions**: one shared skeleton, one shared animation set, and a proportion-independent paper-doll attachment scheme are committed *before Phase A*. See the note preceding Phase A in §7.4. Record the direction now, build no Effigy assets:
- Effigies wear the *same* militia kit over a non-human chassis. The kit is the tell that they are Breakers; the chassis is the tell that they were not built for it.
- Caretaker origin should be visible in the hands: too gentle, too many fine joints, designed to handle things carefully. The militia bolted a weapon onto something built to carry a child.
- Effigy hands are the strongest single art idea available for the setting. Worth building the day a second arms set is affordable.

### 3.2 First-person arms — the commission brief

This is the asset the player looks at for 100% of playtime. It gets the highest
per-triangle scrutiny in the project. Everything below is written so an external
artist could quote and deliver it without another conversation.

#### 3.2.1 Silhouette rules — binding

1. **The support forearm is the largest single shape on screen and must be the
   quietest.** It crosses the frame diagonally from the lower corner. If it is
   the brightest or the most detailed thing in the composition, it competes with
   the weapon, which is the object carrying the information. Value it darker
   than the receiver.
2. **The gun's outline must survive a 4-pixel blur.** A player firing while
   sliding never resolves surface detail. The read is: one long horizontal mass,
   one break in the top line (the optic), one break in the bottom line (the
   magazine). Three shapes. Anything that does not contribute to those three is
   texture, not geometry.
3. **One dominant feature per archetype, and no archetype may borrow another's.**
   The list is fixed in §5 and restated concretely in §5.1: the sniper's
   oversized cylindrical optic, the shotgun's under-barrel tube, the rocket's
   shoulder tube, the machinegun's drum and bipod, the burst rifle's tall
   dedicated optic, the SMG's absent stock, the sidearm's absence of everything.
4. **No hardpoint, hook, launcher, tether, or grapple mount anywhere** — on the
   weapon, the gloves, the sleeve, or the sling. Do not tease a verb that does
   not exist.
5. **Nothing crosses the screen centre at rest.** The crosshair region stays
   clear at hip; the sight occupies it only when aiming.
6. **The gun's mass sits low-right, the muzzle points up-left.** The blockout's
   framing is the reference: the assembly occupies roughly the lower-right
   quarter at FOV 90 / 16:9, and the barrel runs toward, not across, the centre.

#### 3.2.2 Proportions — the numbers the blockout already commits to

`BreakerViewmodelRig.cpp` authors every part in **centimetres**, so the authored
mesh can be modelled to the same real-world scale and dropped in without a
rescale pass. The anchors an artist needs:

| Quantity | Value | Note |
|---|---|---|
| Rig origin | The firing hand / trigger group | Everything is authored relative to it; stocks are negative X, barrels positive |
| Axes | X forward, Y right, Z up | Matches Unreal, matches the code |
| Rifle overall length | ~80 cm | The reference. Every other weapon's length is stated against it in §5.1 |
| Rifle receiver | 34 × 5 × 7 cm | |
| Sighting line above rig origin | 4.4 cm (sidearm) → 15 cm (rocket) | Per weapon. Drives the ADS pose, so it must be modelled accurately |
| Rig origin from camera, hip | 40–72 cm forward, 9–19 cm right, 7–22 cm below | Per weapon; bigger weapons sit further out and lower |
| Forearm | ~6 cm across | Anything thicker eats the frame |
| Glove | ~9.5 cm cube-ish | |
| Camera FOV the framing assumes | 90°, 16:9 | Settings persist FOV, so the arms must not fall apart at 70 or 120 |

**O2 applies:** every one of these is a PLACEHOLDER, is unplaytested, and can be
retuned on the character instance (`ViewmodelLayoutOverrides`, `ViewmodelScale`,
the shoulder anchors) with no recompile. Model to them, but expect a pass.

#### 3.2.3 Palette — from the Fieldplate style guide

The player's kit is **militia hardware** and takes its values from §3.1's layer
grammar and Pillar 3, not from the UI ramp. The UI tokens in
`UI-Style-Guide-Fieldplate.md` govern the *interface*; the two rules that cross
over into the world are the teal object law and the function-accent hues.

| Surface | Direction | Blockout stand-in (linear) |
|---|---|---|
| Receiver, hard parts | Gunmetal, uniform matte, factory finish | `0.022, 0.024, 0.026` |
| Barrel, shroud, tube | Darker gunmetal, near-black | `0.010, 0.011, 0.012` |
| Polymer furniture (stock, grip, magazine) | Desaturated olive-slate, the militia colourway (`#4A5049`) | `0.018, 0.022, 0.017` |
| Cheap / mass-produced polymer (SMG only) | Lighter, visibly stamped | `0.040, 0.043, 0.037` |
| Working mechanism (shotgun pump, sidearm sights) | Bright steel — the one place value contrast is spent on purpose | `0.075, 0.080, 0.082` |
| Field-fabricated markings (rocket only) | Hazard amber (`#D89A2E`), hand-painted | `0.300, 0.170, 0.030` |
| Personal marks (sniper dope card) | Warm off-white (`#D8CFBA`) | `0.160, 0.150, 0.125` |
| Glove | Olive, darker than the sleeve | `0.014, 0.017, 0.013` |
| Sleeve | Slate, low chroma | `0.028, 0.032, 0.034` |

**Teal object law, verbatim from the style guide: "teal is a noun, never an
adjective."** Saturated teal (`#08B8A8` suppression hardware, `#26F2D9`
Anomalous) belongs to rift and suppression OBJECTS. **The player's body, arms,
gloves and weapons may never carry it**, with exactly one exception already
written into §3.3: an Anomalous-rarity item, which is rift-derived in fiction.
This is enforced in code by `RiorsEdge.Characters.ViewmodelChromaLaw`.

Second rule, learned from a screenshot rather than argued: **the gym is bright
concrete under a bright sky and its floor is authored at linear 0.33.** A
first-pass proxy at linear 0.08 came back mid-grey and disappeared into the
floor. The player's kit is the darkest thing in a normal frame, and the *spread*
between its own values (about 7x) is what separates its parts.

#### 3.2.4 What reads at speed, in first person

The game is judged at 15–40 m, in motion, at 1/60th of a second, while the
player is airborne or sliding. What survives that, in order:

1. **Overall length and mass.** The player identifies the gun before they
   identify anything on it.
2. **The break in the top line.** An optic, or its absence.
3. **The break in the bottom line.** A magazine, its length, its rake — this is
   the tell that carries capacity, and it is why the SMG's 35-round stick is the
   longest thing on it and the machinegun's 120-round drum is the biggest part
   in the game.
4. **Value, never hue.** Nothing in a normal frame is saturated. Anything the
   player must read at speed is carried by light-against-dark.
5. **Motion.** The recoil spring moves the whole assembly; that motion is read
   before any surface is.

Explicitly does NOT read at speed and must not be relied on: panel lines,
fasteners, decals, insignia on the weapon, small text, roughness variation.
Author them, but never as the thing that distinguishes two weapons.

#### 3.2.5 Rig, topology, and technical expectations

| Property | Spec | Rationale |
|---|---|---|
| Arms rig | Single FP arms skeletal mesh, UE5 Manny-compatible hierarchy | Lets you retarget bought animation packs without a bespoke rig |
| Skeleton | **ONE skeleton shared by Human and Effigy** [O14] | See the note before Phase A in §7.4. Cheap today, ruinous later |
| Triangle budget | 25k tris both arms, LOD0 only (no LODs needed — always at camera) | FP arms never LOD |
| Weapon budget | 15–20k tris LOD0, LOD1 at ~40% for the third-person/paper-doll draw | §7.5 |
| Texture | 2× 2048 (arms, gloves) | Gloves separate so gear-slot variation is possible later |
| Materials | One master material, instanced. No unique unwraps — trim sheet | §7.5 rule 1 and 2 |
| FOV | Separate FP FOV for the arms/weapon draw, default 70–80, **exposed and configurable** | Master sheet 5.4 requires FOV changes be subtle and configurable; settings already persist FOV per `CONTEXT.md` |
| Camera-space offset | Arms parented to camera, weapon parented to arms, not to the character mesh | Standard, and keeps movement-driven camera roll from detaching the weapon |
| Gloves | Sockets for gear variation at wrist and knuckle from day one | Gloves are a real equipment slot (GLOV) carrying crit, handling, airborne accuracy |
| Shadows | FP arms and weapon cast **no** world shadow | A viewmodel shadow is a gun-shaped shadow from a gun nobody else can see. The blockout already sets this |
| Visibility | Owner-see only | Multiplayer correctness; the blockout already sets this |

#### 3.2.6 Separability — what must stay in pieces

The blockout is replaced **piece by piece**, per §3.4, and the pieces are only
swappable if the authored asset is split the same way the code pool is. Binding:

| Piece | Must be its own mesh / socket | Why |
|---|---|---|
| Weapon body | Separate from arms | §3.4 replaces the weapon first; it is the largest screen-space object |
| Barrel + muzzle device | Own mesh, own socket `Muzzle` | The muzzle flash, tracer origin and future VFX all hang off it, and the code already positions a light there per archetype |
| Optic / sight | Own mesh, own socket `Sight` | The ADS pose is derived from the sight's height. Move the sight and the aim pose must follow automatically, not be re-authored |
| Magazine | Own mesh | The reload animation shows the mechanism, and reload speed is a real affix |
| Working mechanism (pump, bolt, charging handle) | Own mesh | §5 requires a visible mechanism the player can watch move |
| Gloves | Separate from forearms | GLOV is a real equipment slot with its own affixes and its own marquee-affix tell |
| Forearm / sleeve | Separate from gloves | So a gear-slot glove variant does not re-author the arm |
| Personal item | Own ~200-tri mesh, socket `Personal` | §3.1's single cheapest humanising detail |
| Marquee-affix stabiliser | Own mesh, socket on the glove | §3.3's Accuracy While Airborne tell, with an idle rotation |

**Do not delete the blockout.** §7.5 rule 9 and the `CONTEXT.md` architecture
rule both say it: every replacement keeps a fallback until it reaches parity.
The code rig is that fallback, and it costs nothing because it ships no assets.

**Motion set required for the slice.** Ordered by how often the player sees it:

1. Idle / breathing (weapon down-ish)
2. Sprint (weapon carried low, off-axis, arms swing — this is the single strongest "movement game" tell)
3. Slide entry / slide loop / slide exit — **weapon braced, off-hand down, low camera** ; the slide is a locked base-kit verb and it needs its own arms pose, not a crouch reuse
4. Wall ride left / right — off-hand reaches toward the wall. 0.85s max duration per master sheet 5.3, so the pose must read instantly.
5. Dash — a short, hard camera-space lurch. **Redirect, not launch.** Dash "redirects momentum" per 5.3, and the animation must not imply a rocket.
6. Air / falling
7. Fire (per archetype), reload (per archetype), aim-down-sights transition
8. Air jump — off-hand snap. **RULED [O25] — base kit for everyone (two jumps); no longer Kinesis-granted.** Author it as default competence every player will see constantly, not as an earned unlock. (Swift's later third jump is a separate, unimplemented, class-innate ability — also not tree-granted.)

**Explicitly not in the motion set:** no dodge roll, no parry-as-dodge, no grapple throw, no melee lunge that reads as a traversal verb.

### 3.3 Third-person / gear-visible body

Needed for the paper-doll (§6), for multiplayer, and for the Anchor social hub. One base body, modular by equipment slot.

Modular split — six visible slots (WAIST is a mesh; NECK is small; P1/P2 are weapons):

| Slot | Mesh part | Notes |
|---|---|---|
| HELM | Head covering | Must never fully hide the player's face in the Anchor — social space needs faces |
| BODY | Torso shell | Largest silhouette contribution |
| GLOV | Hands/forearms | Shared authoring with FP gloves |
| BOOT | Feet/shins | Deepest affix pool in the game; deserves the most variants |
| WAIST | Belt/pouches | Cheap, high visual return per triangle |
| NECK | Small hanging element | Lowest priority; can be a single mesh with material variants |

**Rarity read on gear.** Rarity is the endgame, so it must be visible without the UI:
| Rarity | Visual treatment | Cost |
|---|---|---|
| Standard | Base mesh, base material | 0 |
| Uncommon | Base mesh, tinted material variant | Material param |
| Exceptional | Base mesh + one attachment (extra pouch, plate, strap) | 1 small mesh |
| Aberrant | Base mesh + attachment + an emissive signature mark tied to the fixed signature affix | Material + mask |
| Anomalous | Unique silhouette element, rift chroma present on the item itself | 1 bespoke mesh per Anomalous |

Anomalous being the only rarity permitted rift chroma is deliberate and reinforces Pillar 3: the rule-rewriting item is visibly rift-derived.

EXTENDS — the master sheet fixes rarity colours but not gear visual treatment. This is new, and it commits to only ~1 bespoke mesh per Anomalous (there are 3 build-defining legendaries in the slice, so 3 meshes).

> **CORRECTION (O32).** That parenthesis conflates two different things. Legendary
> is not a rarity: **Anomalous is the rarity**, and **legendary is a separate
> field** naming an authored item. Every legendary is Anomalous, but most
> Anomalous drops are not legendaries, so "1 bespoke mesh per Anomalous" would
> commit to a bespoke mesh for every fifth-tier drop in the game. The rule this
> row should carry is **one bespoke mesh per LEGENDARY**, and generic Anomalous
> drops get the rarity treatment ladder only. See open question 9 for the budget
> consequence — the legendary pool is ruled to grow and has no target size.

**Marquee-affix tell (master sheet 3.4 explicitly asks for this).** *Accuracy While Airborne* is called out as the affix that most sells "movement FPS" and is told to be *visually obvious on the item*. Implementation: any HELM or GLOV rolling Accuracy While Airborne at T2 or better gets a stabiliser element — a small gyroscopic housing on the helmet temple, or a wrist brace on the glove — with a slow idle rotation. Visible in the paper-doll and in first person on the gloves.

### 3.4 The blockout replacement sequence

Do not replace the blockout in one pass. Replace it in the order the player notices:

1. **Weapon mesh** (P0). The single largest screen-space object. Replacing this alone makes the game look 60% finished.
2. **Hands/gloves** (P0). Second-largest screen-space.
3. **Arms/sleeve** (P1).
4. **Motion set** (P1) — poses first, then transitions, then secondary motion.
5. **Third-person body** (P2) — only needed for the paper-doll and hub.

Do not delete the blockout meshes. Keep them as a fallback per `CONTEXT.md` architecture rule ("do not replace working template assets before the C++ replacement reaches feature parity").

### 3.5 CRITICAL — dodge and block must not be animated as inputs

**RULED [O1] — law, not assumption: dodge is a passive chance to fully evade and block is a passive chance to reduce damage. They are defensive layers, not player inputs. There is no stamina pool. Parry is the only defensive input and carries its own short cooldown.**

The obvious art instinct — a dodge-roll animation, a shield-raise stance — would lie to the player about the control scheme and take a week to undo.

**RULED [O1] — CONFLICT CLOSED. The passive-chance reading wins.** The stamina pool is removed entirely; block and dodge are passive chance layers with **no player input, no stance, no bound key, and therefore no animation of any kind**. The stance/input framing in `CONTEXT.md` line 47 and master sheet 3.8 / 3.15 / 7.11 is superseded — those are rename/strike targets, not live design. §3.2's motion set does **not** gain block or dodge poses, now or later. Defensive animation work is unblocked and its answer is: author none.

**Parry is the only defensive player input**, and it runs on **its own short cooldown**. Art consequences:

- Parry, when built, is the single defensive verb that gets an authored anticipatory pose and a distinct readable timing. It is tree-granted (Bulwark), and **RULED [O25] — Parry is now the only tree-granted verb in the game**: air jump is base kit (two jumps for everyone) and Swift's later third jump is a class-innate unlock, so neither is tree-granted any longer. Parry alone should look *earned*; the movement verbs around it read as default competence instead.
- Parry must be visually unmistakable from a block, because block has no animation at all. There is no shared "defensive" vocabulary between them — one is a proc, the other is a verb.
- Do not author parry until it is built; do not pre-author a generic "defend" pose that parry might inherit. The risk this section exists to prevent — a dodge roll or shield-raise that lies about the control scheme — applies equally to a speculative shared defensive pose.

Under the passive-chance reading, the art is **reactive feedback, not anticipatory animation**:

| Event | Feedback | Explicitly NOT |
|---|---|---|
| Dodge procs (full evade) | Sharp camera-space lean *away* from the incoming hit direction, 0.12s, plus a whip-crack audio cue and a brief motion smear at screen edge. Damage number is absent, not zero. | A roll. A dive. Any change in player position. Any i-frame window the player could learn to time. |
| Block procs (damage reduced) | Impact-direction screen ring, heavier hit-stop than an unblocked hit, dull impact audio. Off-hand does *not* move. | A shield. A raised arm. A stance. |
| Neither procs | Normal damage feedback | — |

Rule: **neither feedback may change the player's world-space position or velocity.** A passive proc that moves the player would silently become the best movement tech in the game and break every movement guardrail in master sheet 5.4.

Second rule: the dodge feedback must be *legible enough to feel lucky* and *not legible enough to feel controllable*. If testers start reporting they "dodged that one," it is correct. If they report they "timed a dodge," the feedback is too anticipatory and must be shortened.

**Acceptance criteria — player presentation:**
- [ ] No animation in the FP motion set is triggered by block or dodge.
- [ ] Dodge/block feedback produces zero delta in player world position — verified by logging position across 100 procs.
- [ ] Sprint, slide, wall ride, and dash each have a distinct arms pose identifiable from a single frame.
- [ ] FP FOV is exposed in settings and persists (already true per `CONTEXT.md`; must not regress).
- [ ] No hardpoint, hook, launcher, or tether geometry exists anywhere on the player model.
- [ ] The blockout meshes still exist and can be swapped back via a Blueprint variable.

### 3.6 ASSET LIST — exact paths and names

Naming follows `CONTEXT.md`'s content conventions exactly: `SK_` skeletal mesh,
`SM_` static mesh, `M_` material, `MI_` material instance, `T_` texture, `BP_`
Blueprint, `PHYS_` physics asset, `ABP_` anim blueprint, `AS_` anim sequence.
All game-owned content lives under `Content/ProjectBreaker`. Move and rename
only inside Unreal Editor; fix redirectors before merging.

**P0 — the arms (Phase A).**

| Asset | Path |
|---|---|
| FP arms skeleton (shared, Human + Effigy) | `Content/ProjectBreaker/Characters/Player/SK_Breaker_FPArms_Skeleton` |
| FP arms mesh | `Content/ProjectBreaker/Characters/Player/SK_Breaker_FPArms` |
| FP gloves mesh | `Content/ProjectBreaker/Characters/Player/SK_Breaker_FPGloves` |
| Physics asset | `Content/ProjectBreaker/Characters/Player/PHYS_Breaker_FPArms` |
| Personal item | `Content/ProjectBreaker/Characters/Player/SM_Breaker_PersonalCharm` |
| Anim blueprint | `Content/ProjectBreaker/Characters/Player/ABP_Breaker_FPArms` |
| Viewmodel assembly | `Content/ProjectBreaker/Characters/Player/BP_BreakerViewmodel` |

**P0 — materials and textures (Phase A/B).**

| Asset | Path |
|---|---|
| Master material | `Content/ProjectBreaker/Materials/M_BreakerMaster` |
| Militia kit trim sheet | `Content/ProjectBreaker/Textures/T_MilitiaKit_Trim_2K` |
| Armoury trim sheet | `Content/ProjectBreaker/Textures/T_Armoury_Trim_2K` |
| Arms instance | `Content/ProjectBreaker/Materials/MI_Breaker_FPArms` |
| Gloves instance | `Content/ProjectBreaker/Materials/MI_Breaker_FPGloves` |
| Armoury steel / polymer / bright-steel / hazard-amber instances | `Content/ProjectBreaker/Materials/MI_Armoury_{Steel,Polymer,Bright,Hazard}` |

**Weapons.** One folder per archetype; the folder name matches the archetype's
code name in `EBreakerWeaponArchetype` so a reader can pair them without a
lookup. Every weapon ships the same five separable meshes.

| Archetype | Folder | Meshes | Priority |
|---|---|---|---|
| Rifle | `Content/ProjectBreaker/Weapons/Rifle/` | `SM_Rifle_Body`, `_Barrel`, `_Sight`, `_Magazine`, `_Mechanism` | **P0** — sets the armoury vocabulary |
| Shotgun | `Content/ProjectBreaker/Weapons/Shotgun/` | `SM_Shotgun_Body`, `_Barrel`, `_Sight`, `_TubeMagazine`, `_Pump` | **P0** |
| Rocket | `Content/ProjectBreaker/Weapons/Rocket/` | `SM_Rocket_Body`, `_Muzzle`, `_Sight`, `_Grip`, `_RearVent` | **P0**, highest risk (open ruling, §9.2) |
| Sniper | `Content/ProjectBreaker/Weapons/Sniper/` | `SM_Sniper_Body`, `_Barrel`, `_Scope`, `_Magazine`, `_Bolt` | P1 |
| SMG | `Content/ProjectBreaker/Weapons/SMG/` | `SM_SMG_Body`, `_Barrel`, `_Sight`, `_Magazine`, `_Bolt` | P2 — kitbash first |
| Burst Rifle | `Content/ProjectBreaker/Weapons/BurstRifle/` | `SM_BurstRifle_Body`, `_Barrel`, `_Optic`, `_Magazine`, `_Mechanism` | P2 |
| Machinegun | `Content/ProjectBreaker/Weapons/Machinegun/` | `SM_Machinegun_Body`, `_Shroud`, `_Sight`, `_Drum`, `_Bipod` | P2 |
| Sidearm | `Content/ProjectBreaker/Weapons/Sidearm/` | `SM_Sidearm_Body`, `_Barrel`, `_Sight`, `_Magazine`, `_Slide` | P2 |

**CORRECTION to §5's header.** That section is written for five archetypes. The
code now ships **eight**: Rifle, SMG, Sniper, Shotgun, Rocket, **Burst Rifle,
Machinegun, Sidearm**. The three additions are P2 art and are described
concretely in §5.1 below. The slice still closes on three finished weapons.

**Motion set** — `Content/ProjectBreaker/Animation/Player/`, one `AS_` per
entry in §3.2's motion list, prefixed `AS_FPArms_`: `Idle`, `Sprint`,
`SlideEnter/Loop/Exit`, `WallRideLeft/Right`, `Dash`, `Fall`, `AirJump`, and per
archetype `Fire`, `Reload`, `AimIn`, `AimOut`, `SwapIn`. Reload length must be
driven by the weapon's `ReloadDuration` property, never hardcoded, because
Reload Speed is a real affix.

**Explicitly NOT on this list:** any dodge, block, parry or defensive pose.
[O1], §3.5. Author none.

### 3.7 Handoff notes

- **Reference build:** run the capture harness and look at what exists before
  modelling anything. `UnrealEditor-Cmd.exe <project> -game -BreakerAutoPlay
  -BreakerScreenshots=16 -BreakerCycleWeapons=2 -windowed -ResX=1920 -ResY=1080`
  walks all eight archetypes through hip and aimed poses, firing, and writes
  PNGs to `Saved/Screenshots`. That is the framing to match.
- **Scale check before anything else.** Model to centimetres, rig origin at the
  trigger group, and drop the mesh in against the blockout at `ViewmodelScale`
  1.0. If it does not sit in the same envelope, the proportions are wrong and no
  amount of positioning will fix it.
- **The ADS pose is derived, not authored.** Give the sight an accurate height
  above the rig origin and the aim pose follows. Do not hand-place an aimed
  transform; it will drift the first time a part moves.
- **Deliver the pieces separately** per §3.2.6, even if the concept was modelled
  as one object. A merged weapon cannot be replaced piece by piece and cannot
  animate its own mechanism.
- **The blockout stays.** Keep it swappable via a Blueprint variable; §7.5
  rule 9 is not negotiable and it costs nothing, because the fallback ships no
  assets.
- **Open rulings that block finishing work.** Rocket self-damage/self-knockback
  (§9.2) blocks the Rocket's launch pose and recovery. Cipher-or-person (§9.3)
  blocks the third-person face and therefore the paper-doll head. Neither blocks
  the arms or the weapon bodies — start there.
- **Values are frozen.** Everything numeric in §3 is O2 PLACEHOLDER and none of
  it has been playtested. Automation proves the ordering and the palette law;
  screenshots proved the framing; nothing has proved the feel.

---

## 4. Anchor environment direction

Brief from master sheet 1.3: *humanity is holding, not thriving. Anchors are small — cities, not nations. Ordinary life continues. The suppression hardware is visible from everywhere in the city and nobody looks at it anymore.*

### 4.1 The one image the Anchor has to sell
A market street, mid-afternoon, ordinary commerce, and a two-hundred-metre suppression pylon in frame that nobody in the shot is looking at. That contrast is the entire environment brief. If a screenshot of the Anchor does not contain both the mundane and the pylon, it is framed wrong.

### 4.2 Direction rules

| Rule | Detail |
|---|---|
| **Lived-in, not ruined** | Repaired, not broken. Patched concrete, scaffolding that has become permanent, awnings, laundry, cabling run late and never tidied. |
| **Density falls off with radius** | Infrastructure degrades rapidly outside the suppression radius (1.3). Building density, light quality, and repair standard all decrease with distance from the pylon. This gives the level a free legible compass. |
| **The pylon is the sun** | It is the visual anchor (literally) of every exterior composition and the primary directional light modifier. Visible from every exterior. Never remarked on. |
| **No signage explaining the setting** | No "REFUGEE PROCESSING" banners, no exposition posters. The world is normal to the people in it. |
| **Warmth inside, cold outside** | Interior/market lighting is warm tungsten and sodium. Exterior and the pylon's own light are cool. The suppression field is not comforting; the people under it are. |
| **Verticality is functional** | The player has wall ride and air jump. The Anchor should be traversable, but traversal routes must be *justified* — fire escapes, scaffolds, service catwalks — never floating platforms. Master sheet 5.4: level design offers movement opportunities without punishing conventional routes. |

### 4.3 Anchor spaces required (slice)

| Space | Function | Priority | Approach |
|---|---|---|---|
| Forge | Respec, crafting, tier upgrade, exalt/corrupt. **Highest interaction count in the game** (13.2) | P0 | Author with care. This is where the player spends real time. Warm, cramped, tool-dense, personal. Kess's Effigy hands should be the focal point of the composition. |
| Vendor stall | Gear, ammo, consumables | P1 | Kitbash from market kit |
| Command post | Contract giver, act gate | P1 | Utilitarian, maps, tired |
| Market street | Social space, connective tissue | P1 | The pillar image lives here |
| Pylon base | Silent thematic centrepiece | P2 | One large asset, seen mostly at distance |

**Slice compromise:** the Anchor is not required for the vertical slice, which is *one graybox biome or arena* (`CONTEXT.md`). Build the **Forge only** as a functional room, since respec is Forge-gated and the loadout/progression loop needs it. Everything else is P2.

### 4.4 Rift interior direction (the actual slice biome)

The slice arena is a local rift, not the Anchor. Direction:
- **Earth geometry, wrong physics context.** A local rift is a breach into elsewhere; the ground should be partly recognisable and partly not. Cheapest possible reading: recognisable Earth architecture at increasingly wrong angles and scales as the player moves inward.
- **Rift chroma at the boundaries** — where geometry meets geometry that should not.
- **No skybox weather.** Rift interiors have no sky the player understands. A dark, low-frequency void with rift-chroma gradients is cheaper and more correct than any sky.
- Arena must support the movement kit: at least three wall-ride surfaces of ≥6m run length, two slide-throughs, and one vertical route reachable only with air jump that grants a *positional* advantage, never a required path (master sheet 7.10 risk #7: do not tax movement).

**Acceptance criteria — environment:**
- [ ] An Anchor exterior screenshot contains the pylon and at least one person not looking at it.
- [ ] Anchor has zero signage explaining rifts, Altered, or severance.
- [ ] The slice arena is fully playable by a player who uses only run/jump/cover (movement guardrail).
- [ ] The slice arena has ≥3 wall-ride surfaces ≥6m, ≥2 slide-throughs, ≥1 optional air-jump route.
- [ ] Rift chroma appears nowhere in the Anchor except on the suppression hardware itself.

---

## 5. Five weapon archetype visual identities

Code currently ships Rifle, SMG, Sniper, Shotgun, Rocket (`CONTEXT.md`). Master sheet 12.3 scopes the slice to three archetypes: hitscan baseline (DONE), multi-shot (shotgun), movement-interacting (rocket).

**Reconciliation note (O40 pass).** The code enum `EBreakerWeaponArchetype` is canonical: **Rifle, SMG, Sniper, Shotgun, Rocket** (the original five, which is what the vertical slice ships against — see `Vertical-Slice.md`), plus **BurstRifle, Machinegun, Sidearm** from the O27 breadth pass. The archetype names here and in §5.1 already match the code; `Docs/Weapon-Foundation.md`'s prior **Scattergun** / **Marksman** naming has been aligned to **Shotgun** / **Sniper** to match. This is a code-alignment fix made during the O40 reconciliation pass, not a numbered ruling in its own right.

**Design principle: the archetype is readable from the silhouette in the corner of the eye.** In a looter shooter the player swaps weapons constantly; the first-person silhouette is the swap confirmation. Each archetype gets one dominant, unmistakable silhouette feature that no other archetype uses.

| Archetype | Slot fantasy | Dominant silhouette feature | Material/finish read | Fire feel | Art priority |
|---|---|---|---|---|---|
| **Rifle** | PRIMARY — commitment. Ramping damage, big mags, range. | Long, straight, balanced. A boxy top-mounted optic. The reference weapon. | Standard-issue: uniform matte, factory finish, minimal personalisation. The one gun that looks *issued*. | Steady, controlled, mid-punch. Modest recoil returning to centre. | **P0** |
| **SMG** | SECONDARY — tempo. Swap speed, on-swap bursts. | Short, stubby, front-heavy, no stock or a folded one. Smallest silhouette. | Cheap and replaceable. Stamped, polymer, visibly mass-produced. | High cadence, low per-shot weight, screen-space buzz rather than kick. | P2 |
| **Sniper** | PRIMARY — reach. Effective range, pierce, headshot. | Extremely long, thin, with an oversized cylindrical optic that breaks the top line. Longest silhouette by 40%. | Personally maintained. Taped, bedded, a hand-marked dope card on the stock. The most *individual* weapon. | One heavy event. Hard hit-stop, long recovery, screen settles slowly. Every shot is a decision. | P1 |
| **Shotgun** | Multi-shot behaviour. Pellet spread. | Wide, thick, blunt. Widest silhouette. Exposed pump or break action — a visible *mechanism* the player can watch move. | Industrial, blunt, unglamorous. Closer to a tool than a firearm. | Maximum single-frame impact. Biggest hit-stop, biggest screen kick, deepest audio. | **P0** |
| **Rocket** | Movement interaction. Radial falloff. | Tube. Shoulder-borne, back-heavy, visibly asymmetric. The only weapon that breaks the player's shoulder line. | Field-fabricated. Welded, hand-painted markings, hazard amber striping. Looks like it might not be safe. | Slow, heavy, committed. Long readied pose, visible projectile, a real reason to reposition after firing. | **P0** |

### 5.1 The eight archetypes, as the code blockout builds them

The table above predates three archetypes. This one is the live contract: it is
what `Source/RiorsEdge/Characters/BreakerViewmodelRig.cpp` actually builds, and
it is what an authored mesh replaces one for one. Every proportion is read off
the weapon's real numbers in `Source/RiorsEdge/Weapons/BreakerWeaponComponent.cpp`
rather than invented — magazine size, cadence and swap time are the three things
the player already feels, so those are the three the silhouette states.

| Archetype | Mechanical fact it expresses | Dominant blockout feature | Length | Bulk |
|---|---|---|---|---|
| **Rifle** | 35 rnd, 620 rpm, 0.50 s swap | Boxy top-mounted optic, straight 30-round stick, fixed stock. The one gun that looks *issued* | ~80 cm — the reference | baseline |
| **SMG** | 35 rnd, 900 rpm, 0.35 s swap | **No stock at all**, stubby barrel, and the longest magazine on the smallest gun | ~40 cm — shortest long gun | light, visibly cheap polymer |
| **Sniper** | 8 rnd, 150 rpm, 0.70 s swap | **Oversized cylindrical optic breaking the top line**, plus an off-white taped dope card at the comb | ~96 cm — longest | long *without* being bulky |
| **Shotgun** | 8 rnd, 85 rpm, 8 pellets | **Tube magazine under the barrel** and a bright-steel pump — the visible mechanism §5 asks for | ~76 cm | widest and thickest |
| **Rocket** | 4 rnd, 55 rpm, 0.80 s swap | **A tube worn high**, breaking the shoulder line, with a forward flare, a rear back-blast cone, an offset side sight, and the only hazard-amber band in the game | ~83 cm | largest single part |
| **Burst Rifle** | 27 rnd, 3-rnd burst, 0.55 s swap | **A tall dedicated optic on a visible riser**, longer thinner barrel, in-line stock. Rifle length, marksman's build | ~89 cm | lighter than the rifle |
| **Machinegun** | 120 rnd, 700 rpm, 4.2 s reload | **A drum**, the biggest part on any weapon, plus a shrouded barrel and a **bipod** — "only while planted" is the archetype's actual rule | ~83 cm | >2x the rifle; heaviest in the game |
| **Sidearm** | 14 rnd, 420 rpm, **0.18 s swap** | Almost nothing: a slide, a grip with the rounds inside, two tiny bright sights. No stock, no separate magazine | ~22 cm | 1/14th the machinegun |

Each row's hands move too: the support hand sits out on the handguard for a
rifle, back on the **pump** for a shotgun, and stacked at the grip for a sidearm
because there is nothing out front to hold.

The ordering claims in that table — sniper longest, sidearm smallest, machinegun
heaviest, sniper long-but-not-bulky, SMG shortest long gun — are pinned by
`RiorsEdge.Characters.ViewmodelSilhouetteOrder`. The values are not: they are O2
placeholders and the test deliberately checks ratios, so the table can be
retuned without going red.

**Shared authoring grammar** — all five weapons share:
- The same fastener and rail vocabulary, so they read as one armoury.
- Magazine/ammo geometry that visually implies capacity (a 30-round rifle mag looks like 30 rounds).
- A reload that shows the mechanism — reload speed is a real affix on GLOV/WAIST/P1/P2, and the animation must scale legibly with it.
- One sling attachment point per weapon, and **no** hardpoint that resembles a grapple mount.

**Rocket is the highest-risk asset**, matching master sheet 12.3's warning. Art-specific risks:
- The projectile must be visible in flight and readable as *the player's* projectile. Rift chroma is forbidden (it is human hardware); use hazard amber for the exhaust.
- Radial damage needs a legible radius. A ground-decal shockwave ring at exactly the damage radius is required, not optional — the player must be able to learn the radius from watching it.
- Self-damage/self-knockback rules are still OPEN (12.5). Do not author a self-knockback pose until that is decided.

**Archetype-per-weapon movement affix.** Master sheet 3.14 requires one movement-affecting affix per weapon archetype in the slice subset. The visual hook: Strafe Speed % rolls on P1/P2, so weapons with a high Strafe roll get a visible sling/stabiliser element. One shared attachment mesh, three material variants.

**Acceptance criteria — weapons:**
- [ ] Each of the five is identifiable from silhouette alone, at 20% screen alpha, single frame.
- [ ] All five share a consistent fastener/rail vocabulary — a tester shown all five agrees they come from the same world.
- [ ] Rocket ground-decal ring radius matches the code's radial damage radius exactly (verify against `ABreakerRocketProjectile`).
- [ ] No weapon carries a hook, tether, or grapple hardpoint.
- [ ] Reload animation length is driven by the weapon's reload time property, not hardcoded, so Reload Speed % affixes read correctly.

---

## 6. Inventory paper-doll — SceneCapture approach

Gear is the entire endgame. The inventory screen is therefore the second-most-viewed screen in the game after the world itself, and the paper-doll is what makes an eight-slot equipment system feel like a character rather than a spreadsheet.

### 6.1 Approach

Render a live third-person character into the inventory UI using `USceneCaptureComponent2D` → `UTextureRenderTarget2D` → material → Slate brush. The existing UI is code-driven Slate (`CONTEXT.md`), so the render target is consumed as a brush resource, not a UMG widget.

### 6.2 Architecture

```
BP_PaperDollStage (a small isolated actor, spawned once, kept in a far-off "studio" location)
├── SM_Backdrop            (matte plane, neutral value, absorbs the capture background)
├── SK_PaperDollMesh       (the third-person modular body, mirroring the player's equipment)
│   ├── HELM / BODY / GLOV / BOOT / WAIST / NECK  (master-pose components off the body)
│   └── P1 / P2 socket attachments
├── Key / Fill / Rim lights (three lights, capture-only channel)
└── SceneCapture2D → RT_PaperDoll (RenderTarget)
```

**Design decisions, and why:**

| Decision | Choice | Rationale |
|---|---|---|
| Where the stage lives | A dedicated "studio" volume ~50,000 units from any playable geometry, in the persistent level | No level lighting bleed; no risk of a player reaching it. Cheaper than a separate world. |
| Capture cadence | **On-demand only.** Capture is disabled by default; `CaptureScene()` is called manually when the inventory opens, when equipment changes, and once per rotation input tick while dragging. | A per-frame SceneCapture is one of the most expensive things you can do in UE. On-demand is the difference between 0.1ms and 4ms. **This is the single most important decision in this section.** |
| Capture source | `FinalColor (LDR) in RGB` with alpha via a `SceneColor (HDR)` + custom depth mask, or simpler: capture over a known matte and key it in the material | Keeps the doll composited cleanly over the inventory background |
| Resolution | 1024×1024 render target, or 768×1024 for a portrait framing | Scales down cleanly; 2048 is waste at inventory panel size |
| Lighting | Three capture-only lights, static, on their own lighting channel; capture ignores world lighting | Gear must look identical regardless of where the player opened the inventory. A player checking their build in a dark cave must see their gear. |
| Post-processing | Its own post-process volume inside the studio, unbound, with world post disabled on the capture | Prevents combat post-FX (damage vignette, low-health) from leaking into the UI |
| Animation | A single idle loop, plus a one-shot "equip settle" when a slot changes | The equip settle is the cheapest possible reward feedback for a new drop |
| Rotation | Mouse-drag yaw on the doll actor, clamped pitch, snap-back to front on close | Rotation is what makes players actually look at their gear |

### 6.3 Performance budget and guardrails

| Guardrail | Value |
|---|---|
| Capture cost target | ≤ 1.5ms GPU per capture on the Windows dev machine |
| Captures per second while dragging | Hard cap at 30, even if input ticks faster |
| Captures while inventory closed | **Zero.** Component `bCaptureEveryFrame = false`, and the stage actor's tick disabled. |
| Stage actor lifetime | Spawned on first inventory open, never destroyed, hidden between uses |
| MacBook fallback | If capture exceeds budget on the low-memory Mac (`CONTEXT.md` development-machine constraints), drop the render target to 512×512 rather than disabling the doll |

### 6.4 What the paper-doll must communicate

Ranked by importance, because this drives what gets authored first:

1. **Which slots are filled and which are empty.** Empty slots show the base bodysuit — visibly unarmoured, so the gap is obvious.
2. **Rarity of each equipped item**, per §3.3's rarity treatment ladder. Anomalous must be unmistakable at a glance.
3. **Equip-limit state** — Aberrant max 3, Anomalous max 1 (master sheet 4.1). The doll should carry a visible tell when the player is at their Aberrant limit, so the constraint feels like a decision rather than an error message.
4. **Marquee affixes** — the Accuracy While Airborne stabiliser (§3.3) must be visible on the doll.

### 6.5 Acceptance criteria — paper-doll

- [ ] Zero SceneCapture cost measured (via `stat GPU`) while the inventory is closed.
- [ ] Capture cost ≤ 1.5ms while open and idle; ≤ 1.5ms per capture while rotating.
- [ ] Equipping an item updates the doll within one frame of the equipment component's replicated change.
- [ ] The doll looks identical in a dark cave and in bright daylight.
- [ ] All five rarities are distinguishable on the doll without reading any text.
- [ ] Combat post-process (low health vignette, damage flash) never appears on the doll.
- [ ] Rotating the doll and closing the inventory returns it to the front-facing pose.
- [ ] Doll renders correctly at 512×512 as the low-memory fallback.

---

## 7. Production sequence — solo dev + AI tooling

The governing constraint is not skill, it is **time per asset**. The plan below is ordered so that the project always looks better than it did last week, and so that nothing expensive is authored before the thing it serves is proven fun.

### 7.1 The three-bucket rule

Every asset goes into exactly one bucket, decided *before* work starts:

| Bucket | Definition | When to use | Rough time cost |
|---|---|---|---|
| **BLOCKOUT** | Primitive geometry, single grey material, correct scale and silhouette | Anything whose *design* is unproven. Always the first version of everything. | 15 min – 2 hrs |
| **BUY / KITBASH** | Marketplace (Fab) asset, kitbashed, retextured to the project palette | Anything the player does not look at closely, or anything with a huge asset count | 1–4 hrs per finished set |
| **AUTHOR** | Bespoke modelled, sculpted, textured, rigged | Only things carrying the game's identity: FP arms, the three P0 weapons, the Altered stage ladder, the Act II turn model, the Forge | 1–5 days each |

**The discipline: nothing is promoted from BLOCKOUT to AUTHOR until it has been playtested and kept.** Combined with the existing playtest gym and report tooling (`Docs/Playtest-Gym-v1.md`), this is the main defence against authoring beautiful assets for mechanics that get cut.

### 7.2 Bucket assignment for every asset in the slice

| Asset | Bucket | Notes |
|---|---|---|
| FP arms + gloves | **AUTHOR** | 100% of screentime. Never buy this. |
| Rifle | **AUTHOR** | The reference weapon, sets the armoury vocabulary |
| Shotgun | AUTHOR | P0 |
| Rocket | AUTHOR | P0, highest risk |
| SMG, Sniper | BUY/KITBASH → author later | P1/P2. Kitbash from a bought pack, retextured, until the slice closes. |
| Third-person modular body | BUY (base) + AUTHOR (kit layer) | Buy a base body/skeleton; author the militia kit over it. Halves the work. |
| Vestige ×3 | AUTHOR | The wrongness cannot be bought. Bought monsters read as bought monsters. |
| Elite modifier | **Material + scale + one shared VFX library** | Zero *mesh* cost by design, but **not zero cost** — budget the shared modifier-VFX library (~six reusable effects) once in Phase C. See revised §2.1. |
| Altered base body | BUY (base) + AUTHOR (stage ladder + kit) | Same split as the player body |
| ALT_First (Act II turn) | **AUTHOR** | Most carefully authored enemy in the game |
| ALT_Commander (boss) | AUTHOR (outer layer only) | Reuses the ALT base body |
| Rift arena geometry | BLOCKOUT → KITBASH | A modular concrete/industrial kit, bought, bent wrong |
| Anchor Forge | AUTHOR (room), KITBASH (props) | Highest interaction count of any space |
| Anchor market/street | KITBASH | Bought modular city kit, retextured to palette |
| Suppression pylon | AUTHOR | One asset, huge silhouette impact, seen from everywhere |
| Gear variants (per slot) | KITBASH from own parts | Build a parts library once, recombine forever |
| VFX (muzzle, impact, rift) | BUY base + retune | Niagara packs are excellent value; retuning is fast |
| Animation | BUY base + author key poses | Retarget a bought FPS animation pack onto the Manny-compatible rig, then hand-author the movement-specific poses (slide, wall ride, dash, air jump) that no pack will contain |

### 7.3 Where AI tooling actually helps (and where it does not)

Be honest about this, because misapplied AI tooling costs more time than it saves.

**Genuinely useful:**
- **Concept and silhouette exploration.** Generating 50 Vestige silhouettes in an hour to find 3 worth modelling. This is the highest-value use by a wide margin.
- **Texture and material generation.** Tileable surfaces, decals, insignia sheets, grunge, wear masks. Very high value, low risk.
- **Colour/palette variation.** Rarity tints, faction colourways, Altered stage progression maps.
- **Reference boards** and material studies.
- **UI iconography** — eight slot icons, affix icons, rarity frames. Large volume, low individual stakes.
- **Retopology and UV assist** in the DCC of choice.
- **Code and tooling**: the SceneCapture setup, editor utilities, batch import scripts, asset validation (e.g. the "no Altered asset referenced pre-Act-II" check in §2.2).

**Not useful yet, do not plan around it:**
- Production-ready rigged, skinned, game-topology characters.
- Anything requiring exact silhouette control across LODs.
- Animation for movement verbs specific to this game.
- The FP arms. Author these by hand.

**The specific high-leverage workflow:** AI-generate silhouettes → pick 3 → block out in 3D → **playtest in the gym** → author only what survived. The playtest gym already exists and produces a clipboard report; use it as the gate.

### 7.4 The sequence — six phases

Each phase ends playable. This follows the `CONTEXT.md` architecture rule: *build systems vertically and keep the project playable after each milestone.*

---

> **NOTE BEFORE PHASE A — plan the rig for two player models on day one. [O14]**
>
> **RULED [O14]: two player models are planned — Human and Effigy. Both are real player options, not cosmetic afterthoughts. Human ships first; Effigy follows.** §3.1 currently defers the Effigy and says "build nothing." That deferral stands for *assets*. It must **not** extend to the *rig and attachment scheme*, because those are exactly the decisions that are cheap to make today and ruinous to change later.
>
> Before a single arm is skinned, commit to:
>
> 1. **One shared skeleton** used by both the Human and the Effigy. Not two rigs, not a "we'll retarget later" plan.
> 2. **One shared animation set** authored against that skeleton, so every movement pose, slide, wall ride, dash, and air jump plays on either model without re-authoring.
> 3. **A paper-doll attachment scheme that does not assume human proportions.** Gear attaches by socket and adapts by parameter — no gear mesh may bake in human limb length, shoulder width, or hand shape as a fixed assumption. The Effigy is a non-human chassis wearing the same militia kit (§3.1), and that is the whole point of the read: the kit says Breaker, the chassis says it was not built for this.
>
> This is the cheapest possible insurance. **A note today versus a re-rig in a year.** If the FP arms, the third-person modular body, the six gear slots, and the bought base skeleton in §7.2 are all committed to human proportions before the Effigy exists, then shipping the Effigy means re-rigging the player, re-authoring the animation set, and re-fitting every gear part already built — a cost that grows with every asset authored after this point.
>
> Nothing in Phase A changes scope because of this. Only the rig and socket decisions change, and they cost time now measured in a conversation. **GAP — the specific socket list and proportion-independent fitting method are not designed here.**

---

**PHASE A — Replace what the player stares at (target: ~2 weeks)**
*Goal: the game stops looking like a blockout, with the least possible work.*
1. Author FP arms + gloves. Retarget a bought animation pack.
2. Author the Rifle. Establish the armoury material vocabulary here — every later weapon inherits it.
3. Hand-author the four movement poses no pack will have: slide, wall ride, dash, air jump.
4. Wire the dodge/block **feedback** per §3.5 (camera lean, screen ring). No animations.
5. Keep the blockout meshes swappable.

*Exit gate:* a tester who saw the blockout build says the game looks like a different project. FP FOV still configurable. Blockout fallback still works.

---

**PHASE B — Establish the palette and the arena (target: ~2 weeks)**
1. Lock the master material library: one master material, instanced everywhere. Palette per Pillar 3.
2. Buy a modular industrial/concrete kit. Retexture to palette. Build the rift arena from it, bent wrong.
3. Blockout-only lighting pass. Rift chroma at geometry boundaries.
4. Validate movement affordances: ≥3 wall-ride surfaces, ≥2 slide-throughs, ≥1 optional air-jump route.
5. Buy and retune Niagara packs for muzzle, impact, and rift ambience.

*Exit gate:* the arena passes the movement-affordance count, and a run/jump-only player can clear it.

---

**PHASE C — The Vestiges (target: ~3 weeks)**
1. AI-generate ~50 silhouettes. Select 3 against the §2.1 rules.
2. Blockout all three, drop them into the gym, playtest for readability.
3. Author whichever survived. Do not author all three at once — do Crawler first, playtest, then the others.
4. Elite modifier: scale + material parameters, **plus authoring the one shared modifier-VFX library** — roughly six reusable, parameterised effects (capsule, decal ring, tether, trail, polygon, streak) per the revised §2.1. **This is real, scheduled work, not a free shader toggle.** Still zero new meshes, and still no per-modifier bespoke effect.
5. Flat-grey test.

*Exit gate:* the flat-grey acceptance criteria in §2.1 pass at 9/10, **and** the shared modifier-VFX library exists with all its primitives parameterised and reusable on both a Vestige and (later) an Altered without modification.

*Note:* the previous version of this phase assumed elites cost nothing. That assumption is withdrawn. **GAP — the phase duration in §7.6 has not been re-estimated to absorb the library; that is an owner scheduling call, and no number is changed here.**

---

**PHASE D — Remaining weapons and the paper-doll (target: ~2.5 weeks)**
1. Author Shotgun and Rocket. Rocket ground-decal ring must match the code radius.
2. Kitbash SMG and Sniper as placeholders.
3. Buy a base third-person body. Author the militia kit layer over it.
4. Build the paper-doll stage per §6. **On-demand capture from the first line of code** — do not build it capturing every frame and optimise later.
5. Build the gear parts library and the five rarity treatments.

*Exit gate:* all §6.5 paper-doll criteria pass, including the `stat GPU` zero-cost-while-closed check.

---

**PHASE E — The Altered (target: ~3 weeks)**
1. Author the Altered base body over the same bought base as the player.
2. Author the three-stage ladder as mesh states. **Author Stage 1 and Stage 3 first** — the extremes define the range; Stage 2 is interpolation between two known ends and is much cheaper once they exist.
3. Author ALT_First (the Act II turn model). Wounded, insignia complete, shoulder repair readable at 20m.
4. Author ALT_Commander as an outer layer over the base body.
5. Behaviour hookup: cover use, flinch, and retreat driven by stage.
6. Run the asset-reference validation: **no Altered asset referenced anywhere pre-Act-II.**

*Exit gate:* an unprompted tester reports that some Altered "look further gone." The stage ladder is the moral spine of the game and this is the only test that matters.

---

**PHASE F — The Forge and Anchor slice (target: ~2 weeks)**
1. Author the Forge room. Warm, cramped, tool-dense.
2. Author Kess's Effigy hands as the focal point. Caretaker origin visible.
3. Kitbash the market street and vendor stall from a bought city kit.
4. Author the suppression pylon.
5. Compose the pillar image: market, ordinary life, pylon in frame, nobody looking.

*Exit gate:* the §4 acceptance criteria pass, including zero explanatory signage.

---

### 7.5 Standing production rules

1. **One master material.** Everything is an instance. This is the largest single time saving available to a solo dev and it is unrecoverable if skipped early.
2. **Trim sheets and shared texture atlases** over unique unwraps. Author one 2K militia-kit trim sheet and reuse it across the player, the Altered, and the Anchor props.
3. **Fixed triangle budgets**, decided before modelling: FP arms 25k, weapons 15–20k, normal enemies 10–16k, boss 45k, player TP body 30k across all modules.
4. **Nanite for static environment geometry, not for characters.** Skeletal meshes still need honest topology and LODs.
5. **Naming discipline** per `CONTEXT.md` content conventions: `SM_`, `SK_`, `M_`, `MI_`, `T_`, `BP_`. All new content under `Content/ProjectBreaker`.
6. **Never hand-edit `.uasset`/`.umap`.** Everything through the editor or supported automation.
7. **Author on Windows.** The MacBook is for C++, docs, and Data Assets (`CONTEXT.md` machine constraints). Do not attempt a lighting build or large import on it.
8. **Git LFS is already routing binary assets** — verify new asset extensions are covered in `.gitattributes` before the first large import.
9. **Do not delete the blockout.** Every replacement keeps a fallback until the replacement reaches parity.

### 7.6 Rough total

| Phase | Weeks |
|---|---|
| A — FP arms + rifle | 2 |
| B — Palette + arena | 2 |
| C — Vestiges | 3 |
| D — Weapons + paper-doll | 2.5 |
| E — Altered | 3 |
| F — Forge + Anchor | 2 |
| **Total** | **~14.5 weeks of art**, assuming full-time and no mechanic churn |

Realistic planning number: **20 weeks**, because mechanic churn is certain and the plan deliberately blocks authoring until mechanics are proven.

---

## 8. Consolidated acceptance criteria — slice art done

- [ ] No blockout geometry visible in a normal playthrough (fallbacks retained but unused).
- [ ] Vestige flat-grey identification 9/10; Altered stage ranking 8/10; unprompted stage recognition confirmed.
- [ ] All five weapons silhouette-identifiable; all five read as one armoury.
- [ ] No dodge/block animation exists; feedback produces zero position delta across 100 procs.
- [ ] No grapple/hook/tether geometry anywhere in the project.
- [ ] Paper-doll: zero cost when closed, ≤1.5ms when open, all five rarities readable without text.
- [ ] Arena playable by a run/jump-only player; ≥3 wall-ride surfaces, ≥2 slide-throughs, ≥1 optional air-jump route.
- [ ] Zero Altered asset references in any pre-Act-II content, verified by search.
- [ ] Rift chroma appears only on rift phenomena, Vestige interiors, Anomalous items, and suppression hardware. *(Scope settled by **RULED [O19]**: routine Rift-element damage VFX is **outside** the reservation and uses the distinct hotter/whiter cyan — "saturated teal is a property of objects, not of damage." §2b.)*
- [ ] No Rift-element damage VFX uses the reserved teal band; it reads as a hotter/whiter cyan and is never mistaken for a rift-origin object.
- [ ] Elites: no new meshes, and every elite modifier read composes from the one shared modifier-VFX library (§2.1).
- [ ] One shared skeleton and one shared animation set, with a paper-doll attachment scheme carrying no baked-in human proportion assumption (§7.4 pre-Phase-A note, [O14]).
- [ ] One master material; all surfaces are instances.
- [ ] Anchor contains no signage explaining rifts, Altered, or severance.
- [ ] Accuracy While Airborne is visually identifiable on HELM/GLOV at T2+, in first person and on the paper-doll.

---

## 9. OPEN QUESTIONS

1. ~~**[BLOCKING] Are block and dodge passive chances or stance/timing inputs?**~~ **CLOSED — RULED [O1]: passive chance, no stamina pool, no inputs, no animations.** Parry is the only defensive input and has its own short cooldown. Defensive animation work is unblocked. (§3.5)

2. **[BLOCKING for the Rocket] Self-damage and self-knockback rules.** Master sheet 12.5 lists this OPEN, and the Rocket currently ignores its instigator. If self-knockback ships, the Rocket needs a launch pose, a distinct camera treatment, and probably an authored recovery — and rocket-jumping becomes a movement verb the master sheet's guardrails (5.4) have not accounted for. Cannot finish the Rocket art until decided.

3. **Is the player character a cipher or a person?** Master sheet 1.8 lists this OPEN. It determines whether the third-person body needs a face at all, whether there is character customisation, and whether the Effigy option is a player choice or lore. This is the largest single swing in total character-art cost in the project — potentially doubling it.

4. Does the slice ship three finished weapons or five? Code has five; master sheet 12.3 scopes three. This plan assumes three finished + two kitbashed. (CONFLICT, §5)

4b. ~~**[OWNER-CHOICE PENDING — §2b] Does the Rift element get a distinct stated shade, or does the Pillar 3 reservation narrow to rift phenomena rather than rift damage?**~~ **CLOSED — RULED [O19].** Rift-element damage gets a distinct **hotter/whiter cyan**; the Pillar 3 teal reservation stays intact; the rule is **"saturated teal is a property of objects, not of damage."** The elements are **Rift / Entropy / Void**. Rift-element damage VFX and `UI-UX-Spec.md` §4.5's element glyphs are unblocked *in rule*. **GAP — no shade, hex, or band is authored; the specific hotter/whiter cyan and the Entropy/Void colours remain an art-authoring step under the O2 value freeze.** (§2b)

5. Is Anomalous permitted to be the only rarity carrying rift chroma? (EXTENDS, §3.3) If Anomalous items are not rift-derived in fiction, this needs a different visual language.

6. Does the Anchor exist in the vertical slice at all, or only the Forge? This plan builds Forge-only. Confirm before Phase F.

7. Do Effigies have legal personhood inside an Anchor (master sheet 1.8)? Affects whether Effigy NPCs appear in civilian spaces or only in militia ones — a real environment-population decision.

8. Elite modifier list is undesigned (master sheet 10.3). **REVISED:** this plan now assumes elites are material + scale + **one shared modifier-VFX library of ~six reusable effects**, with no new mesh and no per-modifier bespoke effect. If any elite modifier needs bespoke geometry, or cannot be composed from the six primitives, that assumption and the Phase C budget both break. **GAP — Phase C's duration has not been re-estimated to absorb the library.**

9. ~~Are the three build-defining legendaries in the slice Anomalous or Aberrant?~~ **RULED — `Decisions.md` O32, and the question's framing was wrong.** They are not "Anomalous *or* Aberrant", because legendary is not a rarity at all. **Anomalous is a rarity** (the fifth tier, gating affix count and tier ceiling, carrying one *rolled* rule rewrite from a generic pool of four). **Legendary is a separate field** — `FBreakerItemInstance::LegendaryId` — naming a specific authored item with a fixed slot, guaranteed affixes and a *hand-authored* rule. **Every legendary rolls at Anomalous rarity; most Anomalous drops are not legendaries.** Aberrant does not enter into it.

    **The art consequence, which is the reason this question was asked: three bespoke meshes, not attachments — but the cost model in §3.3 is now wrong in both directions.** §3.3 commits to "~1 bespoke mesh per Anomalous (there are 3 build-defining legendaries in the slice, so 3 meshes)", which silently assumed one mesh per legendary *and* that Anomalous and legendary are the same population. They are not. The honest budget has two lines:
    - **Legendaries: one bespoke mesh each, and the pool is ruled to GROW.** O32 keeps the drop rate and grows the pool instead, because three legendaries covering three of eight slots is what makes the current wait ~57 hours rather than ~21. So this is not a fixed 3 — it is 3 today, 8 at one per slot, and more after. **Whoever costs art for this needs the target pool size, and nobody has ruled one.** It is the largest uncosted art commitment in this plan.
    - **Generic Anomalous drops carry a *rolled* rule from a pool of four and are ordinary items otherwise.** They cannot each have a bespoke mesh — they are a rarity, not a set. They need the rarity treatment ladder (rift chroma on the item, unmistakable at a glance) and nothing bespoke.

    **Gap for the owner:** this document does not say how a legendary reads differently from a generic Anomalous drop *on the item itself*. Both are rift-chroma'd rule-rewriters under §3.3. If a named legendary is meant to be recognisable as that specific item at a glance — and a looter shooter usually wants it to be — that is a visual language this plan has not authored.

10. What is the target platform and performance envelope? All triangle budgets, Nanite decisions, and the paper-doll capture budget in this document assume a PC target roughly equal to the Windows dev machine. A console or Steam Deck target would revise every number in §7.5.
