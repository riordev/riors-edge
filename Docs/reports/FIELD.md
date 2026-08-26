# FIELD

The lane's open questions for the design seat, in one place. Answered questions
are deleted; git holds them. Findings and status live in the session reports,
never here.

## The fracture mask — does it get a project-owned material?

The pack's second near-death carrier (emissive crack mask `#FF4040`, coverage
0.00 / 0.04 / 0.15 / 0.40 / 0.75, pulsing 2 Hz below 15%) has nothing to write
to. Measured on the body's material, `/Engine/BasicShapes/BasicShapeMaterial`:
exactly two parameters, vector `Color` and scalar `Roughness`. No emissive, no
texture parameter, no static switch — and it is an ENGINE asset, so it cannot be
edited. There are still **zero** `SetScalarParameterValue` sites anywhere in
non-test `Source/RiorsEdge`.

The mask needs a project material carrying a mask input and an emissive path.
That is editor asset work and needs the owner's authorisation.

**Question:** authorise it, or rule that colour carries near-death alone. The
second is a real answer — but it is the single point of failure A3 was written to
remove, and it means a colour-blind screen, fog, or a red wall each take the
whole read.

## Does the F3 diagnostics overlay ship visible on purpose?

Narrowed, not answered. This question had two halves and GLASS closed one in
`2e294d4`: the ungated second label pass now carries occlusion suppression and —
better than I asked for — **counts and prints what it suppressed**, so the
overlay cannot quietly show six of twenty and read as though there were six.

**The other half is untouched: `bDiagnosticsVisible` still initialises to
`true`** (`Playtest/BreakerPlaytestComponent.h:136`). ORDERS Part One-F said the
root question is the default rather than the labels, and it is still the default.
Every playtest the owner has run, including both he has reported on, had a debug
overlay up.

Neither file is FIELD's — the directory is GROUND's, the file is GLASS's — so
this is reported, not fixed.

**Question:** is a debug overlay that ships visible intended? If it is, it is not
a debug overlay and the name is wrong. If it is not, it is one initialiser.

## What photographs the enemy bar?

Nothing does, and A1/A7/A8/A9 shipped unphotographed because of it. The bar culls
at 50 m measured from the PLAYER, and every route in is blocked:

- autoplay spawns the player facing a berm with no enemy inside that radius;
- the crowd probe's grid stands at 60 m, past the cull;
- `-BreakerCaptureTour` moves the CAMERA while the cull still measures from the
  player, so a vantage standing among enemies culls every bar anyway.

The same gap hides O129's ramp, which needs a DAMAGED body and has no headless
way to make one — no `Breaker.` command sets health or deals damage.

**BUILT.** `Breaker.Field.BarProbe` (`Combat/BreakerBarProbe.cpp`) freezes a
four-rank by five-health matrix at 12 m and 35 m ahead of the pawn, so the bar,
the bands, the hatch, the marks and O129's ramp all photograph in one frame. It
suppresses the F3 overlay, because the overlay is not the shipping read.

What is still not FIELD's to decide is the camera-versus-player mismatch. The
bar cull measures from the PAWN while the capture photographs the CAMERA, and
they are not the same heading — the probe's first two runs placed forty bodies
behind the player's shoulder and reported success both times. The probe now
takes its origin from the pawn and its direction from the camera, which is
correct FOR THE PROBE and leaves the underlying split untouched:
`-BreakerCaptureTour` still moves the camera while the cull still measures from
the player, so a tour vantage standing among enemies culls every bar.

**Question:** is that a tour defect or a deliberate limit? It spans the tour
(GROUND) and the bar (FIELD), which is why it is not simply fixed here.

## Whether enemies deal elemental damage at all

Carried in `DECISIONS.md`'s Open list as a bare line since before this lane
existed. It is FIELD's to build, so the measurement belongs here.

Measured across non-test `Source/RiorsEdge`, counting every mention of the
enumerator including UPROPERTY defaults: **35 `Physical`, 4 `TrueDamage`, 3
`Elemental`** — and all three Elemental sites are player-side
(`Abilities/BreakerAbility_Resonance`, `Abilities/BreakerAbility_Siphon`,
`Items/BreakerItemTypes.h`). In `Combat/` it is 22 Physical, 4 TrueDamage and
**zero** Elemental, so no enemy path can produce an element today.

Two things wait on the answer: O5's per-element resistance has nothing incoming
to resist, and the shield-break asymmetry stays parked because `bBypassShield`
has no reason to fire.

**Question:** do enemies get elements. It changes the damage pipeline's shape
rather than a table, which is why it has sat unanswered rather than being cheap.

## The enemy mesh swap — what FIELD needs before pulling anything

ORDERS Part One-K item 2 rules the mesh mine, the owner has approved CC0 enemy
meshes "for the time being", and the gate is: report what swapping a family's
mesh costs before pulling. Two things I need alongside that measurement, both
cheap to answer and both expensive to get wrong.

**1. Where does a vendored mesh live, and who imports it?** GROUND's zone kit is
the precedent — vendored with a licence note in the same commit — but the route
went through `Scripts/compose_fernhall.py` and an import session, and the fonts
went through `Scripts/import_fonts.py` plus an editor console command because
Python could not build the composite. A skeletal mesh needs a skeleton and an
import, and `.uasset` may not be hand-edited. **I need to know whether I author
the import script and the owner runs it, or whether the owner imports and I
consume** — the fonts precedent says the second, and it is a real dependency on
his time rather than mine.

**2. The mapping is DATA, and I want the shape agreed before I build it.** The
ruling's test is that replacing every mesh is a content change with no C++ diff.
A `TMap<EBreakerEnemyFamily, TSoftObjectPtr<USkeletalMesh>>` on a data asset
satisfies that; a `ConstructorHelpers::FObjectFinder` in each subclass does not,
and that is what every existing mesh and material in `Combat/` uses today. So the
first mesh is also a small refactor of how enemy visuals are acquired at all,
and I would rather say that now than discover it as scope.

## The separation shape (ORDERS Part One-C, reported before authoring)

Measured on `a981ec9`, `Lvl_Gym`, `-BreakerCrowdProbe=100 -BreakerCrowdLoad=engaged`,
probe self-reporting `engaged=100%`:

```
  game thread            31.43 ms        (sweep fit predicted 35.63)
  nearest body to pawn        2 cm
  nearest-neighbour min       0 cm
  nearest-neighbour mean     71 cm       against a 90 cm body width
  bodies inside one body width of a neighbour   100 of 111
```

**Both claims were true and I had only checked one.** `nearest` in the probe's
summary is measured to the PAWN; I read it as the crowd clumping into itself and
GROUND read it as clumping onto the player. It does both — the mean
nearest-neighbour distance is 71 cm against a 90 cm body, and 100 of 111 bodies
are inside one body width of another. **71 cm is the number separation has to
move**, and there was no instrument that could state it until now.

### Nothing physical keeps them apart, so this is steering

The enemy capsule is profile `Custom`, radius 45 cm, **response to Pawn =
Overlap**. Nothing in the project sets that profile; it is an engine default
nobody chose. Enemies are bare `APawn`s with no movement component, moving by a
single `AddActorWorldOffset(Step, bSweep=true)` at `BreakerEnemy.cpp:543`.

So bodies interpenetrate freely, and the N² is the swept capsule's QUERY cost
through a dense cluster — not contact resolution. Three consequences:

- **Making them Block is not the fix.** Physics would enforce spacing, but a
  blocking crowd cannot reach the player at all (they would shell at ~90 cm),
  which changes the fight into something nobody designed, and contact resolution
  between a hundred stacked capsules is not obviously cheaper than the query.
- **Separation must be authored as a behaviour**, contributing to
  `DesiredDirection` before the move, alongside the chase and patrol branches
  that already write it.
- **The sweep stays.** It is what stops enemies walking through walls. Separation
  makes it cheap by making the cluster sparse, rather than removing it.

### The neighbour query is the trap, and it has a known shape

ORDERS asked whether a separation pass reintroduces the term it removes. **It
does, if written the obvious way.** Asking each body for its neighbours by
iterating every enemy is O(N²) — that is literally what `Breaker.Field.CrowdReport`
does, deliberately, because it is a one-shot diagnostic and 111² is free once.

The fix is a **uniform grid rebuilt once per frame**: O(N) to build, and each
query touches only the cells within the separation radius. Cell size at the
separation radius makes that a small constant.

**And it is self-stabilising in a way worth stating plainly.** The grid degrades
to O(N²) only when every body occupies one cell — which is precisely the state
separation exists to prevent. The worst case is therefore the FIRST FRAME after a
mass spawn, and it decays immediately. A structure whose pathological case is the
thing it removes is a safe structure; one whose pathological case is the steady
state is not.

No spatial structure exists anywhere in `Source/RiorsEdge` today, so this is new
plumbing rather than a reuse. It is enemy-crowd-specific and belongs in `Combat/`.

### The falsifiable prediction

The sweep's fit without its quadratic term puts N=100 at **9.79 ms**. If
separation works, engaged N=100 should approach that. If it lands near 31 ms with
spacing improved, the quadratic was never the sweep and this report is wrong —
which is the outcome worth being able to see.

**Questions, and they are design rather than engineering:**

1. **What spacing?** One body width (90 cm) is the minimum that stops
   interpenetration. Comfortable readability is probably more. This sets how a
   pack of fifty occupies a yard, and Fernhall's yards are 100 x 50 m — at 150 cm
   spacing, fifty bodies need a 15 m circle, which is a third of a yard's width.
   **The number is a level-design constraint disguised as a movement constant.**
2. **Does separation apply at contact?** An enemy that reaches the player wants to
   be AT the player; strict separation would push attackers off their target and
   turn melee into a shoving match. My assumption unless corrected: separation
   applies between enemies and is suppressed inside attack range of the target.
3. **Do ranged and melee separate differently?** A Lattice holding a band already
   has spacing behaviour of its own kind, and stacking two rules produces neither.

## Two claims dropped rather than assembled

Recorded because "I checked and it is not true any more" is worth as much as a
question, and both were things this lane would otherwise have kept repeating.

- **The Phase D impact-signal misalignment is stale.** The standing note says
  three "round landed" signals fire at three different times — hit sound at
  trigger, crosshair at +16 ms, world spark at flight time. `ScheduleArrivalSound`
  now puts the hit cue on the round's own flight clock, so at least the sound
  half is already fixed. Not re-measured end to end; not asserted.
- **The elemental figure this lane was carrying was not its own.** It arrived as
  "11 Physical + 1 TrueDamage" and does not match anything measurable now. The
  numbers above are FIELD's own, with the scope stated. Passing on a second-hand
  number is the O145 mistake wearing different clothes.
