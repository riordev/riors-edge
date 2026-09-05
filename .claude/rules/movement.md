---
paths:
  - "Source/RiorsEdge/Movement/**"
  - "Source/RiorsEdge/Characters/**"
---

# Movement (KIT)

- Verbs: walk, sprint, two jumps (Swift three), crouch, dash on cooldown
  (O40), slide, vault, mantle. **No wall ride (O144), no grapple, no tether,
  no stamina (O1).** Parry is the only tree-granted verb (O25).
- Traversal runs in `MOVE_Custom / CustomModeLedgeTraversal` with the
  saved-move pass. The pawn holds no traversal state; ask `IsTraversingLedge()`.
- `MaxStepHeight` is authored at 45. `LedgeMinimumHeightCm > MaxStepHeight`
  is an invariant. Band edges carry `LedgeBandEpsilonCm`; authored geometry
  never sits exactly on a band edge.
- Movement is client-predicted and server-reconciled (O52). A new verb ships
  with its saved-move bit or it rubber-bands every remote client.
- Momentum generation is capped per second per source; aim-down-sights states
  are exempt from the speed threshold (O92).
- Every feel number is O2 until the owner has felt it. Motion cannot be
  photographed — trace it (`-BreakerMoveTrace`) and say so in the report.
