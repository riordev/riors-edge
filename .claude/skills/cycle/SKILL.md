---
name: cycle
description: Run BUILD → SUITE → status.py and report the result honestly. Use before any commit, and whenever asked whether the tree is clean.
allowed-tools: Bash(python *) Bash(git status *) Bash(git diff *)
---

Run the cycle in order. Stop at the first failure and report it; do not
continue past a red build or a refused status report.

This worktree's Windows path (use it where the commands say `<repo>`):
!`pwd -W 2>/dev/null || pwd`

Fresh worktree? `Binaries/` and `Intermediate/` are per-worktree, so the
first build here is a full build (allow 10–20 min). If meshes under
`Content/Breaker/Meshes` are tiny text files, run `git lfs pull` first.

## 1. Build

Editor open? Check first — Live Coding holds the lock:
!`tasklist 2>/dev/null | grep -i UnrealEditor.exe || echo "no editor process visible"`

You are in a lane worktree, so `-NoHotReloadFromIDE` is already on the
command below (the lock is a false positive here — it keys off the shared
UnrealEditor.exe, not this worktree's DLL). Then:

```
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" RiorsEdgeEditor Win64 Development -Project="<repo>/riors_edge.uproject" -WaitMutex -NoHotReloadFromIDE
```

Check the exit code directly. Never pipe the build through `tail`.

## 2. Suite

```
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<repo>/riors_edge.uproject" -ExecCmds="Automation RunTests RiorsEdge; SoftQuit" -unattended -nop4 -nosplash -nullrhi -abslog="<repo>/Saved/Logs/suite.log"
```

`SoftQuit`, never `Quit`. Results are in `Saved/Logs/suite.log` only.

## 3. Read the result

```
python Scripts/status.py
```

Exit 0: report `passing / expected-red / unexpected-red` from its output.
Exit 1: a pinned section is out of band — name it; that may be your finding.
Exit 2: the run was partial or clobbered — the report is refused. Rerun the
suite; do not read a grep instead.

## 4. Report

State the three numbers and whether `unexpected red` is zero. If it is not,
name the test and stop. A red that is not in `Scripts/status-pins.json` is a
regression, and widening a range to clear it is choosing an answer without
saying so.

Only after a clean cycle: stage files **by name**, commit with the session
report as the message, `git fetch && git rebase origin/main`, then
`git push origin HEAD:main`.
