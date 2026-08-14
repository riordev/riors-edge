# Playtest Feedback Log

**Last reconciled against: O40**

## 2026-08-14 — Session 4 (class identity, the Forge economy, the skill matrix)

Seven findings. Two of them turned out to be one bug, and three of the UI
findings were confirmed or corrected by CAPTURE rather than by reading code.

| Finding (owner's words) | Root cause | Response |
|---|---|---|
| "when selecting a different class i can only see the swift nodes" | `DevForceClass` set `State.PermanentClass` but deliberately KEPT the held `UBreakerClassDefinition`. `GetAvailableTrees` unions `ClassDefinition->BranchTrees`. | Re-fetch on class MISMATCH (answers the recorded objection about stomping an authored Data Asset), plus the class filter on the reader. |
| "i dont see proper ability selection based on what character im at" | **The same bug.** `IsAbilityUnlocked` answers from the same stale definition. | Same fix. Three new identity tests — `RecalculateStats` re-derived from state, so every NUMBER was right while the whole front end was wrong, and no attribute test could ever have caught it. |
| "the forge should be at the forge npc" | **NOT DONE this session.** The `bIsAtForge` gate already exists on Temper/Reforge/Attune/Respec; the menu passes `true` unconditionally. | Open. The plumbing is there; it needs an NPC-proximity source and a UI tell. |
| "resources for crafting should drop from mobs at a reasonable rate" | The Forge wallet's only income was SALVAGE, so the economy could recycle but never grow. | `RollCurrencyDrop` on the item pipeline's shape, credited before the item roll's early-return. Its simulation-vs-projection test immediately caught a real defect: the roll scaled the SAMPLE and the projection scaled the MEAN, so at ilvl 10 a trash mob paid 0 or 2 Slag and never 1. |
| "i dont see keystones in the skill trees" | Drawn, but the word "KEYSTONE" lived only in the hover card, and a tier-gated keystone printed the EMPTY status line. Separately, the screen's private purchase mirror never implemented O37, so keystones painted purchasable and the click then failed. | Amber KEYSTONE caption, always-stated lock reason, and the mirror now checks commitment and the cornerstone investment gate. |
| "the constellations dont expand like they should / I dont see any layers or details to them" | No expand code existed — it had never been built. Plates were anonymous chip rows with no name, tier, cost or effect. | OPEN CONSTELLATION expands to the node list banded by TIER, reusing the class board's vocabulary. |
| "the skill tree is hard to read when text or numbers are cut off" | NOT a recurrence at the old marker sites (those are correctly fixed). New sites, same mechanism: non-Fill alignment arranges a text block at its measured width and Slate clips the run to that box. | Value columns and `MakeButton` now fill and justify. |

**Found while looking, not reported:** ELEMENTS is sealed with six authored
nodes, and both sealed lines sat inside the "no nodes" branch — it rendered in
sealed-hardware teal and never said SEALED or named Rift / Entropy / Void.

**Photographed for the first time:** the node detail rail. It is hover-only and
the harness has no mouse, so no agent had ever seen it; it now survives the
rebuild a purchase triggers, which is what made it visible to capture.

**Residuals, both visible in the capture and both left open deliberately:**
BACK is still clipped in the header — the trailing group is right-anchored by a
fill spacer and overflows the panel by a fixed amount, so shortening upstream
tabs moved everything except the overflow. And the ELEMENTS plate sits below the
viewport fold, which is the same structural "the board is taller than its
viewport" problem that puts tier-3 keystones off-screen below 1080p. That one
wants a layout decision, not another nudge.


Owner playtest findings and the actions taken, **newest first**. This is the
gym's paper trail — wave-mode reports and re-anchoring decisions cite it. Append
a session per playtest, as a finding → root cause → response table.

## 2026-08-14 — Session 6 (post-O29: movement, ground, loot, HUD, skill board)

The largest single session in the log, spanning three merged lanes. Two things
are worth stating before the table.

**Half of these were found by LOOKING, not by reasoning.** The ground tearing
needed a new grazing-angle capture vantage to be visible at all; the clipped
rank numbers were diagnosed by measuring glyph runs out of a PNG; the wave
banner overprint and every damage number were literally unphotographable until
`-BreakerCaptureHUD` existed — and those two are exactly the two that shipped
broken. That is not a coincidence, and it is the argument for the harness.

**Two reports were consequences of O29 rather than of the thing reported.** The
damage numbers and the tier cap both moved because item level ran to 120 and
affix values roughly doubled. Diagnosing them as type or as loot problems would
have produced the wrong fix twice.

| Finding | Root cause | Response |
|---|---|---|
| "i never could do a 3rd jump" | **Not a bug in the grant — unreachable BY CONSTRUCTION.** Every link was verified and every one was correct: the permanent-class read, the `OnProgressionChanged` bind, the tick-poll backstop, `DevForceClass`'s broadcast, the clamp on a banked jump surviving a swap. The gate read `FBreakerProgressionState::CharacterLevel` against 20, and **nothing in the project writes that field** — declared with a default of 1, no XP loop, no assignment anywhere. The condition was false for every character that has ever existed | `SwiftThirdJumpUnlockLevel` 20 → **1**: a gate must key off something that moves, and until an XP loop exists nothing does. `RefreshJumpGrant` warns once if it is ever set above a level the game can produce; the budget is logged whenever it changes. **The test gap is the real fix**: the pre-existing `JumpGrant` test proved the RULE and passed the entire time the feature was dead, so `JumpGrantMatrix` now asserts the SHIPPED CONFIGURATION against a default-constructed progression state |
| "gravity needs to be tuned down just a little bit (needs to make the character slightly more floaty)" | **Rise versus descent, the fourth report on this arc.** The rise had already been walked back to 1.38, a hair over its original 1.35, while the fall still ran 1.80x on top of it | `FallGravityMultiplier` 1.80 → **1.55**; `GravityScale` deliberately untouched. **One value moved**, so the next report attributes cleanly (O26). Apex height unchanged by construction at 181 cm, so no ledge, gap or wall-ride approach changes reach; airtime 0.90 → 0.93 s, landing 939 → 871 cm/s. Next dial if still heavy: `LandingMinimumSpeedScale` → 1.0 |
| "a lot of the textures on the ground were tearing" | **The ground was coplanar with ITSELF**, three populations on the apron — and the visible majority was not coplanarity at all. The 200 tint patches were placed by rejection-free random sampling at one fixed height, so overlapping pairs shared a surface exactly; each patch was a 4 cm cube that **cast a shadow**, and at 150–200 m both that shadow and the lip's own shaded side face are sub-pixel and alias into a stippled dashed line tracing every patch outline; and the jump-gap trench floor was authored with its top at exactly the apron's top. The apron-against-template-floor hypothesis was checked against the runtime frame and **rejected** | Overlapping placements rejected against the rotated footprint (196 placed from 420 attempts, density unchanged); patches are shadowless **planes** with no lip at all; the trench floor sits on the new `GroundOverlayLift` (6 cm, `EditAnywhere`, 0 reproduces the bug for an A/B). Confirmed gone in the after-capture |
| "damage numbers font size is too high" (**second report**) | **Width, not size — and what changed was O29, not the type.** They had already been cut ~35% once (40/64/80 → 26/40/52). Item level to 120 and roughly doubled affix values turned three-digit hits into six-digit ones, and `FormatTicker`'s thin space makes a six-digit number eight glyphs, which at crit size covers the target | Sizes **held** — they are the only thing separating body from weak point from crit, and a third cut collapses the hierarchy. `BreakerUI::FormatDamage` abbreviates above 10 000 (12.4k / 148k / 1.24M), holding every damage number to at most five glyphs at any magnitude the power curve can produce. `RiorsEdge.UI.DamageNumberFormat` pins the width budget across twelve magnitudes |
| A Warden hit registering with no health movement | **Design gap, and the item worth the most this session.** Frontal armour ate the hit and nothing said so, which reads as a broken game rather than as a wrong angle. Needed nothing new in `Combat/`: `FBreakerDamageResult` already carries `RawDamage` and `MitigatedDamage` | Two reads, because the player needs one at the crosshair and one at the target. The hit marker's ticks pull outward and close into corner brackets — **geometry, not a third colour**, so it cannot be confused with the gold weak-point tick; the floating number recedes to muted with an `ABSORBED -47%` caption. Threshold 0.20. **Rejected after looking**: the first pass drew both reads in OrangeDeep, and on screen an absorbed crit then sat one value step from an ordinary crit — the two states most needing separation reading most alike |
| `WAVE 01` and `4 HOSTILE` printing on top of each other | **The fixed-offset defect**, third instance: a left-aligned string and a right-aligned string share a row and nothing in the code knows they share it. The plate was a fixed 260px with its divider at a fixed 55%, and a 28px title renders wider than the 143px gutter | Both strings measured, laid out left-to-right from the measurements, divider X from the title's width, plate sized from the content with 260 as a minimum only. The file was then **swept** for the same shape: the speed readout, the ammo/weapon-name pair, the status chips (which had **no bound at all** — three DoTs walked them off the vitals plate) and the loot popup. `FitSpecPixels` is the shared helper |
| No sense of where anything is in the field | Design gap | Minimap, top-right, 320×176 **landscape and field-aligned** rather than square and rotating, because the field is a 25 000 cm long axis against ~8 000 of width — a square window spends most of its area on empty flank and rotation destroys the alignment that answers the only question this field raises. 56 cm/px puts the encounter pocket on the map from the safe ring. Costs zero extra iteration: it fills its blip array inside the loop that already walked every enemy. **Authored here — no minimap exists on the owner's design canvas, so every decision in it is a proposal awaiting a ruling** |
| "not every single enemy needs to drop an item … I was getting way way too many Aberrants at this item level when it shouldn't even be fundamentally possible, and every single enemy dropped an item" | **Both halves were one structural gap, confirmed in code.** `GrantLoot` called `RollRarity` unconditionally on every death, so kill count WAS item count; and `RollRarity` was a **flat** table, so a 2.5% Aberrant weight applied to a trash mob at area level 1 exactly as it did to a boss at 50. Nothing in the loot path read level or rank at all, so "shouldn't even be fundamentally possible" was literally true | Three steps where there was one: **drop chance by rank**, then a **rarity gate**, then the weighted roll. Gate rule in one sentence, so the owner can overrule it: a rarity rolls only when the drop's item level is at or above its unlock AND the monster's rank is at or above its minimum, with gated-out weight redistributing across what remains. 692 items/hour → 134; ~17 Aberrants/hour at ANY level → zero below area level 25 and 0.90/hour above. `LootPerHour` simulates 200 hours through the real `RollDrop` and asserts it matches the published projection, so the documented rate cannot drift from the shipped one |
| "the item level tier capping at 8 might make for awkward feeling progression, let's bring that to 6" | A single slope of one tier per ten levels put the **character cap** at T8 — a third of a back-loaded ladder — so a player who finished the levelling game had met only the shallow lower half of a curve authored for the endgame | Two slopes: ~8.3 item levels per tier to the cap, ~14 after it. The levelling game now crosses half the ladder and finishes standing on the shoulder of the curve; the endgame is slower per tier **because each tier is worth more**, not as a tax. `GetDropItemLevel`'s clamp to 50 was also removed in the same pass — the last link in the 74x endgame gap |
| "numbers clip" on the skill board (**second report**) | **Not the marker.** The 48px box holds 40px of content around 20px of text. `SButton` centres its child at the child's DESIRED width, an `STextBlock`'s desired width is its MEASURED width, and Slate clips the drawn run to that same box — measuring and rasterising round independently. Proof is in the baseline capture: two identical `0/2` markers disagreed, the Tier 1 one sitting at a different fractional X because of its 2px purchasable ring. **The previous pass read this as "the box is too small" and moved 30px → 36px, which only reshuffled the rounding** | Markers sized from a measurement plus ring and button padding, with the spec's 48/44/64/60 as a **floor**, so `10/10` grows instead of clipping. The tier gutter measured the same way against the longest `OPENS AT n` it will actually print. The defect class is recorded in `UI-Style-Guide-Fieldplate.md` beside the `SHorizontalBox` one |
| "scrolling is off by a little bit" | **Two nested scroll boxes fighting for one wheel gesture** | The board is a **viewport** now: laid out once at full authored size and moved by a **render transform** set imperatively from the input handlers, so nothing rebuilds per frame and nothing measures its own arrangement. Wheel zooms about the cursor (0.5x–2.0x), drag pans, RESET VIEW returns to the opening zoom, and both survive a purchase rebuild. Boards open at **1:1 deliberately**: fit-to-width was implemented, photographed and reverted, because COMPARE ALL is ~2600px in a ~1300px column and fitting means 0.5x — 5px type against FIELDPLATE's 11px floor |

**Rulings taken this session:** O29 (endgame power is gear depth, item level to
120, tiers T12..T-1 back-loaded), O30 (the Core tree opens to redesign around
build axes), O31 (content shape — Destiny × PoE, every build participates), and
O32 (legendary drop rate holds, the pool grows; legendary and Anomalous are
different axes).

**Consequences reported rather than silently absorbed**, all still open:

- The Mana inversion weakens **Overcast's deterrent**: doubled generation now
  applies to a much larger regen, so a full debt repays in under two seconds and
  the cost is almost entirely the damage window.
- Several **Void Whisperer and Spellblade nodes** buy a share of what is now the
  smaller half of Mana income; **VW3 Patience** became one of the strongest
  nodes in the class. Flagged with dials, not retuned.
- The **wave budget curve and the density caps contradict** each other from
  about wave 8. The caps win and the solver reports the shortfall rather than
  choosing silently.
- The **power band now measures ~15x against an authored 8–10x**. Two tests fail
  on `main` deliberately because of it. See CONTEXT.md.

**Not playtested.** A capture shows composition; it cannot say whether the arc
feels right, whether the minimap window is the right scale, or whether 134
items/hour is a satisfying rain. The gravity change in particular is unverified
as a feeling.

## 2026-08-13 — Session 5 (post-FIELDPLATE, post-feel pass)

**Report:** 1.5 min, 414 shots, 40.3% acc, 36.5% weak-point rate,
5213 damage, 13 reloads. **Melee trash 34 kills avg TTK 1.81s**,
ranged trash 3 kills avg **1.53s**, elite 1 kill avg **3.01s**.

**Reading — the strongest TTK sample yet, and it splits.** Elite is ON
TARGET (3.01s vs ~3s). Melee trash is ~1.8x slow (1.81s vs <1s). Session 4's
2.61s came from 29 kills; this is 34, engagement-gapped, with the ranged
archetype separated out. The chassis correction is now a single ratio rather
than a guess: trash health ~220 -> ~120, or an equivalent damage raise.
STILL AWAITING THE OWNER RULING (O2 freezes the authoring).

Two riders on the NEXT measurement, both introduced this session:
- The weak-point forgiveness halo raises damage per hit by 8-14% at a 50-60%
  weak-point rate. Set `WeakPointToleranceCm = 0` for a clean re-anchor run.
- Falloff softened, but the rifle's effective DPS is UNCHANGED across the
  whole 9-19 m band, so a rifle sample inside 20 m is uncontaminated by it.

**Owner findings, verbatim, and what each turned out to be:**

| Finding | Root cause | Response |
|---|---|---|
| "weakpoints dont feel forgiving" | **Bug.** Body hitbox tops at Z 62, weak-point sphere starts at Z 58; in the overlap the box's front face is far in front of the sphere, so the bottom of the head was unhittable from the front | World-space forgiveness halo, `WeakPointToleranceCm` 0 -> 14 (effective radius 20 -> 34 cm) |
| "wall riding doesnt work but jumping does and its awkward" | **Bug.** `WallRideMinimumSpeed` was 700 = `WalkSpeed`, and the gate is read AFTER wall contact where velocity is the along-wall component. Walking could never enter at any angle; sprinting failed past ~50 deg. `TryWallJump` returns false unless already riding, so the "awkward jump" was the plain second jump (O25) | Gate 700 -> 450, entry rule extracted to a pure tested function, wall jump given its own exit floor |
| "numbers clip ... clunky and awkward" | **Bug, two independent causes.** Plate hard-sized 1760x1000 inside a maximised viewport's ~920px client height; and a fixed 168x86 node label box holding three auto-wrapping blocks that ran through the tier below | Geometry derives from the measured viewport once per rebuild; board scrolls both axes; compact effect line; `Damage` added to `StatTargetLabel` (damage nodes printed "+4% STAT") |
| "cant really see the numerical significance of your points" | Design gap | Node cards lead with `DAMAGE 1.06x -> 1.10x`, projected through a COPY of the live aggregator so it cannot drift; pinned BUILD TOTALS rail splitting `TREE +x% · GEAR +y%` |
| "there should be a button to select your subclass" | Data model gap | Branch strip built as BROWSING only; commitment needs a data-model change and an O15 balance ruling — recorded, not invented |
| "projectiles are ugly and weird" | Approach, not parameters | Tracer moved to a pooled WORLD actor (additive, still depth-tests). Shotgun stops drawing one streak for eight pellets |
| "damage numbers font size is too high" | Spec authored for a desk-distance mock | Body 40->26, weak 64->40, crit 80->52; hierarchy preserved |
| "gravity is too high" (after 1.60 -> 1.45) | Rise vs descent confusion | The heaviness is the RISE, paid every jump; the floatiness was the DESCENT. Rise -> 1.38, heavy fall multiplier kept |
| "hip firing feels worse than ads" | ADS had all upside, no cost | ADS now pays aim-in time and a movement spread penalty; hip accuracy deliberately NOT buffed, which would delete the decision |
| "dmg fall off is too high" | Tuned for a small arena | Softened per archetype, ordering pinned by test rather than values |
| "cant really feel [dash]" | No feedback at all | `OnDashStarted` -> FOV punch scaled by speed + direction-signed camera roll |
| "only ability that felt good to press was the ultimate and thats when ttks were correct" | — | Read as evidence that trash health, not the abilities, is the cause of "unimpactful". Feeds the TTK ruling |
| "walk speed feels weird but i think its a map scope issue" | Agreed | Stock template geometry; needs the authored gym map (editor work) |
| "cadence / reload: unsure cant tell without a proper model" | Asset gap | No weapon meshes or audio exist |

**Rulings taken this session:** O25 (two jumps base kit for everyone, Swift
innately unlocks a third later — supersedes air-jump-as-tree-verb) and O26
(movement drops in priority).

**Caught at integration, not by any agent:** two files each declared a bare
`ShapeCube` in an anonymous namespace. Fine per translation unit, but a unity
build concatenates files into one TU. Each agent's adaptive non-unity build
excludes exactly the file being edited, so all four compiled clean alone and
only collided when the whole module built together.

## 2026-08-13 — Session 4 (first ENGAGED-TTK report)

**Report:** 1.6 min, 352 shots, 56.5% acc, **71.9% weak-point rate** (Lead
mark-consumption working), 29 kills avg TTK **2.61s** vs <1s target,
2 elite kills avg **6.18s** vs ~3s target (n=2, low confidence).

**Reading:** first legitimate divergence measurement — trash and elites
both ~2-2.5x slower than the O18 targets. Chassis correction owed:
trash health ~220 -> ~90-100 (or equivalent damage raise), elite scaled
accordingly. Awaiting owner tuning ruling (O2).

## 2026-08-13 — Session 3 (first TTK report)

**Report:** 1.8 min, 279 shots, 59.1% accuracy, 51.5% weak-point rate,
16 kills avg TTK 5.37s, 4 elite kills avg TTK 2.05s, 3632 damage dealt.

**Reading vs O18 targets:** the 5.37s trash figure is an INSTRUMENT
artifact, not a chassis miss — the sampler measured wall-clock from first
damage to death, so tag-and-return play inflates it. Continuous-fire math
(220 HP vs 24 dmg @ 600rpm) gives ~0.9s body / ~0.5s weak point, at or
under the <1s target. Elites (2x HP) measured 2.05s BECAUSE they get
focused — under the ~3s target. Damage dealt was ~70% of HP killed:
chain detonations did the rest (density mechanic functioning).

**Action:** TTK sampler switched to engagement-gapped time (gaps between
damage events capped at 1.5s). Next report measures fighting time.
**Owner findings same session:** skill-tree screen caused hard hitching
(per-frame Alt lambdas — removed); slice points didn't seed on existing
saves (seeding relaxed + dev grant button); abilities lacked in-game
descriptions and visual feedback (HUD ability names, first-use callouts,
activation flashes, window bars, Overdrive vignette, Skim burst, Lead
mark diamond); Weapon Damage % affix added for TTK testing range.

Owner playtest findings and the actions taken. Newest first. This is the
gym's paper trail — wave-mode reports and re-anchoring decisions cite it.

## 2026-08-13 — Session 2 (post-QoL wave)

**Owner findings:** presentation "awkward and sloppy" — the safe-zone read
as a giant teal floor, loot beams were 40m flagpoles, world labels spammed
the screen at any distance, and the field still felt small despite the
apron because the fight started close-in.

**Actions (same night):**
- Safe zone: full-radius teal disc replaced with a small center pad plus a
  12-post teal boundary ring — teal back to objects, ground back to ground.
- Loot beams: 8.0 → 2.2 height, thinner, dimmer; pickup cube slightly
  larger; HUD chips capped at 15m (was 30m).
- Diagnostics world labels (dummy/enemy): 25m range cap, smaller, faded.
- Combat pushed out: encounter line +26m, arena/wave center at +42m past
  the safe ring (was +24m), arena marker ring widened to 14m radius, wave
  packs spawn on an 11m ring.

**Still open from this session:**
- The template level's stock geometry (grey grid walls, orange blocks)
  crowds the space and clashes with the O24 palette. Options: author a
  proper gym map in-editor, or a runtime declutter pass that hides
  non-floor template meshes. Owner call — map authoring is editor work.
- Loot pickups on rooftops/high ledges from scatter can be hard to reach.
- First-person weapon blockout still reads as grey boxes (known, awaiting
  presentation pass).

## 2026-08-12 — Session 1 (first wave-mode run)

**Owner findings:** report output buggy (unspecified); ran out of ammo
after 3 waves with no recovery path; no visible bullet feedback; enemies
walked in flat straight lines; map too small; backpack hard to read; no
good gear available to measure real TTKs; HUD information scattered —
wants a Destiny-2-style compact cluster.

**Actions:** the full QoL wave (commit 5581012) — HUD overhaul with
tracers/damage numbers/health bars, ammo economy (kill drops, wave-clear
refill, camp supply crate), three-gear enemy approach, 2.5x field with
pockets/sniper lane/wall-ride walls, ground loot with rarity beams and
F-pickup, inventory tabs with dev gear grants.

**Unreproduced:** the "buggy report" — F2 output needs a paste next
session to diagnose.

## 2026-08-14 — Movement + ground pass (third jump, gravity, tearing)

**Owner findings:** "i never could do a 3rd jump"; "gravity needs to be tuned
down just a little bit (needs to make the character slightly more floaty)";
"a lot of the textures on the ground were tearing".

**Third jump — the real cause was not a bug in the grant.** Every link in the
chain was verified and every one was correct: the class read, the
`OnProgressionChanged` bind, the tick-poll backstop, `DevForceClass`'s
broadcast, and the clamp that stops a jump banked against three surviving a
swap to two. The gate read `FBreakerProgressionState::CharacterLevel` against
a threshold of 20, and **nothing in the project writes that field** — it is
declared with a default of 1, there is no XP loop, and a repository-wide search
for an assignment returns the declaration alone. The feature was unreachable by
construction, not late. `SwiftThirdJumpUnlockLevel` now defaults to **1**
(a gate must key off something that moves; until an XP loop exists nothing
does, so it defaults to reachable), `RefreshJumpGrant` warns once if it is ever
set above a level the game can produce, the budget is logged whenever it
changes, and `RiorsEdge.Movement.JumpGrantMatrix` asserts the shipped
configuration against a default-constructed progression state — the state the
game actually runs in. The pre-existing `JumpGrant` test proved the rule and
passed for the whole time the feature was dead.

**Gravity — one value, on the descent.** `FallGravityMultiplier` 1.80 → 1.55;
`GravityScale` deliberately untouched at 1.38. Four reports now, and they are
about different halves of the arc: the rise has already been walked back to a
hair above its original 1.35, while the fall still ran 1.80x on top of it.
Apex height is unchanged **by construction** at 181 cm, so no ledge, gap or
wall-ride approach changes reach; airtime 0.90 → 0.93 s and landing speed
939 → 871 cm/s, still under `LandingHeavyFallSpeed`. Next dial if it is still
heavy: `LandingMinimumSpeedScale` → 1.0.

**Ground tearing — confirmed by capture, before and after.** Three coplanar
populations, all on the apron: the 200 tint patches overlapped **each other**
at one fixed height (dozens of pairs); each patch was a 4 cm cube that cast a
shadow, and at 150–200 m both the shadow and the lip's own shaded side face are
sub-pixel and alias into a stippled dashed line tracing every patch outline —
that was the visible majority of it; and the jump-gap trench floor was authored
with its top at exactly the apron's top. Now: overlapping placements are
rejected against the rotated footprint (196 placed from 420 attempts, so the
density holds), patches are shadowless planes with no lip, and the trench floor
sits on the new `GroundOverlayLift`. A grazing-angle capture vantage was added,
because this defect is invisible from every vantage the harness already had.
The dashed seams are gone in the after-capture.

**Not playtested.** A screenshot shows composition; it cannot say whether the
arc feels right. The gravity change in particular is unverified as a feeling.
