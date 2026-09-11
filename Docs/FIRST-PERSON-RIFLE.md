# First-person rifle

The rifle uses `Gun_Rifle` with an arms-only Manny mesh. The world body remains
owner-invisible and casts the character's shadow. Camera arms and weapon are
owner-only, collisionless and do not cast world shadows.

`BreakerArms` generates `SKM_FirstPersonArms` through Unreal's mesh-description
APIs. Every retained triangle belongs to one arm, with at least 90% of each
vertex's weight in that arm's descendants. Disconnected source vertices and all
higher LODs are removed. Skeleton and material slots are retained. A fresh
`BreakerArms -AuditOnly` invocation validates the saved asset and render weights.
The full mannequin is never a first-person fallback.

The idle pose's grip sockets fit both rifle contact points. A shoulder cant and
separate hip/aim distances keep the arm boundaries outside the intended view.
The firing wrist carries the gun during reload; the ammunition system supplies
the animation's progress. Refused reloads leave the idle pose unchanged, and
swapping weapons cancels the pose. Remote clients without an authoritative reload
clock retain idle rather than inventing progress.

Firing uses the shared recoil spring. The supplied fire clip is mesh-space
additive and requires a native additive layer before it can play over idle.
Every archetype wears the same arms at the same hand points, hip offset and aim
distance. The Sidearm wears `Gun_Pistol` and the Sniper `Gun_Sniper`; the other
five wear `Gun_Rifle` until a textured model exists for each, scaled to the
archetype's own silhouette length. The imported materials, fixed rifle magazine and stock reload
gesture remain placeholders; this is not a finished animation/art set.

Muzzle effects and tracers follow the presented weapon. The rifle emission point
is measured from its front-cap geometry and checked against the source asset in
automation. Gameplay does not access discarded CPU mesh vertices. Damage traces
continue to originate from the player's aim.

An isolated capture run can use `-BreakerCaptureRifle=Hip`, `ADS`, `Fire` or
`Reload`, together with `-BreakerScreenshots=N`. Fire/reload captures invoke real
weapon verbs and spend normal ammunition. Use a unique `-UserDir` for every run.
