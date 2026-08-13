# Playtest Feedback Log

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
