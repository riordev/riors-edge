# GLASS
branch: claude/competent-goodall-e3a1b5 → main   base: 227fa98   suite: 512 / 3 / 0
current: GLASS-5 — the muzzle fallback is sized on screen, not in the world: BreakerFX::MuzzleFallbackRadiusCeilingCm bounds it by reticle clearance at the shipped muzzle view offsets (3.5 cm under a 3.8 cm aimed ceiling, was 14 cm), pinned by RiorsEdge.UI.EffectMoment.MuzzleClearsReticle
next: GLASS-2 — per-archetype weapon fire cue (weapon_fire_<archetype>.wav → weapon_fire.wav → synth), ruled yes; then GLASS-3 the BreakerMenu.cpp split
blocked-on: nothing — three questions open in Docs/reports/GLASS.md (consumer-less tags, death colour, who authors the four NS assets)
crossings this cycle: none — the new test reads UBreakerWeaponComponent's public MuzzleViewOffset / AimedMuzzleViewOffset defaults; Weapons/ is not edited
