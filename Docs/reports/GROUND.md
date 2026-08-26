# GROUND

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

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

## The rift door announces itself as TRAVEL (needs one line in GLASS's file)

Found by capture, not by the suite — the door is placed geometry and
automation cannot see a label. The door spawns correctly on `marker_rift`,
the beacon reads at lane distance, and the F prompt says **ENTER RIFT**. The
line directly above it says **TRAVEL**.

`BreakerPlaytestHUD.cpp:1724` draws the overhead label as a literal
`TEXT("TRAVEL")`, while the prompt beneath it correctly calls
`GetPromptLabel()`. So the first new interactable in the world states two
different things about itself, one of them wrong, stacked vertically. That is
O132's shape exactly — two things printing under one word — on a rift object.

**The crossing:** the fix is a getter on `ABreakerTravelPoint` (mine) that
the HUD calls instead of the literal — default `TRAVEL`, overridden to `RIFT`
on the door — and swapping the literal is one line in `UI/` (GLASS's).

I have deliberately NOT added my half yet. An uncalled getter is a dead API,
and this file already carries the note that `GetPromptLabel` spent a whole
milestone as one; adding a second would repeat the defect I would be fixing.

- **Question:** route the one-line swap to GLASS and I will land the getter in
  the same cycle, or tell me to take both halves as a declared crossing.

## How many bodies make one yard feel populated? (ORDERS Q1, design half)

The engineering half is ruled and understood: 50–100 is a concurrency budget
on the awake set, not an allocation to divide among yards. The design half is
still open and still the owner's — how many bodies a yard needs to read as
inhabited is a standing-in-it number.

Not blocking. The marker chain and the grammar split come first either way,
and both are independent of it.

## More kit assets — yes or no? (ORDERS Q5, owner's call)

Ten `.glb` pieces vendored, all ten already spent by the composer. Layout can
make five yards; it cannot make them look like five places with no unused
vocabulary. A download, so it needs the owner's say-so like the fonts.

Blocking only the *authoring* of yards two through five, not the marker
chain, the grammar split, or the connection rule. Worth answering before that
authoring starts rather than during it.

## Does the wave budget take a party term? (O133)

Recorded owner-held from an earlier cycle and unaddressed since. Less urgent
under Part Two — a yard's population is driven by difficulty rather than a
wave index — but O82 still rules the party axis real, and
`Game.Waves.PartyScaling` pins the current answer as measured rather than
intended. No action wanted; listed so it is not lost.
