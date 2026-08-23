# Rior's Edge — working rules

## Current work

The documentation rewrite is complete. `Docs/` is ten files: `VISION.md`,
`DECISIONS.md`, seven specs under `spec/`, and a generated `STATE.md`. The
corpus that preceded it is recoverable at the `pre-corpus-deletion` tag.

Open: 29 items in `DECISIONS.md`. Four `make status` sections sit out of band and
every one is a target. 46 asserted invariants have no test.

The chassis has four live findings and they constrain each other. THE O91 RETUNE
ANCHORS AT THE CAP, NOT LEVEL 1 (O116): time-to-die is 4.97s at the cap against
a 4-5s target and 24.64s at level 1, so the low end comes down rather than the
high end up — base monster damage from 16 toward ~79 and `d` from 0.055 to
~0.021. Reading O91's "damage growth comes down" literally, anchored at level 1,
would take cap TTD to five times its target. The boss band is two errors (O114)
and closing either alone leaves it out of band. Hits-to-die is the authoritative
inversion figure (O113); the 32% between it and the damage-vs-defence proxy is
entirely the flat base max health, now flagged at its declaration.

Next, in order: `Progression.PowerBand.ArchetypeSpread`, the last promoted
finding and the only remaining route to the 20 target; then the ~400 stale doc
citations in `Source/`; then the Class Points delete, which is fully ruled
(O111) and which the Kinetic doctrine waits on.

Waiting on a ruling rather than on work: the flat side of the damage pools —
the ability lane has no flat line at all, and it is the half of the parity
deficit that degrades fastest with depth.

Update this section as the last step of each session.

## Build and test

**The cycle is BUILD → SUITE → COMMIT → PUSH, in that order, no shortcuts.**
"Clean" means **zero unexpected** `Result={Fail}`.

**Deliberate reds are legal only when enumerated.** A test may be red on
purpose when it encodes a target the game does not yet meet — that is a work
item with a number on it, which is worth more than an open question in a
document. Each one names the finding it encodes and the condition that deletes
it, and `make status` reports expected-red separately from unexpected-red. A
red test that is not on that list is a regression, without exception. Never
widen an asserted range to make a red go green: that is choosing an answer
without saying so.

**Never build while the editor is open.** Live Coding holds the lock and the
build fails. Close the editor, or press Ctrl+Alt+F11 in it, first. One
exception: when the owner has the *main* tree's editor open and you are
building in a separate worktree, the lock is a false positive — the guard keys
off the shared `UnrealEditor.exe`, not the project DLL. `-NoHotReloadFromIDE`
is the correct override in that case only.

**A command that prints something reassuring has not told you it succeeded.**
Check the exit code of the thing you actually care about. This keeps arriving in
new shapes: piping a build through `tail` swallows its exit code, and a newline
where an `&&` was meant breaks the chain so the next command runs regardless —
which is how a `git push` reported success after the `git merge` before it had
failed. Neither printed anything alarming. Reading a reassuring result out of
the wrong file is the same failure: suite results are in
`Saved/Logs/riors_edge.log`, never stdout.

Build:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" RiorsEdgeEditor Win64 Development -Project="C:/Users/rior/Documents/GitHub/riors-edge/riors_edge.uproject" -WaitMutex
```

Suite — headless, no RHI. **Results do not reach stdout**; grep the log file:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/rior/Documents/GitHub/riors-edge/riors_edge.uproject" -ExecCmds="Automation RunTests RiorsEdge; Quit" -unattended -nop4 -nosplash -nullrhi
```

```bash
grep -c "Result={Fail}" Saved/Logs/riors_edge.log
```

A count *below* the previous passing total means tests went missing, which is
itself the regression. `make status` reports the current total; do not pin it
here.

Standalone game:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/rior/Documents/GitHub/riors-edge/riors_edge.uproject" -game -windowed -ResX=1920 -ResY=1080
```

### The capture harness

The project photographs itself, and reading your own screenshots is expected of
any visual work. Frames land in `Saved/Screenshots/breaker_NN.png`; the process
exits ~2.5 s after the last. Capture runs on a **core ticker, not a world
timer**, because opening a menu pauses the world.

| Switch | Form | Effect |
|---|---|---|
| `-BreakerAutoPlay` | flag | Skips the title menu into the gym. **Required for `-BreakerCaptureMenu`**, which is parsed inside its branch. |
| `-BreakerScreenshots=N` | int 1–60 | N frames then exit. First at 6.0 s, then every 2.0 s. |
| `-BreakerCaptureMenu=<SCREEN>` | string | `INVENTORY`, `SKILLTREES`/`SKILLS`, `SETTINGS`, `CLASS`/`CLASSSELECT`, `PAUSE`, `CHARACTERSELECT`, `CHARACTERCREATE`, `DEVSANDBOX`/`SANDBOX`. Anything else **silently** falls back to the main screen. |
| `-BreakerCaptureBoard=<BOARD>` | string, not a bare flag | `CORE`, `COMPARE`, `BRANCH<n>`. Also the back door to FORGE/ABILITIES when combined with `-BreakerCaptureMenu=INVENTORY`. |
| `-BreakerCaptureTour` | flag | Eight authored field vantages instead of the player's eyes. |
| `-BreakerCaptureHUD` | flag | Fabricates the HUD events a headless run cannot reach. **Returns a false negative on damage-number aggregation** — see below. |
| `-BreakerCycleWeapons=<seconds>` | float > 0 | Walks the viewmodel through the archetypes. |
| `-BreakerBossOnStart` | flag | Spawns the Field Marshal during the gym build. |

Two permanent limits: the harness **cannot move a mouse**, so every hover state
and every zoom/pan gesture is structurally unverifiable by it; and a vantage set
is a hypothesis about what can go wrong — the ground-tearing defect was
invisible from all seven existing vantages and needed an eighth.

**`-BreakerCaptureHUD` photographs damage-number aggregation as BROKEN, and it
is not.** Every fabricated hit carries a null target, and the merge predicate
refuses a null target because an expired weak pointer is not a target — so the
numbers that would collapse into one in play stay separate in the capture. The
switch photographs the damage-number HIERARCHY faithfully (colour carries kind,
size carries weight) and cannot photograph its AGGREGATION at all. **An
instrument that returns a false negative is worse than one that returns
nothing**, because the next reader files a bug against working code. Read the
merge through `RiorsEdge.UI.Damage.Aggregation`; read the capture for layout.

**Perturb an instrument to find out whether it is lying.** The overlap that
exposed this got worse when the damage-over-time lifetime was raised — a change
to the SUBJECT moved an artifact that belonged to the INSTRUMENT, which is the
tell. When a measurement looks wrong, change something the measurement should be
insensitive to and see whether the defect moves with it. This was found by
accident once and is worth doing on purpose.

### Machines

Unreal 5.8 on both, Git LFS installed per machine, and the repository never
inside iCloud, OneDrive, Dropbox or a network drive.

- **Windows** is the work machine: extended playtests, shader builds, profiling,
  packaging, content-heavy work. Needs Visual Studio 2022 with *Game
  development with C++*, Unreal tooling, and a current Windows SDK.
- **The MacBook has limited memory** — treat it as lightweight authoring and
  integration: C++ compilation, docs, Data Asset setup, small graybox edits,
  short smoke tests. Keep editor scalability low. Do not run the editor, IDE
  indexing and shader compilation at once. Do not repeatedly delete the DDC.
  Needs Xcode and its command-line tools.
- **Switching machines:** close Unreal, commit and push, then pull before
  opening elsewhere. Never commit `Binaries`, `Intermediate`, `Saved` or
  `DerivedDataCache`. Binary Unreal assets cannot be merged — coordinate
  ownership of maps and Blueprints rather than resolving conflicts in them.

## Code discipline

These are the invariants that belong to no single file. Everything else that
used to live in a traps list now lives as a comment at the thing it is true of.

**A pure-maths test proves the rule, never the wiring.**
Where a system's rules are arithmetic, extract them into a world-free header
with no `AActor`, no `UWorld` and no subsystem, and make the actor a thin
caller. That is the only reason the suite can cover the wave budget or the
rarity gates at all. The limit is the other half of the rule:
`RiorsEdge.Movement.JumpGrant` passed for the entire life of a feature no
player could reach, because it proved the rule against a level the game cannot
produce. Where a rule has a shipped configuration, assert that configuration
too, against the default-constructed state the game actually runs in.

**Never grant a test more than the game grants.**
A reachability test that hands itself points, gear, or levels the shipped
configuration does not produce is asserting something about a character that
does not exist. This is exactly how six branch keystones stayed unpurchasable
for a milestone with a green suite.

**Enums serialized by value are append-only, forever.**
Never insert, never reorder, never reuse a retired value. Renaming an
enumerator is safe; moving one is not. The failure is silent — the save is not
corrupt, it is valid data that now means something else.

**Anonymous-namespace helpers in a `.cpp` carry a `Breaker<Subject>` prefix.**
Unity builds merge translation units, so a bare `MakeMaterial()` collides with
another file's. The project has shipped this twice.

**Do not hand-edit `.uasset` or `.umap`.** Create and modify them through the
editor or supported automation.

**Automation cannot see a layout.** Anyone doing visual work is expected to run
the capture harness and read their own screenshots. Looking has found arms
rendering offscreen, a sealed courtyard with the field stranded outside it, and
a class screen dimming all five names to unreadable — every one of them
shipped with a green suite. A screenshot is still not a playtest.

## Documentation discipline

These rules exist because this project's documentation once reached 37,000 lines
across 37 files, four layers of authority, and documents whose opening
paragraphs told the reader which parts of themselves to disbelieve. Every rule
below prevents a specific thing that actually happened.

**Docs state present-tense intent. Nothing else.**
Not status, not build state, not history. A document is never "partially
built" — the *game* is partially built. If a spec says the game does something
it does not yet do, that is correct: the spec describes intent.

**No annotation genre.**
No STATUS banners. No "Last reconciled against". No "SUPERSEDED", no
strikethrough, no "~~this used to say~~". No provenance labels
(TRANSCRIBED / AUTHORED / RECONCILED). When text becomes wrong, edit the text.

**A spec is 300 lines maximum.**
Over budget means the system is under-decided, not under-documented. Cut. Do
not split a spec into two files to stay under the limit.

**Never cite another doc as authority.**
If two documents need the same fact, one of them is in the wrong place. Move
the fact; do not cross-reference it.

**Never write a document about the documents.**
No map, no index, no synthesis pass, no authority chain, no reconciliation
report. If you feel the need for one, the corpus has already failed.

**Rulings live in DECISIONS.md, one line each, live only.**
Superseded rulings are deleted. Git has them. A ruling that needs a paragraph
of context to understand is not yet a ruling.

**Build state is generated, never written.**
`make status` — or `python Scripts/status.py` — writes `Docs/STATE.md`. Every
section declares a direction: a **ceiling** falls and never rises, a **floor**
rises and never falls, a **band** stays inside. Pins live in
`Scripts/status-pins.json` and are authored deliberately, never generated from
a run: where the current state is the problem, the pin is the target. Do not
hand-maintain any of this in prose. If you notice dead content, fix the
generator or fix the content.

**History lives in git.**
No `archive/` directory. No "kept for the record" sections. No dated log
entries inside a spec.

## Design discipline

**Do not author design content for a system whose plumbing does not exist.**
If a node cannot pay — no stat target, no aggregation lane, no condition —
do not write the node. Widen the vocabulary first, then author. This is the
rule that would have prevented two thirds of the tree from being silent.

**Reachability is part of definition-of-done.**
A feature merges with its in-game path: spawn table, drop gate, UI hook, or
key binding, plus a shipped-configuration test. Content the player cannot
reach is not built.

**Every number is a placeholder until measured.**
Placeholders are fine. A placeholder the player cannot feel is not a
placeholder — it is dead content.

**Balance targets are tests, not prose.**
The build-variance band, the weapon/ability parity band, and the More ceiling
are pinned by automation. If a target is worth writing down, it is worth
asserting.

## Session discipline

**One spec per session when writing docs.** Fresh context each time. Two or
three source files, not the whole tree.

**Plan mode for anything touching multiple files.** Approve plans, not diffs.

**A plan that proposes to "reconcile", "preserve", or "carry forward" is
wrong.** Reject and re-scope.
