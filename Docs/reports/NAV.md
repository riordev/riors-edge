# NAV — questions for the design seat

- **The safe zone under the navmesh.** Enemies stop at the safe-zone edge by a
  tick-time prediction (`ABreakerEnemy::Tick` writes HELD and zeroes the
  direction). With a navmesh in play, the zone could instead be a nav
  modifier area that paths route around, so a chase never aims through it.
  Which is wanted: a hard edge the body stops at (today), or ground the body
  never plans across? Not blocking NAV-2.
- **CLAUDE.md capture line.** Proposed addition to the capture-harness
  switch list: `-ExecCmds="Breaker.Nav.Probe"` (a wall, one enemy behind it,
  two vantages; logs `REACHED … touches=N`). CLAUDE.md is an owner ruling, so
  the line is proposed here.
