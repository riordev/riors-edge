# The four moment systems (O190, ASSETS-5)

A Niagara system is content, authored in the editor by a person. The code
ships the slot, the fallback and this recipe; the systems land here without a
line of plumbing changing. The renderer (`UI/BreakerEffectRenderer.cpp`)
resolves each asset lazily by path, caches the answer including "none
authored", and draws the pooled primitive fallback until the file exists.

## The four assets

| Moment | Asset path | Colour (O179) | Fallback while unauthored |
|---|---|---|---|
| Muzzle | `/Game/Breaker/FX/NS_Muzzle` | Orange | tongue 34 cm x 3.2 cm down the barrel + flare cone (base 1.6 cm at the muzzle, 22 cm to the apex) + blink light, 0.06 s; all scaled by the gun's kick |
| Impact | `/Game/Breaker/FX/NS_Impact` | Orange; Gold on a weak point | 5 shards (9 x 1.6 cm) thrown out of the surface in a 55-degree cone at 420 cm/s, falling under gravity for 0.35 s; the tracer renderer's spark stays |
| Cast | `/Game/Breaker/FX/NS_Cast` | Cyan by default; the casting verb passes its own | glow 40 cm + blink light |
| Death | `/Game/Breaker/FX/NS_Death` | Harm red; Gold on a weak-point kill | glow 55 cm + blink light |

Directory and prefix are fixed by `BreakerFX::MomentAssetName` in
`UI/BreakerEffectMomentMath.h`: name the system after its moment and nothing
in code has to learn it. `NS_JumpPad` is not a stand-in for any of the four
and is never resolved for a moment.

## The one parameter: `Color`

Every system exposes a **user parameter named `Color`, type LinearColor**.
The renderer writes it on every activation
(`SetVariableLinearColor(TEXT("Color"), ...)`, `UI/BreakerEffectRenderer.cpp`)
with the O179 verb colour for that moment. Bind every emitter's colour —
sprite, ribbon, light, mesh — to that user parameter. Any colour authored
directly into an emitter is ignored by the contract: one `NS_Impact` serves a
body shot in Orange and a weak point in Gold through this parameter alone,
and a system without it plays its authored colour, which breaks the verb
colour law for every moment it draws.

## The activation contract

- Components are **pooled**: a fixed ring of Niagara components is reused
  oldest-first. The system's asset is set on the component if it differs
  from the last one played, then the component is placed and activated.
- **`Activate(bReset = true)`** on every play. Author every emitter as a
  one-shot that completes on its own; loop nothing. A retrigger on the same
  slot restarts from zero.
- The component is placed with **`SetWorldLocationAndRotation`** at the
  moment's location, with its **forward (X) axis along the supplied
  direction**: the shot's travel for Muzzle, the surface normal for Impact,
  the cast's facing for Cast, up for Death. A zero direction becomes world
  up. Author directional emitters along local +X.
- Scheduled moments (a hit that lands after a tracer's flight) wait in a
  pending ring and activate on the tick they fall due; the system itself is
  never told to start late.

## Size and duration targets

The fallbacks in `BreakerFX::MomentFallback` (`UI/BreakerEffectMomentMath.h`)
are the placeholder sizes the authored systems replace. Every figure is
O2 PLACEHOLDER until the owner has felt it.

**NS_Muzzle** — replaces a tongue (a 34 cm stroke, 3.2 cm thick, down the
barrel), a flare (a cone with its 1.6 cm-radius base at the muzzle tapering
to an apex 22 cm forward) and a blink light of radius 360 cm at intensity
2600, all at intensity 4.0 for 0.06 s fading over the last 0.04 s. The
tongue's length and the flare's radius and length are multiplied by
`BreakerFX::MuzzleFlashScale(kick)` = 0.75 + 0.08 x the archetype's
viewmodel kick units, so a pistol flashes at 0.91 and a sniper at 1.55. The
muzzle sits at a fixed camera-space offset, so its size is a screen size:
**the flash must clear the reticle**. The rule is
`MuzzleReticleClearanceRadians` = 1.5 degrees off the view axis, angle
against angle so it holds at any field of view including the aim narrow;
`MuzzleFallbackRadiusCeilingCm` gives the largest radius that clears at a
given muzzle offset, and the flare's scaled base radius stays under it at the
largest shipped kick. Nothing in the system reaches the centre of the screen.

**NS_Impact** — replaces five shards: 9 cm x 1.6 cm cubes leaving the
surface inside a 55-degree cone about the normal at 420 cm/s, falling under
980 cm/s^2 for 0.35 s, brightness dropping straight from 3.4 to zero over
that life (`BreakerFX::ShardPose`, `ShardAlpha`). The tracer renderer's spark
still marks the point underneath. One-shot sparks, a short burst leaving the
surface along +X, no lingering glow: the damage number carries the read.

**NS_Cast** — replaces a 40 cm glow at intensity 3.2, 0.2 s total, fading
out over the last 0.12 s, with a blink light of radius 380 cm at
intensity 2200 on the same timing. O179: cast MOMENTS get world flashes;
windows stay HUD bars, so this is one flash, not a sustained aura.

**NS_Death** — replaces a 55 cm glow at intensity 3.6, 0.45 s total, fading
in over 0.04 s and out over the last 0.3 s, with a blink light of radius
520 cm at intensity 2600 on the same timing. The longest of the four: a body
leaving reads after the damage number has started to rise.

## Checking a system in

Drop the asset at its path and play: the next moment of that kind resolves
the system on first use and the primitive fallback stops drawing for it.
Verify the colour follows the verb (an Impact on a weak point turns Gold with
no change to the asset) and, for the Muzzle, that the reticle stays clear on
a kicked frame. `/photograph` with `-BreakerCaptureHUD` reads the frames.
