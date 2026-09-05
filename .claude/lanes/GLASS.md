# GLASS
branch: claude/glass-h1-lane-0bd6c1 → main   base: 0bf3b1e   suite: 507 / 3 / 0
current: GLASS-1 — the four moments (muzzle, impact, cast, death) have a Niagara slot each behind ABreakerEffectRenderer::PlayMoment, lazily resolved from /Game/Breaker/FX/NS_<Moment>, pooled fallback until the owner authors a system; HUD fires muzzle, impact and death; cast is KIT's to call at its sites
next: GLASS-2 — per-archetype weapon fire cue (weapon_fire_<archetype>.wav → weapon_fire.wav → synth), ruled yes; then GLASS-3 the BreakerMenu.cpp split
blocked-on: nothing — three questions open in Docs/reports/GLASS.md (consumer-less tags, death colour, who authors the four NS assets)
crossings this cycle: UI/BreakerEffectRenderer.h public surface grew PlayMoment → KIT/FIELD/GROUND (published path, additive, nothing of theirs changes)
