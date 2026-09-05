---
name: desk
description: Run one desk cycle — the top block of .claude/DESK.md lands in one build, one suite, one commit, then stops for the owner to play. The only way work happens.
disable-model-invocation: true
allowed-tools: Bash(bash Scripts/ue-build.sh*) Bash(bash Scripts/ue-suite.sh*) Bash(bash Scripts/ue-capture.sh*) Bash(python Scripts/status.py*) Bash(git *)
---

Queue:
!`sed -n '/^## Cycle/,/^## Later/p' .claude/DESK.md`

Tree:
!`git status --short | head -20; git log --oneline -1; git fetch -q origin && git rev-list --count HEAD..origin/main | xargs -I{} echo "{} behind origin/main"`

## The cycle

1. If behind `origin/main`: `git pull --ff-only` first.
2. Take the **top unchecked block** (one "## Cycle" heading). Spawn a
   `scout` per item, in parallel, each with the item text. Read the
   findings. If two items collide on a function, do them in sequence,
   not in parallel. If a scout says stop, stop and tell the owner why.
3. Spawn a `hands` per item, in parallel, each with its finding and its
   explicit file list. Disjoint file lists only.
4. Spawn `reviewer`. Fix anything it blocks on (yourself, or a second
   `hands` pass). Repeat until `CLEAN`.
5. `/cycle` — one build, one suite, `python Scripts/status.py`. Unexpected
   red is fixed or its item is reverted; never widened.
6. If an item is visual: `/photograph` the frame it changes and read it.
7. Stage by name. One commit; the message lists each item as landed,
   found-not-built, or reverted, with the numbers measured. Push
   `origin HEAD:main`.
8. Move the landed items to **Done** in `.claude/DESK.md` (keep three
   cycles there), leave anything reverted in place with a one-line reason,
   add anything found.
9. **Stop.** Tell the owner in five lines what to look for when he plays.
   Do not start the next block.

A cycle that cannot finish in one build is too big: split the block and
say so.
