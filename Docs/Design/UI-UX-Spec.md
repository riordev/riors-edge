# UI / UX Specification — Rior's Edge (Project Breaker)

**Scope:** slice (see `Vertical-Slice.md`).
**Last reconciled against: O40**

Status: design pass 1. Authored against the Master Sheet, `CONTEXT.md`,
`Docs/Character-Progression-Architecture.md`, and `Docs/Layer-Ownership.md`.

**AUTHORITY CORRECTION (O28).** This document was written when
`Master-Sheet-Import.txt` was treated as law. **It is not, and has not been
since O28** — it is historical source material and is not to be cited as
authority again. The chain is `Decisions.md` → `CONTEXT.md` → `Design-Overview.md`
(map, not law) → the per-domain docs. Where this document defers to the master
sheet below, read that deference as void and check `Decisions.md` instead.

**SCOPE CORRECTION.** This is the broad UX domain document. It is **not** the
visual authority. FIELDPLATE is: `UI-Style-Guide-Fieldplate.md` owns palette,
type scale, shape and motion, and the HUD, Inventory, Skill Tree and Ability
Icon specs own their screens and carry the implementation-status sections
recording what is built and what has been photographed. Where this document
and a FIELDPLATE spec disagree about a colour, a size or a layout, **the
FIELDPLATE spec wins** — it is the one the code reads tokens from.

Everything marked **EXTENDS** adds detail the master sheet does not contain and does not contradict. Everything marked **CONFLICT** contradicts or pressures an existing decision and needs a ruling before implementation.

**Rulings applied to this document** (see `Docs/Design/Decisions.md` — those rulings are law and supersede any conflicting text here): **O1** (stamina pool removed; block/dodge passive; Parry the only defensive input), **O5** (three elements), **O18** (TTK/TTD seed targets exist; see §4.6 and §12), **O19** (the three elements are **Rift / Entropy / Void** — "Time" is renamed; Rift-element *damage* colour ruled — see §4.4/§4.5), **O8** (the endgame farm content type is named **Frontier**), **O11** (Aberrant: up to 3 equipped, global), **O12** (scalar tiered crafting currencies).

**O8 sweep result for this document:** the content-type word "Anomaly" does not appear anywhere in this spec, so no rename was required. Every occurrence of **Anomalous** in this document is the **rarity tier**, which *keeps its name* under O8 and must not be renamed. Any future UI string naming the endgame farm content type uses **Frontier**.

---

## 0. Product position

Rior's Edge is a first-person movement shooter first and an ARPG second. That ordering decides every UI argument in this document:

1. **The centre of the screen belongs to combat.** Nothing persistent lives within 20% of screen height/width of centre except the crosshair and the hitmarker.
2. **Screen-space reading happens at the periphery, world-space reading happens on the target.** Enemy state (health, ailments, weak points) is read off the enemy, not off a bar at the top of the screen.
3. **The player is frequently airborne, sliding, or wall-riding.** Anything requiring a steady eye — item comparison, tree navigation, crafting — is a full-screen modal that pauses or occurs in the Anchor. There is no in-combat inventory management.
4. **Gear is the entire endgame (LOCKED, level cap 50 hard stop).** Therefore the item tooltip and the comparison view are the single most important screens in the game, not the HUD. They receive the most design and the most iteration budget.
5. **Solo is the primary balance target (LOCKED).** All HUD affordances must be legible with zero teammates. Party UI is additive and never load-bearing.

### The single readability rule

> If a player cannot answer "am I about to die?", "is my ability up?", and "did that shot connect?" using only peripheral vision while airborne, the HUD has failed.

Everything else — DPS, stack counts, buff durations, drop feed — is secondary and is allowed to be slower to read.

---

## 1. Implementation tiering — Slate now, UMG later

The project currently ships an asset-free Slate front end (`SBreakerMenu`) and a Canvas-drawn playtest HUD (`ABreakerPlaytestHUD`). Both are code-driven, zero-asset, and testable on the memory-constrained Mac. That is the correct posture and should be held until the systems below stop changing shape.

### Tier A — build/keep in code-driven Slate + Canvas HUD now

These are systems whose *rules* are still being authored. Code-driven UI keeps iteration cost near zero and does not create `.uasset` merge conflicts across the two machines.

| Screen | Reason it stays code-driven |
|---|---|
| Combat HUD (health/shield/armour/ammo/movement state) | Values and attributes still moving |
| Ability + ultimate slots | Class kits are Data-Asset-driven and unbuilt |
| Inventory / paper doll | Item model is settling; comparison logic is the deliverable |
| Item tooltip + comparison | The highest-value screen; needs the most iterations |
| Skill tree (functional list/grid form) | ~100 nodes unwritten; needs a *data* view before a *map* view |
| Class select | Ships now, permanent decision, minimal art dependency |
| Settings | Already persistent; no reason to move |
| Death / respawn | Needs to exist for the encounter slice; text-only is acceptable |
| Vendor / Forge (functional) | Crafting verbs exist on paper only |

### Tier B — waits for UMG + a presentation pass

Do not start these until Tier A stops changing and an art direction exists.

- Damage numbers as world-space pooled widgets with motion/arc/scale
- Rarity beam / drop-feed VFX and pickup pings
- Skill tree as a **constellation map** with authored node art, connective lines, parallax starfield
- Item card frames, rarity borders, item icons, hover glow
- Anchor diegetic screens (Forge Keeper terminal, Quartermaster stall)
- Boss health bar, encounter intro cards, rift-entry transitions
- Any animation longer than a colour lerp

### The rule for crossing the boundary

A screen may move from Tier A to Tier B only when: (a) the underlying data model has not changed for two milestones, and (b) the screen has been played, not just compiled. `CONTEXT.md`'s handoff discipline applies — do not claim a screen exists because it compiles.

**EXTENDS** — none of this is in the master sheet; it is an implementation-sequencing rule derived from the project's stated Mac/Windows machine constraints.

---

## 2. Colour language

### 2.1 Rarity (LOCKED — from Loot 4.1)

| Rarity | Name | Colour | Hex (proposed) | Equip limit |
|---|---|---|---|---|
| 1 | Standard | White | `#E6EAF0` | — |
| 2 | Uncommon | Blue | `#3C8FE0` | — |
| 3 | Exceptional | Purple | `#9B4DE0` | — |
| 4 | Aberrant | Red | `#E03B3B` | max 3 equipped |
| 5 | Anomalous | Teal | `#20D6C8` | max 1 equipped |

**EXTENDS — hex values.** The master sheet names colours only. These are proposed values chosen for: distinguishability under deuteranopia/protanopia (the Red/Teal pair carries the two highest-stakes rarities and is the *most* separable pair in every common CVD simulation), and adequate contrast against the existing dark panel base `#03060A` at 86% alpha used by `DrawPanel`.

**Accessibility requirement:** rarity must never be signalled by colour alone. Every rarity-bearing element also carries a **rarity glyph** and a **border weight**:

| Rarity | Glyph | Border |
|---|---|---|
| Standard | (none) | 1px, no accent |
| Uncommon | `·` | 1px accent |
| Exceptional | `··` | 2px accent |
| Aberrant | `◆` | 2px accent + corner notches |
| Anomalous | `◈` | 3px accent + full-perimeter accent |

### 2.2 Functional colours (EXTENDS)

These must not reuse rarity hues at high saturation, or the player will misread a status effect as a drop.

| Meaning | Colour | Notes |
|---|---|---|
| Health | `#E62E24` | Matches current HUD `(0.9, 0.18, 0.14)` |
| Shield | `#14A6FF` | Matches current HUD `(0.08, 0.65, 1.0)` |
| Armour | `#D9E5F2` | Neutral, deliberately not a bar |
| ~~Stamina (block/dodge pool)~~ | ~~`#F0C24A`~~ | **RULED [O1] — REMOVED.** The stamina pool is deleted entirely; block/dodge are passive chance layers. This functional colour has no referent and must not be used. |
| Class resource | per class (see 2.3) | The one element that changes per character |
| Ability ready | `#2EB3FF` | |
| Ultimate ready | `#BF59FF` | Matches current `(0.75, 0.35, 1.0)` |
| Cooldown fill | `#3B4756` | Desaturated; never coloured |
| Weak-point hit | `#FFBF0D` | Matches current `(1.0, 0.75, 0.05)` |
| Crit hit | `#FFF2C2` | Brighter/whiter than weak point — see §4.4 |
| Incoming damage | `#FF1F0D` | Matches current damage vignette |
| Positive delta (comparison) | `#4CD97B` | |
| Negative delta (comparison) | `#E0555A` | Deliberately distinct from Aberrant red |
| Locked / unaffordable | `#5A6675` | |

**RULED [O1] — the stamina pool is removed entirely.** Block and dodge are passive chance layers, not spends; there is no shared pool, no regeneration rule, and no HUD element for it. The previous text here (a shared 100-point pool regenerating after a delay, flagged as an unrepresented gap) is void. `Stamina`/`MaxStamina` are removed from the attribute set and combat component.

**The surviving UI trace of block/dodge is the DODGED / BLOCKED feedback popup**, which already ships. It is a transient, world-anchored proc readout in place of the absent damage number — not a resource display. See §4.2 and §4.4.

### 2.3 Class resource colours (EXTENDS)

Five classes, five resources (Progression 7.5). Each needs a distinct hue that reads against both the health red and the shield blue.

| Class | Resource | Colour |
|---|---|---|
| Caster | Mana | `#5B7BFF` |
| Swift | Momentum | `#3BE0A0` |
| Gunsmith | Scrap | `#E08A2E` |
| Tank | Grit | `#C4A15E` |
| Support | Charge | `#E0D64A` |

Swift's Momentum green and Support's Charge yellow are the risk pair against the positive/negative delta colours; those colours only appear in menus, never on the combat HUD, so the collision is acceptable.

### 2.4 Affix tier marking (EXTENDS)

The tier scale is T8 → T1 → T0 → T-1 with a deliberate value spike at the top (Affixes 3.1). The master sheet states T0 is "visibly marked on the item." Extending that to the full scale:

- **T8–T2** — affix line printed in neutral text, no tier badge.
- **T1** — tier badge `T1` in muted gold.
- **T0** — tier badge `T0` in bright gold, plus the affix line renders at 110% weight.
- **T-1** — tier badge `T-1` in white-on-gold inverted chip, plus a leading `▲` on the line.

The visual jump between T1 and T0 must be larger than the jump between T3 and T2, mirroring the value curve. That is the entire point of the marking.

**Marquee stat treatment:** `Accuracy While Airborne` is called out in Affixes 3.4 as the affix that most sells "movement FPS" and is instructed to be made visually obvious. It renders with a persistent inline glyph (`✈`) at every tier regardless of roll. It is the only affix with this treatment. If a second affix ever earns it, that is a design decision requiring a note here, not a UI convenience.

---

## 3. Interaction patterns

### 3.1 Established (LOCKED by current implementation)

- **Click-to-equip.** Single left click on a backpack card equips it into its slot. No drag-and-drop anywhere in the game. Drag-and-drop is unusable on controller, requires precise pointer control, and buys nothing when items have exactly one legal destination slot.
- **Escape backs out one menu layer at a time.** Already implemented in `SBreakerMenu::HandleEscape`; do not add screens that break this chain.
- **Slot filter chips** in the backpack. Already implemented (`BackpackSlotFilter`).
- **Best-rarity-first sort** as the backpack default. Already implemented.

### 3.2 Extended interaction grammar (EXTENDS)

| Action | Mouse | Controller | Notes |
|---|---|---|---|
| Equip | Left click | A / Cross | Instant, no confirm |
| Inspect / pin tooltip | Hover | Right stick focus | Tooltip follows focus |
| Compare | Automatic on hover | Automatic on focus | Never a separate mode |
| Mark as junk | Right click | X / Square | Toggles a junk flag |
| Salvage single | Middle click | Y / Triangle hold 0.4s | Hold-to-confirm |
| Salvage all junk | Button in header | Button in header | Modal confirm with count |
| Lock item | `L` | R3 | Blocks salvage entirely |
| Filter cycle | Chip click | LB / RB | Cycles slot filters |
| Back | Escape | B / Circle | One layer |

**Two-key destructive rule:** any action that permanently destroys an item requires either a hold-to-confirm or a modal with an explicit count. Salvaging a T0 item by accident is a materially worse outcome than in most ARPGs, because T0 items are the *only raw material* for T-1 (Affixes 3.1). The UI must treat T0 and above as semi-precious.

**Auto-lock rule (EXTENDS):** items are auto-locked when they satisfy any of: rarity ≥ Aberrant, contains a T0 or T-1 affix, or is currently equipped. Auto-lock is visible (a lock glyph) and manually removable. This is a safety net, not a policy the player cannot override.

### 3.3 Input reservations

Progression 7.11 leaves open: *"What dedicated input slots do Block and Dodge use? Dodge should use a dedicated action rather than double-tap detection."* CONTEXT.md confirms block/dodge input actions are not yet bound.

**RULED [O1] — resolved. Block and dodge are passive; there is no stamina pool.** The former CONFLICT is closed. Consequences, now binding:

- Block % and Dodge % are **stat lines**, not HUD elements with cooldowns.
- There is no stamina pool and therefore no stamina HUD element of any kind.
- Progression 7.11's open question about block/dodge input slots is **struck**.
- `UBreakerCombatComponent`'s block stance / dodge window API is a *system-internal* proc, not a bound input.
- **Parry is the only defensive player input**, and it runs on its own short cooldown (tree-granted, Bulwark). It is the sole defensive element permitted a cooldown-like HUD treatment.
- The only defensive UI that ships is the **DODGED / BLOCKED feedback popup** already implemented.

### 3.4 What the UI must never do

- Never gate a combat-relevant decision behind a menu the player must open mid-fight.
- Never use a hover-delay tooltip in combat. Tooltips are menu-only.
- Never animate a number the player is expected to read precisely (ammo, health value).
- Never place a persistent element in the centre 40% × 40% of the screen.
- Never signal state by colour alone (see §2.1).
- Never reflow the HUD between fights. Positions are fixed; only content changes.

---

## 4. HUD

### 4.1 Wireframe — combat HUD

```
┌──────────────────────────────────────────────────────────────────────────┐
│ [TL] objective / rift timer          [TR] compass strip · rift depth     │
│                                                                          │
│                                                                          │
│                                                                          │
│                                                                          │
│                            ┌───────────┐                                 │
│                            │           │                                 │
│                            │     ✛     │  ← crosshair + hitmarker only   │
│                            │           │                                 │
│                            └───────────┘                                 │
│                          (protected centre box)                          │
│                                                                          │
│                                                                          │
│                                          [MR] status effect column       │
│                                          [MR] drop feed (transient)      │
│                                                                          │
│ ┌─ VITALS ────────────┐ ┌─ WEAPON ─────┐ ┌─ ABILITIES ─────────────────┐ │
│ │ MOVE STATE  SPEED   │ │ SLOT 1 RIFLE │ │ [ Q ] [ E ]        [ X ]    │ │
│ │ ███████ HEALTH      │ │              │ │  ABIL1  ABIL2       ULT     │ │
│ │ ███████ SHIELD      │ │   24 / 108   │ │                             │ │
│ │ ▓▓▓ ARMOUR          │ │  1 P   2 S   │ │  [resource bar ▬▬▬▬▬▬▬▬ ]   │ │
│ └─────────────────────┘ └──────────────┘ └─────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
```

**RULED [O1]** — the `STAM ▬` element previously drawn on the armour row has been removed from this wireframe. No stamina element exists on the HUD.

The bottom band matches the shipped `ABreakerPlaytestHUD` layout (left vitals panel at x=24, weapon panel at +352, ability panel at +634). This spec keeps that geometry and adds content, rather than proposing a relayout — the existing positions are already validated in play.

### 4.2 Vitals panel (bottom-left)

**Contents, top to bottom:**

1. **Movement state + speed** — `SPRINT   SPEED 950`. Already shipped. Retain. This is a movement game; the speed readout is genuinely diagnostic and doubles as a build-feedback channel once movement affixes are equipped. Keep it in the shipped build, not just diagnostics.
2. **Health bar** — 205px wide, red, with numeric `current / max`.
3. **Shield bar** — 205px, blue, directly beneath health, same width. Shields route before health (Scaling 6.1), so the visual stack must read bottom-up: shield depletes, then health. **Draw shield above health** so the depletion order reads downward on screen, matching the mitigation order. *(Note: this inverts the current shipped order, which draws health above shield. Intentional change.)*
4. **Armour** — numeric only, no bar. Armour is a mitigation *coefficient* (`EffectiveArmor / (EffectiveArmor + 100)`, capped 80%), not a pool. A bar would lie about what it is. Display as `ARMOUR 140 · 58%` — the raw value and the resulting mitigation percentage. **EXTENDS** — showing the derived percentage is the single most useful stat-literacy affordance in the whole HUD and costs one string format.
5. ~~**Stamina** — a thin amber underline beneath the armour row.~~ **RULED [O1] — DELETED.** There is no stamina pool and no stamina readout. Nothing replaces it in this panel; the armour row is the last line of the vitals panel.

**Passive-defence feedback (the surviving trace of block/dodge).** Because there is no resource to read, the only block/dodge UI is the **DODGED / BLOCKED popup** already shipped: a short-lived text proc readout at the impact point, in place of the damage number for a full evade, and alongside the reduced number for a block. It is feedback that a passive layer fired, never a state the player manages. Presentation rules live in §3.5's prohibitions and §4.4's damage-number typing; no new element is added to the vitals panel.

**Damage direction:** the existing full-screen border flash on damage is a *presence* indicator, not a *direction* indicator. Add a directional arc: a 60°-wide arc segment on a ring at 22% screen radius, pointing at the damage source, fading over 1.2s, stacking up to four sources. This is the one exception to the "nothing near centre" rule, and it is transient.

**Low-health state:** below 30% health, the screen edges gain a persistent desaturated red vignette (configurable intensity, per Movement 5.4's requirement that camera/FOV/shake effects be subtle and configurable). No heartbeat audio-visual pulse — it competes with the movement-state readout.

### 4.3 Ability / ultimate slots

**LOCKED constraint:** a character equips exactly two class abilities plus one ultimate (Progression 7.5). The HUD therefore has exactly three ability slots, permanently. It never needs to scale.

Slot anatomy (each 112 × 54 px, matching shipped `DrawAbilitySlot`):

```
┌──────────────┐
│ Q            │  ← keybind, top-left, always visible
│              │
│    ICON      │  ← Tier B; Tier A shows the ability's short name
│              │
│ ▓▓▓▓▓░░░░░░  │  ← cooldown sweep, left-to-right fill
└──────────────┘
   3.2s           ← remaining, only while on cooldown
```

**States:**

| State | Presentation |
|---|---|
| Ready | Full colour, 2px accent top bar, subtle idle brightness |
| On cooldown | Desaturated to `#3B4756`, sweep fills L→R, numeric remaining |
| Cooldown complete | One-frame white flash + 0.15s outward pulse |
| Unaffordable (resource) | Full colour but resource cost text in red, slot dimmed 40% |
| Ultimate charging | Accent bar fills as a *charge* meter, not a cooldown sweep |
| Ultimate ready | Accent bar fully lit + slow 1.2s breathing pulse |

**Ultimate is deliberately differentiated.** Cooldown reads as "wait." Charge reads as "earn." The ultimate should be the only element on the HUD that pulses when available, so its readiness is detectable in peripheral vision without a glance.

**Cooldown Reduction feedback (EXTENDS):** Cooldown Reduction % is a Helmet exclusive and an Aberrant signature exists ("Crits reduce all cooldowns by 0.5s"). When a cooldown is reduced by an external source, the sweep must visibly *jump backwards* with a brief accent flash rather than smoothly interpolating. The player needs to see the proc fire, or the Aberrant signature is invisible and therefore feels worthless.

### 4.4 Damage numbers policy

This is a contested area in every looter shooter and it needs a policy, not a toggle dump.

**Ruling: damage numbers are ON by default, world-space, aggressively pooled, and semantically typed.**

Rationale: the entire endgame is gear (LOCKED). The player's only feedback channel for whether an affix is doing anything is the number. Hiding it hides the game's endgame.

**Rules:**

1. **World-space, anchored to the impact point**, drifting up and slightly outward, 0.9s lifetime, ease-out.
2. **Aggregation window: 120ms per target.** All damage to the same target inside a 120ms window merges into a single number. This is mandatory — Multishot (+3 at T-1), Pierce, Ricochet, and shotgun pellets otherwise produce a wall of digits that communicates nothing. **EXTENDS.**
3. **DoT ticks aggregate on a 500ms window** and render in a dimmer, smaller style. They must never out-shout direct hits.
4. **Semantic typing:**

| Type | Size | Colour | Extra |
|---|---|---|---|
| Normal hit | 1.0× | `#E6EAF0` | — |
| Weak point | 1.25× | `#FFBF0D` | — |
| Critical | 1.4× | `#FFF2C2` | slight scale-punch on spawn |
| Critical weak point | 1.6× | `#FFD84D` | scale-punch + brief ring |
| DoT tick | 0.7× | ailment / element colour (§4.5) | dimmer, no punch |
| Shield damage | 1.0× | `#14A6FF` | — |
| Absorbed / zero | 0.8× | `#5A6675` | shows `0`, never hidden |
| Overkill | — | — | not shown; see below |

5. **Crit and weak point are visually distinct, not merged.** Crit is the only multiplier of its kind (LOCKED, Scaling 6.3) and the weak-point multiplier is a separate step in the damage order (6.1 step 2 vs step 3). Merging their presentation would make the player unable to tell which one they are building for. This matters because Precision constellation, Headshot Damage (helmet exclusive), and Crit Damage are three separate investments.
6. **Absorbed / zero damage is shown, not suppressed.** A grey `0` teaches the player about armour caps and immunity phases. Silence teaches them the game is broken.
7. **No overkill numbers, no cumulative DPS meter, no damage-per-second readout.** A DPS meter converts a build-crafting game into a spreadsheet-optimisation game and pushes the community toward a single correct answer. The Forge's build preview (§7) is where numbers get compared, deliberately out of combat.
8. **Hard cap: 40 simultaneous numbers.** Oldest cull first. Non-negotiable for a game where a Multishot+Pierce+Ricochet build hits eight enemies per trigger pull.

**Damage-type colour [O19].** Where a damage number or DoT tick is coloured by element, **Rift-element damage renders in a hotter, whiter cyan — never the reserved teal band.** The rule, verbatim: **"saturated teal is a property of objects, not of damage."** Saturated teal remains the Anomalous rarity / rift-phenomena colour and must not appear as a damage colour. Entropy and Void damage colours are unauthored. **GAP — no element colour values exist; see §4.5.**

**Settings exposed:** off / self only / all; scale multiplier 0.5–2.0; aggregation window 60/120/250ms. Default: all, 1.0, 120ms.

### 4.5 Status effects

**Player statuses** — vertical column, mid-right, right-aligned, growing downward. Max 8 visible + `+N` overflow chip.

Each entry: `[glyph] NAME  4.2s` with a thin depletion underline. Buffs sort above debuffs. Within each group, sort by *remaining duration ascending* so the thing about to expire is nearest the top of its group and its position is predictable.

**Enemy statuses** — rendered on the enemy, not in a screen-space list. Above the enemy's nameplate: a compact row of ailment glyphs with stack counts.

```
        ┌──────────────────────────┐
        │  ELITE · ALTERED SCOUT   │  ← name, elite prefix
        │  ▓▓▓▓▓▓▓▓░░░  ███░░      │  ← health bar / shield segment
        │  🩸3   ☠5   ❄            │  ← ailment glyphs + stacks
        └──────────────────────────┘
```

**Ailment glyph set (EXTENDS):**

| Ailment | Glyph | Colour | Shows stacks |
|---|---|---|---|
| Bleed | drop | `#C42A2A` | yes |
| Poison | trefoil-ish | `#7FC42A` | yes (cap visible) |
| Ignite | flame | `#FF7A1F` | no |
| Chill / Slow | crystal | `#5FD8FF` | no, shows potency ring |
| Shock | bolt | `#E0D64A` | no |

**RULED [O5] — exactly THREE element glyphs, not four. RULED [O19] — the three elements are RIFT, ENTROPY, and VOID** ("Time" is renamed to **Entropy**). The `Ignite / Chill / Shock` rows above are **placeholder naming** and will be re-flavoured onto Rift / Entropy / Void when the per-element resistance model is implemented (resistances apply after armour, before shields). Reserve three element glyph slots in the layout budget, not four, and do not author a fourth.

**Element names are now FINAL — Rift / Entropy / Void [O19].** The naming half of the former 6b gap is closed; use these names in every future UI string and rename target.

**Rift damage colour rule [O19]:** Rift-element *damage* renders in a **hotter, whiter cyan** — never the reserved teal band. The rule, verbatim: **"saturated teal is a property of objects, not of damage."** Saturated teal stays with Anomalous rarity, rift phenomena, and suppression hardware (`Art-And-Modelling-Plan.md` Pillar 3, reservation intact).

**GAP — glyph SHAPES and COLOUR VALUES for Rift / Entropy / Void are still unauthored.** Names are final; visual identity is not. No shade, hex, or band is written here: Rift damage is constrained only by the verbal rule above, and Entropy and Void have no colour direction yet. The existing hex values in the table above are placeholder and carry no ruling. Do not author element colour values in this document.

Ignite, Chill, and Shock — i.e. the three element slots under their placeholder names — remain **BLOCKED** — Affixes 3.7 and Scaling 6.1 both state elemental lines cannot ship until a resistance model exists. Their glyphs are specified here so the layout budget is reserved, but they must not be implemented before the resistance step exists in the damage pipeline. Bleed and Poison are physical and can ship now; `UBreakerStatusComponent` already runs them.

**Poison stack cap must be visible.** Poison Stacks is a discrete affix (+1 → +7). A player who has invested in stacks needs to see `☠ 7/9`, or the investment is invisible.

### 4.6 Enemy nameplates and health

- **Normal enemies:** health bar appears only on damage, fades after 2.5s. No persistent nameplate — a movement shooter with dense packs cannot afford permanent labels. **[O18]** — TTK/TTD **seed targets now exist** (trash a little under 1s, rare/elite ~3s, boss 20–45s; TTD 4–5s bare). The nameplate fade and the boss bar's phase segmentation must be checked against those seed targets, not against a guess. No timing value in this document is changed here; the seed targets are inputs wave mode measures divergence from (O2 freeze still applies).
- **Elites:** persistent nameplate with elite modifier name. CONTEXT.md notes elites already exist (`ConfigureElite`: 1.5× scale, 3× health, 2× damage). The modifier name must be shown; an unlabeled elite is an unexplained difficulty spike.
- **Boss:** dedicated top-centre bar, Tier B. Segmented by phase count. This is the one permitted violation of the "nothing at top-centre" convention and only during a boss encounter.
- **Weak points:** highlighted only while aiming (ADS) and within effective range. Persistent weak-point highlighting turns every encounter into a whack-a-mole against a glowing dot and undercuts the "advanced movement is optional, not mandatory" guardrail (Movement 5.4).

### 4.7 Drop feed

Transient list, right side, above the status column, 4 entries max, 5s lifetime each.

```
  ◈  ANOMALOUS   Boots
  ◆  ABERRANT    Necklace
  ··  Exceptional  Gloves
```

**Rarity floor is a setting**, defaulting to Exceptional and above. Below Exceptional the feed becomes noise. Standard and Uncommon are explicitly designated fodder (Loot 4.2) and do not deserve a notification.

**Anomalous and Aberrant drops additionally trigger a one-shot centre-screen banner** (0.8s, fades, non-blocking, no input capture). A max-1-equipped Anomalous is a build-defining event and the master sheet frames finding a second one as *"a question, not an upgrade."* The UI should mark the moment.

---

## 5. Inventory / equipment

The current Slate paper-doll (`BuildInventoryScreen`, `MakeGearCard`) is the seed. This section specifies where it goes.

### 5.1 Wireframe — inventory screen

```
┌── INVENTORY ────────────────────────────────────  [ESC BACK] ───────────┐
│                                                                          │
│  ┌── EQUIPPED ────────────────┐  ┌── BACKPACK ─────────────────────────┐│
│  │                            │  │ [ALL][HELM][BODY][GLOV][BOOT]        ││
│  │  HELMET      ┌──────────┐  │  │ [NECK][WAIST][P1][P2]      SORT ▾   ││
│  │  ┌────────┐  │          │  │  ├─────────────────────────────────────┤│
│  │  │ ◆ item │  │          │  │  │ ◈ Anomalous · Boots         ilvl 46 ││
│  │  └────────┘  │  PAPER   │  │  │   Slide momentum uncapped           ││
│  │  BODY        │  DOLL    │  │  ├─────────────────────────────────────┤│
│  │  ┌────────┐  │  (Tier B │  │  │ ◆ Aberrant · Gloves         ilvl 44 ││
│  │  │ ·· item│  │   silhou-│  │  │   Sliding applies Bleed             ││
│  │  └────────┘  │   ette)  │  │  ├─────────────────────────────────────┤│
│  │  GLOVES      │          │  │  │ ·· Exceptional · Helmet     ilvl 44 ││
│  │  BOOTS       └──────────┘  │  │   +14% Cooldown Reduction  T1       ││
│  │  NECKLACE                  │  ├─────────────────────────────────────┤│
│  │  WAIST                     │  │              ...                     ││
│  │  PRIMARY                   │  │                                      ││
│  │  SECONDARY                 │  │ [SALVAGE JUNK (7)]  [MARK FODDER]   ││
│  │                            │  └─────────────────────────────────────┘│
│  │  ◆ ABERRANT   2 / 3        │                                          │
│  │  ◈ ANOMALOUS  1 / 1        │  ┌── TOOLTIP / COMPARISON ────────────┐ │
│  │                            │  │  (see §5.3)                        │ │
│  │  ── CHARACTER ──           │  │                                    │ │
│  │  HP 1840   SHIELD 620      │  │                                    │ │
│  │  ARMOUR 140 (58%)          │  │                                    │ │
│  │  CRIT 34% × 2.15           │  │                                    │ │
│  │  MOVE +22%  SLIDE +31%     │  │                                    │ │
│  └────────────────────────────┘  └────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Equip limit counters — load-bearing, not decorative

`◆ ABERRANT 2/3` and `◈ ANOMALOUS 1/1` sit permanently in the equipped column. From End-Game 9.2: *"The equip limits on Aberrant and Anomalous are what keep the endgame from becoming pure accumulation."* If the limit is invisible, the constraint that carries the entire endgame is invisible.

When the player hovers an item that would exceed a limit, the counter turns red and the equip button becomes a **swap picker** — a small inline list of the currently-equipped items of that rarity, each clickable to designate as the one being replaced. Equipping is never silently blocked and never silently un-equips something the player did not choose.

**RULED [O11] — the Aberrant limit is up to 3 equipped, GLOBAL.** The former OPEN (Loot 4.9) is closed and the wireframe above is correct as drawn: a single `◆ ABERRANT 2/3` counter, not per-slot markers. **The equip-limit UI is unblocked and may be built.**

Note: each Aberrant carries 1–2 unique modifier affixes intended to help define builds. The specific modifiers are owner-authored later — the tooltip's signature-first ordering law (§5.3) already accommodates 1–2 signature lines, and no further UI ruling is needed. **GAP — the modifier list itself does not exist yet;** do not author placeholder signature text in the UI.

### 5.3 Item tooltip — the most important screen in the game

```
┌────────────────────────────────────────────────────┐
│ ◈  DRIFTWALKER'S TREAD                             │  ← name, rarity colour
│    Anomalous · Boots · Item Level 46               │  ← rarity/slot/ilvl
├────────────────────────────────────────────────────┤
│  ANOMALOUS                                         │  ← rule rewrites first
│  Slide momentum retention is uncapped.             │     always at top,
│                                                    │     teal, boxed
├────────────────────────────────────────────────────┤
│  PREFIXES                                     3/4  │
│  ▲ +38%  Slide Momentum Retention        [T-1]     │
│    +20%  Slide Speed                      [T1]     │
│    +12%  Movement Speed                   [T2]     │
│                                                    │
│  SUFFIXES                                     2/4  │
│    +180  Health                           [T1]     │
│    +9%   Dodge Chance                     [T3]     │
├────────────────────────────────────────────────────┤
│  6 / 7 affixes · 1 open suffix                     │  ← crafting grammar
├────────────────────────────────────────────────────┤
│  [EQUIP]   [LOCK 🔒]   [SALVAGE]                   │
└────────────────────────────────────────────────────┘
```

**Ordering law (EXTENDS).** Affix lines are ordered:

1. Anomalous / Aberrant fixed signatures — **always first, always visually boxed.** These rewrite rules; they are not stats. Loot 4.6's design test is "if the affix could be expressed as a percentage, it does not belong on an Anomalous" — the UI must honour that by never mixing them into the stat list.
2. Prefixes (offense / movement / handling / utility), sorted by tier descending, then by value descending.
3. Suffixes (defense / resource / sustain / find), same sort.

Prefix and suffix are never interleaved. The 4-prefix / 4-suffix cap (Affixes 2.5) is stated as `3/4` and `2/4` headers, and the footer states the open-slot grammar explicitly — *"I need a suffix slot open"* is quoted in the master sheet as the intended crafting language, so the UI should speak it.

**Tier badges are right-aligned in a fixed column** so a player can scan a tooltip vertically and read tier quality as a shape without reading any words. This is the single highest-value legibility decision in the tooltip.

### 5.4 Comparison

**Comparison is automatic and always-on, never a held key.** Hovering a backpack item shows the tooltip *with* the currently-equipped item of that slot inline, not in a second floating panel.

```
┌────────────────────────────────────────────────────┐
│ ·· FIELD TREAD                    vs  DRIFTWALKER  │
│    Exceptional · Boots · ilvl 44                   │
├────────────────────────────────────────────────────┤
│  PREFIXES                                          │
│    +22%  Slide Speed        [T1]    ▲ +2%          │
│    +8%   Movement Speed     [T4]    ▼ -4%          │
│    +18%  Air Control        [T2]    ▲ new          │
│    —     Slide Momentum             ▼ lost -38%    │
├────────────────────────────────────────────────────┤
│  NET EFFECT                                        │
│    Health          1840 → 1660      ▼ -180         │
│    Move speed      +22% → +18%      ▼ -4%          │
│    Slide speed     +31% → +33%      ▲ +2%          │
│    Air control     +0%  → +18%      ▲ +18%         │
│    ⚠ Loses: slide momentum uncapped (Anomalous)    │
└────────────────────────────────────────────────────┘
```

**Design rules for comparison:**

- **Show the derived character totals, not just the item deltas.** A player cannot evaluate "+22% Slide Speed" without knowing their current total. The `NET EFFECT` block is the answer, and it is what makes the additive-Increased-bucket rule (LOCKED: flat sums → one additive Increased bucket → More reserved for trees/Anomalous) legible. Because Increased percentages all live in one additive bucket, deltas are *actually additive* and the arithmetic shown is honest. This is a direct benefit of the locked aggregation rule and the UI should exploit it.
- **Never show a single "item score" number.** A score number collapses a build-crafting game into a comparison of one integer, and it will be wrong for any build that cares about a non-weighted stat. This is a permanent prohibition.
- **Losing an Anomalous or Aberrant signature is called out explicitly** with a warning line, because it is a rule loss, not a stat loss, and no delta arithmetic will surface it.
- **Weapon-vs-weapon comparison compares archetypes too.** Swapping a Rifle for a Sniper changes cadence, magazine, falloff, and spread — the tooltip must show those base stats alongside affixes or the comparison lies. Primary and Secondary have opposed design intents (commitment vs tempo, Gear 2.4); the tooltip should label which one an item's affixes lean toward.

### 5.5 Salvage flow

1. Player marks items junk (right click / X) or uses **auto-mark fodder**: marks every unlocked Standard and Uncommon item. Justified directly by Loot 4.2 — *"Standard and Uncommon must stay fodder."*
2. `SALVAGE JUNK (7)` shows the live count.
3. Clicking opens a modal: item count by rarity, the material yield, and an explicit list of anything ≥ Exceptional included.
4. Confirm requires a second click on a differently-positioned button (not the same screen coordinate as the opener).
5. Result is a brief material-gain toast. No animation, no per-item ceremony.

**Locked and auto-locked items are never salvageable by any bulk action**, only by individually unlocking them first.

**RULED [O12] — crafting materials are 3–4 tiered SCALAR currencies, not item-derived.** The salvage modal's yield line is therefore a short list of scalar currency gains, not an item list, and no material occupies an inventory or stash slot. **GAP — the currencies themselves (count, names, tiers, yields) are owner-authored and not designed here.** Do not author counts or names in this document.

---

## 6. Skill tree UI

### 6.1 The scale problem

Two separate point pools, two separate trees, ~100 nodes total:

| Pool | Points | Levels | Structure |
|---|---|---|---|
| Class Points | 30 | 1–30 | 3 branches, one permanent class |
| Core Points | ~65 | 1–50 + ~15 from world content | 6 constellations |

The budget target is *"two constellations fully developed and a third partially"* (Progression 7.4). The UI's job is to make that budget legible while the player spends, not after.

The vertical slice ships ~15 nodes with a compressed cap of 10 (Progression 7.9). **The slice UI must therefore work at 15 nodes and scale to ~100 without a rewrite.** That constraint alone rules out a hand-authored fixed-position map for Tier A.

### 6.2 Two-tab structure

```
┌── PROGRESSION ──────────────────  CLASS: SWIFT (locked) ──  [ESC] ─────┐
│  [ CLASS TREE  12/30 ]   [ CORE TREE  0/65 ]                           │
├────────────────────────────────────────────────────────────────────────┤
```

Class Tree and Core Tree are **tabs, never a merged view.** Layer-Ownership.md is explicit that the class layer and tree layer own different things; merging them in UI invites the player to treat them as one budget, and they are not — one stops at 30, the other runs to 50.

### 6.3 Class tree — three-column branch view

```
┌── CLASS TREE ── SWIFT · MOMENTUM ────────── 12 / 30 pts ── [RESPEC] ───┐
│                                                                        │
│  ┌─ FRENZY ────────┐ ┌─ KINETIC ───────┐ ┌─ MARKSMAN ──────┐          │
│  │  invested: 8    │ │  invested: 4    │ │  invested: 0    │          │
│  ├─────────────────┤ ├─────────────────┤ ├─────────────────┤          │
│  │ ● ● ● ○         │ │ ● ● ○ ○         │ │ ○ ○ ○ ○         │  tier 1  │
│  │ ── gate 5 ──    │ │ ── gate 5 ──    │ │ ── gate 5 ──    │          │
│  │ ● ● ○           │ │ ● ● ○           │ │ ○ ○ ○           │  tier 2  │
│  │ ── gate 12 ──   │ │ ── gate 12 ──   │ │ ── gate 12 ──   │          │
│  │ ◆ ○             │ │ ◆ ○             │ │ ◆ ○             │  tier 3  │
│  │ ── gate 20 ──   │ │ ── gate 20 ──   │ │ ── gate 20 ──   │          │
│  │ ★ CAPSTONE      │ │ ★ CAPSTONE      │ │ ★ CAPSTONE      │  tier 4  │
│  └─────────────────┘ └─────────────────┘ └─────────────────┘          │
│                                                                        │
│  ── EQUIPPED ─────────────────────────────────────────────────────────│
│  ABILITY 1 [ Dash Strike ]  ABILITY 2 [ ---- ]  ULTIMATE [ ---- ]     │
└────────────────────────────────────────────────────────────────────────┘
```

**Investment gates are rendered as horizontal rules with a running requirement**, and the branch header shows `invested: N` live. The player must be able to see how far they are from the next gate without arithmetic.

**OPEN (Progression 7.11):** *"Are branch nodes freely mixed with investment gates, or mutually exclusive at major tiers?"* The wireframe above assumes **freely mixed with gates** — the player may spread across all three branches but reaches capstones only by concentrating. If major tiers become mutually exclusive instead, this view needs an explicit "choosing this locks out X" confirmation modal, which is a different screen. **This is open question #2.**

**Ability equip strip is on this screen, not a separate one.** A character equips exactly two abilities plus one ultimate; that decision is downstream of tree investment and belongs adjacent to it.

### 6.4 Core tree — constellation navigation

Six constellations, ~65 points, budget target of "two full plus one partial."

**Tier A (now) — the hub-and-spoke list.**

```
┌── CORE TREE ────────────────────────────── 41 / 65 pts ── [RESPEC] ───┐
│                                                                       │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐                        │
│  │ PRECISION  │ │  VOLLEY    │ │ AFFLICTION │                        │
│  │  ████████  │ │  ███░░░░░  │ │  ░░░░░░░░  │  ← invested / total    │
│  │  18 / 18   │ │   7 / 18   │ │   0 / 18   │                        │
│  │  COMPLETE  │ │            │ │            │                        │
│  └────────────┘ └────────────┘ └────────────┘                        │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐                        │
│  │  ELEMENTS  │ │  BULWARK   │ │  KINESIS   │                        │
│  │  ░░░░░░░░  │ │  ░░░░░░░░  │ │  ████████  │                        │
│  │  BLOCKED   │ │   0 / 18   │ │  16 / 18   │                        │
│  │  no resist │ │            │ │  ✦ AIR JUMP│  ← verb grant marker   │
│  └────────────┘ └────────────┘ └────────────┘                        │
│                                                                       │
│  BUDGET  41 spent · 24 remaining                                      │
│  ▸ enough to complete Kinesis (2) and partially open one more (22)    │
└───────────────────────────────────────────────────────────────────────┘
```

Clicking a constellation zooms into its node list. Returning is one Escape press.

**Three things this view must do:**

1. **Show the budget consequence, not just the balance.** The `▸` line is the whole design of this screen. The intended shape is two full plus one partial; the UI should state what the remaining points can actually buy so the player converges on that shape by understanding rather than by regret.
2. **Mark verb-granting nodes distinctly.** Air jump (Kinesis) and parry (Bulwark) are the **only** tree-granted verbs in the entire game (LOCKED). They get a `✦` marker at the constellation level and a full-width highlighted card at the node level. Everything else in every tree is a modification of something the player already has; these two are new capabilities and must not be mistaken for percentages.
3. **Mark BLOCKED constellations honestly.** Elements is blocked until an elemental resistance model exists in the damage pipeline. **[O5] / [O19]** — when it unblocks, the constellation covers exactly three elements (**Rift / Entropy / Void**); its current Ignite/Chill/Shock node naming is placeholder and is a rename target, not new content. In a dev/slice build it shows as `BLOCKED — no resistance model`. In shipping it simply does not appear. Do not ship a constellation that grants nothing.

**Tier B (later) — the constellation map.** Star-field background, nodes as stars, connective lines as constellation edges, camera pan/zoom, each of the six occupying a region. This is the presentation payoff and it is genuinely a good fit for the fiction (timelines, erased worlds, star maps). It is also the single most expensive UI in the game and must not be built before the node list is final. **Building the map first would lock node counts and adjacency into art.**

### 6.5 Node card

```
┌───────────────────────────────────────────────┐
│  ✦  AIR JUMP                    KINESIS · T2  │
│                                               │
│  Grants: a second jump while airborne.        │
│                                               │
│  RANK 0 / 1                                   │
│  Cost: 2 points                               │
│  Requires: 8 invested in Kinesis              │
│                                               │
│  ⚠ This is one of two verbs granted by any    │
│    tree. Affixes can scale it; nothing else   │
│    can grant it.                              │
│                                               │
│  [ ALLOCATE ]                                 │
└───────────────────────────────────────────────┘
```

For ranked nodes, show `RANK 2 / 5` with the *current* and *next* values side by side: `+12% → +18%`. Never show only the per-rank increment; the player is choosing a total, not a step.

**Rule-rewrite nodes get prose, percentage nodes get numbers.** Layer-Ownership.md states that a tree node reading as a flat percentage is doing the affix layer's job. The UI should make that uncomfortable: rule-rewrite nodes render in a wider card with a sentence; percentage nodes render as compact one-liners. If a designer opens the tree and sees a wall of compact one-liners, the tree has drifted and the UI has told them so. **EXTENDS — this is a deliberate use of UI as a design smell detector.**

### 6.6 Respec

LOCKED: Class Point respec is free at a Forge; Core Point respec is free unless playtesting establishes a reason for friction; respecs require Forge interaction; class selection is permanent.

- The `[RESPEC]` button is **visible everywhere but only enabled at a Forge.** When disabled it reads `RESPEC — requires Forge`. Hiding it would leave the player unable to discover that respec exists.
- Respec is a full-tree reset with a single confirm, not per-node refunds. Per-node refunds invite fiddling and make the Forge-gate pointless.
- The confirm modal names the Forge Keeper. Highest interaction count of any NPC in the game (NPCS 13.2); the UI is where that relationship is actually built.

---

## 7. Vendor and Forge

### 7.1 Forge — five crafting verbs

From Loot 4.7. Each is a distinct interaction, not a dropdown.

```
┌── THE FORGE ──────────────────────────────── KESS ────────  [ESC] ────┐
│                                                                        │
│  ┌── ITEM ──────────────────┐  ┌── OPERATIONS ─────────────────────┐  │
│  │  ·· EXCEPTIONAL          │  │                                    │  │
│  │  Rift-Cut Vambrace       │  │  ADD AFFIX          [ 240 mat ]    │  │
│  │  Gloves · ilvl 44        │  │  fills 1 open suffix               │  │
│  │                          │  │                                    │  │
│  │  PREFIXES           4/4  │  │  REROLL VALUE       [  40 mat ]    │  │
│  │  +26% Reload Speed  [T1] │  │  within current tier band          │  │
│  │  +10% Crit Chance   [T1] │  │                                    │  │
│  │  +35% Crit Damage   [T2] │  │  UPGRADE TIER       [ 900 mat ]    │  │
│  │  +22% Weapon Dmg    [T2] │  │  one affix, one tier · cannot      │  │
│  │                          │  │  reach T-1 · cost escalates        │  │
│  │  SUFFIXES           2/4  │  │                                    │  │
│  │  +180 Health        [T1] │  │  EXALT / CORRUPT    [ T0 required ]│  │
│  │  +9%  Block         [T3] │  │  ⚠ the only route to T-1           │  │
│  │                          │  │  ⚠ can fail · consumes the T0      │  │
│  │  6/7 · 2 open suffixes   │  │                                    │  │
│  └──────────────────────────┘  │  DIVINE ORDER       [ 320 mat ]    │  │
│                                │  rerolls which affixes are present │  │
│  [ CHANGE ITEM ]               │  keeps the count                   │  │
│                                └────────────────────────────────────┘  │
│                                                                        │
│  ── RESULT PREVIEW ───────────────────────────────────────────────────│
│  Health 1840 → 2020   ▲ +180      (preview only — nothing spent yet)  │
└────────────────────────────────────────────────────────────────────────┘
```

**Rules:**

- **Every operation states its constraint inline**, not in a tooltip. `UPGRADE TIER` states "cannot reach T-1" on the button. That constraint is the load-bearing rule that keeps T0 items valuable as material (Affixes 3.1); it must not be discoverable only by failure.
- **EXALT / CORRUPT gets a distinct visual treatment and a two-step confirm.** It is the only route to T-1, it consumes a T0 affix, and it *should carry a failure or downside chance* (Loot 4.7). A destructive gamble on the rarest input in the game gets the game's most emphatic confirmation: a modal stating exactly what is consumed, the failure chance, and what failure does.
- **Result preview before spend.** For deterministic operations (Upgrade Tier), show the exact result. For stochastic operations (Reroll Value, Divine Order, Exalt), show the **possible range** and the odds, never a single predicted value. Showing a predicted value for a random operation is a lie the player will remember.
- **Affix count and prefix/suffix headroom are always visible** during crafting. The crafting grammar is explicitly "I need a suffix slot open" (Affixes 2.5) and the UI should make that sentence readable at a glance.

### 7.2 Vendor / Quartermaster

Deliberately plain. NPCS 13.7 suggests making the vendor the Anchor's most ordinary person, with no opinion about rifts, to ground the hub. The UI should match: a flat two-column buy/sell list, no ceremony, no rarity fireworks.

```
┌── QUARTERMASTER ─────────────────────────────────────────  [ESC] ─────┐
│  [ BUY ]  [ SELL ]                          CREDITS  12,480            │
├────────────────────────────────────────────────────────────────────────┤
│  ·· Exceptional · Helmet · ilvl 42                        3,200        │
│  ·  Uncommon   · Waist  · ilvl 40                           420        │
│  AMMO · Reserve refill                                        60        │
│  CONSUMABLE · Field stim ×5                                  180        │
├────────────────────────────────────────────────────────────────────────┤
│  [ SELL ALL JUNK (7) — 1,240 ]                                         │
└────────────────────────────────────────────────────────────────────────┘
```

Vendor stock does not sell Aberrant or Anomalous items. The equip-limited rarities are the endgame chase; buying them off a shelf deletes the chase.

---

## 8. Class select

**LOCKED: class selection is permanent per character.** CONTEXT.md confirms `ChoosePermanentClassById` already exists behind a BREAKER CLASS menu screen. This screen therefore has a single job: make sure nobody makes this choice by accident.

```
┌── CHOOSE YOUR CLASS ───────────────────────────────────────────────────┐
│                                                                        │
│  ⚠  THIS CHOICE IS PERMANENT FOR THIS CHARACTER.                       │
│     Skill points can be respecced at a Forge. Your class cannot.       │
│                                                                        │
│  ┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────────┐              │
│  │ CASTER  ││  SWIFT  ││GUNSMITH ││  TANK   ││ SUPPORT │              │
│  │  Mana   ││Momentum ││  Scrap  ││  Grit   ││ Charge  │              │
│  └─────────┘└─────────┘└─────────┘└─────────┘└─────────┘              │
│                                                                        │
│  ┌── SWIFT ──────────────────────────────────────────────────────────┐│
│  │  RESOURCE   Momentum — generated by movement and evasion           ││
│  │                                                                    ││
│  │  BRANCHES   FRENZY    close-range aggression                       ││
│  │             KINETIC   velocity and traversal specialist            ││
│  │             MARKSMAN  precision at range                           ││
│  │                                                                    ││
│  │  PLAYS LIKE  Fastest character in the game. Converts evasion into  ││
│  │              offence. Rewards never standing still.                ││
│  │                                                                    ││
│  │  SOLO        Strong. All five classes are solo-viable.             ││
│  └────────────────────────────────────────────────────────────────────┘│
│                                                                        │
│                       [ CHOOSE SWIFT — PERMANENT ]                     │
└────────────────────────────────────────────────────────────────────────┘
```

**Rules:**

- Confirm button carries the word PERMANENT and names the class. Not "Confirm."
- One additional modal: `Lock SWIFT permanently for this character? This cannot be undone.` Two steps for an irreversible decision, consistent with §3.2's destructive-action rule.
- **Every class card states solo viability**, because solo is the primary balance target (LOCKED) and Support in particular carries an unfair reputation. Support's card must explicitly say it has self-use and offensive conversion paths in every branch (Party Play 11.1).
- **Do not show numbers here.** A player choosing a class at level 1 cannot evaluate stats and will fixate on the wrong one. Show fantasy, resource, and branch names.
- **Prototyping order matters for this screen too.** Swift and Caster ship first (Progression 7.5). Unbuilt classes must be shown as `COMING SOON` and be un-selectable, never hidden — hiding them makes the roster look like five is actually two.

---

## 9. Death and respawn

The vertical slice needs death/retry (Types of Content 10.2). There is currently no death UI.

```
┌────────────────────────────────────────────────────────────────────────┐
│                                                                        │
│                            YOU WERE KILLED                             │
│                          Altered Skirmisher                            │
│                                                                        │
│                    ┌──────────────────────────┐                        │
│                    │  RUN SUMMARY             │                        │
│                    │  Time in rift    04:12   │                        │
│                    │  Enemies killed     34   │                        │
│                    │  Items found         6   │                        │
│                    │  Deaths this run     2   │                        │
│                    └──────────────────────────┘                        │
│                                                                        │
│              [ RESPAWN AT CHECKPOINT ]    [ RETURN TO ANCHOR ]         │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

**Rules:**

- **Name the killer.** In a movement shooter, death is usually a readability failure. Naming the source is the only way the player learns what went wrong. If the killer was a DoT, name the ailment and its source (`Bleed — applied by Altered Skirmisher`); death-by-invisible-tick is the single most frustrating unexplained death in an ARPG.
- **No death penalty is specified anywhere in the master sheet.** The screen assumes none. If a penalty is later added it appears here, stated explicitly, before the respawn buttons.
- **A minimum 1.2s input lockout** before the buttons become active, to stop the player from mashing through the killer's name.
- **The run summary exists so death is informative rather than only punitive.** It also gives the playtest loop a natural data surface — `UBreakerPlaytestComponent` already tracks comparable statistics.
- No slow-motion, no ragdoll ceremony, no long fade. In a fast movement game, time-to-retry is a feel stat. Target: under 3 seconds from death to playing again on `RESPAWN`.

---

## 10. Menu information architecture

```
TITLE
 ├─ NEW CHARACTER ──→ CLASS SELECT ──→ (permanent lock) ──→ game
 ├─ CONTINUE ──────→ game
 ├─ SETTINGS
 └─ QUIT

IN-GAME (ESC)
 └─ PAUSE
     ├─ CHARACTER
     │   ├─ INVENTORY      (paper doll · backpack · tooltip · compare)
     │   ├─ PROGRESSION    (tab: CLASS TREE | tab: CORE TREE)
     │   └─ LOADOUT        (weapon slot archetypes · ability equip)
     ├─ SETTINGS
     │   ├─ CONTROLS       (sensitivity · invert · bindings)
     │   ├─ DISPLAY        (FOV · camera roll · shake · vignette)
     │   ├─ INTERFACE      (damage numbers · drop feed floor · HUD scale)
     │   └─ ACCESSIBILITY  (colourblind mode · glyph emphasis · hold/toggle)
     ├─ RESUME
     └─ QUIT TO TITLE

ANCHOR-ONLY (interaction, not menu)
 ├─ THE FORGE          (crafting · respec)
 └─ QUARTERMASTER      (buy · sell · salvage)
```

**Rules:**

- **Escape backs out exactly one layer.** Already implemented; this IA is built to respect it. Maximum depth from gameplay to any screen is 3 (`ESC → CHARACTER → INVENTORY`).
- **Forge and Vendor are never in the pause menu.** They are Anchor interactions. Respec being Forge-gated is a LOCKED product decision; putting a respec button in the pause menu would silently undo it.
- **Direct hotkeys bypass the chain** for the three most-used screens: `I` inventory, `K` progression, `L` loadout. These open the screen directly and Escape returns straight to gameplay.
- **Settings are reachable from title and pause with identical content.** Divergent settings screens are a permanent source of bugs.
- **Camera roll, FOV change, and shake must be exposed and individually configurable** — this is a stated Movement guardrail (5.4), not a nice-to-have.

### 10.1 Accessibility minimums

| Requirement | Rule |
|---|---|
| Colourblind | Rarity carries glyph + border weight in addition to hue (§2.1) |
| Text size | HUD scale 0.8–1.5×; menu text ≥ 14px at 1080p |
| No colour-only state | Every state has a shape, glyph, or position change |
| Hold vs toggle | Sprint, aim, crouch, slide all offer both |
| Motion | Camera roll, shake, FOV punch, and damage vignette individually configurable to zero |
| Input | No double-tap-to-act anywhere; explicitly noted in Progression 7.11 as conflicting with precise strafing and accessibility |
| Timing | No timed menu inputs; the only enforced delay is the death-screen lockout, which is a lockout not a window |

---

## 11. Build order

| Phase | Deliverable | Gate |
|---|---|---|
| 1 | Armour-percentage display on existing HUD (stamina readout **cut — RULED [O1]**) | **UNBLOCKED [O1]** |
| 2 | Item tooltip with prefix/suffix split and tier badges | Item model stable (it is) |
| 3 | Automatic comparison + NET EFFECT block | Phase 2 |
| 4 | Equip-limit counters + swap picker | **UNBLOCKED [O11]** — global, max 3 |
| 5 | Salvage flow with lock/auto-lock | **UNBLOCKED [O12]** — scalar tiered currencies |
| 6 | Death / respawn screen | Encounter slice |
| 7 | Damage number policy (aggregation, typing, cap) | Phase 1 |
| 8 | Ability slot states incl. ultimate charge differentiation | Class kits exist |
| 9 | Class tree three-column view | Node content authored |
| 10 | Core tree hub view + node cards | Node content authored |
| 11 | Forge screen | Crafting implemented |
| 12 | Vendor screen | Currency implemented |
| — | **Tier B begins only after 1–12 have been played** | — |

---

## 12. Acceptance criteria

### HUD

- [ ] A player can read health, shield, ammo, and ability readiness while airborne without moving their eyes off centre screen.
- [ ] No persistent HUD element intersects the centre 40% × 40% box.
- [ ] Shield renders above health and depletes downward, matching damage resolution order.
- [ ] Armour shows both raw value and derived mitigation percentage.
- [ ] ~~Stamina is visible and pulses below 25%.~~ **RULED [O1] — struck.** Replaced by: no stamina element appears anywhere in the HUD, and a dodge proc renders a `DODGED` popup in place of a damage number.
- [ ] The ultimate is the only pulsing element when ready, and is distinguishable from an ability at a glance in peripheral vision.
- [ ] A cooldown reduced by an external proc visibly jumps backward rather than interpolating.
- [ ] Damage numbers aggregate within 120ms per target; a 3-Multishot shotgun burst on one enemy produces exactly one number.
- [ ] Crit, weak point, and crit-weak-point are three visually distinct presentations.
- [ ] Zero/absorbed damage renders as a grey `0` and is never suppressed.
- [ ] Simultaneous damage numbers never exceed 40.
- [ ] Poison shows current stacks against its cap.
- [ ] Elite enemies show their modifier name.
- [ ] Weak points highlight only while aiming and in range.
- [ ] **[O18]** Damage-number aggregation, the enemy health-bar fade, and the low-health vignette are validated against the **TTK/TTD seed targets** (trash a little under 1s, rare/elite ~3s, boss 20–45s; TTD 4–5s with no resources/sustain) — a trash kill under one second must still produce one readable number and a readable bar, and a 4–5s death must give the vignette time to read. Seed targets are the reference the wave-mode report measures divergence from; **no timing value is authored here.**
- [ ] **[O19]** No Rift-element damage number, DoT tick, or ailment glyph uses the reserved saturated teal — Rift damage reads as a hotter/whiter cyan. *(Rule only; colour values unauthored — GAP.)*

### Inventory and items

- [ ] Every item's rarity is identifiable with all colour removed (glyph + border test).
- [ ] Tooltip orders: signatures → prefixes → suffixes, never interleaved.
- [ ] Prefix and suffix counts show against their 4-cap; footer states open-slot grammar.
- [ ] T0 and T-1 are visually louder than T1, and the T1→T0 step is visually larger than the T3→T2 step.
- [ ] `Accuracy While Airborne` carries its marquee glyph at every tier.
- [ ] Comparison shows character totals before and after, not only item deltas.
- [ ] No item score number exists anywhere in the UI.
- [ ] Losing an Anomalous or Aberrant signature produces an explicit warning line in comparison.
- [ ] Aberrant `n/3` and Anomalous `n/1` counters are permanently visible on the equipped panel.
- [ ] Exceeding an equip limit opens a swap picker; the game never silently unequips.
- [ ] Weapon comparison includes base archetype stats, not only affixes.
- [ ] Items ≥ Aberrant, or containing T0/T-1 affixes, are auto-locked on acquisition.
- [ ] Bulk salvage cannot destroy a locked item under any input sequence.
- [ ] Destroying any item requires a hold or a counted modal.

### Progression

- [ ] Class and Core trees are separate tabs with separate point counters, never merged.
- [ ] Branch investment totals and the next gate threshold are visible without arithmetic.
- [ ] The Core hub states what the remaining budget can actually complete.
- [ ] Air jump and parry nodes are visually distinct from every other node in the game.
- [ ] Blocked constellations state why they are blocked in dev builds and are absent in shipping builds.
- [ ] Ranked nodes show current and next totals, not per-rank increments.
- [ ] Rule-rewrite nodes and percentage nodes are visually distinguishable at a glance.
- [ ] `RESPEC` is visible but disabled outside a Forge, with the reason stated.
- [ ] The tree UI works unmodified at 15 nodes and at 100 nodes.

### Class select, death, IA

- [ ] Class confirmation requires two steps and the word PERMANENT.
- [ ] Every class card states solo viability.
- [ ] Unimplemented classes appear as un-selectable, never hidden.
- [ ] The death screen names the killer, including DoT sources.
- [ ] Death-to-playing is under 3 seconds on `RESPAWN`.
- [ ] Escape backs out exactly one layer from every screen.
- [ ] Maximum menu depth from gameplay is 3.
- [ ] No Forge or Vendor function is reachable from the pause menu.
- [ ] Title and pause settings screens are identical in content.
- [ ] Camera roll, shake, FOV punch, and damage vignette are individually settable to zero.

---

## OPEN QUESTIONS

1. ~~**Are block and dodge passive procs or bound player inputs?**~~ **CLOSED — RULED [O1].** Passive chance layers. The stamina pool is removed entirely, §4.2's stamina readout is deleted, Progression 7.11's input-slot question is struck, and Parry is the only defensive player input (its own short cooldown). HUD phase 1 is unblocked.

2. **Are class branch nodes freely mixed with investment gates, or mutually exclusive at major tiers?** (Progression 7.11, unresolved.) Freely-mixed gives the three-column view in §6.3. Mutually-exclusive requires a lockout confirmation modal and a visibly different tree shape. **Blocking for progression UI.**

3. ~~**Is the Aberrant 3-equipped limit global or per-slot-chosen?**~~ **CLOSED — RULED [O11].** Global, up to 3 equipped. §5.2 as drawn is correct. **The equip-limit UI is no longer blocking.**

4. ~~**Are crafting materials a separate currency or item-derived?**~~ **CLOSED — RULED [O12].** 3–4 tiered scalar currencies. The Forge and salvage screens need a currency header, not a materials container. **GAP — the currencies are not yet designed.**

5. **Does death carry a penalty?** Nothing in the master sheet specifies one. §9 assumes none. If one is added it must be stated on the death screen before the respawn buttons.

6. ~~**What is the exact stamina cap and its gear-scaling rule?**~~ **CLOSED — RULED [O1].** Moot: there is no stamina pool, no cap, and no stamina bar to draw.

6b. **PARTLY CLOSED [O19] — the three elements are named Rift / Entropy / Void** ("Time" renamed). Exactly three, not four. The teal collision is **ruled**: Rift-element damage uses a hotter/whiter cyan, the Pillar 3 teal reservation stays intact, and **"saturated teal is a property of objects, not of damage."** **GAP REMAINS — glyph shapes and colour values for all three are unauthored**, and no shade, hex, or band is written under the O2 value freeze. Names may be used now; colours and glyph art may not be authored here.

7. **Does the player character have a voice/identity, or are they a cipher?** (Fundamentals 1.8.) Affects whether the class select screen and death screen carry any character framing at all, and whether the Anchor UI ever addresses the player by name.

8. **Are drop-feed and item names localised at authoring time, or generated?** Affix-generated item names (prefix-word + base + suffix-word) versus authored names change the tooltip's name field width budget and the drop feed's line count.

9. **Does the HUD need a party layer for the stated 5-player cap, and if so where does it live?** (Party Play 11.3 is mostly undesigned.) The bottom band is already full. Recommend left edge, vertical, minimal — but this is unbuilt and unspecified.

10. **CONFLICT — enemy health bars vs. the "no HUD clutter" pillar in dense Conquest content.** The Types of Content notes describe 9-player warzones with "thousands of mobs." The nameplate policy in §4.6 (health on damage, 2.5s fade) will produce hundreds of simultaneous fading bars in that mode. Conquest likely needs its own nameplate policy — probably elites-and-above only. Deferred, but recorded so the slice does not build a nameplate system that cannot scale.
