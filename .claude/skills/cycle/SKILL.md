---
name: cycle
description: Run BUILD → SUITE → status.py and report the result honestly. Use before any commit, and whenever asked whether the tree is clean.
allowed-tools: Bash(bash Scripts/ue-build.sh*) Bash(bash Scripts/ue-suite.sh*) Bash(python Scripts/status.py*) Bash(git status*) Bash(git diff*)
---

Run the cycle in order. Stop at the first failure and report it; do not
continue past a red build or a refused status report.

A fresh worktree has no `Binaries/`, so its first build is a full one — allow
10–20 minutes. The build script pulls LFS content first.

## 1. Build

```
bash Scripts/ue-build.sh
```

Check the exit code directly. Never pipe the build through `tail`.

## 2. Suite

```
bash Scripts/ue-suite.sh
```

Results are in `Saved/Logs/suite.log` only, never stdout.

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
