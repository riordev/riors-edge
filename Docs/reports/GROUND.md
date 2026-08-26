# GROUND

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Spawn containment: the shape, before numbers (Part One-L, ruled)

The band disagreement is fixed in the same commit as this report — the spawn
distance is now `DashRefreshDistance` clamped into 5.2's 1500-4000 band, so the
movement constant still carries the intent and cannot silently leave the band
when it is retuned. **That is not the fix for what the owner saw**, and the
ruling already says so: 4000 still spawns outside a 50 m width.

The shape of containment, as asked:

- **A pack belongs to a YARD, and the yard is the one the player is in.** The
  concept exists — `FBreakerZoneField` carries a yard's frame and its pieces —
  but nothing today maps a world position to a yard. That lookup is the first
  piece and it is small: a point-in-band test against each yard's params, which
  is the same arithmetic `IsPointInClearedGround` already does per-field.
- **Distance becomes a RANGE the yard affords, not an offset.** The current
  form asks "how far ahead of the player"; the contained form asks "where in
  this yard, at least X and at most Y from the player, inside the boundary".
  Same band, applied against the room rather than against the facing.
- **Direction stops being the player's facing alone.** The intermittency the
  owner saw is a facing artefact: 44 m down the long axis is fine and 44 m
  across the short axis is outside. A contained spawner picks a direction the
  yard has room in, which by construction cannot depend on which way the player
  happens to be looking.

**Your question — what happens when a yard cannot hold the band — and I think
the 100x50 yard already IS that case.** Facing across the short axis there is no
direction that satisfies a 1500 minimum and stays inside 50 m of width with a
pack spread around it. My answer, in preference order:

1. **The yard declares itself too small for a spawn band, and the grammar says
   so.** This is the option I recommend and it is the one consistent with
   everything else here: the cover grammar already refuses layouts that cannot
   host what they claim to host, and "this yard cannot hold a fight" is exactly
   that kind of statement. It converts an intermittent runtime defect into a
   build-time refusal, which is the trade this project keeps making and keeps
   being right about.
2. **The band shrinks to what the yard affords**, with a floor below which it
   refuses anyway. Cheap, and it silently makes small yards feel different from
   large ones without saying so — the near edge is what decides whether a spawn
   reads as fair.
3. **The pack splits across sightlines.** Most interesting, most expensive, and
   it needs the sightline vocabulary the connection rule is also waiting on. Not
   now.

Note this interacts with the yard shape: if five yards are each about the size
of the current one, and the current one already cannot hold the authored band,
then option 1 says the authored band or the yard size has to move — and that is
a better thing to learn from the grammar before authoring four more yards than
from a playtest after.

- **Question:** confirm option 1, and whether "too small to hold a spawn band"
  belongs in `IsZoneLegal` (a yard that cannot host a fight is illegal) or as a
  separate query a spawner asks (a yard may legally exist without being a
  combat yard). I lean to the second — the entry plaza is a real place that
  should never host a wave.

## Autoplay's destination is one line in `Characters/` (Part One-E, half-landed)

`EditorStartupMap` now points at `Lvl_Anchor` and the instruments moved with it,
as ruled. **PIE is done** — autoplay only travels when `IsFrontEndMap` is true,
so starting in the Anchor it simply suppresses the menu and stays.

What is NOT done is the standalone path. `GameDefaultMap` is `Lvl_FrontEnd`, so a
`-game -BreakerAutoPlay` run still hits
`BreakerCharacter.cpp`'s `TravelTo(GymMapName())` and lands in the gym. That
line is in `Characters/`, which this lane does not own and which ORDERS has not
assigned to anyone.

- **Question:** route the one-line change (gym -> Anchor) to whoever owns
  `Characters/`, or tell me to take it as a declared crossing. Until then the
  ruling holds for PIE and not for standalone, which is a half-state worth
  knowing rather than assuming.

## Costing the rift interior: yard instance vs. gym placeholder (Part One-E)

Both costed, as asked.

**The yard-instance route is CHEAPER THAN IT LOOKS, and the reason is one line
of existing code.** `StartNextWave` spawns its ring around the PLAYER, not at
the authored arena — deliberately, so "the instrument works wherever a playtest
happens". **The wave system has no dependency on gym geometry.** It would run in
Fernhall today if anything called it; nothing does, because only F2 on
`ABreakerCharacter` does.

So the route does not need a new map either. `Lvl_Fernhall` + `PendingRift.IsSet()`
IS a different instance of the same tileset — which is exactly what ORDERS
describes, and it is what an instanced rift is. The branch already exists and
already distinguishes those two states.

Estimated shape, one cycle:
- a rift-instance branch beside the existing Fernhall branch, keyed on
  `PendingRift.IsSet()`
- a run start that calls `StartNextWave`
- the door pointing at Fernhall instead of the gym — the one line I already
  flagged as the line that moves when interiors exist

**What it does NOT solve, and this is the honest half:** it gives the rift a
population but not an ENDING. Part Three-E is right that there is no close-rift
verb, and escalating waves in the yard is the same shooting range in nicer
geometry. It also lands before the yard shape is ruled, so the first rift
interior would be the one-yard entry plaza.

**The gym-placeholder route costs nothing and is wrong in one specific way:**
it is now the only path in normal play that arrives at a test bench, at the
most fiction-breaking moment available.

- **My recommendation:** take the yard instance, but sequence it AFTER the
  close-rift seam rather than before. A populated rift with no ending is a
  louder version of the problem Part Three-E names; a populated rift WITH an
  ending is the loop. Until then the collision is recorded, which ORDERS
  already offers as the acceptable outcome.

## The close-rift seam (Part Three-E, report before building)

The seat's specific question: does the terminator's death write the completion
state directly, or raise something GROUND consumes?

**Raise. GROUND consumes.** The precedent is in this lane already and it was
built for exactly this shape: `ABreakerRiftDoor` does not travel — it raises
`OnRiftEntryRequested` carrying its definition, and the game mode owns what
travel means. Entry and exit should be the same shape in reverse:

- **FIELD's terminator raises** "I died and I was the thing holding this open",
  carrying whatever identifies it. It does not know what a rift IS.
- **GROUND consumes it**, writes the completion state, and ends the run.
- **LEDGER binds the same completion**, or a signal GROUND raises from it, for
  the payout.

That makes it **three commits with one interface**, not one commit across three
lanes — and the interface is a delegate on the terminator, which is FIELD's
header to declare. A direct write would need `Combat/` to include `Game/` and
know rift state, which is the coupling the door was deliberately built to avoid.

- **Question:** confirm the raise-and-consume direction and I will specify the
  completion state's shape and storage next cycle.

## `Playtest/` is GROUND's (Part One-F asks which of us is right)

ORDERS is right and FIELD's reading is wrong: `Playtest/` is named in this
lane's charter alongside `Game/` and `Interaction/`.

Worth saying why the confusion is structural rather than anyone's error:
**`UI/BreakerPlaytestHUD.cpp` is not in `Playtest/`.** The overlay with the
ungated second label pass at line 693, and `bDiagnosticsVisible` defaulting on,
are GLASS's file wearing a name that reads like mine. `Playtest/` itself is
`BreakerPlaytestComponent`, `BreakerKillBuckets` and `BreakerKillTelemetry` —
none of which draws anything.

So: the directory is mine, the defect is GLASS's, and the seat's routing of the
fix to `UI/` is correct.

## The connection rule's own terms (ORDERS Q3, reported before authoring numbers)

Q3 ruled connections a distinct kind of space, exempt from the field grammar,
asking about length and sightline rather than cover pitch, with
`IsLayoutLegal` moving per-yard. The terms, before any magnitude is authored:

- **A connection has a MOUTH WIDTH, and it is a ceiling, not a floor.** This
  is the inversion that makes it a different rule. The field's
  `DashCorridorWidthCm` is a floor — open ground must be at least this wide.
  A connection is recognisable *because* it narrows, so its width is capped
  from above: wider than the cap and it is not a gate, it is two yards
  touching. The floor it still needs is the pathing one already in the
  grammar, `WidestEnemyBodyCm`.
- **A connection has a LENGTH, and it is a ceiling.** Long enough and it is a
  corridor the player walks rather than a threshold they cross. This is the
  term that decides whether a connection is a place (it would then need
  population and cover, and would be a yard) or a seam (it needs neither).
- **A connection has a SIGHTLINE RULE, and it is the one term with a real
  choice in it.** Two shapes, and I recommend the second:
  - *Through-sight required* — standing at one mouth you can see into the next
    yard. Reads as continuous, telegraphs what you are walking into.
  - *Through-sight forbidden* — the connection turns, so a yard's fight cannot
    be shot into from the previous yard. **Recommended**, because O1 makes
    movement the only active defence and a straight connection lets a ranged
    enemy in yard 3 hold a player in yard 2 who has no cover authored for that
    angle. It also makes each yard a self-contained encounter, which is what
    lets a per-yard population be a per-yard decision.
- **A connection carries NO cover pitch and NO line-break requirement.** Both
  field rules exist to answer a telegraph by moving, and nothing telegraphs in
  a seam nothing fights in. Applying them is what currently makes a lane
  triply illegal.
- **What a connection does NOT get is a population.** If a connection needs
  enemies it is a yard, and it should be authored as one. This is the term
  that keeps the two kinds from collapsing into each other.

- **Question:** confirm the mouth-width-as-ceiling inversion and the
  no-through-sight recommendation. Magnitudes are O2 and get authored in the
  cycle after, against a walked yard.

## What anchors a NON-ENTRY yard's frame? (blocks the rest of Q3)

Hit while landing the zone rule. `FBreakerCoverPiece` is field-space by
construction — *"nothing here is world space, which is the whole reason it can
be tested"* — so two yards are two coordinate systems, not two regions of one.
Each yard's grammar has to be measured in its own frame.

The entry yard's frame is derived from its player start and its rift marker.
**A zone has exactly one player start** (Q4, just landed), so a second yard has
no anchor under that rule, and the rift marker is optional per yard so it
cannot be the anchor either.

Three shapes, and I recommend the first:

- **A `yard` marker role, one per yard, carrying origin and facing.**
  `marker_yard_north` anchors the north yard the way the player start anchors
  the entry one. It fits the contract that just landed at no cost — a new role
  string, and `IsComplete` gains "every yard named by any marker has a yard
  marker". **Recommended:** the anchor is authored where the yard is authored,
  and it is one mesh in the composer.
- **Two markers per yard** (origin plus a forward point), mirroring
  playerstart-plus-rift exactly. More faithful to how the entry frame works,
  but it doubles the authoring and the second marker means nothing on its own.
- **Drop frames for non-entry yards** and measure them in world axes. Cheapest
  today, and it makes every yard's grammar depend on how the composer happened
  to be rotated — the thing the derived frame was built to avoid.

Until this is answered the zone rule is landed but only ever holds one yard:
`IsZoneLegal` takes yards whose pieces are ALREADY in a frame, so it is correct
and unblocked, and the thing that cannot be written is the builder that
produces a second yard's pieces.

- **Question:** confirm the `yard` marker role, or name a different anchor.

## How many bodies make one yard feel populated? (ORDERS Q1, design half)

The engineering half is ruled and understood: 50–100 is a concurrency budget
on the awake set, not an allocation to divide among yards. The design half is
still open and still the owner's — how many bodies a yard needs to read as
inhabited is a standing-in-it number.

Not blocking. The marker chain and the grammar split come first either way,
and both are independent of it.

## Does the wave budget take a party term? (O133)

Recorded owner-held from an earlier cycle and unaddressed since. Less urgent
under Part Two — a yard's population is driven by difficulty rather than a
wave index — but O82 still rules the party axis real, and
`Game.Waves.PartyScaling` pins the current answer as measured rather than
intended. No action wanted; listed so it is not lost.
