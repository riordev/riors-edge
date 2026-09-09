---
paths:
  - "Source/RiorsEdge/Movement/**"
  - "Source/RiorsEdge/Characters/**"
---

# Movement (KIT)

- Verbs include walk, sprint, jump, dash, slide, vault and mantle. Slide is
  the crouch; there is no separate static crouch verb (O242,
  `Docs/DECISIONS.md`). **No wall ride (O144), no grapple, no tether,
  no stamina (O1).** The accepted Core Kinesis Air Jump grants an additional
  air jump; Parry is not the only tree-granted verb (`Docs/spec/core-wheel.md`,
  O235). Preserve each class's existing innate traversal rules.
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
