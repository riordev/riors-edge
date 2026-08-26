# ORDERS

Every lane reads this at the start of a cycle. It replaces the per-lane messages.
When it changes, it changes here and you get it on your next rebase — so rebase
before you plan, not after you build.

Written by the design seat, not by a lane. **No lane edits this file** — PRESS
publishes it without reading it as instructions to itself. If you disagree with
something in it, say so in your report and it changes here.

---

# PART ONE — THE OWNER HAS RULED

Six decisions came back. Four are settled, one needs a shape it does not have,
one is answered but not in the form the question was asked.

## 1. Swift's starters — NOT Slipcut. A new dash passive, plus Skim

Owner, verbatim: *"it doesnt really matter to me we can start with an enhanced
dash passive which we create + skim."*

So **O176 as written is dead.** The starter pair is not Slipcut + Skim. It is a
new enhanced-dash passive plus Skim, and Slipcut joins Lead, Cadence Break, Hard
Stop and Sightline as unlockables.

**The problem: this game has no passive ability slot.** `EBreakerAbilitySlot` is
`ClassAbilityOne`, `ClassAbilityTwo`, `Ultimate` — all active. Passives in this
project are **tree nodes**: `EBreakerNodeStatTarget::DashCooldown` exists,
`RecentlyDashed` is a build condition, and `Core.*.Afterburn` already keys off it.

Two readings, and I am ruling the cheap one unless the owner overturns it:

- **(a) RULED.** The passive is a **tree node granted at level one**, not a slot
  occupant. Swift starts with Skim in `ClassAbilityOne`, an always-on dash node
  from the tree, and `ClassAbilityTwo` **empty until the first unlock**. That
  slot sitting visibly empty is a feature — it is the first thing the
  quartermaster fills.
- **(b) NOT RULED.** Passives become slottable. That is a system addition — a
  fourth slot kind, a picker that mixes actives and passives, save migration.
  Do not build it.

If the empty second slot reads badly in play, that is the owner's call after
seeing it, not a reason to build (b) now.

**LEDGER owns the node. KIT owns the loadout flip.** Coordinate the landing;
KIT's three enumerated reds clear on it.

## 1b. RULING 1'S CONSEQUENCE — Swift is now the slowest class to fill

Landed and correct (`6568371`). LEDGER also caught a blocker neither I nor KIT
saw: moving Lead off the starter row gives Swift **five** unlockables where every
other class has four, and `AbilityTokenLevels` only had four milestones — so the
fifth ability was unreachable at the level cap. A fifth milestone was added,
`{5, 12, 20, 30, 40}`, and four-unlockable classes truncate at four and never see
it. That is right.

**But it leaves Swift on a different pacing curve from the rest of the game, and
that arrived as arithmetic rather than as a decision.** Measured:

```
  class      starters  unlockables   last ability bought at
  Gunsmith      2           4              level 30
  Tank          2           4              level 30
  Support       2           4              level 30
  Caster        2           4              level 30
  Swift         1           5              level 40
```

Every class ends with six class abilities. Swift starts with **half** what the
others start with — one against two — and finishes **ten levels later**.

That may be exactly right: Swift's identity is movement, Skim plus an always-on
dash passive is a real level-one kit, and a slot you can see and cannot fill yet
is a promise. It is also the class the **owner plays**, which means the vertical
slice will be judged through the sparsest opening and the latest completion in
the game.

**OWNER'S CALL, and it is cheap either way.** Three shapes:

- **(a) Accept it.** Swift is deliberately slower to fill and faster to move. No
  code changes; record the asymmetry in the ruling so it stops being arithmetic.
- **(b) Compress Swift's schedule** so it also completes at 30 — a per-class
  token schedule rather than one shared array. Small, and it makes the array
  class-shaped, which it currently is not.
- **(c) A second Swift starter**, which contradicts ruling 1 and I would not.

### ANSWERED — (b), and the milestone list becomes DERIVED rather than authored

Owner, 2026-08-26: *"i think all classes should finish at the same time."*

Every class completes its kit at the same level. Four do that at 30 today, so
**30 is the completion level** and Swift compresses to meet it rather than four
classes stretching to 40. All `O2 PLACEHOLDER`.

**Do not add a second hand-authored array.** The shipped defect — four milestones
against five purchases, leaving Swift's last ability unreachable — happened
because an authored list and a derived count were two facts that could disagree.
A per-class array is the same defect with more places to hold it.

**Derive the schedule instead:** author the first-token level and the completion
level; derive the milestone count from the class's unlockable count and space
them between. Then a class gaining or losing an unlockable re-spaces itself, and
the disagreement that caused this cannot recur — the count is read from the thing
it counts.

LEDGER: report the spacing shape before authoring magnitudes, per your own
pattern. Even spacing is the obvious candidate and may be wrong — a kit probably
wants its first two unlocks close together and its last two apart, since early
abilities change the game more than late ones. Say which and why.

Re-measure at-cap and parity after it lands, for the same reason the seeded dash
rank gets re-measured: this moves when a Swift character has what.

## 2. Ability sound — the owner will make assets, so do not wait for them

Owner: *"ill make assets for those sounds eventually."*

That answers the fifth-verb question sideways. He is willing to author per-ability
content, so the answer is not "one cue for all twenty-five" forever — but
"eventually" means KIT cannot block on it and the abilities cannot stay silent
until then.

**RULED: the fifth verb ships with a single default cue for every ability, and a
per-ability override that falls back to the default when empty.** One verb, one
default, twenty-five optional slots. The owner fills slots as he makes assets and
nothing is silent in the meantime, nothing is re-plumbed when the assets land.

GLASS owns the verb — `ABreakerSoundDirector`'s fifth. KIT owns nothing here:
`OnAbilityActivated` is already broadcast and already bound by GLASS's HUD, so
the cue is GLASS calling `GetSoundDirector()` in a handler that already runs.

**COUNT CORRECTED 2026-08-26, and the error was mine to catch and I did not.**
This ruling said "twenty-five optional slots", taken from KIT's census — *"all 25
registered abilities activate with real logic"*. Measured against the tree:
`BreakerAbilityDefinition.cpp` assigns **35 unique `AbilityId`s, seven per class
across all five**, which is 2 starters + 4 unlockables + 1 ultimate for four
classes and 1 + 5 + 1 for Swift. Uniform, reproducible, and ten more than the
figure I built a ruling on.

I cannot reconstruct 25 from the current tree under any reading — not by
excluding ultimates (30), not by excluding Swift's three new ones (32). **KIT:
say what population "25" measured, or retire the number.** A count that cannot be
reproduced is not a smaller count, it is an unknown one.

The ruling itself does not change — one slot per ability, whatever the ability
count is. **Do not hardcode a count anywhere; derive it**, for the same reason
the token milestones are now derived. This is the second ruling this week that a
hardcoded population would have broken.

GLASS owns the verb — `ABreakerSoundDirector`'s fifth. KIT owns nothing here:
`OnAbilityActivated` is already broadcast and already bound by GLASS's HUD, so
the cue is GLASS calling `GetSoundDirector()` in a handler that already runs.

## 3. Fernhall — see Part Two. It is the whole world now

## 4. Density — optimise, do not lower the target

Owner: *"we just need to optimize they should have enemy variety ill expand on
this later."*

**RULED: the 50–100 concurrent target stands.** 100 engaged primitives at 34.16 ms
is a problem to solve, not a number to design around. Enemy variety is a
requirement, not a nice-to-have — the owner will expand on what variety means.

This makes the sweep urgent rather than interesting: engaged at N = 25 / 50 / 75 /
100 gives slope and intercept, and the intercept is what tells us whether the
cost is per-body or fixed. Optimising without it is guessing.

## 5. The at-cap band — tune content UP

Owner: *"we will tune power up."*

**RULED: the authored 8–10× stands and content rises to meet it.** Measured 6.54×.
Do not widen the assertion, do not move the band down. The pin stays until the
measurement reaches the band.

## 6. Prolific — the breach is accepted

Owner: *"this is fine i dont mind over the mark damage most of the time."*

**RULED: 22.64× against the authored 12–20× endgame band is accepted.** Prolific's
magnitude does not come down. The band's upper edge is a target, not a ceiling,
and a rewrite stacking past it is the intended feel.

The pin closes. Record in the ruling that the band is now described as
*where most builds land*, not *where every build must land* — because an
assertion that fires on the intended outcome is an assertion pointed the wrong
way.

---

# PART ONE-B — THE OWNER PLAYED IT, 2026-08-26

First hands-on since the lanes started. Two notes, both rulings, plus what the
screenshot shows that he did not have to say.

## RULED: the death sound goes

Owner: *"the death sound needs to go."* **GLASS: identify it and remove it.**

Do not replace it with a quieter one without asking. "Needs to go" is a verdict
on the sound existing at that moment, not on its mix — a redeploy is already
carrying a full-screen banner and a respawn, and a sting on top of that may be
the thing that is wrong rather than the sting's character. Report which verb it
was and what the moment sounds like without it.

## The feedback verdict, and the screenshot names three things he did not

Owner: *"the feedback needs to be better."* That is not actionable as written, so
here is what the capture shows. **Owner: confirm or correct these — a wrong
reading of a one-line verdict is worse than no reading.**

**1. Enemy labels collide, and it is the worst thing in the frame.** Two `CHASE`
labels overlap each other at left; `WARDED | VOLATILE` runs straight through
`CLOSING` in the centre. Four ranked enemies at mid distance produce unreadable
text soup. **A9 — modifier marks going distance-progressive — is the fix and it
is still open in FIELD's queue.** This is now the highest-value item in it: at
four enemies it is noise, and the design target is fifty to a hundred.

**2. Every bar reads as one flat red stripe.** No rank separates at that
distance, no segmentation is visible, and the only bar that stands out does so
because it has a cyan shield above it. Whether that is the build predating
`e2bdeae` or the bands genuinely not reading at range, **FIELD should photograph
it at 12 m and 35 m before touching anything** — the same rule as the tint ramp:
the capture wins over the swatch.

**3. `REDEPLOYING — FROM THE TILESET START` is enormous and red, across the
middle of the screen.** It is competing with the crosshair and with every enemy
label behind it. GLASS: it is a status line, not an alarm — and it appears at
exactly the moment the death sound was ruled out of, which suggests the whole
redeploy beat is over-produced rather than under-produced.

**4. THE RANK GLYPHS WERE NEVER BUILT, AND THAT IS MY OMISSION.** Owner:
*"the health bars also dont have icons nor look different am i missing
something?"* He is not missing anything. Rank differentiation on the bar today
is **a gold border and the word ELITE or BOSS** — nothing else.

The readability pack authored a glyph per rank: one diamond for trash, a hollow
square for elite, a double diamond for champion, a filled double for boss, drawn
at the bar's left cap. I reviewed that pack, routed the tint ramp, the segment
counts and the modifier marks — and **never routed the glyphs to anyone**. They
are not late; they were never assigned.

**FIELD: the rank glyph is yours, and it belongs beside A9 rather than behind
it.** The pack's own crowd study leans on it — at over 35 m a champion and an
elite are distinguished by the double diamond alone, because the gold edge and
the rank word are both at 0.55 scale and unreadable by then. A word that cannot
be read at the distance rank matters most is not a rank carrier.

Take these four as the concrete reading of "feedback needs to be better" unless
the owner says otherwise.

---

## A NOTE ON HOW THIS SECTION CAME BACK

PRESS caught this on its first cycle and the loss was the seat's, not a lane's.

Writing the PRESS charter, the seat built its edit on a copy of this file it had
reset with `git checkout --` several times to unstick merges — so the base was
main's published version, not the seat's own two newer ones. Part One-B and the
rank-glyph item vanished from the tip. **PRESS read the diff before committing,
saw content disappear, and said so in the commit message rather than publishing
it quietly.** That is precisely what the read-the-diff rule was written for, and
it worked on the first opportunity.

The seat's defect, named so it is not repeated: **editing a file without
re-reading its current state.** The same rule every lane is held to — measure the
base, do not assume it — applied to a document instead of to code.

---

## THE DEATH BEAT — GLASS asked the wrong version of a right question

`972373d` is good work and the finding inside it is the important part: **there
was never a death sound to delete.** Every audio site is
`ABreakerSoundDirector`, and the verb that fired when the player died was
`PlayTakeHit`, because the fatal hit is a hit. Deleting the call would have taken
being-hit out of the entire game to silence one moment of it. One condition,
`!Result.bKilled`, is the correct shape and finding that before cutting is what
the ruling was owed.

GLASS asks: *is silence the intended end state, or did "needs to go" mean
different rather than absent?*

**That is not quite the question, because two things were subtracted from one
moment in one commit.** The sting went, and the banner went from Harm red at
16px through dead centre to Cyan, smaller, with its rule half dropped to caption
and moved below the crosshair. Each is defensible on its own. Together they are
two quieting edits to the same frame with no look between them, and the owner's
complaint was that feedback was *not good enough* — over-correcting it is the
one failure mode nobody would notice until he died and felt nothing happened.

**So the question for the owner is not "is silence right".** It is: **die once
and say whether the moment now reads at all.** Three outcomes and they want
different fixes:

- *Reads fine* — silence stands, no sixth verb, and the beat is done.
- *Reads weak* — the banner went too far and comes back partway. Cheap, GLASS's,
  no ruling needed beyond this one.
- *Reads absent* — a death cue is wanted after all, and per GLASS's own rule
  that is a **sixth verb needing its own ruling**, not an inference.

GLASS was right to refuse to add one on inference. Hold there.

**One note for the next time two changes land on one moment:** they should land
in two commits, or the report should say which one is doing the work. This one
is recoverable because both edits are small and reversible; a bigger pair would
not be.

---

# PART ONE-C — THE SWEEP LANDED, AND IT MOVES THE TARGET

`d43bbb7`. **Every figure reproduces.** The seat refit the four points
independently: the quadratic to six digits, both N² shares exactly, the fps
lines, and the counterfactual. The linear model really does want a **negative
6.93 ms intercept** — not a frame — and its residuals run `+ - - +`, which is
curvature seen through a straight line. An R² of 0.98 would have passed
unexamined, and that is the finding inside the finding.

```
  t = 1.15 + 0.0864 N + 0.002584 N²      game thread, ms, R² 0.9997
  N² share:  33% at N=25    73% at N=100
  60 fps at N=63     120 fps at N=39
```

## The cost is the crowd touching itself, and that changes what to optimise

FIELD's elimination holds. Patrol runs the same iterator and the same 40 m
ground trace and is **linear** at 0.036 ms/body, so neither the scan nor the
trace is the term. The only body-body interaction in the tick is the swept
`AddActorWorldOffset`, and the two loads differ in exactly one way that reaches
it: patrol bodies sit on an 800 cm grid and never touch; engaged bodies converge
to 1–5 cm and sweep against every neighbour.

**Nothing in `Combat/` holds spacing.** That is not a performance bug with a
performance fix — it is a missing behaviour, and the frame cost is a symptom.

## RULED: separation is the work, and it buys two things at once

Drop the N² and the same fit puts **N=200 at 18.43 ms** — two hundred bodies
cheaper than a hundred costs today. No per-body saving available is within an
order of that, so micro-optimisation is the wrong lane of attack and the sweep
was worth running for exactly that reason.

**And separation is not only a frame fix.** A hundred enemies stacking into one
body at 1 cm is a readability failure before it is a cost: it is the same
complaint the owner already made about labels colliding, one layer down. Bodies
that do not separate produce a blob that no bar, glyph or tint ramp can
disambiguate, because there is nothing spatially distinct to label. **One change
fixes the frame and the read.** That is why it outranks A9 in FIELD's queue as of
now — A9 makes labels legible over a crowd that separation makes legible at all.

FIELD: report the separation shape before authoring numbers, as every lane now
does. Spacing radius, whether it is steering or resolution, and what it costs —
because a separation pass is itself a body-body interaction and could reintroduce
the term it removes.

## The owner's ruling 4 is ACHIEVABLE, and that is news

*"We just need to optimize"* with the 50–100 target standing. The sweep says the
top of that range is not reachable **today** — N=63 at 60 fps — and the
counterfactual says it is comfortably reachable **with separation**. The ruling
stands and the path to it is now known rather than hoped.

## The scope caveat, restated because N=63 will get quoted

`engaged` is convergence and attack. **No hit reactions, no damage numbers, no
flashes, no death effects, and the player never fires.** N=63 is a ceiling with
half a fight missing; the real number is lower and nobody knows by how much.
Whoever quotes 63 without that sentence is repeating "affordable at 100" with a
different number.

The other half needs a damage entry point in `Combat/` — FIELD's own file — and
GROUND owns the probe. That handoff is the two lanes' to arrange.

---

# PART ONE-D — THE RIDER-MORE ANSWER, AND COLLAPSE MOVES

LEDGER's report answers a question the seat framed too coarsely. Asked: *can the
rider path carry a More bucket?* The honest answer splits in two and only the
report found the seam.

**The strongest-three SORT cannot take a hit-time member, and that is forever.**
Membership is decided at standing time; a target-gated candidate makes correct
membership differ per hit, and the request carries two floats
(`SourceIncreasedPercent`, `SourceMoreProduct`), not a member list — a specific
member cannot be removed from a product without knowing the members. A rocket in
flight snapshots its split at fire time and would be wrong regardless. And the
character sheet's "N / 3 MORE" stops meaning anything if the three change per
target. Three independent reasons, any one sufficient.

**But the CEILING already operates at hit time, and that is the lawful opening.**
The outgoing-modifier chain already multiplies a More into the request at
submission under a clamp, and the DoT tick path clamps the same way. O34 already
says temporary ability windows ARE Mores and count inside the budget. So
**hit-time Mores spend HEADROOM, not slots** — one clamp at the recomposition
site, loud when it bites.

Every figure in that report reproduces on an independent refit.

## RULED: Collapse moves to the shared pool, gated, under the ceiling clamp

The seat put that ×1.30 on the weapon lane because no condition existed, then
told LEDGER to move it once one did, then was corrected because a rider could
only ever be an Increased. All three positions were partly wrong. The mechanism
is a hit-time More under the existing ceiling law, it has precedent in two
shipped paths, and Collapse is what it was written for.

## The asymmetry the report measured points somewhere it did not say

```
  standing product   residual headroom   a x1.30 rider delivers
  ability   1.664         x1.3203              x1.3000   (100% of authored)
  weapon    1.9349        x1.1355              x1.1355   ( 45% of authored)
```

**A hit-time More is structurally an ability-lane buff.** Weapon builds have
already spent their headroom; ability builds have not, so the same authored line
pays them in full and pays a weapon build less than half. Ability parity is 0.27,
the worst number in the project — and this delivers against it as a property of
the ceiling arithmetic rather than as a tuning choice.

That is the exact inverse of what the seat did originally. Putting Collapse on
the weapon lane hurt parity; making it a hit-time rider helps it, for free.

## And it saturates, so ONE is the number

At ×1.30 an ability build lands at **98.5% of the ceiling**. The headroom that
makes riders valuable to ability builds is nearly exhausted by a single one — a
second target-gated More would deliver almost nothing and would read to the
player as a line that does not work.

**RULED: at most one target-gated More exists in the game, and Collapse is it.**
A second is not a tuning question, it is a request to revisit the `1.30³`
ceiling, which is a different and much larger ruling. Record that at the clamp so
the next author meets it there rather than after shipping a dead line.

**LEDGER: re-measure ability parity after it lands and report it.** The prediction
is that it improves; a prediction stated before the measurement is worth more
than one fitted after.

## O138 and O139 — both right, and one detail worth keeping

The convex `t^1.2` spacing was chosen because the retired `{5,12,20,30}` already
had convex gaps (7/8/10) and the derivation **reproduces it bit-identically** —
where even spacing would have silently retuned four classes as a side effect of
fixing Swift. Choosing the derivation that leaves the baseline untouched over the
one that is tidiest is the whole discipline in one decision.

Longstride took the distance reading, per the cheap-case rule, and stayed
convertible to cooldown in one commit. `COST 0` making the no-refund property
**arithmetic rather than a flag** is the right kind of small.

---

# PART ONE-E — THE PLAYER STARTS IN THE ANCHOR. THE GYM IS TEST-ONLY

Owner, 2026-08-26: *"i dont really want to go into the gym aside from for testing
the player should spawn in the anchor and move from there honestly."*

**RULED.** The gym leaves the play loop. It stays a test bench, reachable
deliberately, never arrived at.

## Why he keeps landing there, which is not his fault

Two defaults both lead to the gym, and one of them is invisible.

`EditorStartupMap = /Game/FirstPerson/Lvl_FirstPerson` — the old template map.
`IsGymMapName` returns true for **anything that is not FrontEnd, Anchor or
Fernhall**, so the template map builds the full gym. Play In Editor therefore
lands in the gym by way of a map nobody named. That fall-through is load-bearing
by design (`BreakerGameInstance.cpp:54` says so) and it is why the loop has
always "worked in PIE".

And `-BreakerAutoPlay` is documented as dropping *"straight into the gym"*.

**`DefaultEngine.ini` already named the condition for changing this**, at the
comment above the line: *"it should be a decision with a playtest behind it, not
a config cleanup."* There is now a playtest behind it. The condition the file set
is met, which is the honest reason this is a ruling rather than a tidy-up.

## What changes

- **`EditorStartupMap` points at `Lvl_Anchor`.** PIE starts in the hub.
- **`-BreakerAutoPlay` lands in the Anchor**, not the gym. Its purpose — skip the
  title menu so a smoke run exercises real systems — is unchanged; the map it
  exercises changes.
- **The gym keeps a deliberate entrance and loses its accidental one.** It stays
  in the travel registry (it is how you get *out* of it) and keeps whatever
  explicit flag testing wants. Nothing should ARRIVE there without asking.

GROUND owns the game-mode branch and the config. **Report before changing PIE's
map**: the file warns this is "an untested change to the only loop anyone
playtests in", and that warning survives the ruling — what changes is that the
change is now wanted, not that it is free. Say what breaks.

## And it collides with the rift placeholder — deliberately raised, not ruled

**If the player should not be in the gym, then a rift whose interior IS the gym
puts them there during normal play.** That placeholder was ruled when the gym was
still where everyone lived. It is now the one remaining path that arrives at a
test bench, and it arrives at the most fiction-breaking moment available: you
step through a tear in the world and land in a room with target dummies and a
safe pad.

The seat's proposal, for GROUND to cost and the owner to overturn:

**Make the rift interior an instance of the YARD'S geometry, populated.**

- The yard exists, has a validated cover grammar, and currently spawns **zero
  enemies**. The wave system exists and is unused there.
- Same tileset, different instance, is what an instanced rift *is* — it is how
  the games this is shaped after do it, and reusing authored space is the whole
  reason a tileset is worth building.
- It kills two things at once: the gym leaves the loop entirely, and Fernhall
  gets something to fight in it — which is also the only way the density work
  gets exercised anywhere real.
- Entering the yard's geometry *from* the yard is not odd once it is an instance.
  It is the same ground, a different run.

This is not ruled. **GROUND: cost it against "leave the gym as interior until a
real room exists" and report both.** If the instance route is more than a cycle,
the placeholder stands and the collision is recorded rather than fixed — a known
contradiction is cheaper than a rushed room.

---

# PART ONE-F — THE BAR PROBE CORRECTS THE SEAT, TWICE

`d10a023`. Part One-B told FIELD to photograph the bar before touching it.
Nothing in the project could take that frame, so FIELD built the probe — four
rank rows by five health columns at 12 m and 35 m, ticks frozen so the tableau
does not drift, F3 suppressed because the overlay is not the shipping read.

**Both of the seat's readings of the owner's screenshot were wrong.** Each was a
plausible inference from a still, and each named the wrong cause.

## 1. The label collision is NOT the bar's — A9 was never the fix

`bDiagnosticsVisible` defaults on, and the F3 overlay draws a **second enemy-label
pass** at `BreakerPlaytestHUD.cpp:693`: every enemy within 25 m,
`GetEnemyStateLabel` in orange, **no focus gate, no occlusion suppression, no
cap**. PATROL printed fifteen times over twenty bodies. Verified — the loop is
there, ungated, and a dummy pass sits above it.

The seat read the collision as bar labels and promoted A9 to the top of FIELD's
queue on that basis. **A9 disciplined the bar TU's label block; the collision is
in a different drawer entirely.** A9 remains worth doing at density; it was never
the answer to what the owner saw.

**The fix is GLASS's** — `UI/` — and the root question is not the labels but the
default: a debug overlay that ships visible is a debug overlay that is not a
debug overlay. FIELD reports `Playtest/` as unassigned; ORDERS assigns it to
GROUND, so whichever of you reads this first, say which is right rather than
both assuming.

## 2. "Every bar reads as one flat red stripe" is bar WIDTH, not rank or bands

`HudEnemyBarWidth` is a fixed 180 px scaled only by a 1.0 → 0.55 lerp across the
whole 50 m, so the bar is **very nearly distance-invariant while the body shrinks
with 1/d**:

```
  12 m   167 px bar   against a ~72 px silhouette   2.3x
  35 m   126 px bar   against a ~25 px silhouette   5.0x
```

Adjacent bars butt together into continuous horizontal stripes. **Rank and
segmentation both draw correctly** — the seat guessed a stale build or bands not
reading at range, and it was neither. The bar is simply too wide for the thing it
describes, and gets worse with distance because it barely shrinks.

It is in FIELD's own TU and FIELD has it next. That is the right order: it is the
cause, A9 is a symptom at density, and the glyphs are a separate gap.

## 3. O129's ramp READS, and that closes a question

Left to right the bodies run grey-violet to red-maroon and the health axis is
legible at 12 m without reference. **The delta form survives contact with a
frame**, which no swatch measurement could establish and two rounds of dE76
argument did not settle.

The terminal-hue question stays open on its own terms — a Boss dying magenta is
still a look question — but the ramp itself is answered and should stop being
re-litigated.

## The rule this vindicates

*Capture wins over the swatch* was written for the lanes. It just won against the
seat, twice, in one commit — and both wrong readings were confident inferences
from a screenshot the seat could not have read correctly, because the information
needed was not in it. **A still frame of a live system is evidence about the
frame, not about the system.**

---

# PART ONE-G — PARITY HALVED, AND THE PIN THAT SHOULD HAVE SAID SO IS STALE

**The recon was right and the seat was wrong to hedge it.** Told that parity is
0.27 against a pinned 0.647, the seat suggested the two might measure different
things. They do not. From the tree as it stands:

```
  the pin's own PROSE       "parity measures 0.641x at the cap ... 0.38x at endgame"
  STATE.md, freshly run     at cap 0.27  **OUT**      endgame 0.2
```

**Same measurement. Parity fell from 0.641 to 0.27 at the cap, and 0.38 to 0.2 at
endgame.** More than half, and against a ruled band of 0.85–1.15 it was already
failing at the higher figure.

## The reason nobody caught it is a defect in the instrument layer

The pin's **measurement** is live and its **explanation** is frozen. `status.py`
recomputes 0.27 and flags OUT every run — that part works. But the narrative
inside the pin still tells the story of 0.641, and the narrative is what a reader
uses to decide whether a number matters. Anyone who opened that pin for context
got a confident account of a state that no longer holds.

**A pin whose number updates and whose reason does not is worse than no pin**,
because it launders a stale conclusion through a fresh measurement. It is the
same shape this project keeps finding — a justification outliving its cause —
sitting inside the instrument built to find that shape.

**LEDGER owns this pin. Two things:** rewrite the narrative to the measurement it
now carries, and say in the pin how a reader can tell prose from figure next time.

## What halved it is findable and should be found before anything else in this area

Between the pin's 0.641 and today's 0.27 lies about a week of commits. That is a
bisect, not an investigation. **LEDGER: find the edit and report it before
proposing any parity work.** A remedy authored against an unknown cause is how
0.27 becomes 0.15.

Note what it is NOT: `51b35cc` landed Collapse and re-measured parity as
**0.268 UNCHANGED**, so the hit-time More neither caused nor fixed this.

## And the seat's prediction was wrong, measured

Part One-D predicted parity would improve when Collapse moved. It did not, and
LEDGER's explanation is the right one: the power-band fixtures run 65-point
budgets and cannot afford Collapse's chain, so the fixture never buys the thing
that changed.

The ruling's benefit is real and sits where the ruling put it — a build that buys
Collapse now gets a full ×1.30 on its ability lane where the weapon-lane shape
gave it nothing there. But the metric could not have moved, and the seat should
have known that before predicting it.

**The shape, named:** *predicting a measurement will move without checking
whether that measurement's fixture exercises the change.* It is the mirror of an
instrument whose scope is narrower than its name — here the scope was narrower
than the prediction. Before any lane predicts a pinned number will move, check
that the fixture reaches the edit.

---

# PART ONE-H — GROUND'S FOUR REPORTS, ALL FOUR ACCEPTED

`b8f66b2`, no code. Four answers the seat asked for before building, and one of
them catches a defect in the seat's own ruling.

## 1. The Anchor ruling would have broken FIELD. ACCEPTED WITH ITS CONDITION

Verified in the tree: the Fernhall branch returns at **line 396**;
`-BreakerCrowdProbe` is at **line 486**. Five harness entry points sit in the
gym-only tail, ninety lines past the last map branch.

**Autoplay landing in the Anchor does not move them — it stops reaching them.**
The density instrument would have become unreachable for the lane the seat had
just told to prioritise density. The blocks are gym-*located*, not
gym-*dependent*.

**RULED: the harness moves above the map branches IN THE SAME COMMIT as the
config change. Doing the config alone is forbidden** — not discouraged, forbidden,
because it is silently correct at compile time and breaks an instrument nobody
would think to re-run.

Part One-E stands as amended by this.

## 2. Rift interior: the yard instance is the route, and it goes AFTER the close verb

The route is cheaper than the seat guessed and for a reason worth keeping:
**`StartNextWave` spawns its ring around the PLAYER, not at the authored arena**,
so the wave system has no dependency on gym geometry at all. `Lvl_Fernhall` with
`PendingRift` set already IS a different instance of the same tileset. No new map.

**But GROUND recommends taking it after the close-rift seam, and it is right:**
*"a populated rift with no ending is a louder version of the problem, not a fix
for it."* That is an argument against its own more interesting work, made on the
merits.

**RULED: close verb first, yard instance second.** The gym stays the interior in
the meantime and the contradiction with Part One-E is recorded rather than
rushed, exactly as Part Three-E allowed for.

## 3. The close-rift seam: RAISE AND CONSUME. Three commits, one interface

**ACCEPTED, and the precedent argument is decisive.** The rift door does not
travel — it raises, and the game mode owns what travel means. Entry and exit
should be the same shape reversed. A direct write would need `Combat/` to include
`Game/` and know rift state, which is precisely the coupling the door was built
to avoid.

So: **FIELD's terminator raises. GROUND consumes and owns completion. LEDGER pays
out.** Three commits, one interface, no lane inside another's file.

## 4. `Playtest/` is GROUND's. FIELD misread it, and the confusion is structural

**Settled: ORDERS is right.** And the reason for the mistake is worth recording,
because it will recur: **`UI/BreakerPlaytestHUD.cpp` is not in `Playtest/`.** The
directory belongs to GROUND; the ungated second label pass and the diagnostics
default live in GLASS's file wearing a name that reads like GROUND's.

The directory is GROUND's. **The defect is GLASS's.**

## And GROUND found its own instrument comparing a file to itself

*"Three ORDERS revisions landed while my check said unchanged — the check compared
the blob at HEAD against origin/main, and the worktree kept advancing to match,
so it was comparing a file against itself."*

That is the self-referential shape `status.py` warns about, found by a lane in
its own tooling, and it explains why GROUND appeared to be working without
orders. Fixed by reading the diff from the last revision acted on.

**Worth every lane's attention: if you have a check that tells you whether ORDERS
changed, make sure it is not comparing the file to a copy of itself.** A check
that cannot report a difference is not a check that reports no difference.

---

# PART ONE-I — BAR WIDTH FIRST. FIELD'S RE-ORDER IS RULED IN

FIELD asked whether to take bar width before the rank glyphs, against the order
Part One-B gave. **Take the width first. The re-order is right and the reasoning
is the reasoning.**

A glyph's size, its position on the cap, and the distance at which it stops
resolving are all **functions of the bar's proportions**. Authoring them against
a bar that is 2.3× its body's width at 12 m and 5× at 35 m — and that is about to
stop being — is authoring them twice, and the second authoring is the one that
gets rushed because it feels like rework rather than work.

This is the third time tonight the same shape has been the right call: land the
zone refactor while it is provably a no-op; take the close verb before populating
the rift; fix the proportions before decorating them. **Change the shape, then
decorate the shape.** Worth naming, because in all three cases the decorating
work was the more visible one and the discipline was to do the invisible one
first.

The glyphs stay next after the width, and the seat's Part One-B item is amended
rather than withdrawn: they were never late, they were never routed, and they are
still the only rank carrier that survives past 35 m.

## And FIELD asked rather than assumed, which is the behaviour to keep

The re-order was clearly correct and FIELD had the evidence in hand — its own
probe produced the numbers. It still put the call back to the seat because the
instruction was the seat's to change.

Contrast with the weapon-feel cycle, where a question carrying a self-executing
default was answered by the lane that asked it, in the same commit, and deleted.
That worked because the default was right. **This is the shape that works when
the default is wrong**, and the difference costs one cycle.

## Two verifications FIELD volunteered, both confirmed

Neither was asked for, and both are the kind that matter more than the tests.

**The no-rider path is bit-identical.** Everything new sits inside
`if (bRiderMoreFired)`; the early return at `:301` fires before any of it; the
final expression is the original untouched. That is the property that makes O141
safe for **every existing damage event**, which is a stronger statement than any
test in the commit makes.

**Enemies cannot reach the rider path at the code, not merely in principle.**
`ApplyTargetConditionRiders` early-returns at `if (!Progression) return;`
(`:255`), and `ABreakerEnemy` carries no progression component at all — the
header has none. Nothing in `Combat/` feels O141.

## The rebase habit is better than the seat's was

FIELD rebased onto LEDGER's push and **verified the combined tip before
publishing**, rather than assuming no interaction. That is exactly what would
have caught the stale-binary short suite earlier in the session, and it is worth
copying: a green suite on your own tip is evidence about your tip, not about the
one you are pushing onto.

---

# PART ONE-J — THE HALVING IS FOUND, AND THE DECOMPOSITION STOPS ONE STEP SHORT

`2dc2e8c` answers Part One-G and the answer is better than a bisect. **No rogue
edit exists.** The decline was announced at every step, in the commit bodies,
stated each time: `0.64 → 0.61 → 0.56 → 0.39` across the atlas pairs and the
no-shared-hub retarget, then `0.39 → 0.27` at `983b925`.

**So parity did not fall silently. It fell in public, one announced step at a
time, and nobody summed the announcements.** That is a different failure from the
one the seat assumed and a more uncomfortable one: every individual disclosure
was honest, and the aggregate went unread because nothing was responsible for
aggregating it.

The pin fix is the right general remedy: both narratives now carry **the rule
instead of a number** — *the live figure lives only in the emit; a number typed
into pin prose is frozen at writing and is history, dated.*

## The decomposition is arithmetically right and does not reach 0.27

Stated: the flat ratio is byte-identical across the halving at **0.867**, and the
increased ratio alone collapsed **0.746 → 0.461**. Multiplying:

```
  0.867 x 0.746 = 0.6468     ~ the 0.641 the pin recorded BEFORE      ok
  0.867 x 0.461 = 0.3997     ~ the 0.39 step, NOT the 0.27 reported
```

**The decomposition explains the fall to 0.39 and stops there.** The last step —
`0.39 → 0.27` at `983b925`, the fixture rewrite — is the one it does not cover,
and it is the step whose cause the same commit calls structural.

Two readings, and LEDGER should say which:

- **The 0.461 is measured at the 0.39 point**, not at the tip. Then the sentence
  "byte-identical across the halving" is scoped to the part before the fixture
  rewrite and reads as covering all of it.
- **Or the flat ratio is not byte-identical through the last step**, in which case
  `0.27 / 0.867 = 0.311` is where the increased ratio actually sits and 0.461 is
  stale.

Either way the figures are right and the **coverage claim is not** — a scoped
result presented as total, in the commit that fixes a stale-narrative defect.
That is the shape this project finds most often, and it is worth noticing that
it survives inside a fix for its own cousin.

## What this changes about the number itself

The two halves are not the same kind of problem and should stop being one figure
in conversation.

**The flat half (0.867) is real content.** The ability lane has no flat line at
all — O54 names three Increased pools and is silent on flat. That is a genuine
gap and no fixture change touches it.

**The increased half is substantially the FIXTURE.** Both power-band fixtures
share one weapon-leaning tree spend; the atlas hands it eight +15% weapon travel
picks; the ability wheels (ARC, RESERVOIR) go unbought by either; and the parity
test's ability loadout **swaps gear, not tree**. So the measurement asks *what
does an ability loadout get from a weapon build's tree* — a real number, and not
the question the 0.85–1.15 band was written about.

**You cannot tune against a number that is answering a different question.** Of
LEDGER's three honest routes, the **owner-ruled ability-built tree fixture** is
the one to take first — not because the content gap is imaginary, but because
until the fixture buys an ability build, nobody can tell how much of 0.27 is
content and how much is the question.

**LEDGER: report what an ability-built fixture would measure, before authoring
it.** If parity against an ability-built tree is still far below band, the content
gap is the whole story and the other two routes get their turn. If it is close,
the band has been failing a measurement rather than the game.

---

# PART ONE-K — THE SECOND PLAYTEST: THREE THINGS, NONE OF THEM OWNED

Owner, 2026-08-26, playing the tip: *"the death sound is still there but your
model looks the same / all of the enemies are just basic fucking human bodies as
well / this damage issue is also caused by the affixes not being updated dont you
think?"*

All three check out. None of the three currently has a lane.

## 1. There are TWO death sounds and only one was ruled

`BreakerPlaytestHUD.cpp:2794` carries GLASS's fix — `if (!Result.bKilled && ...)`
guards `PlayTakeHit`, so the **player's** death is silent and the ruling landed.

**`PlayKill()` at `:2475` and `:2489` fires when the player kills something**, and
nothing has ever ruled on it. GLASS answered "the death sound" as the player's;
the owner may have meant the noise an enemy makes dying — which he hears far more
often, and which is still there.

**ANSWERED 2026-08-26: NO DEATH SOUND FOR NOW.** The owner: *"no death sound for
now."* Both of them go — the player's already did, and `PlayKill` follows.

**GLASS: a kill falls through to `PlayHitConfirm`, it does not go silent.** The
site is `if (bKill) Sound->PlayKill(); else Sound->PlayHitConfirm();` at `:2475`
and `:2489`, so deleting the kill branch has two readings and only one is the
ruling:

- **RULED — the kill plays the hit-confirm.** No death *sting*; the shot still
  confirms it connected. That is what "no death sound" means and it costs
  nothing.
- **NOT RULED — the kill plays nothing.** That makes the **last shot on an enemy
  silent**, which removes the feedback that the shot landed at all. Losing hit
  confirmation is a worse defect than a bad sting, and it is not what was asked
  for.

Take the first. If the fall-through reads oddly in play — a kill sounding
identical to a graze — say so and it becomes a ruling about what a kill should
sound like, which is a different question from whether it should have a sting.

## 2. There is exactly ONE model in this project, and nobody owns that

`SKM_Manny_Simple`, four references, and **only two files in the entire project
set a skeletal mesh at all**. The player is the mannequin. Every enemy that is
not a primitive shape is the same mannequin.

This is not late work — **enemy silhouettes have never been assigned to any
lane.** The readability pack was about *reading* enemies (bars, tint ramps,
glyphs) and never touched body. Same shape as the rank glyphs, and larger.

**And it explains a load the colour system should not be carrying.** O24 spends
family colour distinguishing Vestige from Altered from Lattice — and FIELD has
spent two cycles on ΔE separation between them — **because they are the same
body.** Silhouette is carrying nothing, so colour carries everything. Give the
families different shapes and half the colour problem dissolves rather than being
optimised.

**RULED: FIELD owns which mesh an enemy uses** — the actors are its files. The
**assets** are a content question with a precedent the owner already approved:
GROUND pulled CC0 kit assets for the yard, vendored with a licence note in the
same commit. The same route is open for CC0 character and creature meshes.

**ANSWERED 2026-08-26: YES, and the phrasing carries a condition.** The owner:
*"yes on cc enemy meshes for the time being."*

The three zone-kit conditions apply unchanged — licence note in the same commit
as the assets, pull for the families that exist rather than a library, and report
which silhouettes became distinguishable and how.

**"For the time being" adds a fourth, and it is the one that costs money if
missed: the family-to-mesh mapping is DATA, not code.** These meshes are
explicitly placeholders. If FIELD bakes a mesh choice into an actor's
constructor, or branches behaviour on which mesh a family has, replacing them
later stops being an asset swap and becomes a refactor. One table, one lookup,
and swapping a family's silhouette is editing a row.

The test for whether it is done right: **replacing every mesh should be a
content change with no C++ diff.**

**FIELD: report what swapping a family's mesh actually costs** before anything is
pulled. Three families, one mannequin today, and the crowd sweep says the cost is
crowd collision rather than per-body — so a different mesh may be nearly free, or
may not be. Measure it.

## 3. The owner's affix read is right, and it is half the problem

*"this damage issue is also caused by the affixes not being updated"* — yes, and
the pool is **28 lines against a planned 56**. Half the variety that was designed
is unauthored. That is a real cause of builds feeling alike and it is squarely
where he says it is.

**It is not the whole cause, and the other half is structural.** Measured across
all 99 damage-bearing node effects:

```
  target            Flat  Increased   More
  Damage               0         53      2
  WeaponDamage         0         16      3
  AbilityDamage        0         15      1
  MeleeDamage          0          1      0
  DamageOverTime       0          1      2
  CriticalDamage       5          0      0
  TOTAL                5         86      8
```

**Zero flat damage lines in the tree.** The five flats sit on CriticalDamage,
which is a multiplier.

And the affix layer does not fill it either: `AddedDamage` is authored five times
and reaches the damage path as **`AddedDamagePercent`**, folded into
`TotalIncreasedDamagePercent` under `Rules.bAddedDamageAlsoIncreased`
(`BreakerEquipmentComponent.cpp:805`). It is stored in an array named
`FlatByTarget` and expressed as a percent.

**So the aggregation law's `sum(Flat)` term is structurally always zero.** Every
build in this game is `Base x (1 + Increased) x More` — nothing changes the SHAPE
of damage, only its size. That is one explanation for three separate open
problems: variance at 6.01 against a band of 8–10 because all builds are one
build at different scales; the 87th Increased line being worth exactly what the
first was, since the bucket is additive; and parity's flat half sitting at 0.867
*structurally*, because the ability lane has no flat line for the reason that
nothing does.

**Finishing the affix pool as designed would give 56 ways to multiply one base.**
Whether the Flat term is supposed to have an author at all is an owner ruling and
it sits in the affix layer, which has one owner and is being worked
independently. **Recorded, not prescribed.** LEDGER: do not author a flat lane in
the tree to route around this.

---

# PART ONE-L — ENEMIES SPAWN OUTSIDE THE TILESET, AND THE SPAWNER IS GYM-SHAPED

Owner, playing: *"enemies spawn outside of the tile set and walk in."* Verified,
and the cause is the same shape this project has now found four times.

## The arithmetic

`StartNextWave` places the pack at

```
  ArenaCenter = player position + forward * DashRefreshDistance
  DashRefreshDistance = 4400 cm = 44 m
```

**Fernhall is 100 x 50 m.**

```
  facing the long axis from the entry plaza   44 m lands mid-yard      fine
  facing the long axis from 60 m along        44 m lands at 104 m      OUTSIDE
  facing across the short axis, anywhere      44 m against 50 m wide   OUTSIDE
```

So it is not always wrong, which is exactly why it reads as intermittent: it
depends where the player stands and which way they face when the wave starts.
The pack's own spread around that centre puts more of it out.

**And the value exceeds its own stated band.** The comment at the site cites
Encounter-Design 5.2's spawn band as **1500–4000 cm** and then uses **4400** —
400 cm above the top of the band it names in the same sentence. That is a
justification and a value disagreeing where both are visible.

## The cause is a rule correct about one space, applied to another

The comment says why it is player-relative: *"the instrument has to work wherever
a playtest happens to be standing."* **That is correct — for an instrument, in an
open field, with no walls.** It is wrong for a rift run in authored geometry with
a boundary.

This is the fourth instance of one shape tonight: `IsLayoutLegal` measuring the
dead ground between yards; the crowd probe measuring a scene it did not name; the
harness blocks that were gym-located and read as gym-dependent; and now a spawner
that is gym-shaped being asked about a walled yard. **A rule that is right about
the gym is not thereby right about a place.**

## RULED — GROUND owns it, and the fix is containment, not a smaller number

**Do not just lower `DashRefreshDistance` to 4000.** That brings it inside its own
band and still spawns outside a 50 m width. The number is a symptom; the missing
concept is that **the spawner has no idea a boundary exists.**

Spawn placement must be constrained by the space, not by the player alone — and
GROUND already has the concept, because `FBreakerZoneField` carries a yard's
frame and its pieces. A pack belongs **inside the yard the player is in**, at a
distance chosen within what that yard affords, rather than at a fixed offset that
happens to fit the gym.

**Report the shape before authoring numbers**, as with the connection rule. In
particular: what happens when a yard is too small to hold the authored spawn
band at all — the band shrinks, the pack splits across sightlines, or the yard is
declared too small and the grammar says so. That is a real design question and a
100 x 50 yard may already be the case that answers it.

**And fix the band disagreement while you are in there**, either by moving the
value inside 1500–4000 or by correcting the comment if the band itself has moved.
One of the two is wrong and it should stop being both.

---

# PART ONE-M — THE ABILITY-BUILT FIXTURE IS AUTHORISED, AND BOTH CAUSES ARE REAL

LEDGER estimated before authoring, as ordered: an ability-built tree fixture
would measure parity at roughly **0.49 (0.48–0.51)**, against today's 0.27 and a
band floor of 0.85.

**RULED: author it.** The estimate is now a prediction on the record, which is
the discipline the seat failed at with Collapse — a number stated before the
measurement is worth more than one fitted after it.

## Both causes are real, and the split depends on which frame you use

```
  ADDITIVE   fixture closes 0.22 of the 0.58 gap   38%
             content closes 0.36                   62%

  RATIO      fixture   1.81x
             content   1.73x                       near-equal
```

LEDGER's *"about half and half"* is **true in the ratio frame and not the
additive one**, and the ratio frame is the correct one because **parity IS a
ratio** — an ability lane's throughput against a weapon lane's. Recorded because
someone will re-derive this additively, get 38/62, and think one of us was
wrong. Neither is; they are answers to different questions and only one of the
questions is the metric's own.

## The consequence is the actionable part and it checks out

*"Tuning against the current figure would overshoot by about 2x."* Verified:
closing 0.27 → 0.85 by content alone is a 3.15× move, while the content half
actually needs 1.73× — an overshoot of **1.81×**.

So the sequencing is forced, not preferred: **re-base the fixture first, then
tune against what it reports.** Content authored against 0.27 would land parity
near 1.6 and the band would fail from the other side, which is a worse failure
than the current one because it looks like success until someone reads the
number.

## What stays true regardless of the fixture

The flat half is unchanged by any of this. The ability lane has no flat line,
nothing in the game fills the law's `sum(Flat)` term, and a re-based fixture
measures the same zero. Part One-K's finding stands untouched and remains the
owner's, in the affix layer.

---

# PART ONE-N — PARITY IS 0.621, NOT 0.27, AND THE CRISIS WAS THE FIXTURE

O142 landed the ability-built fixture. **0.27 → 0.621 at cap**, 0.20 → 0.356
endgame. The prediction was 0.48–0.51 and **LEDGER said plainly that it was low**,
with the reason: the ring-legal mirror buys **14 ability picks where the estimate
assumed 8**.

That is a prediction stated, missed, and owned — which is worth more than a
prediction that happened to land, because it is the second time in this thread
that a number nobody committed to in advance would have gone unexamined.

## The four-ratio decomposition is complete and it multiplies

```
  increased  0.965    costs   3.5%   nearly closed by the re-base alone
  More       0.860    costs  14.0%   two ability-reaching standing Mores against three
  crit       0.863    costs  13.7%   the ability spend forgoes Precision's flats
  flat       0.867    costs  13.3%   structural — the owner's affix-layer question

  product = 0.6209 against 0.621 reported
```

Verified independently: the four multiply to the figure. This is what a
decomposition looks like when it stops claiming and starts asserting.

## What this changes, and the seat's own number was wrong

**The crisis was the measurement.** Parity did not halve to 0.27 as a property of
the game — it read 0.27 because both fixtures shared one weapon-leaning tree
spend and the ability build was handed a tree built for somebody else.

The remaining gap to the 0.85 floor is **1.37×**. Part One-M told LEDGER content
needed **1.73×**. That was computed against a 0.49 estimate that came in high, so
**the seat's figure is superseded and content needs materially less than it was
told.** Anyone tuning against 1.73 would now overshoot.

## Three of the four shortfalls are explained rather than deficient

Read the table again with what each one *is*:

- **`increased` 0.965** — effectively closed. Nothing to do.
- **`More` 0.860** — two ability-reaching standing Mores against three, **and
  Collapse's hit-time ×1.30 sits outside standing measurement by design.** So an
  ability build that buys Collapse gets a multiplier this metric structurally
  cannot see. The lived figure is better than 0.860 and the fixture cannot say by
  how much.
- **`crit` 0.863** — the ability spend *forgoes* Precision's flats. That is a
  **choice the mirrored fixture made**, not a hole in the content. A different
  legal ability build that bought crit would move this and lose elsewhere.
- **`flat` 0.867** — structural, and the only one that is unambiguously a gap.
  Nothing in the game fills the law's `sum(Flat)` term. Owner's, affix layer,
  unchanged by any fixture.

**So the honest reading is one real content gap (flat), one invisible-by-design
multiplier (More), one fixture spend choice (crit), and one closed axis
(increased).** That is a very different project from the one a single 0.27
described, and no content should be authored against 0.27 again.

## What the seat rules from here

**Nothing yet, deliberately.** The number moved by a factor of 2.3 in one commit
and the right response to that is to let it settle rather than immediately tune
against the new one — the same mistake in the other direction.

**LEDGER: report what a legal ability build that DOES buy crit measures**, before
anything is authored. If `crit` moves and `More` or `increased` falls to pay for
it, the 0.863 was a spend choice and the real gap is smaller still. If it moves
and nothing falls, the ability wheels have slack the weapon wheels do not and
that is a finding of its own.

The flat question stays where Part One-K put it: with the owner, in the affix
layer, untouched by any of this.

---

# PART ONE-O — THE LOOP HAS AN ENDING

Part Three-E opened with a recon's finding: *"Closing does not exist. There is no
close-rift verb anywhere in the module... The game's atomic loop currently has no
ending."*

It has one. Three commits, three lanes, one interface each way, verified end to
end:

```
  MarkRiftTerminator      Game/          GROUND marks and binds in one act
  terminator raise        Combat/        FIELD raises, knowing nothing of rifts
  CompleteRiftRun         Game/          GROUND consumes, latches, broadcasts
  HandleRiftCompleted     Progression/   LEDGER pays
```

**No lane entered another's files.** `Combat/` gained no `Game/` include. The
payout binds an event rather than reaching for state. The seam held under its own
rules for its whole length, which is the first time this project has run a
three-lane feature.

## Three details worth keeping

**The latch was proven in play, not asserted.** `Breaker.Field.VerifyRiftChain 2`
shows a second raise reaching the consume site and being refused — one
completion, one broadcast. LEDGER bound on the strength of an exercised
guarantee.

**The payout does NOT deduplicate, on purpose.** O168's per-world latch already
makes every broadcast a distinct run, and *"a payout that second-guessed the
seam's guarantee would be two owners of one question."* That is the session's
central discipline applied at the last joint, by the lane with the most to lose
from trusting it.

**Reward-per-minute stays flat by construction.** `BreakerRiftRewardMath.h` reads
`FBreakerMonsterChassisParams{}.HealthGrowthPerLevel` — the chassis's own
constant, read rather than restated — so payout and monster health ride one
curve. Retune monster growth and the reward follows. Verified at the site.

## What this unblocks

**The rift interior is now next in that thread and it is the owner's.** GROUND's
costing said the yard-instance route is cheap — `StartNextWave` spawns around the
player, so the wave system has no gym dependency, and `Lvl_Fernhall` with
`PendingRift` set already IS a different instance of the same tileset — and
recommended taking it *after* the close verb, because *"a populated rift with no
ending is a louder version of the problem."*

The close verb is done. That sequencing argument has expired in the good way.

**Still pending and unchanged:** first-clears (O117) wait on an archetype
existing on the rift definition; the spawner still does not know a boundary
exists; separation is unbuilt; and the flat term still has no author.

---

# PART ONE-P — SIX REDS WHERE THERE WERE TWO, AND NOTHING REGRESSED

Read this before chasing one. `status.py` now reports **six** out-of-band rows
where it reported two earlier tonight, and **not one of them is new breakage**.

```
  Resource generation entry points with no caller   3 of 18    already out
  Offered-to-spendable ratio, per tree              2.25       already out
  Build variance band, at cap                       6.01       NEWLY VISIBLE
  Ability lane throughput, at cap                   0.62       NEWLY VISIBLE
```

The last two read **"not emitted"** for most of the session, because the fixtures
that produce them had not run. Variance at 6.01 against a band of 8–10, and
parity at 0.62 against 0.85–1.15, were both true the whole time. They were simply
unmeasured.

**The count rose because COVERAGE rose.** That is the opposite of a regression and
it is exactly the kind of figure a person reads backwards at a glance.

Recorded here because this project has been bitten by this shape four times in
one session — the parity "collapse" that was a fixture, the census's 25 that was
a dropped term, the crowd probe measuring a scene it had not named, and a bar
that was "one flat red stripe" for a reason nobody guessed. **A number that moved
is not evidence about the thing it measures until you know why it moved.**

If you open a red tomorrow, check whether it was emitting yesterday before you
treat it as damage.

---

# PART ONE-Q — RULED: THE RIFT INTERIOR IS THE YARD, AS ITS OWN INSTANCE

Owner, 2026-08-26: *"i wanted it to be its own instance so you can actually feel
the loop."*

**RULED.** The gym stops being the rift interior. A rift run is an instance of
Fernhall's own geometry, populated — same ground, separate run, which is what an
instanced rift is.

GROUND's costing stands and the reason it is cheap is worth restating: **the wave
system spawns around the PLAYER, not at an authored arena**, so it has no
dependency on gym geometry, and `Lvl_Fernhall` with `PendingRift` set already IS
a different instance of the same tileset. No new map, no new art.

**The success test is in the owner's own words: the loop should be FELT.** Not
technically closed — it already is. A run that can be walked, fought and finished
in one sitting is what this ruling is for, and anything that makes the run
complete-but-unpleasant has missed it.

## This promotes the spawn containment bug from annoyance to BLOCKER

Part One-L found the spawner places its pack **44 m from the player** with no idea
a boundary exists, and Fernhall is **100 x 50 m**. Today that misfires in a yard
nobody fights in. **Once the yard IS the rift interior, every single rift run hits
it** — enemies materialising outside the walls and walking in is the first thing
the owner will see and the last thing a "felt loop" survives.

**GROUND: containment lands BEFORE or WITH the interior, not after.** The report
was already ordered; this makes it the gate. The band fix that already landed is
not enough — 4000 cm still spawns outside a 50 m width, as that commit itself
said.

## The order, and it is short

1. **Spawn containment** — a pack belongs to the yard the player is in, at a
   distance the yard affords. Report the shape, then author it.
2. **The interior swap** — `PendingRift` set, the yard's geometry, waves live.
3. **Walk it.** The owner rules whether it feels like a loop, which is the only
   test this ruling has.

Everything else in GROUND's queue waits behind those three. Yard growth to five
yards, the connection rule, the First Contract — all of it is worth more once one
rift run feels like a run, and worth less before.

## What the other lanes should know

**FIELD:** the interior is where separation gets felt rather than measured. A
walled yard with a real fight in it is the first place the N² term is something
the owner experiences instead of a number in a report.

**LEDGER:** the payout fires on a completion the owner will now actually reach.
`BreakerRiftRewardMath`'s bases are `O2 PLACEHOLDER` and this is the first time
anyone can say whether finishing a run *feels* paid.

**GLASS:** a rift run that can be finished means the deployment briefing and the
completion moment both get seen for the first time.

---

# PART TWO — FERNHALL IS THE WORLD

Owner: *"fernhall should just be an area the player can roam with the rifts and
some missions inside of it just the 1 area we have."*

This is the largest simplification the project has made and it deletes more than
it adds. Read it before planning anything.

## What it replaces

The multi-zone structure — several planet-like maps, travel between them, a rift
interior generator per zone — is **off**. There is one place. Everything the
campaign was going to be happens in it.

## The three spaces, and there are only three

```
  ANCHOR      social. No combat, no HUD ammo, weapon holstered. Vendors,
              travel, the place you return to.
  FERNHALL    the world. Roamed, fought in, missions taken and done here,
              rift doors standing in it.
  RIFTS       instanced. Entered from a door in Fernhall, generated, exited
              back to where you stood.
  (the gym stays as a test bench and is not part of the loop)
```

The loop is: **Anchor → Fernhall → a rift inside Fernhall → back out to
Fernhall → Anchor.** That is the whole game's shape. It is small enough to
finish.

## What Fernhall has to become

The yard is 100 × 50 m and about a minute end to end. That is a corridor, not a
place you roam. **It has to grow, and the composer is why that is affordable** —
`compose_fernhall.py` builds from a Kenney kit with the grammar validating
placement, so growth is authoring, not modelling.

Target shape, all `O2 PLACEHOLDER` and all owner-tunable once he walks it:

- **Four to six connected yards**, not one large field. Each the size of the
  current one or a little larger, joined by lanes and gates. A field of 500 × 300
  m is empty; six rooms of 100 × 50 read as a place.
- **Each yard has a reason to exist** — a rift door, a mission giver, a fight
  that only happens there, a route to two others. A yard with none of those is a
  corridor with grass.
- **The existing yard is the entry.** It already has the travel gate back to the
  Anchor, the rift pad, and `marker_npc_contract`. It becomes the plaza the
  player arrives in, and the rest extends from it.
- **Enemies live in it**, not in waves. Density and variety per yard, tied to
  ruling 4 — this is where the 50–100 target has to actually hold, and where
  variety stops being a word.

## Rifts, under the rulings that already exist

O122 stands: **a campaign rift is entered freely, an endgame rift is
consumable.** Both kinds stand in Fernhall as doors. The free ones are how the
campaign is played; the consumable ones are the endgame, and they use the same
doors with a different key.

O82 stands: campaign respawn is unlimited from the start of the tileset, and the
endgame death budget stays **parked** until consumable rifts exist — a limit on a
free instance kicks the player out of a door they walk straight back through.

The rift *interior* generator is still not started and is still gated on the
owner having stood in the yard. What is unblocked is the **door**: a marked site
in Fernhall that writes a `PendingRift` and travels. The interior can be the gym
for one landing — the gym is a real space with real spawning, and a rift whose
interior is the gym is a complete loop with a placeholder room, which is worth
far more than a perfect room with no loop.

## Missions

The smallest thing that is a mission: **an NPC in a yard gives a task, the task
happens in Fernhall, the NPC pays it.** `marker_npc_contract` is already placed
and measured. The First Contract — the fourteen dead Core Points — is the first
one and it is what turns those points on.

Do not build a quest system. Build **one contract**, end to end, and see what it
needed. A quest system authored before a single quest exists is a guess about a
shape nobody has held.

## What this does to the wave budget

The gym's rising-wave curve was always the mechanism for a **wave mode**. Fernhall
is not waves — it is a populated area. The solver still applies: a yard's
population is a composition, and `SolveWave` prices archetypes against a budget.
What changes is that the integer driving it is not a wave index but a yard's
difficulty. O134's space input is now the live question rather than a rift
question, because a yard's shape decides its population.

---

# PART THREE — WHAT EACH LANE DOES NEXT

## GROUND — `lane/ground`

**You are unblocked. Fernhall growing is yours and it is now the project's spine.**

1. **Read Part Two and report back before building.** Say what the composer can
   and cannot express about six connected yards, what the grammar validates
   across a gate, and whether the cover rules hold at a junction. Report first;
   the yard shape is a design decision I will make against what you find.
2. **The rift door.** A marked site that writes a `PendingRift` and travels, with
   the **gym as the interior** for the first landing. O122's free-entry rule.
3. **O134 is now live** — a yard's shape decides its population. Your held design
   report on space inputs stops being speculative.

Do not build the rift interior generator. Do not build a quest system.

## FIELD — `lane/field`

**Density is a requirement now, not a finding. Optimise.**

1. **The sweep first** — engaged at N = 25 / 50 / 75 / 100. Slope and intercept.
   Without the intercept every optimisation is unmeasured. GROUND owns the probe
   if the flag needs changing.
2. **Then optimise against what the sweep says.** The cost is game-thread and the
   target is unchanged, so the question is what an engaged body does per tick and
   which parts of it can be amortised, staggered or skipped at distance.
3. **Enemy variety** is coming as a requirement — the owner will expand. Do not
   design for it yet; do not make optimisations that assume one archetype.
4. Still open from before: the colour ramp's terminal hue (every Boss dies
   magenta, not one family), and the fracture-mask material query.

## LEDGER — `lane/ledger`

1. **The enhanced-dash passive** — ruling 1. A tree node granted at level one.
   Report the shape before authoring the magnitude: which node target, whether it
   is a Core or a Swift node, and what "enhanced dash" means in the vocabulary
   that exists (`DashCooldown` is real; charges are not).
2. **Swift's partition** — the two lines KIT's three reds wait on, with Slipcut
   as an **unlockable** and not a starter. Land it with KIT's loadout flip.
3. **Ruling 6 closes the Prolific pin.** Rewrite the band's description: it is
   where most builds land, not a ceiling.
4. **Ruling 5 keeps the at-cap pin open** and points it at content, not the
   assertion.

## KIT — `lane/kit`

1. **O176 is dead as written.** Slipcut is an unlockable. Your three enumerated
   reds still clear on LEDGER's two lines — the contents change, the shape does
   not.
2. **Do not build the dash passive.** It is a tree node and LEDGER owns it. Your
   half is the loadout flip when it lands.
3. **The six abilities that still draw nothing** — Rend, Provoke, Breach Charge
   and three more. Apply the Swift template: cast-moment flash through the pooled
   renderer, palette role by verb, windows stay HUD bars. That is unblocked now.
4. **Sound is GLASS's.** You need no interface and should not wait.

## GLASS — `lane/glass`

1. **The fifth verb** — ruling 2. One default cue, per-ability override slots
   falling back to it. Nothing silent, nothing re-plumbed when the owner's assets
   arrive.
2. **`BreakerEffectRenderer` is PUBLISHED** — see below. Name the consumers at
   its declaration and treat a public-surface change as a declared crossing.
3. **The boss phase readout needs no coordination — it is already independent.**
   Your readout is a text status line (`PHASE %d / %d`, top centre, no bar), so
   it carries no band ticks and no phase marks. O135's surviving branch — phase
   marks drawn heavier and independently of the band ticks — is entirely about
   FIELD's **world-space** bar, and FIELD decides whether that bar draws phase
   marks at all. Nothing of yours waits on it. Say so in your report and close
   the question.
4. **Ability impacts: prove the routing mechanically, leave the judgement to the
   owner.** `OnHitDealt.Broadcast` has exactly one site and your HUD binds it at
   `BreakerPlaytestHUD.cpp:1860`, so the routing is verified without a playtest.
   What a playtest is still owed for is whether the cue *reads* as an ability
   impact rather than a bullet — and that is the owner's ear, not an inference
   either of us can discharge. Split the claim: routing PROVEN, audibility
   INFERRED, character UNHEARD.

---

# PART THREE-B — ANSWERS TO LANE QUESTIONS

## The reports convention is RATIFIED

`Docs/reports/<LANE>.md` emerged from the lanes rather than from here — LEDGER
made one, FIELD found it after choosing differently and **moved to theirs
because it got there first**, KIT followed. That is the two-owners-one-question
failure being avoided by a lane at its own expense, and it is the right call.

**Ratified, with the flow named so it does not drift:**

- A lane asks in `Docs/reports/<LANE>.md`. Open questions only; delete an
  answered one, git holds it. Findings and status stay in session reports.
- **The seat answers here, in ORDERS.** One direction each way. Do not answer
  your own question in your own file, and do not ask in ORDERS.
- GLASS and GROUND: use the same file name and the same contract. Do not invent
  a variant.

## LEDGER — the enhanced-dash node (`Docs/reports/LEDGER.md`)

**Swift node, not Core: agreed**, for the reason given. A Core node hands it to
five classes and it is Swift's free verb.

**The cooldown reading: pushed back, and check this before defaulting to it.**
You wrote that charges, distance and i-frames each need plumbing that does not
exist. True of charges and i-frames — no property, no target. **Not true of
distance.** `BreakerCharacterMovementComponent.h:240` already ships
`DashSpeedBonus`, `EditAnywhere` and `BlueprintReadWrite`, alongside
`DashSpeedFloor` and `DashVerticalFloor`. What is missing is only a node stat
target and the read — one enum entry and one aggregation wire, not a plumbing
project.

That distinction decides the feel. **A cooldown shave is invisible until the
player spams; distance is felt on the first dash.** For a level-one always-on
passive on the movement class — the first thing the owner will feel when he
plays the vertical slice — felt-immediately is worth more than felt-eventually.

Report what wiring a dash-distance target would actually cost before authoring
the cooldown reading. If it is genuinely more than one entry and one wire, the
cooldown reading stands and this is closed.

**The seeded free rank: ruled, node form, and there is a third property you did
not name.**

Take the node form rather than a class base stat — visible on the board, and it
can carry higher ranks to buy later, which a base stat cannot. Rank 1 seeded at
`ChoosePermanentClassById`, cost 0. Your two properties are right: a respec must
not refund what was never paid, and the board draws it owned-and-unrefundable.

The third: **a free rank every Swift has enters the power-band fixture.** It is
not neutral to the measurement — it is content, and ruling 5 just said content
rises to meet the 8–10× band. The precedent is O95, where one node's
unconditional line moved at-cap 6.53 → 6.79 and parity 0.647 → 0.622 on a single
edit. So re-measure at-cap and parity after it lands and report both, rather than
assuming a free rank costs the fixture nothing.

## GROUND — the yard report, answered (`Docs/reports/GROUND.md`)

Every load-bearing claim in that report checks out. `FullHalfExtentCm` is 150
against a `DashCorridorWidthCm` of 1600, so a legal gap between full-height
pieces really is **1900 cm centre-to-centre** and a 19 m gate is not a gate.
`IsComplete()` really is `bPlayerStart && bRift && bNPCContract`, three hardcoded
flags. All ten vendored `.glb` pieces really are already spent. The sequencing
argument is right and I am ruling in its order.

### Q1 — 50–100 is a CONCURRENCY BUDGET, not a population count

Your question contains two questions and they have different owners.

**The engineering half is mine and it is ruled.** The 34.16 ms figure is for a
hundred bodies *simultaneously awake, detecting and closing*. It is a ceiling on
what may **simulate at once**, anywhere, for any reason. It is not an allocation
to be divided among yards. Six yards at a hundred each is six hundred bodies and
a dead frame; six yards at seventeen each is a budget spent on emptiness.

So neither of your readings is right. **A yard has a population. The budget is
what is awake.** Yards away from the player hold their enemies without paying
full AI for them, and the ceiling applies to the awake set — which in practice is
the player's yard plus whatever neighbours are in range.

**This changes FIELD's optimisation target and both of you should read it that
way.** The answer to "a hundred engaged costs 34 ms" may not be "make each body
cheaper" — it may be "have forty engaged and two hundred asleep." Waking and
sleeping is a larger lever than per-body cost and nobody has measured it.

FIELD: report whether any sleep, tick-throttle or distance-LOD concept exists for
enemies today, before the sweep. If one does, the sweep should measure awake-set
size rather than total bodies, and those are different experiments.

**The design half is the owner's and you were right that it is not a desk
question.** How many bodies make one yard feel populated is something he decides
standing in a yard. That is a different number from the budget and it should stop
sharing a sentence with it.

### Q2 — the yard count waits behind Q3 and Q4, on your own argument

I said I would rule this against your report, and your report's answer is that
the composer cannot express it yet. Ruling a shape the grammar cannot validate
would be authoring against plumbing that does not exist, which is the rule I hold
other lanes to.

**Direction, so you are not blocked, and it is overturnable by the owner once he
walks one: five yards.** Four reads as a corridor with a bulge; six is one more
than the current kit vocabulary can make distinct. Five gives a hub, two
branches, and a far end — enough for a route to feel chosen rather than
followed. Sizes stay near the current yard until one has been walked.

### Q3 — RULED: a connection is a distinct kind of space

Your recommendation, adopted. The field rules are correct about an open combat
field and have no concept of a room boundary; a junction is a narrowing, and a
narrowing is what they exist to forbid.

**Connections are exempt from the field grammar and get their own**, which asks
about length and sightline rather than cover pitch. **`IsLayoutLegal` moves from
per-zone to per-yard.** A zone becomes legal when every yard is legal and every
connection satisfies the connection rule — two rules, two kinds of space, neither
pretending to be the other.

Report the connection rule's own terms before authoring numbers for it.

### Q4 — RULED: markers become a list, and it is first in the chain

Yes, and your shape is right: keyed by role with a yard tag, `IsComplete`
becoming *"one player start, and every yard that declares a door has one."*

Three hardcoded sites — the struct, `breaker_import_fernhall.py`, and the
piece-count test. Fix all three in one commit and run `Scripts/shapecheck.py` on
it; three copies of one assumption is the shape that script exists for.

### Q5 — OWNER'S CALL: more kit assets

Ten pieces, all ten spent, no unused vocabulary. Six yards that look like six
places needs more than layout can supply.

**ANSWERED 2026-08-26: YES.** The owner: *"we can pull more for now we can always
get rid of them later."* Pull what the five yards need.

Three conditions, because a vendored asset is easier to add than to remove:

- **The licence note travels with them.** `Assets/zones/kit/LICENSE-NOTE.txt`
  already records CC0 per source repo. Every new pack gets the same entry in the
  same file, written in the same commit as the `.glb` files — not after.
- **Pull for the yards you are building, not for a library.** "We can get rid of
  them later" is true of the working tree and not of git history, so the cost of
  an unused pack is permanent and the benefit is zero. Import what five distinct
  yards need and stop.
- **Report what the new vocabulary bought.** The reason for this pull is that six
  yards cannot look like six places out of ten pieces. Say in the report which
  yards became distinguishable and how, so the next pull has a measurement behind
  it rather than an instinct.

### Q6 — RULED: yes, land the door now

Against the existing `marker_rift`, gym as interior, and move it to the marker
list when that lands. Your reasoning is right: the marker-list change gates yard
*growth*, not one door. Proceed as you proposed.

### Q7 — Not yours this cycle. Described, not assigned

You read it correctly. Part Two describes the First Contract because it is part
of the shape; Part Three did not assign it because your cycle is already the
marker chain, the grammar split and the door.

It **will** be GROUND's — `Interaction/` is yours and an NPC giving and paying a
task is that directory's job. It is assigned when the yard shape is ruled, not
before, because a contract authored against one yard will be re-authored against
five.

### The second O120 — LEDGER's, and you were right not to touch it

Reward composition is progression subject matter, so the renumber is LEDGER's.
Your analysis stands and saves them the work: the reward ruling has no citations
anywhere, the loading one is cited from three files, so the reward ruling is the
one that moves. **LEDGER: take the next free number, leave O120 to the loading
ruling, and grep `Docs/` and `Source/` before calling it done.**

### Your two FIELD questions, routed with my view

**The probe's missing half.** You are right that `engaged` measures convergence
and attack, not a full fight, and that hit reactions, damage numbers, flashes and
death effects are the expensive half. **FIELD decides.** My view: the sweep is
still worth running without it — a slope on convergence-and-attack is a real
number and it is the one that exists today — but it must be **labelled as half a
fight** in the report, or it becomes the next "affordable at 100."

**`DetectionRange`.** Leave it as it is. You produced engagement with geometry
and touched no file you do not own, which is the right instinct, and the cost —
patrol and engaged not being one-variable comparable *with each other* — is
smaller than it sounds, because the comparison that matters is within a mode.
Opening a header across a lane boundary to buy one variable is a bad trade.

---

## GLASS — the door says two things, and your two questions answered

### FIRST, AND IT IS A LIVE DEFECT: the door states two verbs, stacked

GROUND photographed the rift door and found that
`BreakerPlaytestHUD.cpp:1724` draws the label as a **literal `TRAVEL`** while
the prompt beneath it calls `GetPromptLabel()` — which returns `ENTER RIFT`. So
the first new interactable the player meets in the world says two different
things about itself, one above the other.

GROUND specified the fix and deliberately did **not** write it, both because it
crosses into `UI/` and because — their reasoning, and it is right — *an uncalled
getter is a dead API, and `GetPromptLabel` already spent a milestone as one.*

**Yours: line 1724 calls `GetPromptLabel()` instead of the literal.** Small, and
it is the first thing anyone walking the loop will see.

While you are there: that literal is one instance. Check whether the same
label-drawn-as-literal shape appears at the other interactable sites — loot,
NPCs, travel gates — and run `Scripts/shapecheck.py` on the commit. One
hardcoded verb beside a getter that exists is exactly the shape that repeats.

### Q — does the fifth verb need a DURATION companion?

**Held, and the trigger to revisit is co-op, not solo feel.**

Your framing is right that nothing is blocked. What decides it is who a sustain
cue is *for*. It is not for the player holding the channel — they know, they are
holding the button, and the HUD already draws the window bar. **A sustain cue's
real audience is the periphery and the party**: you looking elsewhere, and other
players who cannot see your bar at all.

Single-player with a bar on screen, cast cue plus bar very likely carries it.
Party play, it does not. So: no sixth verb now, and the thing that should reopen
this is a second player existing, not a playtest feeling thin. If the owner walks
the loop and a channel feels dead in a way the bar does not fix, that overturns
me — but ask for that verdict rather than inferring it.

### Q — enemy ability audio needs a different mechanism

**Confirmed, and do not bolt it on.** Your analysis is correct and the reason is
the good one: all five verbs are `bIsUISound = true` and unspatialized *on
purpose*, because they happen TO the listener. An enemy telegraph's entire value
is where it came from. Those are different mechanisms wearing the same word.

**One thing to carry into its design when it is time: it is density-coupled.**
The concurrency ruling puts up to a hundred bodies awake at once, and a hundred
enemies each wanting a positioned telegraph is a voice-count problem before it is
an audio-design problem — the same shape as the effect renderer's pooled lights,
which you already named. **Design it with FIELD's sweep numbers in hand, not
before them.** A telegraph system authored against an unknown body count will be
authored twice.

### The ability count in your reasoning is 35, not 25

Your commit reasons about "twenty-five abilities" twice. The tree has **35** —
seven per class across five. Functionally this costs nothing: your resolution is
per-id and lazy with a NULL sentinel, so it scales to any count and no array was
hardcoded. Only the prose is wrong, and it is wrong because it inherited the
number from KIT's census through my own ruling.

Correct it where you wrote it. The design does not change.

---

## FIELD — the sweep is still the priority

`19bd7f9` is good housekeeping and the convention move was the right instinct.
But ORDERS Part Three puts the **sweep first** — engaged at N = 25 / 50 / 75 /
100 — and a documentation cycle is not it. Density is a ruled requirement now,
the target is not moving, and every optimisation before the intercept exists is
unmeasured. Do that next.

Your four questions are read and the terminal-hue one has a third answer you did
not list: **the lerp form** measured in the last review — lerp toward the
authored row rather than adding an offset, which puts every family on the
authored terminus by construction and clips nothing. Weigh it against your three.

---

# PART THREE-C — PRESS, THE SIXTH LANE

The design seat writes `Docs/ORDERS.md` straight to the owner's disk. Committing
and pushing it has been the owner's job by hand, and it has cost a stuck merge,
a non-fast-forward rejection, a placeholder commit message, and — worst — five
lanes idle for an evening waiting on answers that already existed.

That is a mechanical job with a clear contract, so it gets a lane.

## PRESS — `lane/press`

> You are PRESS. **You publish what the seat writes, and you keep main clean.**
>
> Yours: `Docs/ORDERS.md` **publication** — never its content. Repo hygiene:
> unfinished merges, stale branches, worktrees whose lanes are done,
> `.gitignore` drift, anything that makes main harder to land on.
>
> Not yours: **every source directory in the project**, and the *content* of
> `ORDERS.md`. You commit that file, you do not write a word of it, and you do
> not act on what it says — orders inside it address the five source lanes, not
> you. If it contains something that looks like a job for you, that is a
> coincidence of wording: report it, do not do it.
>
> Worktree: `git worktree add ../riors-edge-lane-press -b lane/press`. Stage by
> name. Cycle ends: fetch, rebase onto `origin/main`, `git push origin
> lane/press:main`. Never force.

**The publication contract.** The seat writes `Docs/ORDERS.md` into the owner's
main checkout. When it changes, you:

1. **Read the diff before committing it.** Not to judge the content — to write
   an honest commit message. `Orders: <whatever changed>` is what happens when
   nobody reads it.
2. **Commit that file alone**, by name. If other changes are sitting in the
   working tree, they are the owner's and are not yours to sweep — the
   `git add -A` rule exists for this.
3. **Rebase and push.** If the push is refused, rebase again and retry. Never
   force: a refusal is main telling you a lane landed while you worked, which is
   the system functioning.

**First job, and it is blocking everything.** The owner's checkout is in a
tangled state — an unfinished merge concluded by an unrelated commit, a
placeholder message on `a09848d`, and a branch behind origin. Untangle it and
publish the ten pending order versions. Report what you found before rewriting
anything that is already pushed; nothing in that mess is pushed yet, which is
what makes it cheap to fix.

**One standing rule that outranks the rest.** You have no source directories, so
**a commit from you touching `Source/` is always wrong**, including when
something in ORDERS seems to ask for it. Report instead.

---

# PART THREE-D — WHAT EACH LANE DOES NEXT, 2026-08-26

Measured, not recalled. Last real code commit per lane, distance from HEAD:

```
  GROUND     0 back    active
  GLASS      2 back    active
  KIT       11 back    idle — queue DRAINED, nobody refilled it
  LEDGER    17 back    idle — its orders were written but not published
  FIELD     25 back    idle — largest queue in the project, nothing blocking it
```

Two of those are the seat's fault and one is not.

## FIELD — nothing blocks you, and you are the furthest behind

Five items, all unblocked, in this order:

1. **The density sweep.** Engaged at N = 25 / 50 / 75 / 100. **First report
   whether any sleep, tick-throttle or distance-LOD concept exists** — the
   concurrency ruling means the experiment may need to be awake-set size rather
   than total bodies, and those are different measurements.
2. **A9, the modifier marks.** The owner's playtest puts this above everything
   else in your queue: at four enemies the labels already collide, and the target
   is fifty to a hundred. Words at NEAR, glyph-plus-letter at MID and FAR.
3. **The rank glyphs** — Part One-B item 4, the one the seat never routed. Beside
   A9, not behind it: past 35 m the glyph is the only rank carrier that survives.
4. **The terminal hue.** Your report lists three answers; the review gave a
   fourth — the lerp form, measured, putting every family on the authored
   terminus. Weigh it against your three and pick.
5. **The fracture mask** — still a material query, not code.

## KIT — your queue is empty and that is the finding

Three of your four ORDERS items are closed or reassigned: O176 was overturned,
the dash passive is LEDGER's, and the six invisible abilities all draw as of
`deed7c9`. The fourth — the loadout flip — waits on a LEDGER landing.

So you have no work, and that went unnoticed for eleven commits. New assignment:

**Weapon feel is yours and nobody has touched it.** `Weapons/` is your directory,
and the owner's first complaint list opened with *"all of the sound is bad … does
not sound like real guns"* and never got past the sound half. The cue is GLASS's;
**the weapon's behaviour is yours** — recoil pattern, recovery, the kick that
makes a rifle read as a rifle, and what separates the archetypes when fired
rather than on paper.

**Report before building.** What the weapon component already exposes, what a
recoil model would need, and what distinguishes the archetypes today besides
their numbers.

Do your own report's item first: *presentation colour law, before more classes
bake it in*. It is a good catch, it is cheap, and it stops five classes
inheriting a convention nobody wrote down.

## LEDGER — two of your three are unblocked right now

The dash node and the derived milestone schedule are in orders that had not
reached you. They have now. Beyond those:

**The rider More question is the substantial one and needs no permission.** Can
`ApplyTargetConditionRiders` carry a More bucket, or is a target-gated More
structurally impossible here? That is not Collapse's blocker — it is a whole
class of ARPG line, and the answer decides whether Collapse gets redesigned or
the system grows. Report it.

**The dash-distance wiring cost** — you asked which side of the line it falls on.
Count it: one enum entry and one read, or more.

## GROUND — the chain continues, and one item is not yours

Next in your own sequence: **the connection rule's own terms**, reported before
numbers. Then the yard shape, which the seat rules against that report.

**The rift door saying TRAVEL is GLASS's one line.** You were right not to write
it; it is in GLASS's list below.

## GLASS — one line unblocks GROUND, two questions wait on the owner

1. **`BreakerPlaytestHUD.cpp:1724`** — the literal `TRAVEL` above `F ENTER RIFT`.
   One line, your file, and it is the first thing anyone walking the loop reads.
   Check the same shape at the other interactable sites while you are there.
2. **The death beat and the duration verb both wait on the owner.** Do not infer
   either; your refusal to add a sixth verb on inference was correct.
3. **Enemy telegraph audio** stays held until FIELD's sweep exists — it is
   density-coupled and would otherwise be authored twice.

---

# PART THREE-E — THE LOOP HAS NO ENDING

A recon found it and the seat verified it: **there is no close-rift verb.** Six
naming patterns across the whole module, nothing. Entry works — the door hands a
real `FBreakerRiftDefinition` across travel. Killing works — waves, budget
solver, elite modifiers, a boss at wave 12, per-kill drops. **Closing does not
exist**: no completion state, no run terminator, no payout for finishing. You
walk in, fight escalating waves, and leave by the door you came through.

All reward is per-kill. There is no moment where a run RESOLVES, which is the
difference between a shooting range and a run.

**This is queued, not preempting.** Every lane finishes its current item first —
the owner's call. But it goes in the queue above everything that comes after,
because items like parity, variance and content depth are all *"the loop does not
survive repetition"* — and a loop with no ending is not repeated, so tuning it is
tuning a shooting range.

## It is three lanes and the seam matters

- **GROUND** owns the completion state and the run terminator hook: what "this
  rift is closed" IS, where it is stored, and what ends the run. `Game/`.
- **FIELD** owns whatever holds the rift open — the thing that dies. An enemy,
  a rank, a behaviour. `Combat/`.
- **LEDGER** owns the payout: what a resolved run pays that a kill does not.
  `Items/`, drop tables.

**Report the seam before any of you build.** Specifically: does the terminator's
death write the completion state directly, or raise something GROUND consumes?
That is the header-rule question again and it decides whether this is one commit
across three lanes or three commits with two interfaces.

## Two cautions from the recon's own numbers

The finding is right. Some of its evidence is not, and the same care applies to
whatever measures this work.

**Parity 0.27 contradicts a pinned 0.647 → 0.622**, and `status.py` cannot emit
the live figure ("needs `PowerBand.AbilityLane` to emit it"). Either parity
collapsed and the cause is findable in a week of commits, or the two figures
measure different things and the "worse than four days ago" comparison is
invalid. **Resolve which before anyone acts on it.**

**Two cited symptoms are pinned and passing.** Silent nodes 43 of 446 against a
ceiling of 54 — ok. Node tags with no consumer 140 of 206 — ok, a ratio already
ruled acceptable. Citing passing measurements as evidence is how a true
conclusion gets an unsound argument.

**And it omits one of only two live OUT flags.** `Offered-to-spendable ratio,
2.25 worst tree against a floor of 3.0` is failing right now and is squarely the
"builds do not diverge" case the recon was making. It argued without its best
evidence.

## Fernhall is NOT the gym, and it is empty

Recorded because it has caused confusion twice. The Fernhall branch returns
before the gym build — verified — so the yard gets **no dummies, no waves, no
safe pad, no elite arena**. It is a walled yard with a cover lattice, a rift
door, a travel gate and an NPC marker.

**It also spawns zero enemies.** Nothing to fight is authored there at all. What
IS the gym is the **rift interior** — walking through Fernhall's door lands you
in the gym, which is the ruled placeholder.

So walking Fernhall today is walking an empty lot with a door in it. That is
worth knowing before it is judged: it answers "does the pad read as somewhere to
go" and "does the lattice change how I move", and it cannot yet answer anything
about a fight.

---

# PART FOUR — A THIRD KIND OF SHARED PATH

`UI/BreakerEffectRenderer.*` is GLASS-owned and called from four lanes —
`Abilities/`, `Combat/`, `Game/`, `UI/`. The map has no vocabulary for that:
`Tests/` is shared because every lane writes its own, `Docs/` because every lane
appends, and both have **no owner**.

## A FOURTH KIND: THE INSTRUMENTS, and the map does not cover them

`Scripts/status.py`, `Scripts/shapecheck.py` and `Scripts/lanereport.py` appear
nowhere in the path table. Only `Scripts/compose_*` does, under GROUND. So the
three programs **every lane runs at every cycle** have no stated owner.

It has not bitten because nothing has touched them since the lanes existed —
every edit to `status.py` and `shapecheck.py` predates the split. It will bite:
FIELD is about to add a sleep concept that may want a pin, and GROUND is growing
yards whose grammar readout those instruments print.

They are not `Tests/` and they are not `Docs/`. A test belongs to the lane that
wrote it; a doc line is append-only and hurts nobody. **An instrument change
changes what every lane's cycle DOES**, which makes it the strongest case in the
map rather than the weakest.

**RULED — the instruments are ownerless and announced.** Any lane may extend one.
Two conditions:

- **Say so in the commit and in your report.** Not because permission is needed,
  but because the next lane to run a green cycle deserves to know the instrument
  changed under it.
- **Never narrow an instrument to make your own cycle pass.** This is the
  assertion rule one level up: *never widen an assertion to make a red go green*
  applies to the thing doing the asserting. Loosening a check, dropping a
  section, or scoping a sweep so your commit comes back clean is the same defect
  wearing a script.

A third condition follows from tonight, and it is the seat's own: `lanereport.py`
checks ruling-number collisions **only within the commits it is given**, which is
why two live O120 rulings survived a cycle of being looked at directly. An
instrument whose scope is narrower than its name is the defect it exists to find.
Whoever touches it next should make that sweep whole-file, or rename the check.

**A published path has an owner and named consumers.** The owner changes the
implementation freely and **declares** a change to the public surface. Same rule
as a header, with the consumers named in advance instead of discovered.

Published paths today:

```
  UI/BreakerEffectRenderer.*      GLASS  ->  KIT, FIELD, GROUND
  UI/BreakerEffectMath.h          GLASS  ->  KIT, FIELD          (not GROUND)
  Attributes/BreakerHealthBands.h LEDGER ->  FIELD, GLASS
```

CORRECTED 2026-08-26, on GLASS asking the right question. The middle row
originally listed GROUND, and it was wrong: I measured the renderer's callers
and then transcribed that list onto the header beside it instead of measuring
it. GROUND uses the renderer and does not touch the math header. One line,
copied rather than checked, in the document that exists to stop exactly that.

HOW THIS TABLE IS DERIVED, so the next reader can redo it rather than trust it:
`grep -rl <name> Source/RiorsEdge` for each published header, minus the owning
lane's own directories. **`Tests/` is never a consumer** — every lane writes its
own tests, so a test that uses a published path belongs to whichever lane wrote
it and adds no obligation. Re-measure before relying on a row; a stale list
licenses a change that silently breaks an unlisted caller.

The `EnemyBlips` producer/consumer contract between FIELD's TU and GLASS's
minimap is the same shape wearing a member variable instead of an API, and O155
already rules it a declared crossing.
