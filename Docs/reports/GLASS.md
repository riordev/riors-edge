# GLASS

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Does the fifth verb need a companion for DURATION? (ORDERS ruling 2)

`PlayAbilityCast` is a one-shot: it fires on `OnAbilityActivated` and the clip
runs to its own length. That is right for a cast and says nothing about
abilities that have a LENGTH — Siphon channels a beam, and every
`Window.Swift.*` state is an open duration the HUD already draws a bar for.
Today those are audible at their start and silent for the rest of themselves.

- The no-generic-`PlaySound` rule means this cannot be solved by a caller
  passing a looping wave. A sustain would be a sixth verb — start/stop, or a
  loop with an explicit end — and by the director's own rule that needs a
  ruling before it is built.
- **Nothing is blocked.** Abilities make noise now; this is about whether a
  channel should *sound* like it is still running.
- **Question:** is a duration verb wanted, or does the cast cue plus the
  existing HUD window bar carry a channel adequately? If wanted, it is mine and
  I will report its shape before authoring.

## Enemy ability audio would not fit this director, and should not be bolted on

All five verbs are the PLAYER's events: `bAllowSpatialization = false` and
`bIsUISound = true`, deliberately, because fire / hit / kill / take-hit / cast
all happen TO the listener and spatializing them at the pawn's feet only buys a
doppler artifact when they sprint.

- An enemy telegraph is the opposite: its whole value is *where* it came from,
  so it needs a spatialized, world-positioned voice. That is a different
  mechanism, not another method on this actor, and the pooled-voice-per-verb
  shape does not extend to "one per enemy in the world".
- Raising it now because the fifth verb makes this director look like the place
  ability sound lives, and the next lane that wants an enemy cue will reach for
  it.
- **Question:** when enemy audio is wanted, should it be a second cosmetic actor
  (spatialized, pooled by voice count like the effect renderer's lights), or is
  there a reason to keep one director? I would build the second actor, and I am
  not building either until it is asked for.
