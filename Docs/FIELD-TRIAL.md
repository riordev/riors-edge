# The opening field trial

Run `Scripts/ue-field-trial.ps1` in PowerShell. It opens an isolated fresh-save
game. Play normally; the recorder never grants rewards or advances objectives.
The printed run directory holds the saves and observations. Keep it to resume.

1. Create a Caster. Check health against maximum and read the starting kit.
2. Follow the opening objectives out of the Anchor. Aim down sights at rest,
   strafe, stop and fire. The sight should stay aligned and settle predictably.
3. Cast Rot while moving the reticle to another enemy during the wind-up.
   Its footprint should follow the aim and land beneath the final target.
   Queue one more cast, then change targets again. Damage interruption should
   clear the preview and leave no zone from the cancelled cast.
4. Earn and inspect the normal quest rewards. Spend available points by choice.
   Describe the effect of one choice before opening the totals screen.
5. Enter the entry rift, fight its waves, finish it and choose a reward.
   Return to the Anchor and equip or compare what came back.
6. Quit. Run the script with `-ResumeDirectory <the printed absolute directory>`.
   Check equipment, points, quest progress and reward ownership after loading.

Aim for a satisfying thirty minutes; the route has no artificial time limit.
Continue toward the Breach when checking later progression. Never use quest-state
shortcuts for this measurement: they can omit the equipment and XP earned en route.

The opt-in `-BreakerFieldTrial` recorder appends JSON lines under the isolated
Saved/FieldTrials directory. Samples contain elapsed process time, map, objective,
quest flags, area and character levels, wave, XP, equipment item levels, available
points, health, resource, backpack count, cumulative kills, deaths and interrupted
casts. Map changes retain the counters; restarting the process starts a new file.
Cancelled pending casts count as interruptions, including death/cancellation.
Wave and objective durations can be read from consecutive samples (one-second
cadence). These are elapsed times, including menus and pauses, not active-fight DPS.
Run `python Scripts/field_trial_report.py <run-directory>` to summarize each
recording into objective/wave intervals and list the final equipment.
Deaths are recorded immediately after the recorder binds the player; a death in
the first second after pawn creation can precede that binding.

Record what caused each death, whether a missed cast was understandable, and
whether the earned upgrade changed the next fight. These answers require play.
The accelerated `Scripts/ue-loop-probe.ps1 -ActTwo` checks earned-path wiring and
disk persistence separately; its timings never establish encounter balance.

`-BreakerCaptureADS` holds aim for the visual harness. Combine with ordinary
capture switches to inspect the settled sight; it does not simulate human input.
