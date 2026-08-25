# Rior's Edge — working rules

## Current work

PHASE 4'S WHEELS ARE AUTHORED: 117 of 117 across all twelve, at
`4b9bffa`, every commit through BUILD → SUITE → COMMIT → PUSH at
438 passing / 4 expected red / 0 unexpected. The atlas landed in six
pair commits behind three infrastructure commits: five aggregation
lanes (IncomingDamageReduction, RecoilRecovery, WeaponSpread,
StatusChance, StatusDuration — tree side only, the affix lines are the
affix owner's), the FixtureIdsResolve rank assertion (proven by
perturbation before it was trusted), and the thirteen fixture rows
re-pointed to rank 1 with the composed before/after in the commit.

FIVE CORE REWRITES ARE LIVE WITH CONSUMERS landed in the same commits
as their nodes: Deadeye (granted weak points keep the crit roll — the
purchased O104 exception), Last Round (the magazine's final round
cannot be a non-crit), Threshold (bleed application banks
deterministically instead of rolling), Interposition (a hit inside the
window after a successful block cannot land unblocked), Execute
(armour ignored below quarter health, resolved at the Stage-6 seam).
Rule halves still waiting on systems that do not exist carry
Frenzy-precedent stat halves on live lanes, each named WAITING at its
node: Overrev (no fire-rate ramp), Cadence Break (no cooldown-on-hit),
Convergence Point (no loop valve — O109), Sequence (no reactions),
Fester (no stack diminish to refuse), Short Circuit (the dodge-refund
OPEN QUESTION, deliberately unanswered), Iron Frame (nothing
interrupts a player cast), and the ungated lines on Reach,
Counterweight, Reserve, Answering Fire, Overpressure (each missing a
condition the vocabulary cannot say — Answering Fire's is one of the
four never-true Recently* conditions, whose recorder is the unlock).

RULINGS THE PASS SURFACED, all awaiting the owner: Slipcut's printed
rule is ALREADY BASE KIT (PrepareSlideJump preserves slide-jump speed
unconditionally); Phantom Step's spec text (dash-through) diverges
from its shipped, CONSUMED rule (dodge invulnerability); KINESIS's
flavor ("no damage attached to any of it") contradicts its six +2%
damage rims, and Light Footing kept its shipped dodge/move lines over
the spec row; the ruled nine-More roster yields FIVE unconditional
generalist Mores (22/22/25/26/30) where the old tree argued for two —
a stationary build composes 1.30 x 1.26 x 1.25 with zero commitment;
rim ring traversal is UNGATED because AND-only prerequisites cannot
say either-neighbor (the spec's hexagon edges and entry points are
geometry until an any-of vocabulary is ruled); Siege reads "boss"
through TargetElite. Saves owning Core.Velocity.RedlineDoctrine ranks
stop resolving under the rename (gym-only, O62).

Measured at head: empty-lanes 0 of 31 (first zero ever), dead-tags 141
of 206 and falling (three formerly dead tags gained consumers),
unauthored conditions 11 of 24 (Stationary authored, on Set's
conditional More), silent 43/54, scaffolding 42/50, Core at 171
offered against 65 = 2.63x. The floor's worst tree is the Caster
trios' 2.25 — Core's distance to the spec's 3.42x is exactly step 5,
the 51 travel points and the three-way chooser, NOT YET ORDERED. The
board UI has no cluster entries for the five new wheels (the menu's
hardcoded list — another lane, the Constellation field is authoritative
data). The at-cap band read 5.69 and parity 0.59 mid-migration; both
recompose when travel and the re-pointed fixtures' build-out land.

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
`Saved/Logs/suite.log`, never stdout, and never `riors_edge.log` -- that is the
project default every other run overwrites.

Build:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" RiorsEdgeEditor Win64 Development -Project="C:/Users/rior/Documents/GitHub/riors-edge/riors_edge.uproject" -WaitMutex
```

Suite — headless, no RHI. **Results do not reach stdout**; the log file is the
only record. **`SoftQuit`, never `Quit`:**

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/Users/rior/Documents/GitHub/riors-edge/riors_edge.uproject" -ExecCmds="Automation RunTests RiorsEdge; SoftQuit" -unattended -nop4 -nosplash -nullrhi -abslog="C:/Users/rior/Documents/GitHub/riors-edge/Saved/Logs/suite.log"
```

`Quit` calls `RequestExitWithStatus` with force set, which kills the process
before the last test's completion line is flushed — so the alphabetically final
test had no result in the log and was **counted nowhere**: not passing, not
failing, not missing. Had it been red, every run reported clean. `SoftQuit`
exits gracefully and the log completes.

**Read the result through `make status`, not through a grep.** The grep that
used to live here counted `Result={Fail}` lines, which is exactly the count that
cannot see a test with no result at all:

```bash
python Scripts/status.py
```

It refuses the report at exit 2 on two reconciliations, and names what is
missing. **Every DECLARED test must have started** — the outer check, against
the test names in the source tree, which the run does not get to author. Then
**started must equal completed** — the inner check, against the run itself.

The inner one alone was not enough, and the gap is why `-abslog` is above.
`riors_edge.log` is the project default, so the editor, a standalone run and the
capture harness all open it and rotate the suite record away. Reading that
clobbered file, the inner check found zero started and zero completed, balanced
them, raised nothing, and the report printed **`unexpected red: 0`** — the one
line this whole cycle is read for — off a log with no suite in it. Zero balances
zero. A count checked only against itself cannot tell you it is short.

The outer check catches empty, clobbered, partial, filtered and killed runs with
one comparison, because every one of them leaves a declared test with no start
line. **Do not replace it with a pinned minimum count**: that is a second copy of
the passing total, hand-maintained, wrong the first time a test is added, and a
number that only ever goes up cannot tell you it is short either.

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
of context to understand is not yet a ruling. A ruling that gates on a
measurement names that measurement's section key — ``gated on `section-key```
— and `make status` lists every ruling whose named gate is currently in band.
Four rulings went stale from exactly this shape (O71, O77, the More split,
O106); a gate nobody can enumerate is a gate nobody reopens.

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
