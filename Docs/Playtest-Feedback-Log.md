# Playtest Feedback Log

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
