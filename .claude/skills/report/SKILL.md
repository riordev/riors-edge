---
name: report
description: End a lane session. Updates the lane pointer, writes the commit-message session report, and leaves open questions in the lane's report file. Use as the last step of every session.
---

Current state:
!`git status --short | head -30`
!`git log --oneline origin/main..HEAD 2>/dev/null | head -20`

## 1. The lane pointer — `.claude/lanes/<LANE>.md`, fifteen lines maximum

Overwrite it. It is a pointer, not a log:

```
# <LANE>
branch: <branch>   base: <sha>   suite: <pass> / <expected-red> / <unexpected-red>
current: <item id> — <one sentence>
next: <item id> — <one sentence>
blocked-on: <ruling or lane, or "nothing">
crossings this cycle: <file:member → lane, or "none">
```

Nothing else goes in it. Narrative belongs in the commit message.

## 2. The commit message — the session report

Subject: one sentence in the project's voice (what the game now does, not
what you did). Body, in this order, terse:

- **Landed:** what changed, which directory, which test pins it.
- **Measured:** every number you changed or observed, with its instrument.
- **Found, not built:** defects and gaps recorded at their sites.
- **Crossings:** file, member, direction, other lane.
- **Owed the owner's hands:** anything only feel or a playtest can judge.

## 3. Open questions — `Docs/reports/<LANE>.md`

Add only questions the design seat must answer. Delete any question this
session answered. Never put status or findings in this file.

## 4. Push, or say why not

`git fetch && git rebase origin/main && git push origin HEAD:main`.
Fast-forward or refusal, never force. If refused, say what conflicted and
push the branch itself (`git push -u origin HEAD`) so nothing is lost.

Do not edit `CLAUDE.md`, `Docs/STATE.md`, or `Docs/ORDERS.md`.
