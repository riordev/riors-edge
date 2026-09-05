# NAV
branch: claude/lane-nav-1-f1c147 (pushed to main)   base: 57ae1c6   suite: 509 / 3 / 0
current: NAV-1 — landed: AI/ controller + floating mover + runtime nav bounds; enemy paths around a wall (touches=0), filmed.
next: NAV-2 — cover behaviour rides the nav: BreakerCoverBehavior picks a cover point and paths to it; filmed losing and re-acquiring LOS.
blocked-on: nothing
crossings this cycle: Combat/BreakerEnemy (Mover, Tick movement block, ParkPooledBody, ReviveFromPool; NAV → FIELD), Config/DefaultEngine.ini nav section, RiorsEdge.Build.cs modules.
probe: `bash Scripts/ue-capture.sh Gym -ExecCmds="Breaker.Nav.Probe"` — log prints `REACHED after N s, touches=K`.
