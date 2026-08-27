# GROUND

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## A connection has no marker, so nothing can find one

Found by looking rather than by testing, which is the point of looking. I built
a zone capture tour, wrote a vantage called "the seam from above", and it
photographed the wall between the two yards instead. The derived midpoint lands
at x 81; the seam spans x 101-118.

**THE CAUSE IS THAT A CONNECTION IS AUTHORED IN TWO PLACES THAT NEVER MEET.**
Its geometry — floors, walls, the dog-leg — is in `compose_fernhall.py`. Its
terms — mouth width 1000, length 2900, no through-sight — are in
`FernhallConnections()`. **Nothing checks that they agree, and nothing in code
can locate a seam at all.** I wrote "the geometry honours these numbers; it does
not derive them" when I authored it, and this is that sentence biting.

It is the same shape as the 19.8 m lane figure this lane already fixed once: a
number asserted in one place, honoured in another, measured by neither. The
connection rule I built validates the terms against each other and is silent
about whether any geometry matches them — so a composer that widened a mouth to
20 m would still pass, because the rule reads the authored 1000.

**What I would build, and it is a shape so I am reporting first:** a
`connection` marker role, TWO per connection, one at each mouth. That makes
mouth position derivable, makes length measurable (the walked distance between
mouths, not an authored guess), and makes the two-mouth rule structural in the
export rather than only in the struct. Mouth WIDTH still needs authoring — a
marker is a point — unless mouths come in pairs per side, which doubles the
authoring for a term that is already a ceiling.

Not urgent: one seam exists and its numbers are honest today because I authored
both halves in the same hour. It stops being honest the first time someone
edits one half.

- **Question:** confirm the two-mouth marker role, or say the terms staying
  authored is acceptable and I will pin the composer's numbers in a test
  instead — which is cheaper and catches drift without making the geometry
  derivable.

## What a roaming player meets between rift doors (queue item 5, reported before authoring)

Fernhall's roam space is empty: cover, two rift doors, a travel gate, an NPC
marker, no enemies. The question is population, encounters, or neither.

**THE GRAMMAR ALREADY ANSWERS PART OF IT, AND IT RULES OUT "NEITHER".** Every
yard is validated against a COMBAT grammar — cover pitch so a LATTICE telegraph
can be answered by moving, line breaks so a Skirmisher has something to break
sight behind, a dash lane wide enough to cross at speed. Two yards now pass it
identically. **That grammar is meaningless in a room nothing fights in.** If the
roam is pure traversal then the cover lattice is scenery and the yards should
have been authored as corridors, which they are not. Something fights here.

**THE TENSION NOBODY HAS NAMED: the rift interior is the SAME GROUND.** A rift
run is Fernhall's geometry with `PendingRift` set. So roam-population and
rift-population are the same yard with different rules, and a player who fights
trash walking to a door and then steps through it to fight trash in the room
they were just standing in has been given one space twice. That is the thing
most likely to make the loop read as thin, and it is not fixed by tuning either
population.

**WHAT A ROAM FIGHT LACKS is exactly what O168 gave the rift: an ENDING and a
payout.** A rift resolves — the terminator dies, the run completes, LEDGER pays.
A roam fight resolves into nothing. Population authored without noticing that is
Part Three-E's finding one level out: a shooting range in the corridor outside
the shooting range.

Three shapes, and I recommend the third:

- **Population** — persistent enemies per yard, respawning. Matches Part Two's
  *"enemies live in it, not in waves"* most literally. But it puts the same
  fight on both sides of a door, and respawning trash between the player and a
  destination is a toll rather than content.
- **Encounters** — set pieces that trigger, resolve and stay cleared. Gives a
  roam fight the ending a roam fight lacks, and the machinery is nearly built:
  a cleared-encounter state is the completion latch with a different key. But
  it is a system, and Part One-AA's rule against authoring a system before its
  content applies to me here as much as to a discovery reward.
- **SPARSE AND NON-RESPAWNING, and the yard's real content is what is IN it**
  — recommended. A handful of bodies per yard, cleared once and staying
  cleared for the visit, existing so the walk is not empty rather than to be a
  fight worth having. The fight worth having is through the door. **This is
  what One-AA's "exploring is content" implies**: the second yard is the first
  thing in this game that can be FOUND, and a yard is worth walking to because
  of its door, its giver and the fact that it exists — not because of the trash
  between here and it. Population competing with that is population working
  against the reason the yard is there.

Two things I am NOT deciding, both the owner's by rule 2: how many bodies is
"sparse" (`Breaker.Rift.Population` makes that a walk-and-pick), and whether a
cleared yard re-populates on re-entering Fernhall.

- **Question:** confirm sparse-and-non-respawning, or name one of the other two.
  I have authored nothing. If the answer is encounters I would want it sequenced
  after the owner has walked a rift run, because an encounter is a small rift
  and building the small one before the big one has been judged is the wrong
  order.

## WITHDRAWN: yard size against crowd spacing

I asked the seat to rule yard sizes against a spacing figure. **FIELD has
retracted the arithmetic that question rested on and I am withdrawing it rather
than leaving the seat to rule on it.**

Their corrected numbers: fifty bodies at 150 cm hex-pack into 97.4 m², an 11.1 m
circle — not the 15 m I was given — and against a 75 x 40 m combat band that is
**3.2% by area**. The original "a third of a yard" was a diameter-against-width
ratio stated as though it were occupancy.

At 3% the coupling I claimed does not exist. Spacing is a movement constant at
the densities Fernhall fields, and it becomes a level-design constraint only for
line-abreast formations or waves needing a second rank — a converging crowd is
neither.

The instinct that the two should not be decided in different rooms was fine; the
number justifying it was not, and a question resting on a retracted figure is
worse than no question. **Nothing is wanted from the seat here.** The second
yard was authored at the entry yard's footprint, which is what the earlier
direction already said.

## Does the wave budget take a party term? (O133)

Recorded owner-held from an earlier cycle and unaddressed since. Less urgent
under Part Two — a yard's population is driven by difficulty rather than a
wave index — but O82 still rules the party axis real, and
`Game.Waves.PartyScaling` pins the current answer as measured rather than
intended. No action wanted; listed so it is not lost.
