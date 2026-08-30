# AUDIO lane — open questions and owner asks

## Kill-confirm choice (gated on Q13: a kill never sounds like a graze)

The kill verb currently falls through to PlayHitConfirm, so a kill and a
graze share one sound. Three candidates are staged under
`Content/Breaker/Audio/candidates/`, consumed by nothing, all PCM16
44.1 kHz from the converted Kenney set. Measured, not heard:

- **A** `kill_confirm_A.wav` (impactBell_heavy_002) — soft attack, 0.70 s
  tonal bell ring, centroid drifts up as the strike decays into ring.
  A toll: the most "final" of the three, and the furthest in genre from
  the 90 ms pistol transient that is hit_confirm.
- **B** `kill_confirm_B.wav` (lowFrequency_explosion_001) — slow attack,
  1.0 s sub-heavy boom (centroid 134 -> 352 Hz, all rumble). Weight
  without brightness; separates from every bright verb in the mix.
- **C** `kill_confirm_C.wav` (impactMetal_heavy_001) — hard attack,
  0.36 s falling metallic clang (1644 -> 574 Hz). Same percussive family
  as a hit but heavier and lower: the smallest step from today's sound,
  the cheapest to read at high kill rates.

Audition, per candidate, from the repo root (the swap is local — do not
commit it; the second command restores):

    copy /Y Content\Breaker\Audio\candidates\kill_confirm_A.wav Content\Breaker\Audio\kill_confirm.wav
    git checkout -- Content/Breaker/Audio/kill_confirm.wav

Then fire at anything in the gym (`-BreakerAutoPlay=Gym`); every kill
plays the swapped file. Note PlayKill itself is still uncalled by the
"no death sound for now" ruling — the kill CONFIRM path through
PlayHitConfirm is what sounds today, so the A/B rides the normal kill
flow with the file swapped.

Not reachable from this seat: `C:\AssetLibrary\Sonniss` holds one
unextracted 2.8 GB zip on the owner's box — a richer candidate pool if
the Kenney three all read as toys.

## UI verb mapping (held on Q21 — the shell; wire nothing until ruled)

Proposed one-line mappings from the converted set, one veto per line.
Durations and spectral direction measured; nobody has heard these
together.

| Moment | File (under kenney/) | Why |
|---|---|---|
| Confirm | interface-sounds/confirmation_001.wav | 0.29 s warm rise (518 -> 1091 Hz), affirmative without shrillness |
| Back | interface-sounds/back_002.wav | 0.09 s low blip — the pack's own back gesture, quick and unceremonious |
| Tab-switch | interface-sounds/toggle_001.wav | 0.14 s flat neutral flick; direction-free suits lateral movement |
| Refusal | interface-sounds/error_005.wav | 0.50 s falling buzz (1678 -> 412 Hz) — the only error that sags |
| Purchase | interface-sounds/confirmation_002.wav | 0.54 s two-stage bright rise — richer than plain confirm, reads as a transaction |
| Menu open | interface-sounds/maximize_001.wav | 0.26 s rising — mirrored pair with close |
| Menu close | interface-sounds/minimize_001.wav | 0.26 s falling — the mirror |
| Hover tick | ui-audio/rollover1.wav | 0.22 s flat soft — quiet enough to spam |

The combat verbs stay what they are: weapon_fire / hit_confirm /
kill_confirm / take_hit (OpenGameArt cuts), ability_cast (Kenney
forceField riser). Any of these UI rows would arrive as new director
verbs — the no-generic-PlaySound rule stands, so each is a ruling, not
a call site.
