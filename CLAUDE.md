# Rior's Edge — working rules

## Build and test

<!-- FILL THIS IN from Docs/HANDOFF.md §3 before deleting it. Build command,
     test command, how to run the game, the two-machine constraint. This is the
     only section of the old handoff worth keeping. -->

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
`make status` reports silent nodes, unmapped enum targets, unreachable
rarities, dead tags, and unreferenced content. Do not hand-maintain any of
this in prose. If you notice dead content, fix the generator or fix the
content.

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
