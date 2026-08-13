# Art and Modelling Plan — Rior's Edge (Project Breaker)

Owner: Art / Character direction
Status: Design pass. No assets authored yet.
Scope: Vertical slice only, with production hooks for Act I–II.
Reads from: `Docs/Design/Master-Sheet-Import.txt` (LOCKED decisions are law), `CONTEXT.md`, `Docs/Layer-Ownership.md`, `Docs/Character-Progression-Architecture.md`.

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
| Dodge/block are passive chance layers, not inputs | **They must not have a triggered player animation that reads as a dodge roll.** See §3.5. This is the single most likely art mistake in the project. |

CONFLICT — the vertical slice as scoped in `CONTEXT.md` names three weapon archetypes, while five archetypes now exist in code (Rifle, SMG, Sniper, Shotgun, Rocket). This document plans art for all five because the code shipped them, but marks two as **P2 art** so the slice can close on three finished weapons. See §5.

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
| Elite modifier | +50% scale, additional module repetition, rift-chroma bloom in the interior gaps | Existing `ConfigureElite` (1.5x scale, 3x health, 2x damage) | inherits | inherits | Material + scalar only, **no new mesh** |

The elite modifier deliberately costs zero mesh authoring. It is a scale multiplier plus a material parameter and one extra emissive mask. This is the correct pattern for a solo dev: elites are a *shader and scale* feature.

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

Current state (`CONTEXT.md`): `BP_BreakerCharacter` carries blockout first-person arms and weapon geometry, explicitly flagged as replaceable presentation. Recommended next action #5 in `CONTEXT.md` is exactly this replacement.

### 3.1 What a Breaker is, visually

Militia, not military. Equipped by an organisation that has money for the things that keep people alive and nothing left over. The read is **competent, funded unevenly, personally maintained.**

- Silhouette: human, armoured at the torso and shins, deliberately *light* at the shoulders and hips so the movement kit is believable. A player who wall-rides and slides cannot look like a heavy infantryman.
- Layer grammar, inside to out: bodysuit → soft armour vest → hard plate at chest and shins → webbing/pouches → outer shell garment.
- Every hard surface is scuffed at the edges and clean in the recesses. Wear is where a body touches things.
- One personal item per character, always visible in first person: a wrist wrap, a taped ring, a charm on the sling. This is the single cheapest humanising detail available and it costs one 200-tri asset.

**Effigy variant (machine Breaker).** The master sheet establishes Effigies as machine Breakers, originally caretakers, repurposed. **DEFER for the slice** — an Effigy player option doubles every arms asset and every gear-fit problem. Record the direction now, build nothing:
- Effigies wear the *same* militia kit over a non-human chassis. The kit is the tell that they are Breakers; the chassis is the tell that they were not built for it.
- Caretaker origin should be visible in the hands: too gentle, too many fine joints, designed to handle things carefully. The militia bolted a weapon onto something built to carry a child.
- Effigy hands are the strongest single art idea available for the setting. Worth building the day a second arms set is affordable.

### 3.2 First-person arms — the replacement spec

This is the asset the player looks at for 100% of playtime. It gets the highest per-triangle scrutiny in the project.

| Property | Spec | Rationale |
|---|---|---|
| Arms rig | Single FP arms skeletal mesh, UE5 Manny-compatible hierarchy | Lets you retarget bought animation packs without a bespoke rig |
| Triangle budget | 25k tris both arms, LOD0 only (no LODs needed — always at camera) | FP arms never LOD |
| Texture | 2× 2048 (arms, gloves) | Gloves separate so gear-slot variation is possible later |
| FOV | Separate FP FOV for the arms/weapon draw, default 70–80, **exposed and configurable** | Master sheet 5.4 requires FOV changes be subtle and configurable; settings already persist FOV per `CONTEXT.md` |
| Camera-space offset | Arms parented to camera, weapon parented to arms, not to the character mesh | Standard, and keeps movement-driven camera roll from detaching the weapon |
| Gloves | Sockets for gear variation at wrist and knuckle from day one | Gloves are a real equipment slot (GLOV) carrying crit, handling, airborne accuracy |

**Motion set required for the slice.** Ordered by how often the player sees it:

1. Idle / breathing (weapon down-ish)
2. Sprint (weapon carried low, off-axis, arms swing — this is the single strongest "movement game" tell)
3. Slide entry / slide loop / slide exit — **weapon braced, off-hand down, low camera** ; the slide is a locked base-kit verb and it needs its own arms pose, not a crouch reuse
4. Wall ride left / right — off-hand reaches toward the wall. 0.85s max duration per master sheet 5.3, so the pose must read instantly.
5. Dash — a short, hard camera-space lurch. **Redirect, not launch.** Dash "redirects momentum" per 5.3, and the animation must not imply a rocket.
6. Air / falling
7. Fire (per archetype), reload (per archetype), aim-down-sights transition
8. Air jump (Kinesis-granted) — off-hand snap. Must look *earned*, since it is one of only two tree-granted verbs in the game.

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

**Locked constraint: dodge is a passive chance to fully evade and block is a passive chance to reduce damage. They are defensive layers, not player inputs.**

The obvious art instinct — a dodge-roll animation, a shield-raise stance — would lie to the player about the control scheme and take a week to undo.

CONFLICT — `CONTEXT.md` line 47 currently describes block as a "frontal-only block stance" with a "dodge negation window," and master sheet 3.8 says BLOCK *"requires a shield or stance, works FRONTALLY only."* Master sheet 7.11 also lists an OPEN question: *"What dedicated input slots do Block and Dodge use?"* and 3.15 asks *"Whether Block requires a shield item or is a stance."* The passive-chance framing given as a hard constraint for this document contradicts the stance/input framing in the code and the master sheet. **This must be resolved before any block/dodge animation work begins.** This document proceeds on the passive-chance reading and specifies accordingly. If the stance reading wins, §3.2's motion set gains two poses and this section is void.

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

**Design principle: the archetype is readable from the silhouette in the corner of the eye.** In a looter shooter the player swaps weapons constantly; the first-person silhouette is the swap confirmation. Each archetype gets one dominant, unmistakable silhouette feature that no other archetype uses.

| Archetype | Slot fantasy | Dominant silhouette feature | Material/finish read | Fire feel | Art priority |
|---|---|---|---|---|---|
| **Rifle** | PRIMARY — commitment. Ramping damage, big mags, range. | Long, straight, balanced. A boxy top-mounted optic. The reference weapon. | Standard-issue: uniform matte, factory finish, minimal personalisation. The one gun that looks *issued*. | Steady, controlled, mid-punch. Modest recoil returning to centre. | **P0** |
| **SMG** | SECONDARY — tempo. Swap speed, on-swap bursts. | Short, stubby, front-heavy, no stock or a folded one. Smallest silhouette. | Cheap and replaceable. Stamped, polymer, visibly mass-produced. | High cadence, low per-shot weight, screen-space buzz rather than kick. | P2 |
| **Sniper** | PRIMARY — reach. Effective range, pierce, headshot. | Extremely long, thin, with an oversized cylindrical optic that breaks the top line. Longest silhouette by 40%. | Personally maintained. Taped, bedded, a hand-marked dope card on the stock. The most *individual* weapon. | One heavy event. Hard hit-stop, long recovery, screen settles slowly. Every shot is a decision. | P1 |
| **Shotgun** | Multi-shot behaviour. Pellet spread. | Wide, thick, blunt. Widest silhouette. Exposed pump or break action — a visible *mechanism* the player can watch move. | Industrial, blunt, unglamorous. Closer to a tool than a firearm. | Maximum single-frame impact. Biggest hit-stop, biggest screen kick, deepest audio. | **P0** |
| **Rocket** | Movement interaction. Radial falloff. | Tube. Shoulder-borne, back-heavy, visibly asymmetric. The only weapon that breaks the player's shoulder line. | Field-fabricated. Welded, hand-painted markings, hazard amber striping. Looks like it might not be safe. | Slow, heavy, committed. Long readied pose, visible projectile, a real reason to reposition after firing. | **P0** |

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
| Elite modifier | **Material + scale only** | Zero mesh cost by design |
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
4. Elite modifier: scale + material parameters only.
5. Flat-grey test.

*Exit gate:* the flat-grey acceptance criteria in §2.1 pass at 9/10.

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
- [ ] Rift chroma appears only on rift phenomena, Vestige interiors, Anomalous items, and suppression hardware.
- [ ] One master material; all surfaces are instances.
- [ ] Anchor contains no signage explaining rifts, Altered, or severance.
- [ ] Accuracy While Airborne is visually identifiable on HELM/GLOV at T2+, in first person and on the paper-doll.

---

## 9. OPEN QUESTIONS

1. **[BLOCKING — resolve before any defensive animation work] Are block and dodge passive chances or stance/timing inputs?** This document's hard constraint says passive chance. `CONTEXT.md` line 47 describes a "frontal-only block stance" and a "dodge negation window" in shipped code. Master sheet 3.8 says block "requires a shield or stance, works FRONTALLY only," and 7.11 and 3.15 both list this as OPEN. The two readings produce completely different art. Resolve first. (CONFLICT, §3.5)

2. **[BLOCKING for the Rocket] Self-damage and self-knockback rules.** Master sheet 12.5 lists this OPEN, and the Rocket currently ignores its instigator. If self-knockback ships, the Rocket needs a launch pose, a distinct camera treatment, and probably an authored recovery — and rocket-jumping becomes a movement verb the master sheet's guardrails (5.4) have not accounted for. Cannot finish the Rocket art until decided.

3. **Is the player character a cipher or a person?** Master sheet 1.8 lists this OPEN. It determines whether the third-person body needs a face at all, whether there is character customisation, and whether the Effigy option is a player choice or lore. This is the largest single swing in total character-art cost in the project — potentially doubling it.

4. Does the slice ship three finished weapons or five? Code has five; master sheet 12.3 scopes three. This plan assumes three finished + two kitbashed. (CONFLICT, §5)

5. Is Anomalous permitted to be the only rarity carrying rift chroma? (EXTENDS, §3.3) If Anomalous items are not rift-derived in fiction, this needs a different visual language.

6. Does the Anchor exist in the vertical slice at all, or only the Forge? This plan builds Forge-only. Confirm before Phase F.

7. Do Effigies have legal personhood inside an Anchor (master sheet 1.8)? Affects whether Effigy NPCs appear in civilian spaces or only in militia ones — a real environment-population decision.

8. Elite modifier list is undesigned (master sheet 10.3). This plan assumes elites are always material + scale with no new mesh. If any elite modifier needs bespoke geometry, that assumption and the Phase C budget both break.

9. Are the three build-defining legendaries in the slice Anomalous or Aberrant? Master sheet 12.5 asks whether unique weapons and Anomalous items are the same system. Determines whether they need bespoke meshes (Anomalous) or attachment + emissive mark (Aberrant) — a difference of roughly a week.

10. What is the target platform and performance envelope? All triangle budgets, Nanite decisions, and the paper-doll capture budget in this document assume a PC target roughly equal to the Windows dev machine. A console or Steam Deck target would revise every number in §7.5.
