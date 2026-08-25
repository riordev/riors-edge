# Rior's Edge — working rules

## Current work

GLASS LANE, THE COST GETS A NUMBER AND THE BOSS ROW READS THREE (457 / 4 /
0 on the rebased tree, up one for O135's pin, same expected-red roster,
`Docs/STATE.md` byte-identical). O130 claimed the bar's split "cost the HUD
class nothing" — a claim about ACCESS standing in for a claim about
SURFACE. The moved TU reads EIGHT private members of ABreakerPlaytestHUD,
four of them mutable state: S, DrawBorder, DrawSpecTextCentered, World,
then EnemyBlips, DrawnLabelBounds, LastFocusBarEnemy, LastFocusBarTime.
O130 amended to carry the number; the judgement that eight beats a context
struct stands, but it needed the figure visible.

`EnemyBlips` NAMED AT ALL THREE SITES, each pointing at the other's lane:
producer in `Combat/BreakerEnemyHealthBars.cpp`, consumer in DrawMinimap,
and the declaration, which said the two halves existed without saying they
had different owners. O155 makes a change to any of the four state members
a DECLARED CROSSING — the first place the ask-for-the-header rule cannot
apply, because the interface IS a member variable: no header to publish,
no compile error when it breaks, and the frame ordering (fill before read,
or the map draws last frame's hostiles) is invisible to the suite.

A5 IS BUILT AND PHOTOGRAPHED: `BOSS  PHASE 2 / 3`, top centre, no bar. The
readability pack contradicts itself (label `phase 2 of 4`, readout `PHASE
3 / 4`) and the fight ships THREE; the count is REFLECTED off
`EBreakerBossPhase` so a fourth phase updates the readout by existing.
Both halves of the fraction are exposed (`GetPhase()`, enum cardinality),
so O120 permits the fraction. NO BAR IN THE ROW by rule — the boss already
has a world bar and a second would be a second owner of one question. The
boss is reached via the game mode's public `IsBossAlive()` (two pointer
checks) and only then a cached scan; `ActiveBoss` is private with no
accessor, and publishing one deletes `ResolveEncounterBoss` entirely. The
capture preview grew a fourth case so the row is photographable without a
boss.

O156 RAISED THE BAND QUESTION AND LEDGER'S O135 ANSWERED IT MID-CYCLE,
INVERTING IT. A multiple-of-three band count is the WRONG answer: the
gates are AUTHORED floats (0.66 / 0.33), not exact thirds, so six bands
put a boundary 0.7% of the bar from the gate — near-coincident, the very
failure the proposal meant to avoid, and the original arithmetic had
assumed thirds. Eight keeps every boundary ≥3.5% clear. Bands are damage
feedback, phase gates are behaviour thresholds: two facts, two marks,
meant to be distinguishable. What survives for the bar is phase marks
drawn heavier and INDEPENDENTLY of the band ticks — combat lane's.

AUDIO, FOR KIT: THE CENSUS IS WRONG AND KIT IS NOT BLOCKED. "Zero sound
anywhere" is false about the project — `ABreakerSoundDirector` ships four
verbs (fire / hit / kill / take-hit) on CC0 samples with a synth fallback,
pooled voices, lazily spawned by the HUD. It is true of ABILITIES: nothing
in `Abilities/` or `Classes/` touches audio. But the interface KIT would
ask for ALREADY EXISTS AND IS ALREADY BOUND —
`UBreakerAbilityComponent::OnAbilityActivated` drives the HUD's ability
flash today, so an activation cue is GLASS calling GetSoundDirector()
inside a handler already running. KIT publishes nothing and should not
hold a cycle. Ability IMPACTS are likely already audible (one
`OnHitDealt.Broadcast` site, the universal damage path) — inferred from
routing, owed one playtest. THE BLOCKER IS A RULING, NOT PLUMBING:
`BreakerSoundDirector.h` forbids a generic `PlaySound(AnyWave)` surface
without one, so an ability cue is a FIFTH VERB — one cue for all 25, or
per-ability cues (a content pipeline, not an interface). Owner's call.

GLASS LANE, THE BAR MOVES OUT (456 / 4 / 0 on the rebased tree, same
expected-red roster, `Docs/STATE.md` regenerates byte-identical). The
enemy health bar is FIELD's question and now lives in FIELD's directory:
`Combat/BreakerEnemyHealthBars.cpp`, 273 lines out of
`BreakerPlaytestHUD.cpp` (3,596 -> 3,323). The split cost the HUD class
NOTHING — `DrawEnemyHealthBars` stays a private member and only its
DEFINITION moved, which is legal in any translation unit and keeps full
private access, so no widened access, no exported helper, no friend. The
five visibility constants were read by nothing outside the pass and moved
whole under `BreakerEnemyBar` rather than `BreakerHUD`: adaptive
non-unity pulls both changed files out of the blob, so THE BUILD CANNOT
PROVE UNITY-SAFETY HERE and the rename is the guard, not the green.
O130 rules the ownership and the TU.

The bar body was drawn twice (enemy loop, gym dummy loop) with the fill
line and the seven-line shield block near-verbatim, plus duplicated
distance geometry; both are now `BreakerEnemyBarPlace` +
`BreakerEnemyBarDrawBody`. Appearance-preserving because `BreakerUI::Alpha`
SETS the channel rather than scaling it and every token arrives from
`Hex()` at A=1, so `Alpha(C, 1.0f)` is exactly C — had it multiplied, the
collapse would have silently dimmed the gym. THE SEGMENTED BAR NOW LANDS
IN ONE PLACE; before this it would have been invisible in the gym, which
is the surface the owner actually plays.

O131 records the visibility rule as ONE RULE ACROSS TWO LANES: trash
aimed-at-only with the 0.6 s fade, and the 8%-health trash mob in a pack
of eighty — which neither visibility rule shows — carried by the BODY's
tint ramp and fracture mask (FIELD's O129). Focus-only is correct only
because that half exists. The DUMMY block keeps its 1.5 s recency window
as the stated exception and now says so at the constant: a dummy is a gym
instrument, four of them never move, and "did that hurt" is the one
question it exists to answer. A comment `d967342` had made false (the
retired six-second rule) is corrected.

GLASS OWES, NOT STARTED: A5, the boss phase readout — the readability
pack contradicts itself (label `phase 2 of 4`, readout `PHASE 3 / 4`), so
it draws phase-count-only until the encounter exposes a total, per O120.
GLASS PICKED UP FROM KIT'S CENSUS: zero sound anywhere is an Audio item.

DECISIONS.md HAS DUPLICATE O-NUMBERS: two different O125s (dash-lane
corridor, health bands) and two O120s, from lanes allocating concurrently
against the highest number each could see. Not rewritten — another lane's
line — but allocation needs to happen at push time, or it recurs.

GROUND LANE, THE READOUT PASS (453 / 4 / 0 on the rebased tree, same
expected-red roster).
THE FERNHALL LANE DEFECT IS CLOSED, AND THE ANSWER INVERTED THE
DIAGNOSIS. The yard prints two lanes and they were printing under one
word, but the chest lane is NOT unguarded: `MinimumOpenLaneWidth` is
full-height only (correctly — chest cover is under MantleStepHeight and
is mantled, not rounded), and the ground the player dashes down is held
by the CORRIDOR rejection instead — no cover of any class within
`CorridorHalfWidth` 900 of the centreline. Pulling the chest pairs to
+-5 m goes RED, and `RiorsEdge.Zone.Fernhall.LaneGuard` proves which rule
objects by perturbing the yard rather than asserting it. O125 rules it.
`DescribeCoverField` now names the lane's CLASS, prints the corridor
margin beside it as an OFFSET (never a width — centre-to-centre versus
face-to-face is the same defect in a new place), and states every band's
DIRECTION in status.py's vocabulary. The Fernhall grammar test logs the
readout on PASSING runs too; it used to print only on failure, so the
numbers were visible exactly once — when they were already wrong.

THE WAVE SOLVER'S PARTY AXIS HAS COVERAGE (`RiorsEdge.Game.Waves.PartyScaling`):
the elite interpolation pointwise across 1-5 plus both clamps, the
per-player body ceiling, the FLAT ranged cap, the per-player Warden cap
reached rather than merely respected, and `IsCompositionLegal` at every
party size over 30 waves. IT FOUND SOMETHING (O126): the budget curve
carries no party term while 5.3's caps are per-player, so at wave 3 solo
buys a Warden, a Lattice and nine Skitters on 18 points while five
players buy three Wardens and NOTHING ELSE. Both legal; pinned as
measured, not intent, and it goes red the day a party term lands.
O127 records the question rift interiors must answer before any solver
code: what a room's shape does to its budget. Archetype roster and
cross-solve state stay HELD; the endgame-pacing question (periodic is
predictable) is in DECISIONS' Open list.

CROWD PROBE READOUT, SCOPE STATED: `-BreakerCrowdProbe` measures
PURSUING, UNENGAGED bodies — they run the full chase tick but spawn 60 m
out and never close, so no shot, hit reaction, damage number or death
effect is on the frame. The log line and header now say so. A
combat-live grid is a separate flag whose name must say so.

DEFECT FOR THE OWNER, NOT FIXED HERE: `Docs/DECISIONS.md` has TWO O120
rulings (loading progress, and reward composition). Both cite-able; only
the loading one is actually cited. Renumbering is a ledger call and the
reward ruling is LEDGER's subject, so it is reported, not touched.

THE KIT LANE OPENED WITH A CENSUS, NOT A FEATURE (lane/kit). Over
the 25 registered fallback abilities (Swift 4, Caster 7, Gunsmith 7,
Tank 7, Support 7): every definition names a real UGameplayAbility
and every ActivateAbility runs real logic — the owner's "make sure
all abilities are implemented" is answered YES for the registered
set, and the gap is PRESENTATION, not activation. Zero of 25 make a
sound or shake a camera. In-world visuals: 12 of 25 draw something
(five Caster abilities call ABreakerEffectRenderer directly; Rot and
Fracture draw through their spawned actors; the deployable/zone
actors carry Turret, AmmoCrate, MineCluster, Disruptor, AnchorPoint,
Suppress, FieldAssembly — but Turret fires invisibly, the crate
dispenses invisibly, and Mine Cluster's individual charges have NO
visual, only the cluster marker, while the player must avoid the
charges). 8 are HUD-only (Sidearm Rig, Overhaul, Bloodline, Hold,
Metronome, Conduit, Cadence — window bars — plus Lead/Mark's target
diamond). 9 draw NOTHING at all: Skim, CadenceBreak, Overdrive (all
three Swift non-mark abilities), Rend, Provoke, Breach Charge (the
charge is a bare FVector), Ground Zero, Patch, Purge. Swift's
missing three are NAMED from the deleted Class-Kits §1.2: S1 Slipcut
(a designed STARTER — code starters are Skim+Lead where design said
Slipcut+Skim), S4 Hard Stop (its verb already lives as a branch
inside BreakerAbility_Skim), S5 Sightline; population 28 intended =
25 registered + these 3. The class-swap stale loadout
(BreakerAbilityComponent.cpp:92) still occurs — DevForceClass never
migrates AbilityLoadout — but the grant is guarded at
ResolveDefinition (foreign-class id falls back to the class default)
and no non-dev path can change a chosen class. Stale note found:
Lead's header says the mark is "tracked, not consumed";
BreakerWeaponComponent.cpp:1434-1579 consumes it. Nothing repaired,
by rule; the full census with per-ability effect/presentation rows
and the recorded-gap ledger is the session report.

THE PLACE IS BUILT AND GATED ON THE OWNER (`f78013b`, 448 / 4 / 0):
the Fernhall approach yard — the vertical slice's zone — landed end
to end. The GLB route is real: Scripts/compose_fernhall.py authors
the scene from the vendored CC0 Kenney kit (Assets/zones/kit, models
CC0 per each repo's README), bakes world transforms into vertices,
names every mesh under the prefix contract (blk_full_ / blk_chest_ /
wall_ / flr_ / dress_ / marker_); Content/Python/
breaker_import_fernhall.py splits it into 58 static meshes + creates
Lvl_Fernhall; UBreakerZoneBuilder is only the assembly loop (spawn at
identity, markers consumed, complex-as-simple collision, O24 palette
painted on because trimesh STRIPS the kit texture in the bake — an
unpainted piece renders near-black). The suite measures PLACED
geometry for the first time: RiorsEdge.Zone.Fernhall.* runs the
cover-registry validators over the same CollectZonePieces the spawner
uses (lane 2100 vs 1600 floor, pitch 1450 vs 1700 ceiling, line-break
1546, clearance 358, cover 3.00%). Lvl_Fernhall is excluded from the
gym fall-through via the new testable IsGymMapName; travel registry
has a third entry (hub pin moved 2→3 — its no-picker rationale was
discharged by the Fieldplate travel screen); the yard keeps a gate
out so it is not a trap. Reach it: Anchor gate → THE FERNHALL
APPROACH, or `Lvl_Fernhall -game -BreakerAutoPlay` (autoplay on a
non-front-end map suppresses the menu in place). Capture note: first
frames of a cold run photograph shader-compile placeholders (black
kit meshes) — read the LAST frame of a 10-frame run. GATED: the
owner stands in the yard before the rift (marked site, PendingRift
writer, gym as interior) and the First Contract mission are built
into it. marker_rift sits at the far-lane pad, marker_npc_contract
on the entry plaza — both already measured, neither consumed by an
actor yet.

THE VERTICAL-SLICE GOAL RULES THIS LANE: a fight the owner can judge
in ten minutes, readable at 50-100 enemies. HEAD `b8a4efe`, 445
passing / 4 expected red (same roster) / 0 unexpected. LANDED: the
crowd measurement first (`-BreakerCrowdProbe=N` 1-200 +
`-BreakerCrowdSkeletal`, 5 s warmup / 10 s sample; this machine,
1920x1080, load=pursuing-unengaged: 100 primitive enemies 5.48 ms
avg / 182 fps, 100 skeletal mannequins 8.35 ms avg / 120 fps, game
thread dominant, GPU idle — the mannequin is AFFORDABLE at target
density); rank colour (ApplyRankPresentation blends each part's
CAPTURED family paint toward Elite gold / ModifierBearing violet one
tick after every chassis pass; Trash/Boss restore the base; blends
O2); selective bars (above-Trash always inside range, trash only
while aimed-at + 0.6 s fade — this lane's ruled rule replaced the UI
lane's recency retune FOR ENEMIES at the rebase; the 1.5 s recency
window now governs only the dummy block — FLAGGED FOR THE OWNER,
two lanes shipped two trash-bar rules in one day); and the wave-mode
enemy pool (a poolable corpse parks on the same fuse-safe corpse
clock, AcquirePooledEnemy revives by EXACT class, ReviveFromPool
carries the full reset checklist — scale/speed/weave/seams/rank/
paint/ward; scope: the three direct wave spawns Skitter/Lattice/
Warden; Skirmisher and Drudge still churn through bespoke helpers;
weak pointers shrug off ResetPlaytestTargets). NOT YET VERIFIED
LIVE: the pool across real wave churn needs a controller (F4) — no
headless path presses F4. Wave-budget generalisation was delivered
as a REPORT, no extension made, by rule.

PREVIOUSLY LANDED (perceptibility A-E + rift data model), still the
live shape of those systems: three synthesized sounds (fire/hit/kill, no files, no
licence — the provenance is Audio/BreakerSoundMath.h); the pooled
ABreakerEffectRenderer (tracer's sibling: strokes/glows/lights on
clip clocks, handles + EndEffect duration-rewrite, anchored beams);
all seven Caster visuals from each ability's own geometry (Rot's rim
at true effective radius rides every zone); Breaker.Tracer.* console
tuning over one live copy the HUD's impact scheduling shares; the
Anchor ruling (weapon lowered but IN FRAME, vitals + resource +
Riftglass drawn, social trim kept); FBreakerRiftDefinition on the
GameInstance (name/line/level authored, ilvl range and monster
multipliers DERIVED, area-level-1 baseline per ruling) with the
Fieldplate plate on travel via MoviePlayer; and the O106 rewrite —
armour rolls Life/Shield archetypes, base stats derived in
BreakerItemBaseStats.h, gear owns MaxShield's base, out-of-combat
recharge in BreakerShieldMath.h fills shield only while heals fill
life only. The sustain asymmetry IS the rule; no exclusivity rule may
be added. Enemy elemental still doesn't exist (measured: 11 Physical
+ 1 TrueDamage authored sites), so the shield-break asymmetry
deliberately waits on bBypassShield.

INSTRUMENT REPAIRS THIS PASS: -BreakerAutoPlay had only ever
suppressed the menu — since the map split every autoplay capture sat
in the empty front end and exited green; it now takes the menu's own
TravelTo. The -BreakerEffectProbe switch + Breaker.EffectProbe
command photograph the effect renderer (glow + Rot rim, alive at
frame 0, gone by frame 2). Menu and CaptureHUD captures were always
genuine (they draw on the front end by design).

THE DEATH RULES ARE IN (O82 amended, O121-O123, HEAD `c3f6760`):
campaign respawn is unlimited from the tileset start (2 s beat, world
survives, F1 stays the dev reset), a death during a live boss
encounter resets it whole (O121's 20-45 s licence recorded at
ResetBossEncounter), and the rift definition carries its tier plus
GetDeathAllowanceReadout (campaign UNLIMITED, endgame count — always
present, only the value moves). The ENDGAME DECREMENT IS PARKED
behind O122's consumable-entry half by rule. Solo-only; a revive is
additive later. BLOCKED ON THE OWNER: the loading-screen WIDGET
(needs Archivo + IBM Plex imported — a download needing owner
permission); the static plate PNG still prints "3 REMAINING", art
that now disagrees with O123 until the widget replaces it.
Fieldplate pack notes: the three plates are the same campaign frame
(anomaly/raid states not actually exported), README margin 96 vs
tokens' 64. Power-band fixtures still carry archetypeless items on
purpose — re-pointing them at Life/Shield is a deliberate future
change, not a side effect.

QUEUED BEHIND THIS (owner-held): fourteen dead world Core Points,
First Contract chain, Swift's missing abilities, i-frame ruling, the
rift instance itself. Plus the standing queue: Slipcut base-kit
collision, Phantom Step text divergence, KINESIS flavor vs rims,
Siege TargetElite reading, RedlineDoctrine save ranks (O62), board-UI
cluster entries for the five new wheels + Travel. Phase D's one-shot
diagnosis (in the session report): three "round landed" signals at
three different times — hit sound at trigger, crosshair at +16 ms,
world spark at flight time — and the dummy itself is inert (all hit
reactions live on ABreakerEnemy). The Fieldplate menu lane is COMPLETE:
all seven pack screens live in-game — Settings (sidebar + panes),
BREAKERS roster, ENLIST A BREAKER, the Forge's verb bench, the
Quartermaster — on the three imported role fonts (Archivo / IBM Plex
Sans / IBM Plex Mono, Assets/fonts -> Scripts/import_fonts.py ->
BreakerBuildRoleFonts). Owed at their sites: roster doctrine title +
LAST AREA (summary fields), create's MODEL/FACE/VOICE (save fields),
Settings' slide/aim hold-or-toggle + controller deadzone (model
fields), insignia/rarity mark textures (import session). The doctrine
display rename the pack proposes (Skimline et al., level-3 wording) is
with the owner; the UI prints the trees' real names and O86's Forge
until ruled.

THE LEDGER LANE (this pass): HEALTH BANDS ARE STATE. HEAD `c486214`,
451 passing / 4 expected red (same roster) / 0 unexpected.
Attributes/BreakerHealthBands.h is the one segment-count source
(IndexOf/SegmentCountFor, pure; every rank returns a real count,
Trash included — O125; the bar draws what it can, the display limit
is never in the state function). TargetBandBroken is the 25th
condition (target block end, mask 25/32): per-target, previous-hit
lifetime — the bit sits on the target's combat component, written at
ReceiveDamage's foot (hunks agreed with lane/dev), read by the next
hit's rider resolution; ClearBandBreakTracking is published and
lane/dev owes the one-line ReviveFromPool call for pooled corpses.
Overpressure re-gated conditional at 16.0 (Siege's comparable).
COLLAPSE DID NOT MOVE: its intended shared-pool More gated on
TargetBandBroken has NO payment lane — standing aggregation never
holds a target bit and the rider lane is Increased-bucket only by
rule — so authoring it is a dead line; the per-hit More lane is
O125's open bullet, with the composition it would open (TV x
Collapse x Overflow = 2.1632 against the 2.197 ceiling, and the
weapon lane's zero-commitment unconditional product falling 1.9825
-> 1.8605). Ability parity measured 0.268x at cap / 0.202x endgame,
unchanged by any of this — neither power-band fixture can afford
Collapse's chain.

THE FIELD LANE (this pass): TWO WRONG SPACES, AND THE BAR LANDS. HEAD
`e8b2349`, 457 passing / 4 expected red (same roster) / 0 unexpected.
O145: FLinearColor is LINEAR and the perceived colour is its sRGB
ENCODING. Every dE76 this lane reported last cycle read the stored
value as though it were already sRGB and inflated all of them — a
Vestige trash ramp measured 44.7 where it delivers 27.7. The error
survived because the figures were quoted ONE PER RANK, which the
delta form cannot have: travel starts from a family base, so it
differs per family by construction, and the per-rank average is
exactly the format that hides it. Scope is now part of every figure.
O146: the same mistake was in the COMPOSITION. The pack's hexes are
display colours; added to a linear value they deliver a travel that
depends on where the family base sits, spread 30.8 dE76 across the
twelve (family, rank) pairs with Champion the worst rank in the game
to read. Encoding the base, adding the authored offset in its own
domain, decoding back: spread 11.1, worst case 38.8, and NOTHING
retuned — the twenty hexes, both rank hues and both blend weights
stand. Champion was never a blend problem. Gain was measured and
rejected (inflates Boss to 91 dE76, washes every swatch pastel).
Known and open: the delta carries magnitude, not hue direction, so a
Vestige Boss dies MAGENTA. DeliveredSeparation is the new test and it
asserts the delivered colour, not the authored table — the old ramp
test would have passed with Champion at 27.8 forever.

THE ENEMY BAR IS FIELD'S NOW (Combat/BreakerEnemyHealthBars.cpp) and
four of its five items landed: bands from
BreakerHealthBands::SegmentCountFor with a PIXEL-keyed draw limit
rather than a rank-keyed one, a hatched shield fill, modifier marks
beyond 15 m instead of prose, and A7 — which was a real defect: the
focus fade held ONE body, so a crosshair sweep hard-cut every bar but
the last. Now per-enemy; that replaced two private HUD members with a
map and is a DECLARED CROSSING to GLASS. A4 needed no code (ARMOR 2
was never implemented). A1's BOSS half is held: three phases at
100/66/33 against eight bands means phases change INSIDE bands, and
the recommendation to LEDGER is six bands plus gates derived as exact
thirds, so a phase boundary IS a band boundary and needs no second
mark layer.

INSTRUMENT GAPS, all three still open and one now blocking. The crowd
probe's grid still sits at 6000 cm against a 2200 cm DetectionRange,
so BreakerEnemy.cpp:507 takes the PATROL branch and the hundred has
never pursued — b414d5c did not move the grid, it added
`load=pursuing-unengaged` to the summary line, so the claim now
prints in the measurement output a script harvests. A GPU column
moving 0.22 ms across a hundred skeletal meshes is still not drawing
them. And the enemy bar cannot be photographed at all: autoplay faces
a berm, the crowd grid is past the 50 m cull, and the capture tour
moves the CAMERA while the cull measures from the PLAYER. The bar
work above shipped unphotographed and says so.

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
- **Parallel lanes run in git worktrees, never in this checkout.** The main
  checkout belongs to the owner: playtests, live edits, whatever the editor
  has open. Each agent lane works in a sibling worktree on its own branch —
  `../riors-edge-lane-ui` on `lane/ui`, `../riors-edge-lane-dev` on
  `lane/dev`, made with `git worktree add ../riors-edge-lane-<name> -b
  lane/<name>`. A lane's cycle ends: fetch, rebase onto `origin/main`, then
  `git push origin lane/<name>:main` — a push main can fast-forward to, or a
  refusal; never force. Stage files by name, never `git add -A`: a file-level
  fence cannot see the other lane's uncommitted work, and the sweep that
  committed another lane's files "as found" is the failure this rule
  replaces. Each worktree builds its own `Binaries` and `Intermediate`; while
  the owner's editor is open, a worktree build passes `-NoHotReloadFromIDE`
  (the lock note above).

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
