# GROUND

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Yard size and crowd spacing have to be chosen against each other (FIELD's finding)

FIELD measured a Fernhall-equivalent engaged crowd and reports nearest-neighbour
spacing of **71 cm mean against a 90 cm body width, with 100 of 111 bodies
inside one body width of another**. Separation has to move that number, and
their design answer is steering rather than collision — enemy capsules overlap
rather than block, so making them block would shell the crowd at 90 cm and it
would never reach the player, which is a different fight from the one anyone
designed.

**The part that lands on this lane: fifty bodies at 150 cm spacing need a 15 m
circle, which is roughly a third of a Fernhall yard's width.** Spacing is a
level-design constraint wearing a movement constant, and the two cannot be
picked separately — a yard sized without it will not hold the crowd the density
target asks for, and a spacing constant chosen without yard sizes will either
not fit or not read.

THIS IS LIVE NOW. The second yard is authored this cycle, so sizes are being
picked — and I am about to author more against whatever is true today.

- **Question:** rule the yard sizes against a spacing figure rather than
  before one. I do not need the figure itself — I need the two not to be
  decided in different rooms.

## Does the wave budget take a party term? (O133)

Recorded owner-held from an earlier cycle and unaddressed since. Less urgent
under Part Two — a yard's population is driven by difficulty rather than a
wave index — but O82 still rules the party axis real, and
`Game.Waves.PartyScaling` pins the current answer as measured rather than
intended. No action wanted; listed so it is not lost.
