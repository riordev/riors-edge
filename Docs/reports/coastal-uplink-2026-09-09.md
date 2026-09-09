# Broken Coast: restore the coastal uplink

Signal Point now has a physical console objective separate from optional supply caches. Actual player interaction starts a12-second transmission within a7-metre working radius (editable O2 PLACEHOLDER values). Leaving range, breaking sight or dying resets the attempt. Completion records Quest.BrokenCoast.UplinkRestored, retires its map marker and directs return to Anchor. Existing defenders, loot and gates remain the region's actual encounter content; no extra waves or rewards were added.

Native CoastalUplinkRuntime exercises actual console selection, authority/range/LOS refusal, duplicate start, registered actor clock, departure/death resets, completion timing, marker retirement and save archive/recreated-console behavior. Structural guard handling isolates the objective; this is not combat-pressure or human pacing acceptance. PrototypeDestinationRuntime now verifies that opening optional caches does not complete this objective.

Rendered QA: all four1280x720 frames inspected in isolated fresh UserDirs at seat/captures/uplink-ready-1221 and uplink-active-1225. Ready displays F and the restore prompt; active displays countdown without an unavailable action key. The tracker describes range/sight/reset behavior. Actual floor, capsule fit, interaction visibility and nearby selection passed the capture setup. Simulation was paused for these static captures; the native test, not the photographs, proves elapsed completion.

Console and coast remain blockout art. This is one authored objective, not acceptance of natural campaign leveling, return Rifts or the complete seven-step programme.
