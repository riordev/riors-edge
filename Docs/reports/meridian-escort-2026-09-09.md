# Port Meridian ground-crew escort

Departures Terminal now has a ground-crew NPC with a reachable dialogue choice to begin guiding them across Broken Apron into Maintenance Hangar. The existing Survivor actor supplies swept, floor-following movement, nearby-player waiting, a finite attempt clock and death/failure reset. All three route checkpoints require the actual six regional defenders to die. Physical extraction commits Quest.PortMeridian.GroundCrewEvacuated once, retires the moving map objective and directs return. Supply caches stay optional. No new reward/Core-point economy or enemy waves were added.

Default Erased Earth dialogue/admission and its existing runtime test remain intact. Dedicated regional policy hooks prevent unrelated campaign flags from granting this mission. The crew is a guideable NPC, not a damageable protection target. Existing speed/radius/240-second attempt tuning and new editable route offsets are placeholders; a pre-cleared route remains possible.

Native MeridianGroundCrewRuntime passed: actual layout/18 guards/two gates, static capsule sweeps and checkpoint floors, ordinary nearby dialogue/action, partial-roster and authority/range/LOS refusal, real crew world ticks, wall obstruction, player separation with time passing, lethal player death/reset/retry, actual guard-death callbacks, physical extraction and save archive/revisit. AI is disabled and guard kills are explicitly structural fixtures; no combat-pressure or natural-leveling acceptance is inferred.

All four720p frames inspected in seat/captures/meridian-dialogue-1249 and meridian-prompt-1251. The actual nearby crew, standing floor/capsule and sightline passed capture setup. The dialogue wraps with both real choices visible; the F talk prompt and tracker remain in view. Capture opens no choice and pauses simulation; it does not prove mouse focus or escort motion. NPC and scenery remain rough blockout assets.

Build/full suite:862 passing,3 expected failures,0 unexpected. Native census regenerated54 registered quest flags and the actual new dialogue row. Two compile-time local/member name collisions were corrected without changing route behavior.
