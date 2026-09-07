# The Quiet Earth: Survivor rescue

After the Field Marshal assignment is turned in, the Quartermaster offers
the living-signal assignment. Accepting it exposes The Quiet Earth at travel
points. This is a separate preindustrial Earth, with a garden settlement,
stone square, broken causeway and extraction terrace.

Meet the Survivor inside the shelter to begin the escort. The Survivor walks
the authored route using capsule sweeps and ground checks, follows only while
the living player stays nearby, and waits at three encounter checkpoints.
Fifteen finite Vestiges form those encounters: melee pressure with increasing
Lattice support. Actual enemy deaths clear each pocket; despawning an enemy
does not count. The route is independent of the Rift wave controller.

The HUD shows remaining lucidity. Timeout or player death returns the Survivor
to the shelter for another attempt. Accepted/met flags persist. Defeated
pockets stay defeated during the same visit; leaving and returning rebuilds
the finite encounter. Solo menus pause the world and therefore the clock.

Both the living player and Survivor must reach extraction, after every pocket
is cleared. Only that verified runtime event records extraction. Travel back
to the actual Anchor map records arrival; speaking to the rescued resident
then grants two Exceptional items at item level 30 and two Doctrine points.
The cumulative campaign entitlement becomes six. Reloading or repeating the
conversation does not pay again.

## Tuning and implementation

Escort placeholders are editable on `ABreakerSurvivor`: 240 seconds lucidity,
220 cm/s walking, 900 cm following range, and 180 cm extraction radius.
Encounter area level 30 is set in `BuildSurvivorMission`. Route geometry,
checkpoints and explicit enemy combinations live in `BreakerErasedEarthBuilder`.
Quest, mission and dialogue text remain in the existing `Data` JSON files.

`Scripts/ue-loop-probe.ps1 -Survivor` starts with isolated empty saves and
earns Acts I and II before the rescue. It uses accelerated real damage and
relocates the test player alongside the escort, while the Survivor traverses
every production waypoint at its normal speed. This validates progression,
travel and persistence; it does not measure combat balance or human input.
Add `-Photos` to capture the actual scenes during the run.

The map shell is created through Unreal's `-run=BreakerCensus -CreateRescueMap`
editor commandlet. That command refuses existing assets and exits before
canonical data exports; it is not a map modification command.
