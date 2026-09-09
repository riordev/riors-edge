# Enemy plate and Fernhall cache capture

Built native O251 occlusion and inspected every output frame at 1920×1080.
Open boss: FIELD MARSHAL name, health bar and marks visible. Static wall:
the entire plate disappears; native trace records visibility 1 then 0.
The cache is upright, unobstructed and placed in its guarded off-lane pocket.
The capture keeps its actual locked state; it does not simulate clearance.

Reproduce with UnrealEditor-Cmd, the project, explicit
`/Game/Breaker/Maps/Lvl_Fernhall`, `-game -windowed -ResX=1920 -ResY=1080`,
`-BreakerAutoPlay=Fernhall -BreakerScreenshots=2 -BreakerCaptureArrival`,
and a fresh absolute `-UserDir`. Add `-BreakerCaptureEnemyPlate=open`,
`-BreakerCaptureEnemyPlate=blocked`, or `-BreakerCaptureCache`.
Frame 0 shows arrival; frame 1 follows the isolated setup. Inspect both.

Evidence lives in the seat's `captures/plate-open-direct`,
`captures/plate-blocked-direct` and `captures/cache-direct`, under
`Saved/Screenshots/breaker_00.png` and `breaker_01.png`.
An earlier boot-map attempt produced a black frame before travel and is
rejected as evidence; explicit destination launch avoids that race.

The helper uses a weak core-ticker callback because menus pause world timers.
It refuses an existing save directory and marks the pawn nonsaving. Its
frozen actors and camera are capture-only; they do not validate movement,
hover, aim selection, opening the cache, or replication. The native cache
runtime test separately exercises real guard deaths, claim, spawn and pickup.

Remaining presentation: first-frame shader compilation, rough geometry,
placeholder cache body and enemy poses. The near cache capture has no
interaction prompt, so it does not close prompt/focus acceptance.
