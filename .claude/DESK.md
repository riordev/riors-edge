# Desk — next playtest

## OWNER PLAYTEST, 2026-09-10 (SECOND) — THIS OUTRANKS EVERYTHING BELOW

LANDED FROM IT ALREADY:
- "the gym enemies are suddenly inside fernhall" — FIXED, AND THE FIRST
  DIAGNOSIS WAS WRONG. I blamed F1; he replied "i never pressed f1" and he was
  right. NO KEY IS INVOLVED. ABreakerCharacter::Tick calls ResetPlaytest on
  fall-out-of-map recovery — below spawn minus 40 m — and ResetPlaytest calls
  ResetPlaytestTargets, which destroys EVERY enemy in the world and rebuilds
  the GYM's target dummies and standing encounter wherever it is standing. One
  fall through a gap in Fernhall deleted the yard's patrols, the courtyard, the
  quest's elites and every repopulation slot's occupant, and put gym content in
  the hole — silently, with nothing the player did to explain it.
  A fall now calls RecoverFromFall: the player comes back, vitals restore, and
  the world is untouched. Ammunition and playtest stats are NOT reset either —
  a fall is an accident mid-run and a run whose numbers reset stops measuring.
  AND IT LOGS THE POSITION IT FELL FROM, which is the tool for the next half:
  WHERE the hole is. Candidate found by arithmetic and NOT confirmed: the
  composed floor has an enclosed void at x 143..153.5, z 89..92 between the
  substation's north wall and the second seam's corner. It is walled on three
  sides, so reaching it needs a mantle or a dash over a wall. The next fall's
  log line settles it.
  The F1 guard landed too and is still right — that path could do the same
  damage on purpose.
- "multiple bars ... which dont help at all as to what they are ... keep it as
  one combined bar" — shield and health share one track now, split by their
  maxima so the seam does not slide while you are being shot, and the 20% tick
  is cut at 20% of the HEALTH pool rather than of the whole bar. And the
  resource rail finally DRAWS ITS LABEL: it has computed one (MANA, MOMENTUM,
  SCRAP, GRIT, CHARGE) since it was written and nothing ever drew it, which is
  most of "dont help as to what they are". Four rails down to three, all named.
- "enemies should stagger or flinch when shot" — the STAGGER state already
  ships and is applied by a Tank ability and the Warden slam, which is correct
  and stays: a rifle round that staggered would be a stunlock. What was missing
  is the cosmetic half. Bodies rock back along the shot and settle over 0.12s,
  harder on a weak point. The Lattice does not flinch — it is composed
  primitives by ruling and has no named body to rock.

STILL OPEN FROM IT, IN HIS WORDS:
- "theres absolutely no shape to fernhall at all just the main opened portion".
  THE BIGGEST ONE. Three yards of flat ground with cover on it is not shape.
  Wants verticality, structures, enclosure — a composer job.
- "i havent seen any chest" — MEASURED AND ANSWERED. Replaying his own session
  seed put four of six chests 14 to 19 metres off a lane the player walks down
  the middle of. The spread is RIGHT: being off the lane is the reward for
  leaving it. What was missing is that a dark 90 cm box at that range says
  nothing. Each chest now carries a gold mote above the lid on a slow breath,
  which goes out when it is opened. Still not on the map, so it is still found
  by looking. If he crosses a yard and still misses one, the next lever is the
  spread, not the light.
- "rot looks so weird casting sometimes its in the air or half baked".
- "warden or random enemy inside fernhall station rift just reflects a portion
  of my damage back at me" — a modifier doing its job with no tell, or a bug.
  Find which before touching it.
- "the skill menu is so overwhelming and displays way too much information".
- "i cant make it to the end of the rift even with all those extra points
  because my base character is just so weak". Balance, and it is the same
  complaint as "abilities feel irrelevant at the start" from the last message.


## THE LIST IS DONE. FOUR THINGS WANT YOUR HANDS, NOT MINE.

Every item from the Fernhall message landed. What is below is the record; what
is here is only the part that cannot move without you.

1. THE LATTICE HAS NO BODY BY YOUR OWN RULING (2026-08-29,
   Assets/enemies/LICENSE-NOTE.txt). If you are overturning it, say so and it
   is quick: Enemy_EyeDrone and Enemy_QuadShell are both in the repo with full
   rigs and animation sets, and NOTHING references either.
2. THE WEAK POINT'S SIZE. The ball was the hitbox drawn true — 40 cm of mesh
   over a 20 cm collision radius — so it is a ring now rather than a smaller
   ball. If it still reads too big, the honest lever is the HITBOX, and
   shrinking that makes weak-point shots harder. That is your call.
3. THE WATCHKEEPER STANDS IN A BUSH. His authored marker is a few metres from
   dress_trees05. Moving him is a composer re-export.
4. NOBODY HAS HEARD THE GUNS. Eight reports now differ in length, weight and
   colour and none of them clicks or clips — a test can prove that much and
   cannot prove one sounds like a gun.

AND ONE THING TO KNOW: chest items are NOT inside the pinned drops-per-hour
band. That band is projected from a kill rate and a chest is not a kill.


## THE FLAKY ABILITY TESTS ARE FIXED, AND IT WAS NEVER TEST ORDER

THE DIAGNOSIS ON THIS DESK WAS WRONG. It read as order-dependence because a
test passed alone and failed in the suite; it was a 5% COIN FLIP, and a longer
run simply flips the coin more often. Three different tests were failing on it
across four runs, which is what finally gave it away.

THE MECHANISM, measured rather than reasoned: UBreakerAttributeSet's default
critical chance is 0.05, and three fixtures compared damage figures that a
critical multiplies by 1.5.

- MultispellPurchasedRuntime asserts two detonations are EQUAL. One critting
  made them 405 and 270 — exactly the 1.5x, which is why the ratio looked like
  three status types against two and sent this desk after a status bug.
- SnapshotDisciplineRuntime asserts Cleave pays its quoted 15 Mana. A probe
  printed the truth: the swing leaves the victim on 26.6 of 100, so a critical
  KILLS it, a kill pays the Caster class resource back, and the spend reads 11.
- CleaveAcceptedHitRuntime asserts both accepted hits carry Bleed. A critical
  makes the swing lethal, and the file's own next branch says a lethal hit
  cannot carry Bleed.

FIXED BY ZEROING THE CASTER'S CRITICAL CHANCE in those three fixtures. None of
the three assertions is about criticals; SnapshotDiscipline still exercises them
fully, because what it asserts is the PRODUCER's +25 percentage points and that
one sample is recorded and reused, and those read from the base it is given.

A WRONG FIX WORTH REMEMBERING: raising the targets' health for headroom made
CleaveAcceptedHit fail CONSTANTLY instead. A fixture cannot just set MaxHealth —
the pool is re-derived and snaps back to 100, so the assertion then compared
10,000 against 100. Critical chance is read straight off the attributes at cast
time and does stay put.

Five consecutive RiorsEdge.Abilities runs clean, then a full suite: 880 passing,
3 expected red, 0 unexpected. Rot's cast time is no longer pinned by this.

## THE FEEL REVIEW ARRIVES AS A MESSAGE, NOT A FILE

The owner sends it after playing. The lowest-scoring beat that names a
one-thing is the next cycle's work and it OUTRANKS everything in this file:
this desk is the QUEUE, that message is the DIRECTION.

The seven beats and what each is trying to do — kept here because the seat
needs the target to read a score, not because anyone maintains a form. These
are drafted from Docs/VISION.md and the owner overturns any of them in a
sentence; a wrong target misdirects every cycle that trusts it.

1. FIRST 5 MINUTES — you know what you are, what you are holding and where to
   go without being taught, and moving is fun before anything else is.
2. FERNHALL COMBAT — the pack makes you MOVE, danger reads off silhouette, and
   a kill feels like your gun and kit doing work.
3. FIRST TRANSFORMATIVE DROP — it changes how you PLAY and you can tell without
   reading a number. A build that changes nothing observable is not a build.
4. QUESTS — you always know the next thing and why, and handing one in pays
   rather than merely ending.
5. FIRST RIFT — somewhere hostile and foreign running different rules, not the
   same ground with a filter on it.
6. RIFT COMPLETION — lands as something achieved and pays for it. The fiction's
   weight is deliberately unspoken, so the payoff carries the moment alone.
7. QUIT AND RETURN — immediately re-oriented, nothing to re-learn.

Scored against the TARGET, not against a finished game. 1 bad, 3 inert,
5 would show someone. THREE IS THE DANGEROUS ONE — not broken and not doing
anything is how a slice ends up correct and dead.


## OWNER PLAYTEST, 2026-09-10 — READ THIS BEFORE PLANNING ANYTHING

LANDED SINCE HE WROTE THIS (3b0c3465, 0af06a89, ba24dd2d, 236767fc):
- The quiet pass. Damage groan, footsteps, the RELOADING callout and the
  DAMAGE-taken word are DELETED, not muted — call sites, verbs, voices, waves,
  the footstep component and its test. The dev bar is part of the F3
  diagnostics now instead of permanent chrome. "SKILL n" draws only above
  MinLevel; deleting it outright is one if-statement if he wants that instead.
- The three that were MISSING. The weapon name rests on screen (it existed only
  as a swap slide that animated in and left), XP exists at all for the first
  time (a level and a rail), and the duplicate max is gone at full health.
- Gear names. Every affix authors a one-word nameWord; names are now
  "Steady BOOTS of Vigour". The three-word ceiling is enforced by the loader
  and swept over all 96 pooled affixes, and the census exporter round-trips the
  words so a re-export cannot delete them.
- Speed. Walk 672, sprint 1039, both +5% so the 1.55x gain is unchanged and is
  now pinned as a ratio.

STILL OPEN FROM HIS LIST, IN HIS WORDS:
- "the reticle is rough as well and the gun like bounces around your crosshair
  awkwardly" — NOT STARTED. Needs to know what "rough" means before guessing.
- "my gun sounds like a nerf gun" — not started.
- A MINIMAP is wanted. It is the one NEW system he asked for; sequenced after
  the subtractions on purpose.
- Quest text top-right is ugly.
- "all of the abilities just feel so irrelevant and unfun at the start" — the
  biggest item on his list and the least like the others. It is not polish and
  probably is a system change; its own block, once the HUD is quiet.
- Inventory clutter beyond the names.
- FOLLOW-UP: the swap picker's plate was widened 640 -> 800 to hold a
  68-character worst-case name. Names are far shorter now, so it can probably
  go back — left alone because the harness cannot open that modal.

The owner played 9b64906e and the verdict is not about any feature landed this
session. It is that the game reads as a TEST ENVIRONMENT rather than as Rior's
Edge, and that breadth is outrunning proven fun. His words: "The project may be
expanding breadth faster than it proves fun ... I would be wary of adding many
more systems before one 20-30 minute route feels excellent."

THE STANDING INSTRUCTION THAT FOLLOWS FROM IT: no new systems until one route
feels excellent. Prefer DELETION and polish over addition. A cycle that adds a
mechanic needs a reason that survives that sentence.

He also names three risks worth keeping in front of the seat:
- Automated proof as a SUBSTITUTE. Tests cannot establish weapon satisfaction,
  movement flow, encounter rhythm, spatial readability, reward excitement, or
  whether a build choice feels transformative. Activity is still oriented
  toward what can be asserted.
- Maintainability. ~189k lines of C++; BreakerMenu.cpp ~615 KB,
  BreakerGameMode.cpp ~272 KB, several more at 150-220 KB. Splitting by owner
  is already a legitimate work item (lanes.md says so at 3,000 lines).
- Scope. Campaign, seven destinations, 34 abilities, coop networking and
  generated rifts are all competing for one slice's attention.

### HUD, and he calls these "little shit that makes things feel bad"

- "SKILL 1" prints above the ability on E. Delete the label.
- RELOADING draws as giant centred text (BreakerPlaytestHUD.cpp:505) while an
  animation already says it. Delete the callout; the magazine rail already
  turns orange and fills (884-918).
- DAMAGE appears when the player TAKES damage. Not wanted at all.
- The reticle is rough, and the gun bounces around the crosshair awkwardly.
- XP IS NOWHERE ON THE HUD. Checked: BreakerPlaytestHUD.cpp has no experience
  readout of any kind.
- "i dont know what weapon is in my hand". The weapon name exists but ONLY as a
  swap slide (944-950): it animates in on a swap and then leaves. There is no
  resting weapon identity.
- Quest text top-right is ugly ("PUT THE MARKED ONES DOWN 0/3").
- A MINIMAP IS NEEDED.
- The top-left dev bar (F1 RESET / F2 REPORT / F3 DIAGNOSTICS / ESC MENU,
  BreakerPlaytestHUD.cpp:580) is ugly and half of what it offers is no longer
  relevant.
- The health/resource cluster bottom-left reads cluttered in his frames: the
  big number overlaps a small duplicate of itself and a second figure.

### Audio

- The character GROANS when taking damage. Remove it. "so fucking annoying".
- Footsteps, same verdict (Audio/BreakerFootstepComponent ->
  ABreakerSoundDirector::PlayFootstep).
- "my gun sounds like a nerf gun".

### Items

- Inventory clutter is "so ugly".
- GEAR NAMES ARE WRONG. THREE WORDS MAXIMUM. His scheme, verbatim: "assign a
  special word per prefix then item type then affix with a quirky name". So the
  O267 grammar stays but the WORDS stop being raw affix display names —
  "Movement Speed BOOTS of Ailment Avoidance" is the failure case, from his own
  frame. This is the authored-flavour-words data pass O267 always named, plus a
  hard three-word ceiling the naming rule must enforce rather than hope for.

### Kit

- "all of the abilities just feel so irrelevant and unfun at the start". This is
  the starter experience, not a tuning number.

### Movement

- Needs subtle polish and a SLIGHT INCREASE IN SPEED. Note this arrives after
  the landing/slide repairs landed, so it may be a reading of those.

### Codebase

- "a lot of the codebase also might just need large QoL and deletions on things
  that arent relevant anymore at all".


## Working scope

Continue validated blocks until the owner returns. Prioritize playable changes for the next large playtest. Investigate an individual bug for at most five minutes, then record its evidence and park the affected change if unresolved. Independent agents stage on disjoint files; one engine build/suite runs at a time. Every landed block follows BUILD → SUITE → Scripts/status.py → COMMIT → PUSH. Larger changes receive independent review. Never count placeholders or pure-maths tests as complete gameplay.

## Immediate queue

1. Cooperative sandbox landed with real host/guest fire and replicated health/death evidence. Owner slot metadata and guest health/input respawn transport now verified; real Slipcut prediction/payment/server cadence and same-key cleanup now verified; remaining acceptance: other abilities/classes, latency/rejection bank convergence, drop contention, rejoin, human presentation/movement feel and eventual campaign support.
2. Red Basin/Station Zero/Port Meridian/Broken Coast/Shatterpoint prototypes and Station priority hunt landed; Red Basin recovery/extraction, Broken Coast uplink transmission and Port Meridian ground-crew escort also landed; next content acceptance is real return Rifts, route pacing and region scenery. Refine exposed boundaries and repeated cover. Anchor surroundings/work-area dressing landed but remains blockout art.
3. Volatile countdown and ability-menu wrapping/empty-label fixes landed with rendered checks. Inventory destruction prose now wraps within its plate and failed single-item removal reports failure. Continue interactive focus/death flow and other menu screens.
4. Finish remaining Afterimage consumers and real-clock Rot feedback checks. Cache prompt now shares opening eligibility and stays visible at the console; final prop art remains.
5. Kit expansion under O252–O260: current34 registered entries are11 actives and10 passives short of55. Carom, Coup, Backstep, Pyre, Riftlance, Recall, Bulwark and Overwatch are named candidates without sufficient authored mechanics in the current docs. Rover/Barrel need new mechanics; Wildcard has O257 but needs Forge/foreign-grant/resource infrastructure. Do not fill the count with clones or call existing innate nodes slot passives.
6. World Core Point coverage counts mission Unlock.CorePoint references. Four of15 sources are authored; the remaining11 are unmapped content, not eleven broken wallet writes. Startup and flag delivery already settle grants. Assign real source/objective identities before adding any grant; do not award a named source from a merely similar mission.

## Seven-step acceptance checklist

| Step | Present evidence | Remaining acceptance work |
|---|---|---|
| 1 Entropy | Finite funding, original critical samples, paid duration and persistence tests landed | Actual encounter clarity, audible feedback and sustained ability/converted-weapon play |
| 2 Elements/reactions | Entropy, Void, Rift and bounded reactions have native delivery tests | Regression check current builds, readable combined feedback and complete encounter loop |
| 3 Progression/combat | 187 Core nodes; 370 total nodes; no measured silent nodes; Caster doctrines12/24; allocation parity0.93 | Audit actual consumers/acquisition, remaining kit programme, solo-node gaps and gameplay balance |
| 4 Loot | New special budgets, overflow conversion, Unwritten rename and skill-level affixes landed | Authored perks/legendaries, real legal-loadout comparison and UI clarity; legacy-item archive/migration protection now tested |
| 5 Interface | Core wheel and paid purchase/assignment paths exist | Occlusion, clipping across menus/resolutions, NPC/equipment/death focus flows and dev sandbox |
| 6 Fernhall/Rifts | Five outdoor pockets/17 enemies, Watchkeeper contract, campaign flags and local map exist | Further authored spaces, distinct Rift interiors, reward/return and natural leveling; guarded physical caches now validated |
| 7 Visual/audio | Placeholder presentation and combat feedback exist | Anchor/Fernhall identity, weapons/arms/weakpoints, coherent sound/VFX and signature encounters; inspect captures, do not claim final art |

## Destinations and missions

- Anchor 13: main hub; Fernhall Approach: overgrown industrial with a cityscape.
- Red Basin: scorched farmland/crater. Station Zero: overrun research hub. Port Meridian: destroyed airport/terminal. Broken Coast: ocean/flat landscape. Shatterpoint: city sprawl.
- Improve Anchor/Fernhall routes and landmarks; author playable placeholder destinations/tilesets and varied objectives using available assets. Ordinary regional difficulty stays fixed; return Rifts provide harder encounters.
- Existing local-map site IDs contain coordinates: replace with stable identity and preserve discovery before relocating existing sites.
- Campaign Substation enclosure candidate is parked after five-minute native arrival-floor failure: marker600,0,0 lies inside imported floor bounds, but simple WorldStatic floor query returns no hit. Roof, light positions and structural identity pass. No new interior landed; candidate/diagnostics retained outside checkout.
- Native Rift wave/boss/purse/exit-offer checks now wait through the real breather clock. Complete door entry→cross-map return and combined earned Act1 journal/reward persistence remain unproven as one chain; use actual accept/arrival/death/pickup/turn-in consumers, not restored completion flags.
- Rift interior currently reuses the yard. BuildCoverField generates gym-specific sections; FernhallFieldParams validates authored layouts and is not a compatible generation recipe. Use a genuinely band-aware generator or independently composed interiors; retain RiftGeneratedField as a measured probe.
- Rift seed arithmetic now hashes canonical encounter text rather than process-local FName indices; golden70439488 verified for breach.marshalling/area12/base20260814. Nonempty-ID generated arrangements change once; no persisted run-seed schema exists. This is deterministic input arithmetic, not full topology/transport acceptance.
- O262 acquisition/modifier/failure rulings remain needed: where keys drop, allowed modifier pool/count, and refund versus retained admission after failed travel. Current equipment-shaped item storage cannot safely impersonate a key.
- O262: command post consumes a key carrying area level/modifiers and determines layout. Consumable entry and death budget land together. A key, device and tileset are not complete until enter → fight → reward → return works.
- Add mission variety only with a real objective consumer. Current ordinary quest progress supports kills and the specific residue collection path; generic collection needs implementation.

## Multiplayer

Opt-in fixed-Fernhall cooperative sandbox exists: distinct transient Swift profiles, client geometry, authored late-guest start, no persistent writes, scoped friendly-fire prevention. Real two-process guest fire/server damage/death/client health transport passed; see Docs/reports/coop-combat-2026-09-09.md and Scripts/coop-combat-verify.ps1. Owner slot IDs/real GAS specs and actual guest death/server-timer respawn/input restoration are verified. Real Slipcut local/server payment and prediction-key cleanup are verified. Full campaign/progression, other abilities/classes, exact bank convergence under latency/rejection, loot contention, rejoin and human presentation/feel remain unvalidated. Trading, account services and MMO infrastructure stay outside this playtest slice.

## Known issues to time-box

- WARDEN WALK-THROUGH: FIXED, and the parked fixture was never the obstacle. The arrival ring lived in exactly one place, ABreakerEnemy::TickEngagedBehaviour, and ABreakerWardenEnemy::TickEngagedBehaviour replaces that function WHOLESALE without ever calling Super — so it wrote a full-speed vector straight at the player and fell off the end of the function with it intact whenever neither attack was armed. Nothing downstream re-added spacing, by design: the mover has no stop distance while steering because the ring "is always the behaviour's to govern, never the path's". The pass-through is the project's own recorded consequence of the player being a Pawn. The ring is now an extracted method every closing archetype CALLS rather than inherits, so an override cannot silently drop it. Base behaviour is bit-identical; only the Warden changes. Verified: the Boss is the sole override that calls Super, and ArrivalBand is written in exactly one statement, so no fourth class was affected. THE WIRING PIN IS NOW LANDED (RiorsEdge.Combat.Warden.ArrivalRingRuntime) AND THE RECORDED ANSWER WAS WRONG: a bare AActor can never be the target, because IsEligibleThreatTarget (BreakerEnemy.cpp:851) refuses anything that is not an ABreakerCharacter or an ABreakerDeployable with a live combat component, so it is never SELECTED and the engaged tick never runs against it. ResolveSweep/ResolveSlam bailing is true and irrelevant. The fixture uses a real character at its shipped 100 health and grants it nothing: neither attack is ever armed, because the approach leg runs entirely outside SlamRadiusCm (650) and asserts both that it stayed there and that the health is untouched. Leg order is load-bearing — closing inside slam radius arms a 0.9s wind-up that plants the body on every later frame. FIXED AT THE SITE AND DELIBERATELY NOT PINNED: the same override wrote StateLabel = ADVANCE AFTER the ring call, clobbering the ring's own ATTACK/BACK OFF, so a Warden holding station or backing off printed ADVANCE on the playtest HUD. The label now precedes the call as the base chase does; the state is reachable only with both attacks on cooldown, which a damage-free fixture cannot reach.
- TRAVEL POINTS ARE A DEVICE NOW, NOT A POST. Owner: "the travel points can
  stop being just a pillar lets make a minor asset for them". Still primitives,
  because importing a prop is a pipeline job and this is not — but four of them
  instead of one: a low plinth, three raked struts leaning in around the beam,
  and a collar where they meet. Three struts rather than four because three
  reads as a mount and four reads as a cage, and an odd count never presents a
  flat face however the player walks up. The strut placement is derived from
  one radius and one lean rather than three hand-written rotators.
  The beacon column is untouched: it does the navigate-by-it job and he did not
  complain about it. All cosmetic — the capsule, the interaction range and the
  destination list are the same.
  PHOTOGRAPHED in the Anchor, where the hub's travel point reads as a tripod
  with the beam standing through it.
- QUEST-MARKED ENEMIES: THE BUG WAS ARITHMETIC, AND THE DESK'S EARLIER READING
  WAS HALF WRONG. It said the owner was mistaking ordinary elites for quest
  ones. Elites ARE the quest ones — Quest.Pattern's objective is elite-gated —
  and they ARE marked: the rank paint washes them amber and they carry a bar
  above trash. The defect is that the quest asks for THREE marked kills and the
  world stood TWO, so the counter reached 2 of 3 and stopped, which reads
  exactly like a broken quest.
  Two more elites, both in the SUBSTATION: three are met before the depot and a
  fourth waits past it. Nothing went into the entry yard, because a cleared
  entry yard has to stay under the 279 XP that reaches level two and an elite is
  worth more than a Skitter. Measured: entry clear still 207.
  The encounter test asserts the RELATIONSHIP as well as the count now, so a
  population change that drops below what the contract asks fails and says why.
- WEAK POINTS: THE SOCKET HALF ALREADY SHIPPED; THE DRAWING IS WHAT CHANGED.
  Owner: "find a way to fix the critical spots to the models so they dont just
  have random circles coming out of them". The sphere has ridden the Head bone
  and tracked the gait for some time — this is the SEVENTH note claiming
  something missing that ships.
  AND THE CIRCLE IS NOT DECORATION, which is the finding that matters: the
  engine sphere is 100 cm and the authored 0.4 makes it exactly 40, which is
  exactly twice the collision sphere's 20 cm radius. THE BALL IS THE HITBOX,
  DRAWN TRUE. So the first fix — shrinking the visual so it sat inside the head
  — was WRONG and was reverted before it landed: it would have made the picture
  lie about where a weak-point shot lands.
  What landed instead changes the SHAPE and not the size: a dashed gold ring at
  the hitbox's own radius, read from the collision sphere rather than restated,
  so the drawing still says exactly how far the weak point reaches and the head
  is visible through it. Same argument as the Volatile blast going from a
  filled disc to a dashed ring. Photographed with -BreakerCaptureWeakPoint,
  which is new because nothing in the ordinary route puts an enemy near enough
  to judge one.
  STILL OPEN AND IT IS THE OWNER'S: if the ball still reads as too big, the
  honest lever is the HITBOX, and shrinking that makes weak-point shots harder.
  That is a balance call, not a legibility one.
- THE WATCHKEEPER IS A PERSON NOW. Owner: "replace the random npc in fernhall
  with one of the human assets we have for the time being and check the
  dialogue in general". Both halves done.
  THE BODY: the rigged bases and the skeletal path BOTH already shipped — the
  two Anchor NPCs have worn them for some time. The Fernhall contract giver
  goes through a THIRD, generic spawn path that set no body at all, so the one
  person the player meets in the field was a cube with a sphere on top while
  the two in the hub were people. The asset paths were written out at each of
  the two named spawners; they live in ApplyHumanBody now and every NPC through
  the generic path gets a body and a talking idle. Male base, because Kess
  wears the female one in the hub and two people should be told apart.
  THE DIALOGUE: READ AND IT IS FINE. Four nodes, terse and in voice, and the
  flag flow is correct — Offered, Accepted, FlankCleared, TurnedIn, each gated
  on the one before. He says "Six of them" and Quest.Watch asks for six; the
  two agree, which is the thing that would actually have read as broken.
  FOUND, NOT FIXED: he stands in a bush. The contract marker at (13, -14) is a
  few metres from dress_trees05 and its red foliage crosses his shoulder in
  frame. Moving him is a composer re-export, which is a cycle of its own.
  -BreakerCaptureNpc photographs the nearest speaker; the ordinary route walks
  straight past him, which is why nobody noticed the missing body.
- EXCEPTIONALS AT LEVELS 1-13: THINNED, NOT GATED. Owner: "exceptionals
  basically drop instantly which they shouldnt (i think we should reduce their
  drop rate a little bit in the earlier levels 1-13)". Raising the unlock would
  have been the wrong reading — he asked for LESS, not none, and the unlock at
  item level 8 already answers "none" for 1-7. So the Exceptional WEIGHT now
  ramps from 35% of itself at ilvl 8 to its full value at 14 and is untouched
  above that: the gate empties 1-7, the ramp thins 8-13, and 14 onward is the
  table he has not complained about. Measured through the analytic projection:
  a trash-only hour pays 2.32 Exceptionals at ilvl 8 against 6.19 at ilvl 14.
  The pinned drops-per-hour band is unmoved at 134.
  THE SUITE CAUGHT A DELIBERATE DUPLICATE: BreakerDropChanceOverflowTests keeps
  its own copy of the pre-O249 weight formula so it can prove O249 changed
  nothing below the Drop Chance cap. The ramp is a later, separate ruling and
  belongs on BOTH sides of that comparison — carrying it on one side made the
  ramp look like an O249 side effect. This is exactly the repeated-shape defect
  Scripts/shapecheck.py exists to find.
  AND A CHEST DEFECT THE SAME RUN FOUND, which the earlier fix had NOT closed:
  an enemy capsule does not block the Pawn channel the way a chest does, so
  widening the placement overlap still could not see a patrol. Chests ask the
  bodies directly now, with 90 cm of walking room. Three consecutive Fernhall
  runs clean afterwards.
- WEAPON SOUND PASS, PER ARCHETYPE: LANDED. Owner-authorised, and the desk's
  framing was half wrong: the per-archetype ROUTE already shipped
  (ABreakerSoundDirector::PlayWeaponFire, ArchetypeFireWaves), it just resolved
  to an authored .wav per archetype and none is authored — so all eight fell
  back to one shared render. "my gun sounds like a nerf gun" and "i dont know
  what weapon is in my hand" are the same finding, and the second is worse.
  Eight voices now, synthesized: a Sidearm short and bright, a Sniper a hard
  crack with a long tail, a Shotgun mostly body, a Rocket nearly all rush, the
  three rifles close to each other and far from those. THE RIFLE IS UNMOVED on
  purpose — it is the gun the owner has held through every playtest.
  TWO NOISE COLOURS, both memoryless because every sample must stay a pure
  function of its index or the determinism the whole header rests on breaks: a
  first difference (a 6 dB/octave high-pass) for the crack, a quarter-rate
  interpolation for the body and the room. Measured: 2670 crossings against 481
  over the same window.
  A REAL DEFECT FOUND BY MEASUREMENT: the shared envelope's release reached
  zero AT the duration, but the last rendered sample sits one sample short of
  it — so a loud enough cue ended on 1 instead of 0, which is a click on every
  shot. The Machinegun was the first voice loud enough at that instant to show
  it. Fixed in Envelope, which affects every cue in the game.
  NOT ACCEPTED: nobody has heard any of this. An automated run can prove eight
  reports differ in length, weight and colour, and can prove none clicks or
  clips; it cannot prove one sounds like a gun.
- RIFT DEBRIEF: LANDED. Owner-asked, "i really like the entering rift screen so
  maybe when a rift is closed we can add something very similar that shows the
  items we gained ... kinda like a loot highlight". Built as the BRIEFING'S
  TWIN: same struct-first shape, every string and number composed before the
  pane sees them. A kicker, RIFT CLOSED, the area, then the run's haul best
  first (rarity, then item level, then name, so two runs of the same haul
  cannot disagree), the Riftglass and XP gained, and ONE verb — the run is over
  and the only question left is when the player has finished looking.
  ITEMS TAKEN, NOT ITEMS DROPPED, and that needed a real run ledger: it is fed
  by the equipment component's own acquisition delegate, which is the single
  funnel every item entering a backpack already passes through. A backpack
  difference could not tell a drop the player took from one they discarded to
  make room for it.
  A HAUL LONGER THAN THE PANE IS COUNTED, never silently cut: the player counts
  their pack afterwards, and a screen that quietly dropped four items is the one
  failure a reward beat must not have.
  THE CAPTURE CAUGHT THE LAYOUT: the first frame returned the bare column and
  the host stretched it — the kicker started off the left edge and CONTINUE ran
  the full width of a 1920 screen. It wraps in a scrim and a centred column now,
  as the death beat does. -BreakerCaptureMenu=RIFTDEBRIEF photographs it.
  NOT ACCEPTED: photographed on the empty-handed variant only, because the
  harness cannot run a rift to completion. Whether it lands as an achievement is
  beat 6 and the owner's alone.
- SUPPLY CHESTS: LANDED. Owner-asked, "randomly spawning chests the player can
  open with either currency or an item in there weighted more towards lower
  value stuff". Two per yard, six across the three, placed by ONE session roll
  so a route walked twice is not identical the third time; not on the local map,
  because marking them turns "found something while crossing the yard" into
  "walk to the pin". Contents are a pure function of the seed the log prints:
  62% currency, 38% an item rolled at trash odds with the player's own Drop
  Chance deliberately NOT applied, then stepped down a tier half the time.
  Shaped as the cache's subclass of ABreakerNPC, which is what puts it on the
  F key with no change of lane; the difference from a cache is that it is
  UNGUARDED, and that is why it pays less.
  TWO DEFECTS THE SUITE CAUGHT WITHIN A MINUTE OF EACH OTHER: a chest tested
  only its OWN capsule for clearance, so one landed inside a patrol and turned
  a shipped-configuration assertion into a dice throw; and a chest paid the
  KILL currency roll straight through, which is 0-1 Riftglass for a trash body,
  so one credited nothing at all. Chests now clear the widest body that can
  stand there, and the payout has a floor that climbs with the yard.
  A THIRD, AND IT IS A TRAP FOR ANY FUTURE SESSION ROLL: the seed came from
  FMath::Rand, which advances a PROCESS-GLOBAL stream every test in the suite
  shares. It is a GUID hash now.
  OWED, and the owner should know: chest items are NOT inside the pinned
  drops-per-hour band. That band is projected from a kill rate and a chest is
  not a kill, so the measured 134/hour does not see them.
- FERNHALL IS THREE YARDS NOW. Owner-asked: "expand the size a lot as well and
  add more pockets". The DEPOT sits past a second dog-leg seam out of the
  substation's north flank, same 106x56 footprint and the same frame-relative
  lattice, and it carries three pockets of its own: two flanking fights of plain
  melee and a Warden-and-elite set piece in the middle. 113 -> 167 pieces,
  2 -> 3 yards, 5 -> 8 pockets, 17 -> 26 outdoor bodies, and every cover
  measurement is unmoved (uncovered gap 1450 against a 1700 ceiling, corridor
  1050 against a 900 floor). NO RIFT DOOR, deliberately: a yard with no door is
  a legal yard, a second door into the undercroft would be two ways into one
  place, and a third rift definition would be an encounter nobody can reach.
  NOTHING WAS ADDED TO THE ENTRY YARD — its clear has to stay under the 279 that
  reaches level two.
  A MEASUREMENT WAS WRONG AND IS NOW FIXED: the entry-yard XP pin skipped pocket
  2 by name and counted pocket 4, a SUBSTATION fight, as entry XP. It reads the
  body's own area level now, so 267 becomes 207 and no yard added anywhere else
  can move it again.
  OWED: the depot has no quest, no cache and no interactable — it is ground and
  fights. -BreakerCapturePocketRift=<n> photographs the nth tear by distance,
  which is the only way to see the far yards at all.
- POCKET ARRIVAL TEARS: LANDED, AND THE ARRIVAL FICTION IS NOW WHOLE. All five
  yard pockets have a small rift 750 cm behind the formation — a ragged vertical
  lens of dashed segments on the additive glow, teal because teal is
  canon-reserved for rift objects — and every slot in a pocket arrives out of it
  and walks to its post on the shipped patrol path. It flares on the frame a
  body comes through and eases out over 1.6 s. Photograph it with
  -BreakerCapturePocketRift; the ordinary Fernhall route never gets behind a
  fight to see one. THREE DEFECTS THE CAPTURE FOUND AND REASONING DID NOT, one
  of which is a trap for every future in-world visual: (1) Sin(PI) in single
  precision is -8.7e-8, so a fractional Pow of it is NaN and the tear's top
  point NaN'd every segment touching it; (2) A MATERIAL MUST DECLARE
  MATUSAGE_InstancedStaticMeshes TO DRAW ON AN INSTANCED COMPONENT — probed:
  /Engine/EngineMaterials/EmissiveMeshMaterial (the additive glow every tracer
  and beacon uses) does NOT, /Engine/BasicShapes/BasicShapeMaterial (which the
  blast ring uses) DOES, which is exactly why the first tear photographed SOLID
  BLACK while the blast ring drew orange from the same shape. So instancing and
  additive light are mutually exclusive with the engine content in this tree;
  the tear is 27 plain mesh components because it has to read as light; (3) a
  clean lens with three level bars reads as an EMBLEM, not a tear — the edge now
  wanders on a deterministic hash and the fractures are slanted and lopsided.
  STILL OWED: the flare fires AT the spawn, not before it, so nothing warns the
  player a body is coming; a lead-in needs the refill split into two stages. No
  audio cue. The courtyard keeps its authored doorway and gets no tear, which is
  right — it is the one pocket with real geometry to walk out of.
- ARRIVAL FICTION: THE COURTYARD HALF IS LANDED. Its roster was DISCARDED at its call site, so the one pocket with an authored doorway was the only one that never repopulated. The slots register now and a returning patrol appears at the bay mouth, faces its post and walks there on the shipped patrol path (ConfigureEncounter makes the post the leash origin). The clearance gate reads where the body APPEARS rather than where it is going, which is the honest reading of the rule it encodes. The pocket tag is carried on the slot instead of being printed back from an int, which could not express the courtyard's own tag. STILL OWED: the other four pockets have nothing to arrive from — authoring mouths across the yard is a composer job — and no arrival VFX or audio cue exists, though the pooled primitives that would draw one without art do.

- ARRIVAL FICTION, THE ORIGINAL SCOUT: SCOUTED, PLAN READY, NOT BUILT, and the reason is geometry rather than plumbing. Both arrival sites are single-line location decisions already fed a point by their caller (RefillOutdoorSlot spawns at Slot.Home; AcquirePooledEnemy at the SpawnLocation passed to it), so an emergence point needs NO new plumbing to reach them. The distance half is already engineered and honest too: the wave solve places its arena through SolveContainedSpawnCentre against an authored band and LOGS when the yard cannot hold it. What is missing is something to come OUT of. The composed Fernhall contains no door, gap or opening usable as an arrival point except two seam mouths (compose_fernhall.py:97-98, 155-157), which are walked routes; every other wall_* piece is a solid abutting slab. ONE REAL DOORWAY SHIPS AND IS ALREADY BUILT: BreakerFernhallCourtyard::MakePlan/Build replaces a perimeter piece with an 8m x 7m bay of shoulders and lintel (BreakerFernhallCourtyardBuilder.cpp:145-147), it is in the shipped yard (BreakerZoneBuilder.cpp:685-686), and its transform comes from a pure function the game mode already calls. Door_Metal art is committed, placed in four builders and asserted by a test. No portal, spawner or ingress actor exists; ABreakerRiftDoor is a player travel point and is NOT reusable. No arrival VFX or audio cue exists, but the pooled primitive vocabulary that would draw one without art does (ABreakerEffectRenderer AddGlow/AddStroke/AddBlinkLight) and GROUND is already a named consumer. NO FRUSTUM OR VIEW TEST EXISTS ANYWHERE in the project, so the repopulation clearance gate is a plain distance where it could be a view test. THE CYCLE WHEN IT COMES: register the courtyard roster as slots (it is discarded at its call site today and registers none, a gap this session opened) and give that pocket the doorway as its arrival point, walking the body out of the bay. Do NOT build a general arrival-point system first: for four of five pockets there is nothing to arrive from, and authoring mouths across the yard is a composer job, not a code one.

- RIFT OBJECTIVES: CLEAR IS BLOCKED ON CONTENT, NOT ON PLUMBING, AND THE PLAN ABOVE MISSES IT. Both shipped rift encounters carry an AUTHORED STORY BOSS — Data/missions.json gives fernhall.substation the Holdfast (Quest.Deeper.SweepDone) and breach.marshalling the FieldMarshal (Quest.Breach.MarshalDown) — and CompleteRiftRun only writes those journal flags when bVerifiedStoryBossDeath is true, which requires the authored boss to have died. A Clear run has no boss, so running Clear on either shipped rift would SILENTLY STALL THE CAMPAIGN. There is no third rift for it to live in, and content the player cannot reach is not built. The completion predicate itself is a half-hour (live enemy count at the final wave, beside the existing terminator) and the runtime-only-vs-serialized question is already answered by the house rule: keep it runtime-only until it has proved itself. WHAT UNBLOCKS IT: one authored rift with no Boss beat, or an owner ruling that a story rift may complete on a Clear and set its flag some other way. Do NOT take the paragraph below at face value — its "unifying shipped verbs" reading is right about the verbs and wrong about where they could run.

- RIFT OBJECTIVES, THE ORIGINAL SCOUT: THE VOCABULARY IS ALREADY RULED AND THE BLOCKER IS ALREADY CLEARED. O117 (DECISIONS.md:266) authors EIGHT rift archetypes: Clear, Sever, Hold, Hunt, Escort, Carry, Collapse, Silence, and says in terms that what is missing is not vocabulary but a rift completion event to fire on. That event shipped afterwards as O168: ABreakerGameMode OnRiftCompleted plus CompleteRiftRun (BreakerGameMode.cpp:263-307). The condition O117 named has been satisfied and nobody went back for it; BreakerRiftRewardMath.h:21-24 still carries the note that the archetype does not exist on the definition yet. FIVE OF THE EIGHT VERBS ALREADY SHIP AS WORKING ACTORS, every one sited OUTSIDE rifts: escort (ABreakerSurvivor, ABreakerMeridianGroundCrew), a channel/hold (ABreakerCoastalUplink), a two-step recover-then-deliver (ABreakerBasinRecorder), a priority-target hunt (UBreakerContainmentHunt) and a collection counter (ABreakerFeedstockPickup). So this is UNIFYING SHIPPED VERBS, not inventing any. The one verb that genuinely does not exist is Carry: nothing in the project is physically carried. FBreakerRiftDefinition carries five fields and no objective kind, and the whole wave-then-boss loop hinges on one call at BreakerGameMode.cpp:4584 marking the boss as the run's terminator. OWNER RULING OWED BEFORE CODE: which archetype is cycle one, and does the objective kind go on the definition (serialized, so an enum there is append-only forever) or stay a runtime-only selection until it has proved itself? Recommend Clear first: it needs only a live-enemy-count completion predicate beside the existing boss terminator, and it is the one the owner named first.

- INVENTORY DE-BLOAT (hover): SCOUTED, BLOCKED ON A LAYOUT RULING. The hover half is genuinely unbuilt: a backpack card draws name PLUS the complete affix list (BreakerMenu.cpp:5350) while the EQUIPMENT column already draws name only (4956-4972), so the de-bloated shape already exists on the same screen. There is exactly one affix-list producer (MakeAffixLines, 4609) and its own comment declares it the single place. USE THE SKILL TREE'S FIXED RAIL as the pattern, NOT Slate's SToolTip: the project has zero uses of SToolTip, SetToolTip, OnMouseEnter or IsHovered, and a floating tooltip is unphotographable even at rest, whereas a fixed host draws its placeholder in every frame, which is the same argument that gave the tree its rail (SkillDetailHost, BreakerMenu.h:826). THE BLOCKER IS NOT THE PANEL, IT IS WHAT THE CARD BECOMES: the entire readability budget (LongestAffixNameChars, MinAffixColumnWidth, MinReadableCardWidth, SolveCardsPerRow, AffixNameWrapWidth and their tests) exists SOLELY so affix text fits a card. If affixes leave, the card can be far narrower and the cards-per-row cap stops being a readability number: that is a layout re-derivation, not a silent deletion. Docs/spec/art-and-ui.md:82-83 also states as present-tense intent that cards wrap affixes and choose a readable column count, and would have to be edited. THE THIRD DUPLICATE-NAME BUG IS LANDED, STRUCTURAL HALF ONLY (owner-ruled): BreakerSwapItemName was a hand-copied duplicate of ItemDisplayName that never applied O267's grammar, and the stash carried a third copy of the eight slot words. All three now call UI/BreakerItemNameText.h and the output is byte-identical everywhere. THE ROW GEOMETRY IS NOW SOLVED AND LANDED TOO, and the claim that the wrap width was underivable was wrong: the delta-mark column is one DeltaGlyphColumn per compared affix, which is COUNTABLE before layout, so it is an argument to NameWrapWidth rather than an unknown. At ModalWidth 640 the arithmetic does not close — the longest headline the grammar can build is 68 characters, about 843 px, against about 284 px of column at the worst delta count — so the plate is 800 and the test walks the affix ladder for the worst case rather than hard-coding one. Candidate rows lost their slot-word line (it existed only because the name used to BE the slot word) and grew 64 -> 72; the incoming row keeps its kind line and is 96. NOT PHOTOGRAPHED: the harness cannot open this modal, so only the owner can accept the wrap. OWNER RULING OWED: does the affix list leave the card entirely, or does the card keep the two lines that named it?

- WEAPONSPREAD, THE ONE EMPTY LANE: FOUND, NOT BUILT, AND DELIBERATELY SO. It is not unbuilt plumbing — it is a fully wired lane whose authors were deleted one commit ago. The lane is ordinal 26 of EBreakerNodeStatTarget, registers as paid, composes to FBreakerNodeStats::WeaponSpreadReduction as a divisor floored at 0.01, and is CONSUMED at UBreakerWeaponComponent::GetNextShotSpreadDegrees — the single function feeding the hitscan cone, the projectile cone AND the HUD crosshair gap, so the consumer is complete and the crosshair already cannot lie about a purchase. It reads empty because commit 99f75337 ('Activate full 22 Core') replaced the hand-built Core tree with the 22-wedge roster; that commit took empty lanes from 54 to 1, and the survivor is this one, because the replacement's spread node (Core.Precision.ColdBarrel) targets a DIFFERENT, OLDER lane — WeaponBaseSpreadReduction, a Flat percentage off the base cone only. SO THE PROJECT NOW HAS TWO TREE SPREAD LANES doing arithmetically different jobs at different points in the same function, which is the real finding and is worth more than the empty-lane count. Deleting WeaponSpread is BARRED: append-only serialized enum with pinned neighbours above and below. DO NOT AUTHOR IT YET — the house rule wants a canon row in power-and-scaling.md plus a conformance test before a new multiplier lane merges, no canon row exists for EITHER spread lane, that spec is at exactly its 300-line budget so something must be cut to make room, and the divisor is uncapped with no floor on the resulting cone outside Fan's branch, so a number authored today would have nothing watching it. OWNER RULING OWED: do the two lanes merge into one, or is the base-cone-only lane a deliberately different verb worth keeping beside the total one?

- CLEAVE'S REACH: FIXED, and the desk had the MAGNITUDE wrong plus a second defect it never recorded. The mechanism was right — UBreakerMeleeSweep::IsInsideArc compared actor origins while the overlap feeding it is a sphere against collision, so a body already touching the sphere was thrown away. But it was NOT 'both capsule radii': the sphere is centred on the swinger's own origin so the swinger's radius never entered the query, and the target's CAPSULE is ECR_Ignore on the exact channel the overlap runs on (BreakerEnemy.cpp:224). What the sphere touches is BodyHitBox, extent (42,42,58). The wasted band was ONE box half-extent, not two capsule radii. THE SECOND DEFECT: the shortfall was YAW-DEPENDENT — a box reaches 42 face-on and 59.4 corner-on, so the same body at the same distance was hittable or not according to how it stood. IsInsideArc now takes TargetRadiusCm (defaulted 0, so every existing caller and pure test keeps the old rule) and SweepTargets feeds it from the very component the overlap admitted the candidate by, so the arc can never disagree with the query. The forgiveness is INSCRIBED (HorizontalReach = min of the horizontal half-extents), which is the largest value true from EVERY yaw — circumscribed would forgive more but only from some angles, keeping the yaw-dependence and moving it outward. Both conditions the code itself named as the trigger for this API change had already fired: a second melee verb ships (Tank T1 Rend, BreakerTankAbilities.cpp:269-323) and the note's own 'when it is worth making' had arrived. RangeCm LEFT AT 650 deliberately — the honest reach just grew by 42cm against a trash body, so the number wants the owner's hands before it moves again.
- WalkSpeed moved 595 -> 640 alone; SprintSpeed stayed at 990, narrowing the sprint gain to 1.55x. If the next report is "sprint no longer feels like anything", raise that one dial and nothing else.
- Core overview names landed but the node markers are still small grey squares at the fit zoom, and the harness cannot hover: node detail, tooltip and wheel-zoom legibility remain unverified by anything but the owner's hands.
- CASTER CAST TIME, owner-ruled and NOT BUILT: "mana is the casters cooldown but there should be like a cast time for spells ... they shouldnt be able to appear instantly". No Caster ability has a pre-effect delay; Cleave's AnimationLockSeconds is a POST-effect lock on re-activation, which is not the same mechanism. Needs rulings before any code: does incoming damage interrupt a cast, does movement, is Mana spent at the start or at the landing, and does an interrupted cast refund? A cast bar is a HUD window (O179: windows stay HUD bars) and does not exist. Its own block.
- LANDING/SLIDE: BOTH O264 FINDINGS REPAIRED AND TRACED. The queued slide pays the scrub and is caught at SlideEntrySpeed rather than exempted (LandedPlanarSpeed, floor clamped to the arrival speed so it can only save a slide, never grant one), and the sliding cap is SlidingSpeedCap(SprintSpeed, mult, BoostedSpeedCeiling) — the extraction IS the fix, because the signature has no velocity term. MEASURED: a crouch-held landing at impact 1946 now scales 0.849 to 781 where it used to read 1.000 and keep 920. The floor itself never engaged in the trace (781 is well clear of 550) and is pinned by the pure test instead. THE OWNER JUDGES THIS ONE: sliding after a sprint no longer mints its own cap, which is a felt change to how a slide chain carries.
- STILL OPEN, and now with numbers: -BreakerMoveTrace never lands with BoostedSpeedCeiling ABOVE the resting cap. The script now has three landing legs (air-crouch, and two high-drop probes) and drop one lands at ceiling 990 against a resting cap of 990 — equal, not above — because the dash tops out at sprint speed while sprinting. So "a dash-then-jump leg would close it" is NOT right on its own: closing it needs a grant that puts the ceiling over the cap, or a leg that is not sprinting when it lands. O264's magnitude at a genuinely raised ceiling is still unmeasured.
- RANGED STUTTER: MECHANISM FOUND AND BOTH HALVES FIXED, and the band was never the culprit. ClassifyBand has real two-sided hysteresis (BreakerRangedBehavior.cpp:19-40) with 300cm of separation on each edge and a pure test already pinning that a settled body never leaves Hold on its own. The un-hysteresised threshold was the one WRAPPING it: bLineOfSight is a raw line trace taken fresh every frame with no dwell, no filter and no memory. HALF ONE: that reading now settles — a change must persist for LineOfSightHoldSeconds (0.25, O2) before it is acted on, via the pure UBreakerRangedBehaviorLibrary::SettleLineOfSight. HALF TWO, and this is the mechanism the desk never had: the blocked branch FORCE-WROTE Band = Advance, which is the exact value ClassifyBand reads back as PreviousBand — so one blocked frame erased the band's memory AND narrowed the outer edge to High-Hysteresis, leaving the body owing 300cm of closing before it could hold again. Repositioning is a different verb from the band, not a value of it; Band is no longer written there. THE SUITE CAUGHT A REAL ERROR IN THE FIRST ATTEMPT: a dwell applied to the FIRST reading made a body that spawns already seeing the player stand blind for a quarter second before committing to a target, which broke BreakerThreatNodesRuntimeTests and would have read in play as blank staring on every arrival. The first look is now believed at once — the dwell filters changes of mind, it must not delay having one. STILL UNVERIFIED BY MEASUREMENT: a second candidate mechanism exists and was NOT touched — every Path->Steer transition hard-zeroes velocity through StopChase, and the Lattice is uniquely exposed because AttackRange=0 makes its path acceptance radius 0 and its Hold direction is lateral, deliberately outside the 35-degree path cone, so every Hold<->Advance change is also a candidate mode change. Nothing in the project samples faster than 0.5s or samples velocity at all, so which mechanism the owner actually sees cannot be read from source. If stutter survives this, that is the next place to look and it needs a velocity probe first. Walk animation remains missing and is unrelated.
- CAPTURE HARNESS: FIXED, and the desk had the mechanism wrong. There was no race and nothing to wait for — naming a capture screen CANCELLED the travel outright, because the -BreakerCaptureMenu= arm and the front-end travel arm were the two halves of one if/else-if with capture FIRST. The "[BreakerAutoPlay] ... travelling to Lvl_Anchor" line the desk read as evidence of a travel in flight printed unconditionally ABOVE the branch: it announced a journey the next line refused to make. Adding the IsArrivalCoverUp wait the desk asked for would have been a provable no-op, because cover is only ever raised BY a travel. Travel now wins and the capture screen opens on the far side, where ShowInitialMenu re-enters on the fresh pawn. -BreakerAutoPlay=FrontEnd is new, so the title screens are still photographed where the player meets them. A menu capture also refuses character-state writes now: the front-end guard used to cover every capture run by accident, and ue-capture.sh passes no -UserDir. PROVED: -BreakerCaptureMenu=MAP on the Anchor now loads Lvl_Anchor and photographs the hub travel list, which had never been seen. It immediately exposed a real O265 regression — seven destinations pushed the map canvas and BACK off the plate — now fixed by letting the canvas yield to the row count.
- ENEMY ARRIVAL AND REPOPULATION, owner-ruled. BOTH HALVES LANDED THIS SESSION. (1) THE EMERGENCE WINDOW: every arrival carries 0.8s of protection (O2), granted at ABreakerGameMode::AcquirePooledEnemy on both branches, using the SHIPPED immunity shape — a keyed 0.0 incoming modifier plus an owned timer, as Hard Stop does — because O228 forbids binary immunity through PushWindowIncomingDamageModifier. (2) REPOPULATION (O268): the ordinary world has patrols and they come back; a rift is an instance with a completion condition and stays finite. Each authored body records a SLOT at placement (class, validated home, facing, elite flag, area level, patrol phase), so a returning patrol takes the post it stood in rather than re-deriving the formation maths — one authored layout, one source of truth. One body returns per interval on a shared world clock, most-overdue first; a pocket that refilled at once would be a wave, which is the rift's verb. The clearance gate (3000cm) is DERIVED, not taste: it must exceed the enemy's own DetectionRange of 2200 or a patrol arrives already hunting, which is the pop-in complaint wearing a different hat. A slot near the player is no candidate but KEEPS its accrued wait. Returning patrols pay ordinary XP and loot; the 267-XP entry-yard pin is unmoved because it measures the authored population at t=0 and that fixture never ticks. THE DESK WAS WRONG TWICE HERE AND BOTH REFUTATIONS PAID: spawn immunity already shipped, and "nothing in the project repopulates" is false — ABreakerEnemy::bRespawns DEFAULTS TRUE, HandleDeath (BreakerEnemy.cpp:1473) already schedules RespawnEnemy, and the gym's standing encounter (SpawnCombatEncounter) never calls ConfigureWave, so its whole roster comes back 3s after death in the shipped build TODAY. ABreakerBossEnemy::TickGalleryRespawn is already a timed count-and-replenish loop. That self-respawn path was deliberately NOT used: it returns a body where it fell with no distance gate and no pace, and the gate and pace are the feature. A LIVE CONTRADICTING RULING WAS OVERTURNED: Docs/ORDERS.md "THE ROAM SPACE: SPARSE AND NON-RESPAWNING" made finiteness the thing separating roam from rift. That work now falls to the rift's objective and its dilapidated dressing — which is why rift dilapidation is LEGIBILITY, not decoration, and has moved up. STILL OWED: authored arrival points (doors, building mouths, small rifts) so a return is legible as well as survivable — the window and the gate make it fair, nothing yet makes it READ; the courtyard roster is discarded at its call site and registers no slots; the prototype travel districts (BreakerPrototypeDestinations.cpp:437-469) have the identical build-once shape and are not yet covered; and no density ceiling applies outside the wave budget.
- MARKED ENEMIES BEFORE THE QUEST: NOT a crediting bug. UBreakerQuestLibrary::NotifyEnemyKilled already refuses progress unless the quest is Active, and the Pattern objective is elite-gated. The enemies the owner reads as "the quest ones" are the ordinary elite population the quest later refers to. Fix is presentation or authored quest-specific spawns, and it belongs with the arrival block above, not as a separate defect.
- CAST TIME (O266): 25 of 34 wind up, cast speed divides them from BOTH gear and tree. ROT LANDED and the desk had its blocker wrong — my own claim. Follow is NOT performed by Tick: BreakerZoneActor::Tick merely calls AdvanceZone, and the follow happens INSIDE AdvanceZone, which the fixture already calls by hand before every follow assertion. Freezing the zone actors was always safe. The real obstacle was never zone ageing: InitializeActorsForPlay makes the controller build a camera manager and Possess aims it at the pawn, so Rot reading GetPlayerViewPoint stopped obeying the fixture -90 control rotation and fired its self-placement trace sideways. One line, Controller->bAutoManageActiveCameraTarget = false, and the fixture converts. NINE STILL INSTANT: four by ruling (Slipcut, HardStop, Closequarter, GroundZero); three self-timed and already reached by cast speed (Fracture own cast phase, Siphon channel, BreachCharge fuse) — worth a ruling on reinterpreting those phases as the cast time; and TWO blocked on owner rulings, Resonance (a wind-up burns the status budget it is paid from, 405 -> 270; does it SNAPSHOT at cast start like DoT sources do?) and Unmake (its window suspends Mana generation, so the bank recovers during the cast and the authored debt interaction is nerfed).
- DEATH SCREEN "UGLY AND NOT INTUITIVE": DIAGNOSIS RETRACTED, NOT REPRODUCED, AND THE OWNER OWES ONE SENTENCE. A capture was read as evidence and three of its four apparent defects were HARNESS ARTIFACTS. -BreakerCaptureMenu=DEATH opens the screen without producing the state that raises it: no death beat runs, so the weapon stays UP where a real death lowers it through WeaponLowerFraction (BreakerCharacter.cpp:1127); PendingRift is unset, so Site.AreaName is empty and the headline loses its place; no wave is active, so Site.Wave is 0 and Line 2 loses the wave. Re-captured with -BreakerCaptureDeployBeat the same screen reads "ERASED AT FERNHALL SUBSTATION / Wave 1 of 3 - gear kept" over two clear verbs, and it is model-driven end to end (BreakerDeathBudget::Model composes place, wave, budget and consequence; the tally draws cells for a budgeted tier; a spent budget leaves RETURN alone). It is in far better shape than the first read claimed. THE HARNESS CANNOT SETTLE THIS ONE: there is no kill or suicide console command anywhere in the project, so a REAL death cannot be photographed at all, and anything that only happens during the death beat is unphotographable. The owner played it for real and something felt wrong; that state is unreachable from here. DO NOT redesign this screen on taste - ask what specifically felt wrong. Standing candidates if a redesign IS wanted, both small and both defensible without the artifact: the consequence ("gear kept" / "gear lost") is the single most important fact on the screen and is drawn at body 14 in TextSecondary, the faintest thing on it; and the column floats on a 0.6-alpha scrim with no plate behind it, which the file's own comment attributes to the zoned frame.

- RIFT DILAPIDATION: FIRST HALF LANDED, and it is LEGIBILITY, not decoration — under O268 the rift and the world run different rules on the same geometry, so the visual is what tells the player which one they are standing in before they act on it. The ruined palette is DERIVED from the living one (Game/BreakerZonePalette.h: desaturate toward luma, stain toward a warm rust anchor, dim) rather than authored as a second table, because a parallel palette drifts the moment somebody adds a prefix and the two stop being the same place. Applied at BreakerZoneApplyColor, the single point EVERY painted surface passes through, gated on PendingRift.IsSet() — which is correctly unset on the way out, so an ordinary yard cannot inherit a ruin. THE CAPTURE HARNESS EARNED ITS KEEP: the first attempt applied the ruin inside BreakerZoneColorFor, and the frames showed the buildings going to ruin while the GROUND stayed pristine — the prefix table paints composed pieces, but the walk strips, joints, drains and skyline carry their own literal colours straight to the paint function. Moving to the choke point fixed it and cannot be missed by a path added later. A claim made in the test was also disproved by arithmetic: the moss does NOT move furthest (the multiplicative dim dominates the drain, so the brightest surface does), and it was wrong about ruins anyway — a dilapidated industrial yard grows MORE overgrown, not less. NOT ACCEPTED YET, AND THE OWNER MUST JUDGE IT: a colour-only change over identical geometry has a ceiling. The silhouette is unchanged, so the read is hue alone and at a glance it can pass for different lighting rather than ruin. Frames captured both ways for comparison. THE SECOND HALF IS NOW LANDED: fernhall_rift.glb is composed by `compose_fernhall.py --ruined` and imported to its own mesh folder; a rift builds from it and falls back to the living yard when none is imported. RUIN IS ADDED, NOT SWAPPED, and the first attempt proves why: swapping the cover pieces for the broken twins is safe (place() forces the authored box) and INVISIBLE (a 0.68x3.01x4.82 broken wall squashed into a 3.0x1.2x1.2 box loses its break), and two captures rendered identically. dress_ruin_* chunks lean against the cover at their own proportions instead; 139 pieces spawn against the yard's 107 with the cover field identical. It also exposed that bRiftInstance was resolved sixty lines BELOW the build that consumes it, so a rift always built the living yard. NOT ACCEPTED: photographed, not played — whether it reads as collapsed or merely littered is the owner's eye. THE ORIGINAL NOTE: Assets/zones/kit/ holds two CC0 megakits (92 MB, LFS-materialised, licence-noted, in the composer's own glTF format) that the composer uses NONE of — including WallAstra_Straight_Broken.gltf and WallBand_Straight_Broken.gltf, literal broken twins of intact walls, plus Door_Simple/Door_Metal/Door_Frame_Square_Blocked and Prop_Vent_Big/Small/Wide for the arrival fiction that is still owed. A second compose target (fernhall_rift.glb, same blk_/wall_/flr_/dress_/marker_ names into a sibling folder) needs no code change at all: CollectZonePieces already takes a folder argument. REJECTED AS A ROUTE: the six Fab projects on this seat are 4.8-26 GB of launcher-delivered content, and .gitignore already rules Fab packs per-seat and never committed wholesale. Nothing there is worth the LFS weight when the broken twins are already in the tree.

- GEAR NAMES FROM AFFIX POOLS: LANDED (O267). PoE grammar over the item base — strongest prefix, base, "of" strongest suffix; tiers count down so strongest is the smallest tier, an untiered special outranks every tiered roll, and a tie breaks on affix id so two identically rolled items never disagree about their own name. The grammar is pure in Items/BreakerItemNaming.h and proved on a bare array. It was FOLDED INTO the existing ItemDisplayName rather than shipped beside it — that function already named legendaries and the starter rifle, and its own comment called the rest a CONTENT GAP "until item names exist". A second name path would have been the same duplicate-concept mistake as the CastSpeed lane. WORDS ARE STILL THE AFFIX DISPLAY NAMES, so an item reads "Sustained Accuracy Sidearm of Cast Speed". Authoring flavour words is a pure data pass that changes the wording without touching the grammar. NOT YET VERIFIED ON SCREEN: the inventory capture starts with an empty backpack, so no rolled name has been photographed. The inventory de-bloat — moving the full affix list to hover — is the other half and is NOT done; the harness cannot hover, so only the owner can accept it.
- RIFT INTERIORS AS DILAPIDATED VARIANTS of existing areas, owner-ruled direction. Answers the parked "reuse the yard vs author new tilesets" question by reusing authored geometry with a decay pass. Expect a lighting and palette difference first; ruined geometry needs art that does not exist.
- DEATH SCREEN INSIDE A RIFT reads as ugly and unintuitive (owner). BreakerDeathScreen.cpp exists with retry-rift and return-to-anchor verbs. Not touched: it has no capture fixture, and the menu capture path photographs the front end (see the harness finding above), so nobody has seen it in its rift state. The fixture comes first.
- CONSEQUENCE OF O266 worth the owner's hands: Parry is an EARNED Core verb (Core.Bulwark.Parry grants Progression.Verb.Parry; nobody has it until they buy it, and it is not class-specific). Its base window is 0.25s against Cleave's 0.35s wind-up, so a parry pressed as the caster starts swinging has LAPSED by impact — the defender must react to the wind-up rather than pre-empt it. Core.Bulwark.Counterweight adds +0.10s of window, taking it to exactly 0.35s, so that one node closes the gap precisely. That coincidence is not designed and nobody has played either side of it.
- CAST SPEED ON THE TREE: DONE. The crash chased across a whole session was NOT a defect — AbilityCastRate already existed as a wired lane, authored by Core Tempo Metronome, Quicken and Cascade and already dividing Fracture own cast phase. The CastSpeed enum entry added beside it was a DUPLICATE CONCEPT, and appending it is what crashed AttunementRuntime. BeginCastIfNeeded now divides by AbilityCastRateMultiplierFor and the suite is green with no new enum, no new canon row and no crash. LESSON: search the enum before adding to it; this one has 84 entries and two of them already meant cast speed.
- ABILITY LANES ON GEAR: BUILT for cast rate, area and cooldown. The bridge is EBreakerAggregatedAttribute — gear and tree both bid Increased percentages there, so +20% of each reads x1.40 and never x1.44, which is the multiplication bug every other line in that block exists to prevent and is now pinned by RiorsEdge.Attributes.AbilityLanes.Additive. Per stat the seams are: an EBreakerStatTarget entry, an EBreakerAggregatedAttribute entry, an attribute-set field (init 1.0, replicated, clamped, composed, OnRep), a bid in BreakerEquipmentComponent, a bid in BreakerProgressionComponent, and the consumer reading the attribute with node stats as the bare-rig fallback. Affixes authored: Ability.CastSpeed (suffix, 3->24), Ability.Area (prefix, 3->26), Ability.Cooldown (suffix, 4->32). STILL TREE-ONLY: AbilityDuration, deliberately — its consumer composes THREE node lanes by Kind (generic, zone/window, buff/window), so a gear line needs a ruling on which of the three it feeds before it can be authored.
- GEAR CANNOT BID INTO ANY ABILITY LANE. Affixes carry EBreakerStatTarget, a different serialized enum with no AbilityCooldown, AbilityArea, AbilityDuration or CastSpeed entry of any kind, so those lanes are tree-only today. A gear cast-speed line needs its own item-side target and composition path — that is why the affix authored during this attempt was reverted rather than shipped against a target the item enum does not have.
- CLASS-LOCKED AFFIX CONDITIONS: SEVEN OF FIFTEEN GENERALISED AND LANDED, AND THE REVERT WAS BASED ON A FIXTURE ARTIFACT. The band is 12.8891x with the lines live — the same figure as before, so the generalisation costs NOTHING and "the dead weight was load-bearing for build variance" is false. BreakerPowerBandTest::MeasurementState is a hand-written bitmask, not an evaluation: it named Airborne/RecentlyDashed/Redline only, so affixes moved onto a new bit read false and went dark, and the optimized build silently lost them. PROVED by scaling the candidate lines to half magnitude and re-measuring: the band did not move one ten-thousandth, and a balance consequence responds to magnitude. No re-tune was needed, and re-tuning to hold 12.0 would have cut conditional lines below the unconditional line they pay a premium over. STILL OPEN AND IT IS A BUDGET CALL, NOT A TECHNICAL ONE: the seven Redline lines want a ResourceCharged twin of ResourceDepleted (gated on the loop not resting full, which is what makes it honest and not the bare ResourceHigh this file refused with reason). It costs the LAST spare condition bit — ConditionVocabulary.Ceiling wants four of thirty-two unspent and both twins leave three — so the owner decides whether that bit is spent here or held for the next axis. Expect it to cost the band nothing either, for the same reason. Anomaly.PhaseShear deliberately stays Swift-locked: the unwritten pool is not part of the class-blind slice roll that made this a problem. The problem is real and quantified: 15 of 97 affixes are gated on Redline or RecentlyDashed; CanUseDash is a hard PermanentClass == Swift check and the momentum loop is inert for every other class; the drop roll is class-blind (verified, no filtering anywhere in the drop path). That is 765 of 5793 roll weight, so 13.2% of every rolled line lands DEAD for four classes in five. The generalisation was implemented in full: two new universal conditions (RecentlyRepositioned = dash OR vault/mantle OR slide-jump; ResourceHigh = the missing top end of the ResourceLow/ResourceDepleted set), a slide-jump recorder added to Movement because the condition could not name a timestamp nobody recorded, and all 15 affixes retargeted. It built and the retarget was clean — zero affixes left on a Swift-only condition. IT BREAKS A PINNED BAND. Endgame build variance falls 12.89x to 9.67x against an authored floor of 12.0. The cause is not a defect: those lines were dead for four classes, so making them universal raises the BASELINE build more than the optimized one and compresses the gap. The dead weight was load-bearing for build variance. Reverted rather than widening the pin. THE RULING NEEDED: accept a flatter endgame band, re-tune the 15 lines downward to hold 12.0, or find variance elsewhere and then generalise. The patch is small enough to redo in one pass once the direction is set.
- Volatile now has a tested outlined countdown; final Niagara presentation and real client cadence/human timing remain.
- Damage numbers can overlap enemy plates. Hover, focus recovery and comfort require real interaction checks beyond static captures.
- Tank.Leech.OpenWound promises gear Life on Hit but Rend reads LifeOnKill because no LifeOnHit target/lane exists. Its accepted-hit tests prove the substitute, not the authored gear source. Core.Affliction.OpenWound correctly supplies Increased DoT. Finish the real gear lane and its delivery policy rather than rename the promise.
- Ignore Me doubles owned-deployable threat, but allied-player doubling lacks an authoritative party relation. Cooperative sandbox identity/friendly-fire protection does not establish party membership.
- Remaining Afterimage consumers require individual source-owned cleanup checks; parked cases below carry exact failures.
- Overhaul Bench Work revoke cleanup is parked after bounded correction: nested death/removal during main ammo settlement skips the old window/delegate teardown even when capacity refunds correctly. Candidate and callback fixtures remain outside checkout; no production change landed.
- Blackout Afterimage is parked: its unkeyed enemy movement overwrite can erase Fleetfoot/other slows, and its shared incoming-damage key lets one caster remove another's effect. Requires source-owned contributions before extending the numerical tail; ordinary marked-hit Charge permission still ends on time.
- Guest Hold/deployable cast audio and HUD pulses lack an owner acceptance receipt. The narrow RPC candidate is parked: GAS activation success can still represent immediately rejected placement, while active-state filtering rejects valid instant casts. No optimistic success cue was added.
- Desktop input probe launched an isolated profile but exposed no targetable game window to the computer-use tool; mouse-focus acceptance remains unverified.
- Three generation entries remain uncalled; one aggregation lane and one target remain empty/unrouted. Consult current STATE for exact identities.
- Other low-priority cleanup only after verifying use: empty modifier test helper; unused HUD dimensions; stale movement comments; duplicated boss timings; slot-invalid fixtures; inert affix leans; menu string-table migration.

- Refractor can waste a fork seat on a protected coop player before damage rejection. Legal real-shot fixture and candidate eligibility repair remain parked after bounded audit.
- Bare Swift coop smoke profile suffered ambient deaths during ordinary movement funding. Network receipts are not a balanced encounter acceptance.
- Caster cooperative expansion is parked after bounded network acceptance failure: metadata/real grants/native isolation passed, but two actual guest Cleave requests were rejected by authority; first run logged player death25ms before request. Swift unchanged rerun passed after an initial rejection. Whole expansion restored; no Caster combat claim. Logs and candidate are retained outside checkout in coop-caster-verification-stage/parked-runtime.

- THE COMMITTED CENSUS WITNESS WAS NEVER TRUE, and it blocked every report: status.py exited 2 rather than speak, because six of the fifteen Core authoring hashes in Data/progression.json disagree with the committed sources at EVERY revision, not merely at HEAD. That census was exported from a tree carrying uncommitted edits to those six files. Re-exported from this checkout: the diff is the six fingerprints and nothing else, so the census CONTENT was always right and only its witness was stale. If a seat exports the census with local edits in the tree again, the next seat inherits a repo that cannot report its own suite.

## Decisions and measurements requiring explicit care

- DropChanceReachesEveryRank measures probability while O249 converts capped excess into rarity value. Keep the enumerated finding until its replacement measurement is explicitly settled; do not change pins for green results.
- Prolific1.51 remains enumerated with its deletion condition; the eight-Unwritten fixture is not a legal loadout.
- At-cap variance4.05 is below8–10. Ability allocation parity is now0.93 after mirrored fixture allocations and Arc critical access; it is not a native sustained-throughput measurement.
- Native starter diagnostic: after separating accepted direct and periodic damage, Cleave coefficient1.5→2.4 brings seconds10–20 rifle ratios to0.935/0.962 at levels1/20. This is stationary close-range weapon-derived Cleave, not general Ability-pool parity. Earned Fracture has independent Ability-pool evidence. Rifles exhaust initial ammunition during20–30s, so late totals cannot establish parity; see Docs/reports/cleave-tempo-2026-09-09.md.
- Core offers429/65=6.6x; closed22-wedge ring is active. Do not reopen retired hub-entry or2.63x questions.
- Potential future ultimate redesign (Unmake as a decisive event) and skill-level stacking cap need explicit design treatment; retain current rules meanwhile.
- Gear ResourceOnKill currently pays immediately through the attribute bank outside Mana::GrantMana conditional metering; its suspension runtime test explicitly expects this payment. Should gear kill income remain immediate or join the capped queue? Keep current behavior until ruled; this is not a confirmed defect.
- Anchor13's name alone does not establish twelve other settlements.

## Latest validated baseline

Current local build/full suite: 879 passing, 3 expected failures, 0 unexpected; native census refreshed for54 quest flags and current ability declarations. Completed historical work lives in git; this queue lists pending work rather than replaying earlier sessions.

## Playtest handoff

Current delivered features and suggested route: Docs/reports/next-playtest-2026-09-09.md. Detailed evidence stays in the linked reports and git; this desk carries remaining work.
