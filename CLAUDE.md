# Rior's Edge — working rules

A first-person looter shooter with ARPG progression, Unreal 5.8, C++-first.
`Docs/VISION.md` says what the game is. `Docs/spec/` says how each system is
meant to work. `Docs/DECISIONS.md` holds live rulings (O-numbers). `Docs/ORDERS.md`
is the design seat's work queue. `Docs/STATE.md` is generated — never edit it.
The desk's queue is `.claude/DESK.md` — read it first, move landed items to
its Done list last.

## The cycle

**BUILD → SUITE → `python Scripts/status.py` → COMMIT → PUSH. No shortcuts.**
Clean means zero *unexpected* `Result={Fail}`. Use `/cycle` to run it.

- Build: `"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" RiorsEdgeEditor Win64 Development -Project="<repo>/riors_edge.uproject" -WaitMutex`
- Suite: `UnrealEditor-Cmd.exe "<repo>/riors_edge.uproject" -ExecCmds="Automation RunTests RiorsEdge; SoftQuit" -unattended -nop4 -nosplash -nullrhi -abslog="<repo>/Saved/Logs/suite.log"`
- `SoftQuit`, never `Quit` — `Quit` drops the last test's result line.
- Results are in `Saved/Logs/suite.log`, never stdout, never `riors_edge.log`.
- Read results through `status.py`, never a grep. Exit 2 means the run was
  partial or clobbered and the report is refused.
- Never build while the editor is open (Live Coding lock). In a worktree while
  the owner's main editor is open, pass `-NoHotReloadFromIDE`.
- Check the exit code of the command you care about. `| tail` swallows it; a
  newline where `&&` was meant breaks the chain.

**Expected-red is legal only when enumerated** in `Scripts/status-pins.json`
with the finding it encodes and the condition that deletes it. Any other red is
a regression. Never widen an asserted range to make a red go green.

## The desk and git

- One session per box, working in the checkout on `main`. No worktrees, no
  lane branches. Close the editor before a session starts.
- Work happens only through `/desk`. It takes the top block of
  `.claude/DESK.md` and nothing else.
- A cycle is one build, one suite, one commit. Then the owner plays and
  writes what he felt into the top of `.claude/DESK.md`. A block that needs
  a second build is too big: split it and say so.
- Stage files by name. Never `git add -A` or `git add .`.
- Push `git push origin HEAD:main`. Fast-forward or refusal. Never force.
  `git pull --ff-only` before you plan, not after you build.
- Never hand-edit `.uasset` / `.umap`. Binary assets cannot merge.
- If meshes are pointer files run `git lfs pull`. Fab/launcher packs are
  per-seat and gitignored. LFS must be installed before pulling.

## Code discipline

- **Pure-maths tests prove the rule, not the wiring.** Arithmetic lives in
  world-free `*Math.h` headers; actors are thin callers. Also assert the
  shipped configuration against the default-constructed state.
- **Never grant a test more than the game grants** — no test-only points,
  gear or levels.
- **Enums serialized by value are append-only, forever.** Rename is safe;
  insert, reorder, reuse is silent save corruption.
- **Anonymous-namespace helpers carry a `Breaker<Subject>` prefix** — unity
  builds merge translation units.
- **Every constant is `// O2 PLACEHOLDER` until measured.** Prefer the
  implementation that authors nothing.
- **Reachability is definition-of-done.** A feature merges with its in-game
  path and a shipped-configuration test. Content the player cannot reach is
  not built.
- **Do not author content for plumbing that does not exist.** No stat target,
  no lane, no condition → do not write the node. Widen vocabulary first.
- **Behavioural gaps are recorded at the site, never faked** with a
  nearest-fit primitive.
- **Automation cannot see a layout.** Visual work runs the capture harness
  (`/photograph`) and the author reads the frames. A screenshot is still not
  a playtest.
- New multiplier lane → canon row in `power-and-scaling.md` plus a
  conformance test, before merge.

## Documentation discipline

- Docs state present-tense intent. Not status, not history, not build state.
- No annotation genre: no STATUS banners, no "superseded", no strikethrough,
  no provenance labels. When text is wrong, edit the text.
- A spec is 300 lines maximum. Over budget means under-decided: cut.
- Never cite another doc as authority; move the fact instead.
- Rulings live in `DECISIONS.md`, one line each, live only. Superseded rulings
  are deleted; git has them. A ruling needing a paragraph is not yet a ruling.
- History lives in git. No archive dirs, no dated log entries, no "kept for
  the record". Session narrative goes in the commit message, never in this
  file.
- Questions for the owner go in `.claude/DESK.md` beside the item that
  raised them; delete each in the commit that lands its answer.

## Session discipline

- Read `.claude/DESK.md`, `Docs/STATE.md` summary, and the DECISIONS the top
  block cites. Two or three source files, not the tree. Then `/desk`.
- **Scouts read, hands edit, the reviewer blocks.** Findings before edits;
  disjoint file lists; a finding that "reconciles", "preserves" or "carries
  forward" is wrong — re-scope.
- One block per session. Fresh context each session.
- Finish the cycle: one commit whose message lists each item as landed,
  found-not-built, or reverted with the numbers measured; push or state
  exactly why not; move landed items to Done. Then stop — the owner plays.
- Never idle on an unruled question — record it in `.claude/DESK.md` and
  take the next item in the block.

## Capture harness

Frames land in `Saved/Screenshots/breaker_NN.png`. Standalone:
`UnrealEditor-Cmd.exe "<repo>/riors_edge.uproject" -game -windowed -ResX=1920 -ResY=1080`
plus switches: `-BreakerAutoPlay[=Anchor|Gym|Fernhall]`, `-BreakerScreenshots=N`,
`-BreakerCaptureMenu=<SCREEN>`, `-BreakerCaptureBoard=<BOARD>`,
`-BreakerCaptureTour`, `-BreakerCaptureHUD` (false-negative on damage-number
aggregation — read that through `RiorsEdge.UI.Damage.Aggregation`),
`-BreakerCycleWeapons=<s>`, `-BreakerBossOnStart`. The harness cannot move a
mouse: hover, tooltip and zoom states are unverifiable by it.

## Machines

Windows is the work machine; the MacBook is light authoring only. Repo never
inside iCloud/OneDrive/Dropbox. Seat-specific paths live in `CLAUDE.local.md`
(gitignored), not here.
