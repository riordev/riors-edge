# NAV
branch: claude/lane-nav-1-f1c147   base: abd205e   suite: 509 / 3 / 0
current: NAV-1 — landed on main: AI/ controller + floating mover + runtime nav bounds; an enemy paths around a wall (touches=0), filmed from two vantages.
next: NAV-2 — cover behaviour rides the nav: BreakerCoverBehavior picks a cover point and paths to it; filmed losing and re-acquiring line of sight.
blocked-on: nothing
crossings this cycle: Combat/BreakerEnemy.{h,cpp}:Mover, Tick movement block, ParkPooledBody, ReviveFromPool → FIELD; Config/DefaultEngine.ini nav section; RiorsEdge.Build.cs modules.
