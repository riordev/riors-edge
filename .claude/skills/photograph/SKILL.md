---
name: photograph
description: Run the capture harness and read the frames. Use for any visual work — HUD, menus, enemy readability, viewmodel, levels — before reporting it done.
argument-hint: [Anchor|Gym|Fernhall] [extra -Breaker switches]
allowed-tools: Bash(bash Scripts/ue-capture.sh*)
---

Frames land in `Saved/Screenshots/breaker_NN.png`; the process exits ~2.5 s
after the last frame. A capture needs a built editor binary here — run
`/cycle` first if `Binaries/` is empty.

```
bash Scripts/ue-capture.sh $ARGUMENTS
```

First argument is the map (`Anchor`, `Gym`, `Fernhall`); anything after is
passed through. Useful switches: `-BreakerCaptureMenu=INVENTORY|SKILLTREES|SETTINGS|CLASS|PAUSE|CHARACTERSELECT|CHARACTERCREATE|DEVSANDBOX`
(anything else silently falls back to the main screen), `-BreakerCaptureBoard=CORE|COMPARE|BRANCH<n>`,
`-BreakerCaptureTour`, `-BreakerCaptureHUD`, `-BreakerCycleWeapons=<s>`,
`-BreakerBossOnStart`, `-BreakerEnemyModifier <n>`, `-BreakerCrowdLoad=patrol|engaged`.

Then **open every frame and read it**. Say what you saw, at which vantage,
and what the harness cannot show: it cannot move a mouse (no hover, tooltip
or zoom), `-BreakerCaptureHUD` false-negatives damage-number aggregation, and
a vantage set is a hypothesis — the ground-tearing defect needed an eighth.
Motion cannot be photographed; trace it and say so.

A screenshot is not a playtest. Say which numbers are owed the owner's hands.
