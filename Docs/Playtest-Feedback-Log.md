# Playtest Feedback Log

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
