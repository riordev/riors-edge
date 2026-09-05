---
name: lane
description: Start a lane session. Reads the lane pointer, the lane's ORDERS section, STATE summary and open questions, then plans one work item in plan mode. Use at the start of every session.
argument-hint: [LANE] [optional item id]
disable-model-invocation: true
---

You are the **$0** lane. One item this session, fresh context.
You are in a lane worktree the app created; never touch the main checkout.

## Ground truth, in this order

Lane pointer:
!`cat .claude/lanes/$0.md 2>/dev/null || echo "(no pointer yet — create one before ending the session)"`

Branch and base:
!`git rev-parse --abbrev-ref HEAD && git log --oneline -1 && git fetch -q origin && git rev-list --count HEAD..origin/main | xargs -I{} echo "{} commits behind origin/main"`

Suite state (summary rows only):
!`sed -n '/^## Summary/,/^## Tests/p' Docs/STATE.md | head -40`
!`sed -n '/^## Tests/,/^## /p' Docs/STATE.md | head -8`

Your open questions to the design seat:
!`cat Docs/reports/$0.md 2>/dev/null | head -60`

## Steps

1. If behind `origin/main`, rebase now — before planning, not after building.
2. Read your section of `Docs/ORDERS.md` (grep for `$0` and for the item id
   `$1` if given). Read only the DECISIONS your item cites. Read two or three
   source files, not the tree.
3. State the item in one sentence: what changes, which directory it lands in,
   which test pins it, and which lane it crosses (if any).
4. Enter plan mode and write the plan. A plan touching more than one file is
   approved before any edit. A plan that "reconciles", "preserves" or
   "carries forward" is wrong — re-scope.
5. Build it. Run `/cycle` before claiming anything is done.
6. Finish with `/report`.

If the owner is away and the item is blocked on a ruling: write the question
to `Docs/reports/$0.md`, take the next item in your ORDERS section, and say so
in the pointer.
