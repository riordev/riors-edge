# FIELD

The lane's open questions for the design seat, in one place. Answered questions
are deleted in the commit that lands the answer; git holds them. Findings and
status live in the session reports, never here.

## A number of mine the seat could not reproduce, and it was wrong

One-T could not reproduce *"fifty bodies at 150 cm need a third of a Fernhall
yard"* and asked which comparison I meant. Recomputed:

```
  hex packing at 150 cm      (sqrt3/2)*1.5^2 = 1.949 m^2 per body
  fifty bodies                                 97.4 m^2
  as a circle                                  11.1 m across   (I said 15 m)
  Fernhall combat band                         75 x 40 m = 3000 m^2
  by AREA                                      3.2%
  by WIDTH                                     11.1 / 40 = 28%
```

I meant the linear one — how much of a yard's width a pack spans. **But I stated
a diameter-against-width ratio as though it were occupancy, and my diameter was
wrong as well: 11.1 m, not 15.** The seat's 3% is the correct area figure.

**So the conclusion I drew from it does not stand.** At the densities Fernhall
actually fields, spacing is a movement constant, not a level-design constraint.
It becomes one only for line-abreast formations, or for waves large enough to
need a second rank — and a converging crowd is neither. I told GROUND to size
yards against spacing on the strength of the bad number, and I have corrected
that to them directly.

## The arrival ring's shape (One-S, reported before building)

**The band controller can express melee's case, so it is reused rather than
re-derived.** `BreakerRangedBehavior::ClassifyBand(Distance, Min, Max,
Hysteresis, Previous)` is a distance-band classifier with hysteresis; nothing in
it is ranged-specific. Melee is the same machine with a **thin band at contact**
instead of a wide one at standoff:

```
  ranged    Min 600   Max 2600   wide band, hold and strafe and fire
  melee     Min 200   Max 260    thin band at AttackRange, hold and attack
```

`GetBandRadialSign(Hold)` is 0, which is exactly "stop closing" — the strafe is a
tangential component the caller adds, so melee simply does not add one. The
controller needs no change. If that turns out to be false when it is wired, that
is a finding about the controller and I will name it rather than work around it.

### Stateless stop, not claimed slots — and the arithmetic decides it

One-S asks which, and why, before either is written. **Stateless.**

- **The oversubscription that slots solve does not occur at shipped density.**
  The ring at `AttackRange` is 2*pi*260 = 1634 cm of arc; at 150 cm spacing it
  seats 10.9 bodies. The rift interior spawns ten. Slots would be machinery for a
  queue that never forms.
- **A slot is a handle, and handles need releasing on four lifetimes** — death,
  pooled park, Wakeful down, and target change. This lane has already paid for
  two unreleased handles this session: the pool's gold that came back only
  because callers happened to re-run a chassis pass, and a one-slot fade that
  hard-cut every bar but the last. A fifth lifetime is not free.
- **Stateless composes with separation; stateful competes with it.** Two systems
  with authority over where a body stands is the tension One-S ruled against in
  the first place.
- **And slots would answer a question the seat reserved.** Past ~18 concurrent
  melee, whether rank-two bodies hold, circle or push through is explicitly the
  seat's call. A slot table decides it in code, silently, by whatever it does
  when it runs out.

**The condition that flips this:** sustained concurrency above the ring's
capacity, where bodies visibly queue rather than spread. That is a measurement
the crowd probe can take, and it is the day the second-rank question has to be
answered rather than the day slots get built.

### The predictions, both falsifiable

1. **The band moves the quadratic coefficient**, because bodies that stop at
   260 cm stop generating the dense-cluster contacts the N^2 term measures. If it
   does not move, the term is not what the sweep says it is.
2. The fit without its quadratic puts N=100 at 9.79 ms. If separation lands near
   31 ms with spacing improved, the separation report was wrong.

I will report the measured stack before and after the ring, and the refitted
coefficient, rather than only the frame time.

## The fracture mask — does it get a project-owned material?

The readability pack's second near-death carrier (emissive crack mask `#FF4040`,
coverage 0.00 / 0.04 / 0.15 / 0.40 / 0.75, pulsing 2 Hz below 15%) has nothing to
write to. Measured on the body's material,
`/Engine/BasicShapes/BasicShapeMaterial`: exactly two parameters, vector `Color`
and scalar `Roughness`. No emissive, no texture parameter, no static switch — and
it is an ENGINE asset, so it cannot be edited. There are still **zero**
`SetScalarParameterValue` sites anywhere in non-test `Source/RiorsEdge`.

**Question:** authorise a project material carrying a mask input and an emissive
path, or rule that colour carries near-death alone. The second is a real answer,
but it leaves colour as the single point of failure A3 was written to remove — a
colour-blind screen, fog, or a red wall each take the whole read.

Lower priority than it was: One-T ruled the crowd work lands before the meshes,
and near-death readability is worth more once bodies are distinguishable at all.
