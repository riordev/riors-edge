---
name: photograph
description: Run the capture harness and read the frames. Use for any visual work — HUD, menus, enemy readability, viewmodel, levels — before reporting it done.
argument-hint: [Anchor|Gym|Fernhall] [extra -Breaker switches]
---

Frames land in `Saved/Screenshots/breaker_NN.png`; the process exits ~2.5 s
after the last frame. Capture runs on a core ticker, so menus are safe.
A capture needs a built editor binary in this worktree — run `/cycle` first
if `Binaries/` is empty.

This worktree's Windows path (use it where the command says `<repo>`):
!`pwd -W 2>/dev/null || pwd`

```
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<repo>/riors_edge.uproject" -game -windowed -ResX=1920 -ResY=1080 -BreakerAutoPlay=$0 -BreakerScreenshots=4 $ARGUMENTS
```

Useful switches: `-BreakerCaptureMenu=INVENTORY|SKILLTREES|SETTINGS|CLASS|PAUSE|CHARACTERSELECT|CHARACTERCREATE|DEVSANDBOX`
(anything else silently falls back to the main screen), `-BreakerCaptureBoard=CORE|COMPARE|BRANCH<n>`,
`-BreakerCaptureTour`, `-BreakerCaptureHUD`, `-BreakerCycleWeapons=<s>`,
`-BreakerBossOnStart`, `-BreakerEnemyModifier <n>`, `-BreakerCrowdLoad=patrol|engaged`.

Then **open every frame and read it**. Say what you saw, at which vantage,
and what the harness cannot show: it cannot move a mouse (no hover, tooltip
or zoom), `-BreakerCaptureHUD` false-negatives damage-number aggregation, and
a vantage set is a hypothesis — the ground-tearing defect needed an eighth.
Motion cannot be photographed; trace it and say so.

A screenshot is not a playtest. Say which numbers are owed the owner's hands.
