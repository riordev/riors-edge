# Decisions

Live rulings, one line each. A superseded ruling is deleted; git holds it.
Numbers are permanent and never reused.

## Rulings

**O1** — Stamina does not exist and may not be re-added. Block and dodge are passive chance layers; Parry is the only defensive input and carries its own short cooldown.

**O2** — Every number is a placeholder until measured, and a new constant is flagged as one at its declaration. When two implementations are possible, prefer the one that authors nothing.

**O3** — More multipliers compose as an unordered product; a build holds at most three; they may be authored on Core constellation Convergence or Keystone nodes and on Aberrant or Unwritten item rewrites.

**O4** — 300–400 hours to a finished build, and a build must be viable and playable by mid-campaign. Breadth of viable options is an explicit product goal — err toward more viable builds, not higher ceilings.

**O5** — Per-element resistance, applied after armour and before shields.

**O6** — Item level is hybrid: zone level plus a content tier bonus plus variance, with an enemy-level fallback so a zoneless space still drops.

**O7** — The fifteen-source world Core Point list is canon, including rift-archetype first-clears.

**O9** — Enemy taxonomy is three orthogonal fields: Archetype, Rank, Modifiers. Modifier count drives Rank; boss is authored.

**O10** — Tick interval is part of the damage-over-time snapshot, and intervals are discrete so stacking has visibly diminishing steps.

**O13** — Rocket carries strong self-damage reduction and full self-knockback control, never immunity. Rocket-jumping is tolerated, never required.

**O14** — The player is a person, lightly: a body, a face, minimal voice. Two player models are planned, Human and Effigy, and both are real.

**O15** — Branch nodes mix freely with investment gates. No mutually exclusive tiers anywhere.

**O16** — No hardcore, no permadeath.

**O17** — The stash is account-wide. Characters are builds; gear is an account asset.

**O18** — Seed targets, stated at the reference archetype of x1.0: trash a little under a second, elite around three, boss twenty to forty-five unless a special enemy claims the exception; time-to-die seven to eight seconds bare, substantially higher invested; a boss hits like its archetype and rank adds no damage. Every other archetype's band is the reference target times its own multiplier, derived and never authored — a Warden trash mob at x3.2 is a 2.9s kill and that is on target, not three times adrift. The boss row already reads this way: 20-45s describes the fielded Field Marshal, x75 rank on a x0.30 archetype. An archetype multiplier is therefore a stated deviation from the reference and the only question it raises is whether the deviation is wanted; a TTK figure that does not name its archetype is not asserting anything.

**O19** — The elements are Rift, Entropy and Void. Rift-element damage takes a hotter, whiter cyan: saturated teal is a property of objects, not of damage.

**O24** — World aesthetic: overgrown Earth. Vegetation over ruins, with weathered functional technology scattered through it.

**O25** — Two jumps are base kit for every class. Parry is the only tree-granted verb.

**O27** — Monsters are content-scaled to area level, never player-scaled. Trash exists to be trivialized by an optimized build, and difficulty lives in rank and modifiers. Choices beat accumulation: per-point accumulation is cut back and the power moves into node choices.

**O29** — The endgame power source is gear depth. Item level runs past the character cap and the area ceiling, the affix tier ladder widens and back-loads, and a post-cap character-power tree is rejected outright.

**O30** — The Core tree is open to redesign, organised around the axes a build is actually built on rather than around fantasies.

**O31** — Content shape is Destiny crossed with Path of Exile. Raids are puzzles rewarded for team play, and every build must be able to make an impact — no encounter may have a build that cannot participate.

**O32** — The legendary drop rate holds; the pool grows instead. Legendary and top rarity are separate axes: every legendary rolls at top rarity, most top-rarity drops are not legendaries.

**O33** — Character identity lives in four independently expandable avenues — class, Core axes, gear affixes, and rule rewrites — and class must never be the sole trunk. A baseline player is viable understanding none of them.

**O34** — One multiplier canon, and one More ceiling across every source. Temporary ability windows are Mores and count inside the budget. Crit and weak point are the two site multipliers: crit build-gated, weak point skill-gated, and nothing else multiplies at the site.

**O35** — Ability damage rides gear depth through the equipped weapon's item-level scalar, anchored to exactly one at the bottom.

**O36** — Two build-variance bands, authored separately: at-cap and endgame. Item levels past the area ceiling are sourced from endgame tier bonus, so pushing tiers is pushing the ladder.

**O37** — Equip caps per axis: one legendary, one non-legendary top-rarity item, three Aberrant. Subclass commitment unlocks a branch's keystone tier and empowers rather than excludes.

**O38** — Elements are post-slice, designed and not cut. Five constellations ship.

**O40** — The single directional dash on cooldown is final.

**O41** — Rior's Edge is a looter shooter with ARPG progression and MMO social structure, unfolding a multi-area story. Movement is a core pillar, not the thesis.

**O42** — The authored Anchor map eventually replaces the runtime hub builder; runtime lighting stands in the interim and any authored light supersedes it.

**O43** — One Core Point per level through 50, and no other per-level point. The slice's opening grant is an advance on that entitlement, not a separate pool. Doctrine Points are not on the level ladder: they arrive two at a time on four main-story benchmarks, eight in all, granted by mission beats.

**O45** — All five classes ship playable.

**O46** — The special-affix seat is filled: both high rarities carry signature affixes.

**O47** — Swift is the projectile-manipulation class — multishot, pierce, chain, ricochet, and momentum manipulating the shot. Its third jump unlocks at level 1, permanently.

**O48** — Chase items stay chased. The gym's area level and the high-rarity item-level gates hold, and the dev sandbox is the sanctioned route for testing gated items.

**O49** — The endgame farm content type is named **Anomalies**, reversing the earlier ruling that gave that word to its alternative.

**O50** — The fifth rarity is renamed **Unwritten**, giving up the word the content type takes. Display strings only; the serialized value does not move.

**O51** — One crafting currency: **Riftglass**, account-wide and scalar.

**O52** — Combat resolves server-side; movement, recoil and the viewmodel are client-predicted and server-reconciled; the slice runs listen-server and the dedicated-server case is deferred.

**O53** — Documentation discipline lives in the working-rules file. This ledger holds live rulings only; there is no authority chain between documents and no document carries a reconciliation marker.

**O54** — Damage has three additive pools: Increased Weapon Damage, Increased Ability Damage, and a smaller, rarer Increased Damage that feeds both.

**O55** — The damage pool is decided by what **delivers** the damage, not by what triggers it.

**O56** — The shared Increased Damage pool rolls on every slot, at roughly 40–50% of a specific pool's value at the same tier.

**O57** — Damage-over-time ticks share one additive Increased bucket with direct damage rather than multiplying against it.

**O58** — The elite stat chassis is three times health and one and a half times damage.

**O59** — Boss rank health triples.

**O60** — Defence is a triad: physical damage reduction, ailment avoidance as a deterministic pre-immunity roll at the application door, and per-element resistance held out of the drop pool until enemies deal elemental damage.

**O61** — Endgame content taxonomy: Anomalies are the primary farm, Raids are seven players with checkpoints and puzzles, Dungeons are smaller raids for four, and Conquest is a warzone for up to nine matchmade players.

**O62** — The gym is non-canon playtest space and binds no continuity rule.

**O63** — Aberrant becomes the stacking tier: each item rolls either **Focused**, a raised tier ceiling consuming no rewrite slot, or **Modified**, one minor rewrite with no raised ceiling. The four existing rewrites become its minor pool.

**O64** — The fifth tier is singular: one equipped, four affixes, major rewrites, deliberately off the power ladder so a good Aberrant can out-stat it.

**O65** — A minor rewrite changes the terms of a rule already being obeyed; a major rewrite changes the shape of what happens on screen, or the contents of the loadout.

**O66** — Major rewrites come in three kinds — delivery, economy, and rule — and the pool is weighted toward delivery and economy.

**O67** — A major rewrite must be observable without opening the character sheet, and pays an authored forfeit printed as a line on the card.

**O68** — Rewrite caps replace the item-count cap: three minor plus one major. A legendary's authored pair occupies the major slot rather than sitting beside it.

**O69** — Rewrite stacking is not prohibited. A minor rewrite's tag names the mechanic it touches for authoring and reporting, never for refusal.

**O70** — A tree offers roughly three to five times the points a character can spend, so most of a build is refusal.

**O71** — Tree composition runs roughly sixty percent ranked minors, thirty percent notables carrying a rule or a condition, and ten percent convergence and keystone.

**O72** — Widen the stat-target and condition vocabularies before authoring to node count.

**O74** — The single More ceiling spans the ability-damage lane. There is no per-lane ceiling.

**O75** — An elite's loot floor rises one rarity step at three modifiers.

**O76** — Affixes own the raw defensive chances and percentages; trees own defensive rule changes and quality. I-frame duration stays tree-only.

**O77** — The shipped keystone rewrites are reconciled to the class More ledger's design.

**O78** — Block is rolled and its reduction applied at the same step, before armour.

**O79** — Facing-dependent armour applies on every armoured enemy.

**O80** — A minimal stagger model exists: a binary interrupt state, a resistance stat, and a per-enemy immunity flag.

**O81** — The top pack-composition tier is renamed off the canon location's word.

**O82** — The death budget is endgame-only and favours solo: an endgame instance grants a solo character two deaths and scales a party's with its size. Campaign respawn is unlimited from the start of the tileset, and a solo death inside a boss encounter resets the encounter rather than spending a budget. An endgame death spends the budget at runtime. Between black and respawn the death screen offers RETRY THE RIFT, a travel through the arrival cover, and RETURN TO ANCHOR; a spent budget leaves RETURN alone.

**O83** — A campaign rift's enemy level is clamped to a small margin above the player's.

**O84** — Shield renders above health, so depletion reads downward.

**O85** — The nameplate policy is a per-mode asset from day one.

**O86** — Commitment is to a **doctrine**: one per character, changed only at the Forge. IT DELIVERS IN TWO HALVES ON DIFFERENT CLOCKS, and conflating them made the board promise what it could not pay. THE PRESENTATION LANDS AT COMMITMENT — free, immediate, whole: the title, the character sheet, how you read to other players in the Anchor. A Caster committed to Void Whisperer *is* a Void Whisperer from that moment, having spent nothing. THE MECHANICS UNFOLD ON THE BENCHMARKS. Under O111 the pool pays two points at each of four main-story benchmarks (mission Unlock beats, not levels), and a keystone needs six invested plus its own point (O272), so no character holds a doctrine keystone before the last benchmark at the level cap — the capstone lands at campaign completion by design, and the doctrine is identity first and capstone last. Anything on the commitment screen promising the keystone is lying to every character below the cap.

**O87** — A top-tier item carries one major rewrite plus generic affixes stronger than an ordinary item's, so it sits on par with a good Aberrant rather than above it.

**O88** — Minor rewrites are class-tagged, and class-tagged items drop for every class regardless of who is playing. That is a rule, not an oversight, and it is never to be "fixed" into smart loot.

**O89** — The Focused/Modified split supersedes the unique modifier affixes Aberrant was promised.

**O90** — The tier-bonus curve reaching the top of the item-level range is deferred until the endgame exists.

**O91** — Monster damage growth comes down until hits-to-die stops falling across the level range. Defence does not scale up to meet it: damage growth is already meant to sit materially below health growth, and the current value does not deliver that.

**O92** — Momentum's generation threshold is a trap and moves — lowered, or exempting aim-down-sights states, or both. A permanent class whose most natural playstyle disables its own resource is a mistake the player cannot take back.

**O93** — No experience at cap. A currency drops instead.

**O94** — Boss time-to-kill is asserted against a **baseline** build in on-level content, and an optimized build beating it substantially is asserted separately. Bosses are meant to die fast to a comfortable build, so a fast optimized kill is correct behaviour rather than a chassis fault.

**O95** — Doctrines author no More multipliers. All three slots live in Core, where a full constellation behind a deep investment gate makes reaching one genuinely expensive — the composed cap only means something if a slot costs. A doctrine pays in rules instead: conversion, condition change, rule rewrite.

**O96** — The two rewrite-impact ceilings — one for a three-minor stack, one for a major or a legendary's authored pair — are derived BEFORE any rewrite is authored against them. The single current ceiling is 97% spent and the restructure adds contributors, so authoring first guarantees a breach.

**O97** — The tree's generic damage target is the **shared** pool, feeding both delivery lanes. Weapon composition is unchanged and every node already authored against it now reaches abilities too; the narrow weapon and ability lines are what new content authors when it wants one lane only.

**O98** — Melee is a tag-keyed slice of the weapon pool, not a fourth pool. It is weapon-delivered, so it waits on a rider keyed off the melee source tag rather than on an aggregation lane.

**O99** — Ability and weapon throughput sit within roughly 15% of each other at the same gear depth: the parity band is 0.85–1.15x, ruled at the cap. It is a target, not a measurement, and it closes by authoring ability affix breadth rather than by changing how the pools compose.

**O100** — The ultimate and two starters are free at level one; the remaining class abilities unlock one at a time, per character, bought with a one-time token at the quartermaster, an Anchor interaction. Not the crafting currency, which is account-wide and would let an established account buy out a new character at level one. One token per unlockable, so acquisition is an ordering choice rather than a scarcity one — do not later tighten the count and read scarcity into it.


**O101** — Keystones are not on the vocabulary pass's critical path and never were: O72 governs the minor and rule tiers. A keystone an enum entry unblocks is a minor with the wrong label. Missing primitives are recorded at each keystone's own site and the node ships honestly short.

**O102** — Primitives are clustered into systems before they are priced. Sixteen items is a list; five of them being one healing-modifier chain is a decision.

**O103** — Tree ids and node ids never move. A re-theme is delivered by the display field, so a branch commitment stored on disk always resolves.

**O104** — Removing a multiplier's gate is a canon event. The multiplier moves into the accounting its gate stood in for: a guaranteed weak point is a build multiplier, and crit does not also multiply on that hit. Gate removal adds no lane, so nothing else would have caught it.

**O105** — A keystone's forfeit may move from the verb to the resource. Momentum never decays airborne and drains on the ground: buildable on the wired decay lane with the existing sign convention, and it deletes both a primitive and a tile-authoring requirement.

**O106** — Base shield is an item property set by the armour piece's archetype (Life or Shield, rolled on drop); affixes add on top. Pools blend, sustain does not: leech and healing fill life only, the out-of-combat recharge fills shield only, and no exclusivity rule may be added — the asymmetry is the mechanism. Mitigation sits ahead of the split and applies uniformly to both pools.

**O107** — Multishot, pierce, chain and ricochet live in the rule tier, so they need no doctrine axis and no rename. The commit that deletes a class's branch trees lands their replacement rule nodes in the same commit.

**O108** — A doctrine keystone is paid by its own doctrine's axis. Frenzy's stacks on consecutive hits to one target, not on weak points, which vacates weak point for Marksman so no two doctrines share an axis.

**O109** — Out of combat a resource may CONVERGE on a midpoint, draining from above and regenerating from below. Decay is barred in safe states — the menu, the Forge, the Anchor — which is what the enumeration always said; "combat-state gated" over-generalised it. Convergence changes where the bar rests, never how fast fighting pays it.

**O110** — A doctrine's self-targeted duplication is additive. "Twice at full value" is one bucket counted twice, never a multiplier: a doubled multiplicative buff is an unbudgeted More arriving through a doctrine, which O95 forbids, and the global clamp would silently eat it — leaving the keystone doing nothing for the solo player it was written for.
**O118** — A gate and the budget it gates against are ONE NUMBER IN TWO PLACES. Every investment gate states the budget it was priced against, and a ruling that moves a budget moves every gate keyed to it in the same commit. Twice a budget moved and the gate did not, and both times the result was content no player could buy behind a green suite: six branch keystones, then all fifteen doctrine keystones needing 11 against a wallet of 8. Doctrine trees therefore carry NO gate — picking a smaller number only re-arms it for the next budget change — and their depth is the per-tier gate, which is derived rather than pinned. `Progression.TreeDepthIsReachable` walks every tree against its own grant and is the standing check; it exists before Core's atlas does, on purpose.

**O120** — Loading progress is never a percentage. The slice streams, so a number would be a guess, and a bar that stalls at 90% costs more trust than no bar at all: the loading screen's animations stay indeterminate and the stage line in words is the honest signal.

**O121** — A boss encounter that grows past a few minutes takes a checkpoint instead of a reset. The reset is fair only because O18 puts a boss at twenty to forty-five seconds, so the encounter's length is what licenses the rule.

**O122** — A campaign rift is entered freely and an endgame rift is consumable. A death limit is a stake only where re-entry costs something, so the two rules ship together or the limit is a loading screen.

**O123** — The death allowance field is always present and reads its mode: campaign prints UNLIMITED, endgame prints the count remaining.

**O119** — The doctrine cornerstone gate is REMOVED, not repriced, and the keystone is an ordinary tier-4 node at cost 2. `CornerstoneInvestmentGate` was 8 points spent in the tree against a wallet of 8, so all fifteen keystones needed 11 and none was purchasable. Picking a smaller value only re-arms the same failure at the next budget change, so doctrine trees carry no cornerstone gate at all — `MakeTree` zeroes it by currency, in one place rather than fifteen — and the keystone keeps `bCornerstone` because that flag is what sites the ultimate's rewrite and what O37 gates on commitment. Cost 2 rather than 3 makes every doctrine node cost two points to max, so eight divides into four picks with nothing stranded. Keystone-XOR-tier-4 was an artifact of the same arithmetic and does not survive it. This ruling replaces the expected-red entry that recorded the defect; the standing check is `Progression.TreeDepthIsReachable`, and the general rule is O118.

**O137** — Reward composes from ONE effective-difficulty figure derived the way the chassis already derives threat — rank x archetype x modifier count — and every channel applies one multiplier of its own on top. No channel keeps its own rank table. THE COMPOSITION READS RIFTGLASS AND XP; DROP CHANCE IS A CONSUMER, NEVER AN INPUT. `BossDropChance` is authored at 1.0, which is its own ClampMax, so against a 0.10 trash floor that axis cannot express a ratio above 10x under ANY tuning — a dead axis rather than a mis-tuned one, and composing against it would bake a 10x ceiling into every reward channel that no future retune could lift. Archetype currently contributes exactly zero to every channel, which is the term this ruling exists to stop giving away: a Warden trash mob is roughly six times the threat of base trash and pays identically. The curve is DERIVED, not authored: the check is reward per second of expected time-to-kill, held inside a band across ranks, so sublinearity has a target rather than a taste.

**O132** — The dash lane is guarded by the CORRIDOR rejection, not by the dash-corridor floor, and the two must never again print under one word. `MinimumOpenLaneWidth` is full-height only — chest cover at 120 cm is under MantleStepHeight 145, so a player goes over it and it does not close a lane — and the ground a player actually dashes down is held instead by the per-piece rule that rejects any cover of any class within `CorridorHalfWidth` of the centreline over the corridor span. The chest-flanked width therefore needs NO floor of its own: it already has one, wearing a different name. What it lacked was a number, so the readout now prints the corridor margin (nearest piece to the centreline, against that floor) beside the full-height lane, and every band in that readout names its direction in status.py's vocabulary — two of them pass by being under their limit and two by being over it, and the figures alone did not say which. THE MARGIN IS REPORTED AS AN OFFSET, NEVER A WIDTH: a width has to choose centre-to-centre or face-to-face, the two differ by a piece's depth, and printing one against a floor written in the other is the same defect in a new place. `RiorsEdge.Zone.Fernhall.LaneGuard` establishes which rule holds the lane by perturbing the yard rather than by asserting it.

**O133** — The wave budget curve carries NO party term while 5.3's caps are per-player, so a larger party receives a smaller, more expensive wave at low waves — measured, not intended. At wave 3 the budget is 18 either way: solo buys a Warden, a Lattice and nine Skitters, five players buy three Wardens and nothing else, because the per-player Warden cap is reachable and the budget is exhausted before a single body follows them in. Both compositions are legal and the solver is right to refuse to invent budget it was not given; this is the same 4.2-versus-5.3 collision the solver already reports on the wave axis, seen on the party axis. `RiorsEdge.Game.Waves.PartyScaling` pins it so it cannot stop being true unremarked, and goes red the day a party term lands — which is when it should be re-read. Whether the budget takes one is owner-held and unanswered here.

**O134** — A rift's enemy budget takes a SPACE input, and what a room's shape does to its budget is the question rift interiors answer BEFORE any solver code is written. The wave budget has no space term at all: it prices archetypes and caps density against a player count and knows nothing about the ground. That is correct for the gym, which is one open field, and it is the whole reason a rift budget would differ from a wave budget — twelve live enemies in an arena and twelve in a corridor are not the same encounter, and the caps that make one readable make the other unsolvable. Widening the solver now would be authoring for plumbing that does not exist (the archetype roster stays hardcoded for the same reason). The order is: rift interiors get shapes, the shapes get measured, then the budget gets its term.
**O128** — One resolver owns the enemy body's `Color` and the layers compose forward: family paint, rank blend, health ramp, reaction. No layer reads the parameter back and no layer keeps a snapshot to restore from. Two capture-and-restore caches over one parameter — the reaction component's and the enemy's rank cache — were a live race with nothing to arbitrate it: a chassis pass deferred its repaint a tick, a flash landed first, and the rank layer captured the FLASH colour as the family paint, so every later demotion restored the body to white. Three layers is six orderings and four is twenty-four, so the fix is not a third guard; composing forward answers every ordering question by construction. A family's paint is DECLARED by its class, never sampled off a material. Scope is the body parts: the Warden's shield, the Skirmisher's muzzle, the Boss's apparatus and the modifier halo each already have one writer.

**O129** — Colour carries HEALTH, and the readability pack's claim that it also carries rank is dropped. Minimum pairwise rank L* separation across the twenty authored colours is 0.31 at full health and 0.02 at three quarters — Elite and Boss are the same lightness — while the health axis travels 39-50 dE76 as delivered. Separation is spent where it measures. The ramp ships as a DELTA from each rank's own full-health entry rather than as an absolute body colour, because absolutes collapse the three families' minimum separation from 11.4 dE76 to 0.0 and O24 spends colour on exactly that read; the delta form holds 11.4 at full health and 10.2 at ten percent, and leaves an undamaged body bit-identical to what it was. Rank keeps its own blend layer and its carriers on the body: scale, the halo or the diamonds, and the bar's width. The body never goes overbright — that vocabulary belongs to the hit flash alone.
**O130** — The enemy health bar belongs to the combat lane and its translation unit says so: `Combat/BreakerEnemyHealthBars.cpp` holds the definition while the declaration stays a private member of ABreakerPlaytestHUD. Two lanes shipped a trash-bar rule on the same day and BOTH MERGED CLEAN, because they touched different lines of one 3,596-line HUD file — a merge that succeeds is not evidence that a question has one owner, and the directory is what names the owner. The split needs no widened ACCESS — a member function's definition may live in any translation unit and keeps full private access, so there is no friend declaration and no exported helper — but it is not free, and the cost is the SURFACE: the moved TU reads EIGHT private members of ABreakerPlaytestHUD, four of them mutable state. Drawing: S, DrawBorder, DrawSpecTextCentered. Handle: World. STATE: EnemyBlips, DrawnLabelBounds, LastFocusBarEnemy, LastFocusBarTime. `EnemyBlips` is worse than coupling — it is a PRODUCER/CONSUMER CONTRACT ACROSS A LANE BOUNDARY, filled by the combat lane's TU and consumed by the UI lane's DrawMinimap, and it lived only in a header comment that did not say the two halves had different owners. Eight members is a real cost weighed against a context struct's own; the judgement is that eight is cheaper, and a ruling that records the declaration staying a member without recording what that costs is half a ruling. Where a draw pass moves out, its constants move with it under a namespace of their own — unity builds merge translation units, and two files declaring the same namespace with the same constant names is a redefinition rather than a duplicate.

**O131** — Trash enemies are barred ONLY while aimed at, fading for 0.6s after the aim leaves; ranks above Trash are barred whenever they are inside the 50 m cutoff. A recency window at ANY length is not a filter at fifty-to-a-hundred concurrent with cleave in the kit, because one AoE lights the whole pack. The bar answers “which one am I shooting” and nothing else — damage numbers already answer “did that hurt”. THE TWO HALVES ARE ONE RULE ACROSS TWO LANES: the trash mob at 8% health in a pack of eighty is the highest-value read on screen and neither visibility rule shows it, so it is carried by the BODY — the tint ramp and the fracture mask — and focus-only is correct only because that half exists. The TARGET DUMMY is the stated exception and keeps its 1.5s recency window: a dummy is a gym instrument rather than a crowd member, four of them never move, and “did that hurt” is the one question it exists to answer.

**O155** — A change to any of the HUD state members the enemy-bar TU shares — `EnemyBlips`, `DrawnLabelBounds`, `FocusBarReleaseTimes`, `ShownEnemyHealth` — is a DECLARED CROSSING: the lane making it says so to the other before it lands, and names which member and which direction. This is the first place the ask-for-the-header rule does not apply, because the interface IS a member variable and there is no header to publish — the two halves are compiled into one class and a change on either side is silent at the compiler and silent at the suite. `EnemyBlips` is the producer/consumer case and the sharpest: the combat lane fills it, the UI lane's DrawMinimap consumes it, and the ORDERING is load-bearing — the fill must run before the read in the same frame or the map draws last frame's hostiles. The contract is named at BOTH ends, each naming the other's lane; a comment at one end only is how it went unnoticed the first time.

**O156** — The boss phase readout takes its phase count from `EBreakerBossPhase` and NOT from the readability pack, which contradicts itself twice over: its Boss row is labelled `phase 2 of 4` while the readout it draws reads `PHASE 3 / 4`, and the fight ships THREE phases — Deployment, Suppression, Commitment, gated at 100/66/33. The count is REFLECTED off the enum rather than written down, so a fourth phase updates the readout by existing. Both halves of the fraction are real exposed values — `ABreakerBossEnemy::GetPhase()` and the enum's own cardinality — so O120 is satisfied and the fraction draws rather than the bare count. THE PHASE LIVES ON THE BODY: the boss's world-space bar carries three phase marks at the authored gates with pips below it, and there is no screen-top boss row; the body says which phase and how much health. A third number, `BreakerHealthBands::SegmentCountFor(Boss)` at 8, was raised here against the phase gates and is ANSWERED BY O135, which inverted the answer this ruling first reached for. A multiple-of-three band count is the WRONG answer, not the right one: the gates are AUTHORED floats (0.66 / 0.33), not exact thirds, so six bands put a boundary 0.7% of the bar from the 0.66 gate — near-coincident, which is precisely the failure this was trying to avoid, and the arithmetic here originally assumed exact thirds. Eight keeps every boundary at least 3.5% of the bar clear of both gates, which is separation rather than misalignment. Bands are damage feedback and phase gates are behaviour thresholds — two facts, two marks, and the marks are MEANT to be distinguishable rather than coincident. Bands are state, not drawn ticks: the chip hatch shows damage, and the phase marks are the only marks on the bar.

**O145** — A perceptual measurement of an FLinearColor converts it as LINEAR and encodes it to sRGB before Lab, and every figure states its scope: which family, blend applied or not, clamp applied or not. Reading the stored value as though it were already sRGB inflated every dE76 this project had reported — a Vestige trash ramp measured 44.7 where it delivers 27.7 — and the error survived because the figures were quoted per rank, a format the delta form cannot have: the travel starts from a family base, so it differs per family by construction and a per-rank average is exactly what hides it. A number without its scope is not a measurement, and this one was load-bearing on a ruling.

**O146** — An authored colour delta is applied in the domain it was authored in. The readability pack's hexes are DISPLAY colours; added to a linear value they deliver a travel that depends on where the family base sits, and the spread across the twelve (family, rank) pairs was 30.8 dE76 — Vestige Champion at 27.8 against Lattice Boss at 58.5, with Champion the worst rank in the game to read health from. Encoding the base, adding the authored offset, and decoding back drops the spread to 11.1 and lifts the worst case to 38.8, with nothing retuned: the twenty hexes, both rank hues and both blend weights are exactly as authored. Champion was never a blend problem. The cost is five of sixty triples clipping at the ten-percent stop, worst overshoot 0.191 on Elite's red. THE DELTA CARRIES MAGNITUDE, NOT HUE DIRECTION — a Vestige Boss dies magenta, not red — which is a look question and stays open.


**O165** — A performance instrument reports the scene it MEASURED beside the scene it was ASKED for, and errors when the two disagree. `-BreakerCrowdProbe` spent its whole life measuring something other than its comment: the grid sat at 6000 cm against a 2200 cm `DetectionRange`, so not one body ever detected the player and every figure taken with it is the cost of N enemies on the PATROL branch — including the "the mannequin is affordable" verdict, which does not survive the correction. The load is now named in the flag (`-BreakerCrowdLoad=<patrol|engaged>`) AND measured from the bodies' own state labels, because a name is a claim and this is precisely the class of claim that was wrong. The guard earned itself immediately: it caught a zeroed `SafeZoneRadius` failing to drop the safe ring, since `IsInSafeZone` compares with `<=` and the pawn spawns at the centre. An unrecognised load is REFUSED with the probe unarmed rather than falling back to a default — the capture harness's silent screen fallback is the precedent not to repeat — and the refusal ends the run, because an exit wired to a summary that never comes is a hang rather than a failure. Historical geometry is kept exactly under the `patrol` name so figures already taken stay comparable; they are re-labelled, never re-interpreted.

**O166** — A rift budget takes THREE space inputs, not one, because 5.3's caps are spatial in three different units and one of them is not spatial at all. Sorted by the reason written beside each: the DENSITY cap (12 live) is about whether a movement player can find a lane, so its unit is navigable ground, not floor area; the RANGED cap (3 sources) is about converging fire removing safe ground, so its unit is sightlines against available cover, and a room with no line breaks cannot afford three however large it is; the WARDEN cap (1 per player) is about frontal armour making geometry unsolvable, so its unit is flanking angle, and a corridor may not afford one where an arena affords two. THE ELITE CAP IS NOT SPATIAL — "two modifier sets to read simultaneously" is attention, and attention does not change with the room — so it takes no space term and must not be given one. Collapsing these into a single room-size scalar would price a long thin corridor and a small arena identically while they fail in opposite directions, which is the specific error this ruling exists to forbid.

**O167** — The rift budget REUSES the wave solver's pricing and REPLACES its caps, and the seam is already where it needs to be. `GetWaveBudget`, the archetype cost table and the spend order are space-free by construction — they price what a body costs and in what order the budget buys, and nothing in them consults the ground — so a rift keeps them unchanged. Everything a room changes is already gathered in `FBreakerWaveBudgetParams`' cap block, so a rift supplies that block and calls the same `SolveWave`. THE MEASUREMENTS O166 ASKS FOR ALREADY EXIST AND BELONG TO THE COVER GRAMMAR: `LargestUncoveredGap` is the movement term, `LargestGapToLineBreak` is the sightline term, and the corridor rule plus `MinimumOpenLaneWidth` are the flanking term. A rift interior is therefore not a new measuring problem; it is the existing validators run against a room instead of a field, which is also why interiors must get shapes BEFORE any of this is written (O134). No magnitudes are authored here and none may be inferred from the gym's: every one is O2 PLACEHOLDER until a room is measured.

**O168** — The rift completion seam: FIELD's terminator RAISES, GROUND consumes and owns the completion state, LEDGER binds the completion event and pays. Three commits, one interface, no lane inside another's file. The event carries the WHOLE `FBreakerRiftDefinition` rather than a subset because it is already the thing that travels and a struct grows; it fires ONLY on completion, and abandonment has NO representation on it — a bool for "abandoned" invites paying a reduced amount for walking out, so leaving by the door you came in is simply the absence of the event. IT FIRES AT COMPLETION, IN-WORLD, AT THE LATCH, not at exit: every consumer writes travel-surviving state (Riftglass is the account-wide scalar, XP is save-backed), so broadcasting while the interior is alive depends on nothing that dies with it, while holding until exit makes the payout hostage to the teardown path — a completion followed by a death, a dev reset or a crash would owe a reward nothing can pay, and "completed but unpaid" is a lost-currency report nobody can reproduce. It is also the felt moment. NO IDEMPOTENCE COUNTER IS NEEDED and the mechanism is why: completion state lives on the game mode, which is per-world and destroyed on travel, so a re-entered door is a new world and honestly a new run, and within one run the state is a one-way latch guarding the broadcast. THE RIFT ARCHETYPE IS NOT ON THE DEFINITION and is not added speculatively, so O117's grouped first-clears wait: the payout lands in two steps, Riftglass and XP composition on the event as specified, first-clears when a rift actually varies by archetype. Entry is the same shape reversed and was built first — `ABreakerRiftDoor` does not travel, it raises and the game mode owns what travel means.

## Open

- **Does endgame pacing stay periodic?** The wave solver holds no cross-solve state, which reads as "the gym has no pacing" and is wrong: `RestInterval` and `BossInterval` ARE pacing, just stateless and periodic. Periodic pacing is predictable, and predictable is fine for a campaign and a problem for endgame — a player who knows wave 6 rests and wave 12 bosses is reading a timetable rather than a fight. The cheap answer is a run seed woven into the params, because it keeps the solver pure and testable; the expensive one is genuine state threaded between solves. Nothing is decided, and O134's space term may change what the seed would even perturb.

- **Was the offered-to-spendable floor of 3.0 derived, or seeded?** The same question DECISIONS already carries against the at-cap band's 8-10x, and it deserves the same standing rather than an inherited assumption. VISION says a tree offers far more than any budget can buy; 3.00x means a character buys a third of one tree. Nine doctrines sit at exactly 3.00 today and twelve will once the Caster trios merge, so whatever the answer is, it is load-bearing across most of the register. Note that the figure clears by identity for any tree built to the twelve-node shape, so raising the floor without changing the shape would put every conforming doctrine red at once, and lowering it would make the section unfalsifiable.


**O111** — Class Points are deleted and there are two pools: Core 65 and Doctrine 8. The migration refunds nothing and reads nothing — spent plus unspent was derivable from the payload alone, and O27 deletes the freed points rather than folding them into either pool: v5 to v6 clears the class rank array, zeroes the class wallet, clears any commitment naming one of the fifteen retiring branch ids from a frozen list, seeds the doctrine wallet at zero because it also clears the commitment that pays it, and stamps. No cost table and no node library. The retired enumerator is never removed and never reused.

**O112** — The node-shape band is unpinned pending re-derivation. Sixty percent ranked minors means sixty percent unconditional stat lines, which O76 gives to affixes outright, so the authored target cannot be met without breaking another rule. The measured fourteen percent is honest and must not drive authoring until a band is derived for a tree whose percentages live on gear.

**O113** — Hits-to-die is the authoritative inversion measurement and the retune solves for it. The damage-versus-defence ratio is a proxy: it compares monster damage to the gear health LINE, where hits-to-die compares it to what a character actually carries. A constant base max health that does not scale drags total growth from x3.67 to x2.78, which is the whole of the 32% between them, so tuning to the proxy leaves hits-to-die still falling by 1.32x.

**O114** — The boss band was two measurement errors and the rank table stands. The test read a rifle dealing 13 where the shipped rifle deals 24, and composed a boss rank onto a trash archetype the game never fields; corrected, trash kills in 0.92s, elite in 2.75s and the Field Marshal in 20.6s, all inside O18. The derivation lands on rank TIMES archetype and never on rank alone — x75 on the boss's x0.30 is the x22.5 a 21s kill over a 0.92s trash kill requires — so O59 stands and the rank table is not to be re-derived a third time. Untouched: hits-to-die and time-to-die carry no weapon term, so O116's anchor inversion stands; ability parity and the at-cap band are ratios and stand; the doctrine work never depended on any of it.

**O115** — An invariant asserting a property is invariant must be paired with one asserting its level. Flatness without magnitude reports green at any value, and it has produced four misses now. A CEILING AND A FLOOR TOGETHER STILL CONSTRAIN NOTHING ABOUT THE SHAPE BETWEEN THEM: MS9's detonation curve carried two bound assertions for the whole life of the feature, both passing, while the rewrite named for flattening the curve was steeper than the curve it flattened — 1.87 against 1.70 inside a 2.2 bound wide enough for both. Where a rule is about SHAPE, assert the RELATIONSHIP between the two curves, not each curve's distance from an edge.

**O116** — The O91 retune is landed and it is solved against ONE authored baseline character at BOTH ends: base monster damage 30.7 melee with `d` at 0.0173, giving time-to-die 7.49s at level 1 and 7.55s at the cap. Neither end was the correct one — the belief that the cap was came from measuring time-to-die against a character carrying Core.Health on every allowed slot, the theoretical best roll, which is the error the at-cap band was already corrected for in the other direction. Magnitude questions take the baseline; the falling-curve question takes the CEILING character, because that finding's force is that hits-to-die fell even for the best-geared defensive build. Both are named at their sites and the baseline is authored once, in one file, for both tests. Hits-to-die is asserted as a TREND with a sawtooth floor, not pointwise: a stepped gear ladder against a smooth damage curve must sawtooth, and pointwise monotonicity made the two tests' feasible intervals for `d` disjoint. An archetype that overrides a chassis value states a ratio to the melee default and moves with it — the ranged 35.0 is 30.7 times the authored 1.14.

**O117** — Rift-archetype first-clears are TWO GROUPED grants, not eight individual ones, and the mission table already answered it. A1-5 and A2-7 each pay a first-clear AND a Core Point, and A2-7 reads “Set complete → Core Point #7” — under individual grants both rows would be paying twice for the same clear and the word “set” would mean nothing. Two grouped slots is also what the canon fifteen-source list allots, so the alternative could not fit inside the total without displacing something else. The eight archetypes themselves are authored — Clear, Sever, Hold, Hunt, Escort, Carry, Collapse, Silence, sited across A1-4 to A2-7 — and what is missing is not vocabulary but a rift completion event to fire on.

**O175** — Swift's kit completes to the designed 6+1: Slipcut, Hard Stop and Sightline land as `Swift.Slipcut` / `Swift.HardStop` / `Swift.Sightline` with §1.2's costs and cooldowns (20/4s, 30/6s, 25/6s) under the O2 placeholder banner. Sightline's "cannot be blocked by cover-state enemies" clause is RETIRED (Part One-U: a dead sentence gets no new referent); its pierce grant is a count on the existing channel stack, so the per-target falloff and the geometry stop stay the weapon's rules.

**O176** — Swift's starters are an enhanced-dash passive plus Skim (owner overturn of this ruling's first form, ORDERS Part One §1): the passive is a TREE NODE granted at level one, never a slot occupant, so Swift's slot one holds Skim, slot two ships EMPTY until the first quartermaster unlock — the empty slot is the feature — and Lead, Slipcut, Cadence Break, Hard Stop and Sightline are the five unlockables. Executes as LEDGER's class-definition bucket lines plus a fifth token milestone (five purchases need five tokens by 50, or ReachableByFifty is red forever) landing together with KIT's DefaultAbilityIdForSlot flip and the StarterPair reshape; until that landing the three catalogue tests it moves are enumerated red. The dash node's shape is LEDGER's report before its magnitude. The v4→v5 save snapshot stays frozen at its history.

**O177** — Hard Stop is an ability, not a Skim mode: the pitch-gated modal branch retires from Skim, whose cast is the redirect whole. Skim Discipline keeps the twice-per-airtime half; Spend to Live's doubled cost and true-immunity window ride the standalone ability at S4's authored 30.

**O178** — Audio splits at the lane boundary: GLASS owns the system — `Audio/`, cues, mixing, the four-verb director and any roster it grows — and KIT owns when an ability fires one: the trigger, the timing, which moment of a cast gets a sound. Ability cues wait on a GLASS-published interface; no sound system may grow inside `Abilities/`.
- The class layer's shape. Under exploration: doctrines of 4-5 transformative nodes at an 8-10 point budget, adjacency replacing investment gates, and the freed points moving to the Core budget.
- The zone-level table, and the mapping from content difficulty to tier bonus.
- Whether 134 items per hour is the right drop rate. It sits mid-band and has never been argued about, and it governs whether a 300-400 hour chase reads as generous or as grinding.
- Whether the at-cap band's 8-10x was ever derived or was seeded. It is the only reason that section is pinned as a target rather than a measurement.
- Where the permanent class choice is made, once the Anchor is authored.
- What replaces the shared pool as the tension between the two defensive constellations.
- Whether weapon archetype outweighing class is intended identity or a table to flatten.
- What becomes of the node ability-grant path. After the one redundant grant retires it has three readers on the node card and no writers.
- Whether the quartermaster has an endgame role. Its stock is finite by construction — one token per unlockable — so it empties once the last ability is bought and is a levelling-window service thereafter.
- Whether deployable stats snapshot at placement or read live, and — the same question wearing its other face — whether a deployable composes the outgoing More chain. It does not today, so a live ability window reaches every ability except a turret, which makes the one multiplier rule the canon actually enforces advisory in one place. The two are entangled: a deployable that snapshots at placement is *right* to miss a window opened afterwards, and one that reads live is not.
- Whether enemies deal elemental damage at all.
- Where Anomalies, Raids, Dungeons and Conquest sit relative to the five canon locations.
- Whether endgame tiers are capped or unbounded.
- Whether one Anchor is the settlement layer, or a network.
- Whether the ending's premise contradicts an endgame made of rifts.
- Whether the two reward bands under each of Champion and Boss collapse to one.
- Whether the dodge resource refund survives as base kit, becomes a tree rewrite, or dies.
- Whether party loot is instanced per player.
- Whether Core Point respec carries friction at the keystone tier.
- Whether one keystone per character is the right ceiling across all fifteen branches.
- Whether the Core tree keeps a hub. A hub that is one of the axes privileges that axis.
- Whether the offered More count grows if the axis count grows.
- The conditional-line payout ratio.
- Whether the modifier scope that disables a defensive layer also disables the class resource it generates.
- How much bespoke geometry elite modifiers need.
- Whether the inventory grid gets the frame width its three-across layout needs.

**O179** — The ability presentation colour law, written down so five classes stop inheriting an unwritten convention: colour is assigned by VERB from the palette's existing roles, one look per verb whatever class performs it — cyan (player/system) for movement and cleansing, orange (weapon/heat) for weapon-economy casts, explosions and deployable arrivals, gold (reward) for heals, leech payouts and weak-point promises (a heal is a payment received), Harm red for a taunt painting its caster as the target, and violet for every ultimate's ignition. Melee sweeps share Cleave's swept-arc composition exactly. Three structural rules ride with it: cast MOMENTS get world flashes while windows stay HUD bars (a world aura pinned to a cast point lies on a moving character); a fixed world point may carry a lifetime-length primitive (a fuse, a placement) and nothing else may; and the camera law — never wrap a primitive around, or run a stroke out of, the one camera guaranteed to stand in it: self-anchored draws sit at the feet off-axis, aim-line strokes start offset to the weapon side. All magnitudes O2.

**O125** — Health bands are state on every rank, Trash included: `BreakerHealthBands::SegmentCountFor` returns a real count everywhere and the bar draws what it can — display may show less than state knows, never disagree with it. The band condition is `TargetBandBroken`, per-target with previous-hit lifetime: the breaking hit pays nothing, the next landed hit does, and every landed hit (a snapshotted DoT tick included) overwrites the bit while a dodge touches nothing. Overpressure takes it in the conditional band. Collapse does not move yet — a target-gated More has no payment lane, since standing aggregation never holds a target bit and the rider lane is Increased-bucket only by rule — so the shared-pool move waits on the per-hit More question below.

- Whether a target-gated More gets a per-hit payment lane. Collapse's intended shape (×1.30 shared, gated on TargetBandBroken) cannot pay through either existing lane; the outgoing-modifier chain's O34 budget clamp is the precedent shape for one. What it would open: TerminalVelocity × Collapse × Overflow = 1.30 × 1.30 × 1.28 = 2.1632 on an ability build — 98.5% of the 2.197 ceiling, and the weapon lane's zero-commitment unconditional product falls from 1.9825 to 1.8605.

**O135** — Boss health-band boundaries deliberately avoid the phase gates. Bands are damage feedback and phase gates are behaviour thresholds — two facts, two marks — and the gates are authored floats (0.66/0.33), not exact thirds, so a multiple-of-three band count is the one wrong answer: six bands put a boundary 0.7% of the bar from the 0.66 gate, and near-coincident marks read as a rendering defect. Boss stays at 8 (every boundary ≥3.5% clear of both shipped gates), pinned in `HealthBands.BossBandsAvoidPhaseGates` against the default-constructed params; bands are state the pin holds, never drawn as ticks. If the gates ever move to exact thirds and coincidence is wanted, that is a joint ruling: gates, count, and the bar's heavy/light marks together.

**O136** — The endgame band 12–20× is where most builds land, never where every build must land: the upper edge is a target, not a ceiling, and a rewrite stacking past it is intended feel (owner: "this is fine i dont mind over the mark damage most of the time" — Prolific composes 22.64× and stands). The layer-fit assertions are retired rather than widened, the rollable layer's only bound is its authored per-step ceiling (1.35; Prolific's own 1.5), `rewrite-impact` re-pins against `MaximumProlificRuleStep`, and the derived layer ceiling survives solely for O96's major/minor-stack partition, which this ruling does not touch.

**O138** — The ability-token schedule is derived, never authored as a list: the first-token level and the completion level are authored (5 and 30 — every class finishes together, owner-ruled), the milestone count is read from the class's unlockables, and spacing is convex (`t^1.2`, O2) — early unlocks close, late far — chosen because it reproduces the retired four-class schedule {5, 12, 20, 30} bit-identically (the authored gaps 7/8/10 were already convex) where even spacing would have silently retuned it. Swift derives {5, 10, 16, 23, 30}. Pinned in `AbilityUnlocks.ScheduleDerives`.

**O139** — Swift's granted dash passive is Longstride: `Swift.Kinetic.Longstride`, rank 1 seeded on every path a character becomes or loads as Swift, cost 0 so the respec-no-refund property is arithmetic rather than a special case, and the reading is DISTANCE (`DashDistance`, +20% dash impulse before the momentum hard cap, O2) — resolved by the seat's own rule: the wiring counted one enum entry, one stats-field wire and one movement read, the cheap side of the stated line, and distance is felt on the first dash where a cooldown shave is felt on the fifth. Converting to the cooldown reading is one commit if overturned. The lane is single-bidder and migrates onto the aggregator the day a gear dash affix exists.

**O140** — The two Swift stand-ins retire, on their own written schedule, now that O175's abilities exist: the Marksman Sightline node's +2 Pierce (authored so its armour rule never rode zero penetrations, retirement promised "the day the grant goes in") comes out and the node is the pierce-armour rule alone; the Marksman Lead node's Swift.Lead grant (a free route around the quartermaster token since ruling 1 made Lead an unlockable) comes out and the node is the two-target rule alone. That was the last `GrantedAbilityIds` writer — `NoPhantomAbilityGrants` now pins the writer count at zero so a new grant announces itself, and whether the writerless path survives stays the owner's open question.

**O141** — Collapse is the game's ONE target-gated hit-time More: `Core.Ruin.Collapse` re-authors to ×1.30 on the shared pool gated on `TargetBandBroken`, paid by the rider path multiplying the request's standing More product under the one O34 ceiling — headroom, never a slot, so it never enters the strongest-three sort and never counts on the sheet's "N / 3 MORE". It delivers in full where headroom remains (an ability build composes 2.1632 of 2.197) and partially or not at all where it is spent (×1.1355 at a weapon build's 1.9349; nothing at saturation) — the asymmetry is the point: a hit-time More is structurally an ability-lane buff, delivering against the 0.27 parity gap as ceiling arithmetic. AT MOST ONE such line may exist; a second is a request to revisit 1.30³ and `TreeContent.OneHitTimeMore` pins the population. This closes O125's open bullet.

**O142** — The parity fixture is re-based on the ability-built tree (Part One-M's authorisation): `AbilityOptimizedRanks` is a ring-legal 65-point structural mirror of the weapon fixture — Reservoir complete from its entry, ARC complete to Overflow, the same Velocity chain for the same shared More, the same Ruin dip, travel on Ability picks — and the measurement moves 0.27 → **0.621** at cap (0.20 → 0.356 endgame). The prediction on record was 0.48–0.51 and it was LOW: the estimate under-counted the mirror's travel purchase (14 ability picks, not 8) and ARC+Reservoir's breadth. The honest remainder to O99's band is 0.62 → 0.85 (×1.37), decomposed live every run: flat 0.867 (structural — no ability flat line, the owner's affix-layer question), More 0.860 (two ability-reaching Mores exist against the weapon lane's three; Collapse's hit-time ×1.30 sits outside standing measurement by design), crit 0.863 (the ability spend forgoes Precision's flats), increased 0.965 (nearly closed by the re-base alone). Content tuning now aims at a number answering the metric's own question.

**O143** — The rift completion payout, O168's third commit: `Progression/BreakerRiftRewardMath.h` composes Riftglass and XP from the rift's effective area level riding the CHASSIS'S OWN growth constant (read from the authored default, never restated — a payout that did not grow with monster health would shrink per minute of fight as the ladder climbs, O137's law), and `UBreakerProgressionComponent::HandleRiftCompleted` binds GROUND's `OnRiftCompleted` at BeginPlay (authority, weak), filters by pawn, grants XP save-backed and Riftglass through the owner's wallet — both travel-surviving, which is what firing at completion was ruled on. A pawn with no wallet forfeits LOUDLY. The payout deliberately does not deduplicate: the seam's per-world latch already makes every broadcast a distinct run. Bases O2 PLACEHOLDER, anchored bit-identical at area level 1; O117's first-clears join when an archetype exists on the definition.

**O144** — Wall-ride's build vocabulary retires with the verb (Part One-R's LEDGER half, landed in KIT's agreed window): `WallRiding` joins the never-evaluable set permanently — a retired verb, not a missing recorder, so the census ceiling moves 4→5 and its target floor becomes 1 — and nothing may re-author against it. `Core.Velocity.Traction` sheds its line and re-targets the day a new verb records a narrow-window state, never before. `Swift.Kinetic.Grind` sheds its line and is MARKED FOR FULL REMOVAL as its own deliberate act: its fantasy was the wall itself, and the deletion owes a save story (a paid rank on a deleted id refunds nothing), the Kinetic count pin, and the fixture rows — three decisions that do not ride tonight's window. A vault or mantle condition is authored as a new appended entry when those verbs record state.

**O180** — A removed node must not tax the save that bought it: on load, a rank row whose id no longer resolves is DROPPED AND CREDITED to its currency's wallet at the same fallback cost the spent recompute charges (1 per rank) — exactly undoing what that charge took. Before this, an unresolvable row was charged at fallback and granted nothing, silently and forever; the audit assertion that recorded the tax now pins the repair. The sweep runs at load only, after the class definition resolves (and skips loudly when none can — a resolver-less sweep must not read "wipe everything to the wallet"), and it makes every future content removal safe by construction. First application: `Swift.Kinetic.Grind` is deleted in the same push, in this order, closing O144's deferral — the number allocated at push time per Part One-U item 19.

**O181** — The ability probe may equip what a character has not unlocked, through one dev-only surface: `UBreakerProgressionComponent::DevForceEquipAbility` bypasses the unlock check alone — class membership, the duplicate rule and the ultimate-slot pairing still refuse, LOUDLY, because the probe must never photograph an impossible loadout — and Part One-U item 16's never-save guard (KIT's, on the probe session) is the condition the whole surface stands on. Never ship a path to it. Number allocated at push time from the rebased ledger.

**O182** — The equip requirement is derived, never stored: `RequiredLevel = min(ItemLevel, MaxCharacterLevel)` in `Items/BreakerItemRequirements.h` (One-AA's ruling; a stored field would freeze the curve into every save, the mistake `BreakerItemBaseStats.h` names). The gate lives at the PLAYER-FACING entry points and never inside `EquipItem`, which stays the ungated mechanism; the roster is EquipFromBackpack today and the stash withdrawal when it lands, pinned in `Items.Requirements`. The gate expires at the character cap — three ceilings share one ladder and a rule that expires with its purpose has no blast radius — and it REFUSES when no character level is readable, because a gate that passes on missing data is a silent bypass on a timer. Number allocated at push time.

**O183** — The container caps: stash 70, backpack 25, both O2 PLACEHOLDER (One-AB, from the owner's numbers — 8.75 and ~3 sets of eight slots). A full backpack's refusal splits by who loses the item: a REFUSABLE entry (ground pickup, stash withdrawal) refuses at the cap and the item stays where it was — never destroyed, never swapped, never auto-salvaged — while a grant that already charged the player (forge output, quest turn-in) lands even past the cap, because refusing a paid item destroys it; each grant source owes its own pre-payment check at its own site. Consumables never share the gear cap when O122's entry tokens arrive. Pinned in `Items.Backpack.CapRefusal`.

**O184** — Flat damage is real and gear-authored: Added Weapon Damage and Added Ability Damage sum before the Increased bucket, roll on the tier ladder, and are scaled by a bounded per-archetype effectiveness coefficient. Trees author no flat; `bAddedDamageAlsoIncreased` retires; flat's share of each band is pinned.

**O185** — The slice ships solo. A two-player PIE listen-server smoke runs every cycle, judged from the client seat: resources, HUD, the custom movement mode, one kill, one drop. No sessions, lobby, party UI or OnlineSubsystem before Anomalies has been played. No new system may read per-player state through a local-only path.

**O186** — A DATA lane owns `Data/`, the loader and the validator. Tabular content is JSON, the source of truth for the game, the tests and the census alike; DataAssets are for asset-referencing tables only. A library migrates in one commit with its output pinned identical; a magnitude change is done when it needs no compile.

**O187** — Gameplay tags are defined natively at their first consumer; the ini declares nothing no consumer requests. The unreferenced tags go.

**O188** — An account lives on its player's machine. The host holds the host's account only; a remote pawn's wallet is seeded from and written back to its owning client's account across the connection. The smoke's shared pool is a defect; GROUND-5 builds the seam.

**O189** — A death flashes Harm, Gold on a weak-point kill — the kill confirm's pair, one event one colour in the world and on the HUD. The killing verb's colour is drawn at the verb's own site, never at the death.

**O190** — Any asset only the editor can author (a Niagara system, an AnimBP graph, a material) is an ASSETS item: the lane that needs it ships the slot, the fallback and a written recipe, and closes on those, never on the frame. The four NS_ systems are ASSETS-5.

**O191** — The safe zone is a null nav area: no body plans across it. A body whose target is inside holds at the nearest reachable boundary point. The Breaker.Nav.Probe switch joins the capture list in CLAUDE.md.

**O192** — base kit slower at level 1, air control 0.55 → ~0.35, and Movement Speed becomes a Boots-only affix with a cap. The MoveSpeed aggregation lane already exists, so it's an affix and a number, not a system. Momentum's thresholds move with it.

**O193** — you found the hole: an earlier ruling removed the death sting as "over-produced," and now the beat is a caption and a timer. This restores it as weapon-lower → camera drop → black → fade-in with one low sound, sequenced by GROUND with KIT/GLASS/AUDIO crossings.

**O194** — Standard/Elite/Champion at +0/+15/+30% scale with the halo and diamonds; ELITE leaves the label; and CLOSING/HELD leaves entirely — that's an instrument reading that went out in the shipped HUD.

**O195** — "TILESET START," "BANKED," "SETTLED" are dev vocabulary on screen. Every player-facing string not already a dialogue or quest row moves to a string table under Data/ so you rewrite copy without a compile; dialogue and quest rows own their own text, one home per string. "Cringey text" becomes a data edit, not a lane request

**O196** — Flat damage is real and it is the weapon's. Every weapon has a base damage line that scales with its item level; prefix affixes on weapons and gear roll flat Added Damage that sums onto that base before any Increased or More applies (`(Base + ΣFlat) × (1 + ΣIncreased) × ΠMore`). A flat value summed into the Increased bucket is a defect.

**O197** — A boss fight is not balanced to one loadout's reserve. Kit growth pays the gap between rounds carried and rounds needed; no ammo rule is written for it.

**O198** — A shield breaks. The Warden's frontal shield is a pool of 15 % of its bearer's max health that absorbs frontal damage until it breaks, then is gone for the fight; flanking still finds the unarmoured rear. The boss's shield follows the same rule.

**O199** — The vitals row is shield current beside health current, with the health max after the value in the small numeric. A zero-max shield draws nothing: no track, no number.

**O200** — The at-cap power band is measured from a rolled best-in-slot at the cap's item level, re-rolled from the live affix table each run; the hand-listed baseline fixture measures baseline viability only. A row that never rolls is a row the measurement never sees, by design.

**O201** — A save carrying a rule that no longer exists resolves to no rule and prints nothing; the fix for a real save is a migration that re-rolls from the current table, never a label.

**O202** — Where a design section's spec.md and its mockup disagree, spec.md is canonical.

**O203** — Rank on the body: an Elite's health bar is the Exceptional rarity colour, a Champion wears two diamonds off its bar ends, and modifiers are drawn system-colour geometry above the bar, never coloured and never a fill; the coloured modifier disc and its light are retired. The 2 px system-colour ellipse at the feet is the contract's mark, not a rank tell: it draws only while an active contract asks for marked kills.

**O204** — The trees keep two counters, Core and Doctrine (O111 pools), and doctrine depth carries no tier gate below the keystone (O272); adjacency on the doctrine board is drawn as links between spent nodes and grants no reachability.

**O205** — At the equip limit the equipment component supplies the swap candidates and the pre-focused one, and validates the choice; the picker offers and never computes the list.

**O206** — The Forge's currency is Riftglass and its five verbs stand as built: Temper lands exactly on the next tier, Reforge rerolls values in band, Attune rerolls which affixes, Salvage and Respec stay. Nothing binds an item to a rift.

**O207** — Where a play measurement and a canvas disagree on a size or a window, the play measurement stands: damage numbers 26/52, magazine 32, aggregation 120 ms.

**O208** — Crit is weapon-orange and gold stays the weak-point promise (O179); an absorbed hit prints its value muted.

**O209** — The Anchor's NPCs are Kess and the Quartermaster until the story names more; a designed name with no dialogue row is not an NPC.

**O210** — XP and level leave the combat HUD once the pause plate carries them; the pause screen has no LOADOUT row.

**O211** — The Core wheel is one tree shared by every class, its points level-driven; Doctrine is class-specific, committed at its story beat, its points story-driven. Class flavour on the Core is a lit recommendation, never a lock; the hub is the only start.

**O212** — The wheel has five domain sectors — movement, weapon, defence, ability, elements — each holding its constellations as wedges. A constellation with a silent node is drawn dark and unselectable until its plumbing exists; the wheel never hides a sector.

**O213** — No bead is a travel node: every rank carries a magnitude. Core respec is free until level N and costs Riftglass after; Doctrine respec stays the Forge's.

**O214** — Act I ends on The Holdfast, a Vestige mass, and no Altered commands before Act II; the Field Marshal is Act II's. The Boss beat names The Holdfast the day its class exists and stays unnamed until then.

**O215** — Doctrine commitment is sited at the first Kess turn-in and never gated on it: the Forge accepts a commitment at any point after the class lock (O86).

**O216** — Act I's givers are the Quartermaster and Kess; Command enters the story when it has a dialogue row (O209).

**O217** — A Volatile blast reaches every live pawn in range, enemies included, at the one monster-scaled number; enemy projectiles and zones stay enemy-safe. Volatile is the only enemy fire that hurts enemies.
**O221** — A rewrite on an Aberrant or Unwritten item may author a More; gear and tree Mores share the three slots under the one ceiling. Ordinary affixes never do.

**O222** — The elements are verbs: Rift displaces (Unstable), Entropy decays (Rot), Void erases (Erased). Each is a buildup status whose threshold scales with the chassis and whose damage snapshots from the applying hit. Bleed and Poison stay physical. Erased pays its snapshotted damage as one delayed burst; an active mark cannot stack or refresh, and consumption cancels its unpaid burst.

**O223** — One reaction per pair, fired by consuming the first status: Rift on Rot is Collapse, Void on Rot is Wither, Rift on Erased is Tear. A reaction spends only the damage already earned; no triple, no chain.

**O224** — Enemies deal elements by family. Resistance reduces buildup rate, never damage. Conversion affixes carry weapon builds into an element.

**O225** — Elements are built one at a time, each end to end before the next: Entropy, Void, Rift, then reactions. Status.Void's armour and healing reduction is retired.

**O226** — Large persistent destinations retain fixed regional enemy levels; tougher return Rifts supply later challenge. The campaign passes through these destinations and leaves them available for revisits, supported by an in-game map.

**O227** — Starter Caster builds should sustain casting competitively with rifles. Patience improves that baseline; purchasing it is not the requirement for sustained casting. Exact recovery magnitudes remain O2 tuning.

**O228** — Afterimage leaves a two-second tail at half of each window's lane contributions, using each lane's existing quantization; anything that is not a lane contribution ends normally.

**O229** — Endurance feeds combined health, shield and front-pool current/capacity into the owner's current/maximum health view without changing condition rules; Hold's cap and enemy-facing thresholds retain physical health values.

**O230** — Ignore Me generates zero threat while allies and deployables generate double; when no non-zero-threat entity exists, targeting falls through to existing selection.

**O231** — Sympathetic's statuses are consumed by the triggering hit, never by another reaction; each affected enemy defers the owner's elemental status applications for three seconds from reaction firing while buildup continues, then applies eligible accumulated buildup; allies, Bleed and Poison are exempt.

**O232** — Ballistics Loud adds the nearest enemy outside the original blast within one further blast radius, without imposing a target cap on ordinary splash.

**O233** — Deployables have health and zones have duration; zones are not destructible, and Threat Decoy grants only deployable health.

**O234** — Core lane rank one opens downstream routing while ranks two and three remain optional; later gateways require an owned node in either neighboring wedge, and keystones still require18 prior local points, so cheap travel does not bypass commitment.

**O235** — Core keeps the full 22-wedge roster and Weapon → Defence → Ability → Status → Movement → Utility ring, wrapping Threat to Precision. Its 429 offered points against 65 measure 6.6x; the former uniform-wedge 3–5x density band does not establish a valid band for rank-one routing. A replacement band needs play data; do not change the numerator or widen a pin to conceal this. Cheapest gateway/lane/convergence routes cost 1/4/10 points in a major and 1/4/9 in a minor; one keystone costs23, two46, three69.

Current native route costs; sector totals omit travel outside that sector. “Lane” means one rank-one minor plus its notable in every wedge.

| Sector | Gateways | One lane each | Every convergence | Every node at minimum ranks, including keys | Full ranks |
|---|---:|---:|---:|---:|---:|
| Weapon | 4 | 16 | 39 | 78 | 91 |
| Defence | 5 | 20 | 47 | 73 | 91 |
| Ability | 4 | 16 | 38 | 64 | 78 |
| Status | 5 | 20 | 48 | 87 | 104 |
| Movement | 2 | 8 | 19 | 32 | 39 |
| Utility | 2 | 8 | 18 | 18 | 26 |

Maximum wedges reached with65, including ring transit; cells show **wedges / minimum cost**.

| Depth | No keystone | One keystone | Two keystones |
|---|---:|---:|---:|
| Gateway | 22 / 22 | 22 / 44 | 21 / 65 |
| Lane | 16 / 64 | 11 / 63 | 6 / 62 |
| Convergence | 7 / 65 | 5 / 59 | 4 / 64 |

**O236** — Overlap seeds editable25% of the third elemental chassis threshold as fresh buildup with no transferred damage credit; only a subsequent accepted direct hit can fund its resulting status.

**O237** — Caster periodic applications retain their applying hit critical sample and never roll again; longer Rot windows redistribute unchanged finite damage, and Terminal or Long Dark persistence pays only remaining funded damage, never replenishing reaction credit.

**O238** — Generic StatusDuration extends periodic elemental tick windows and redistributes their finite budget; Erased delayed payments and Unstable markers keep authored timers.

**O239** — Core respec is free below the level the ability token schedule completes at, and costs Riftglass at or above it: free while the game is still handing the player pieces, priced once the kit is whole. N is not an independent number and is pinned equal to AbilityCompletionLevel.

**O240** — Momentum on the gun stays tighten-only: an empty bar is baseline and never a penalty, because a bar that punishes its empty state rebuilds the trap O92 deletes. The full-bar floor is a felt figure, not a ruled one.

**O241** — The death beat's camera roll is zero. Only the pitch is authored; an unauthored figure that ships becomes canon by accident. The channel stays for the day the beat reads flat.

**O242** — The slide is the crouch. There is no separate crouch verb: cover geometry and the aim blend serve what crouch is for, and a static stance fights a movement kit built on slide-jump, vault, mantle and dash.

**O243** — A rift's tier flips to Endgame only in the commit that makes entry consumable. The death budget stays wired and unreachable until then, because a limit without a stake is a loading screen.

**O244** — The third boss leaves the campaign and becomes the endgame's first: Act III's climax is a meeting and stays one. It is named the day its class exists.

**O245** — A Volatile blast credits the killer of the Volatile, not the corpse. Kills it deals pay the player's kill hooks; the single monster-scaled number of O217 stands until a playtest says otherwise.

**O246** — An ability's numbers have one home in Data/. Compiled defaults are deleted once a schema test replaces the freshness pin that compares them; a fallback that restates the data is drift waiting to happen.

**O247** — Provoke is a forced-target window that also grants threat, not a status. The forced window is the burst and the threat grant is its tail, so the Tank still holds the pack when the window closes.

**O248** — Special items rolled over their affix budget are grandfathered, never stripped. The budget binds new rolls; a save that deletes an owner's affixes is worse than an item that outlives its rule.

**O249** — Drop Chance above its cap converts to rarity weight. A stat that saturates is a stat the player stops reading.

**O250** — A broken Warden front is gone for the fight. A revive restores the body, never the front: a rescue must not hand the boss a second punish window the player already paid to remove.

**O251** — An occluded enemy yields its bar with its marks. A bar drawn through a wall is the readability problem the focus rule exists to solve, and half a nameplate yielding is worse than none.

**O252** — Abilities carry a skill level of 1-15 earned on a shared pool, so every ability a character owns sits at the same level and unlocking a new one never costs the player the level they had. Character levels 1-50 run alongside it; unlocks finish at 30 and skill level keeps climbing.

**O253** — Skill levels roll on Waist and Necklace, gated by affix tier: +1 at T12-T7, +2 at T6-T4, +3 at T3-T1, +4 at T-1 alone. Campaign play crosses only T12-T6, so the top of the ladder is endgame by construction rather than by rule.

**O254** — The ability lane's flat half already ships as Ability.AddedPower and no second flat line is authored. The lane closes through breadth in its Increased pool and an ability counterpart to the weapon-only critical line: the measured shortfall is increased 0.735 and crit 0.812 against a flat that is already 0.867. Skill levels are free power and cannot discharge this band, which compares allocations.

**O255** — A passive ability occupies a class slot and competes with the actives for it. A player who fills both class slots with passives has built a loadout, not opted out of one.

**O256** — Every class carries eight actives, one ultimate and two passives. The ultimate sits beside the eight rather than inside them.

**O257** — Wildcard holds one non-ultimate active from another class in a Support class slot, chosen at the Forge, spending Charge at the borrowed cost plus half again in cost and cooldown. Support never carries a second resource bar, and the tax is what makes it access rather than efficiency.

**O258** — Skim retires and Slipcut is Swift's starter. A directional impulse that redirects horizontal speed is a second, costed copy of the dash Swift already has innately, and SkimDiscipline moves or goes in the same commit.

**O259** — Passives bid into lanes the game does not already crowd — area, duration, deployable stats, resource generation, ammunition. A passive granting a weak damage More is worth exactly zero, because only the strongest three damage More sources apply.

**O260** — The top rarity is Unwritten in code, data and screen alike. The enumerator's value never moves, so the rename is safe everywhere the name is not a stored string.

**O261** — An enemy's death damages other enemies only where a player source says it does: an item, a tree node, an ability. A pack does not thin itself, and the on-death chain ships off by default until such an effect exists.

**O262** — The endgame is a device, a key and a tileset. The campaign runs through authored destinations; past it, a command post in the Anchor consumes a rift key — an item carrying its own area level and rolled modifiers — and opens an instance whose layout is generated from the key rather than authored. A rift's shape is a function of the rift, never of the session.

**O263** — Item levels 1 to 10 carry no equip requirement. The gate exists to pace levelling, and it has nothing to pace in the opening hour: an ilvl 7 drop that cannot be worn until character level 7 makes the first drops of the game into inventory clutter. RequiredLevelFor stays derived and clamped; only its floor moves.

**O264** — A landing latches the boosted speed ceiling into its bleed. The boost still survives a landing rather than vanishing (O-D1(a) stands), but it stops ratcheting: jump, slide, jump held nothing back, so a sprint chain kept its speed indefinitely and the whole kit read as slippery.

**O265** — The map is fast travel. From anywhere that is not the Anchor it offers exactly one destination, the hub; from the Anchor it offers the ordinary registry. Travel is refused in combat, in both directions, so it can never be an escape. A player is never stranded in an instance and never routes between instances without passing through the place that holds their stash and their Forge.

**O266** — Abilities wind up. An authored cast time delays the RESOLUTION, not the press: the Mana goes on the keypress and is never returned, damage taken interrupts the cast, and an interrupted cast is a loss. Movement and defensive verbs author no cast time — a dodge with a wind-up is not a dodge. Cast Speed is a divisor lane over the authored seconds, on the DashCooldownReduction convention.

**O267** — A rolled item names itself after its two strongest lines: strongest prefix, base, "of" strongest suffix. Tiers count down, so strongest is the smallest tier and an untiered special outranks every tiered roll. The name is DERIVED, never stored, for the same reason the equip requirement is. A legendary keeps its authored name — it already has one, and burying it under generated grammar would waste the only names anybody wrote.

**O268** — The ordinary world has patrols and they come back; a rift is an instance with a completion condition and its population stays finite. A cleared pocket returns to its authored composition one body at a time, on the world's clock; a tear never opens within sight of the player, while an authored mouth (a bay doorway, a dock face, a seam mouth) may stand in sight because the walk out is the tell. This REPLACES the roam's finiteness rather than merely reversing it: what makes the roam and the rift feel like different verbs is now the rift's objective and its dilapidated dressing, which is why that dressing is legibility and not decoration. Returning patrols pay ordinary XP and loot — the first clear is what the campaign's pacing is tuned against, and it is unchanged.

**O269** — A pool the player HAS is named by colour, and a pool is not a verb. Health keeps the bone every readout of the player's own state wears; the shield takes a blue that is neither the movement cyan nor the reserved teal band. O179 is untouched: hues still mean one player action each, and this says that the two halves of one shared track are allowed to differ because nothing else can tell them apart. Both pools also print their figures — health over its maximum, the class resource over its pool, XP into the level over what the level costs — which overturns the earlier "the exact total is a thing to check in a menu": a bar answers how much is left and a figure answers whether the next cast or the next pocket is enough, and those are different questions.

**O270** — A closed rift offers three items at the ceiling of the area's item-level band, floored at Exceptional where the level allows it and Uncommon below, salted per run; the player chooses one on the closing card and that one goes into the pack. The other two are gone.

**O271** — A Rot recast spawns a new puddle wherever it lands; nothing merges into a live one. A press during a wind-up queues one cast, which fires when the wind-up resolves. A refused or interrupted cast makes no sound.

**O272** — A doctrine is a wheel of PAIRS: one travel node (single rank, one scaling magnitude equal to what its ranks used to total) whose purchase unlocks one impactful node (single rank, a rule or behaviour tag). No doctrine node has a second rank; no pair is gated below the keystone, so a benchmark's two points always land an impactful node and any pair may be the first. Six pairs are offered and the eight points buy four; the keystone is the impactful half of its own pair and keeps the six-invested gate, so it is the fourth pair by arithmetic. Every node id, tag and consumer stands.

**O273** — A boss holds a standoff ring and faces the player; it closes only to punish, and the slam is its only closer. From the ring it fires one large, slow, telegraphed projectile at the sweep's damage; the tell is the apparatus raise pointed at the player, and a raise pointed anywhere else is still an order. Phases tighten the ring and widen the volley; nothing shortens a wind-up.

**O274** — A tear is an event, not a fixture: it opens where a patrol is about to return, holds while bodies come through, and closes after the last one; the world at rest shows no tears. A body emerges before it hunts: it appears inside the tear on the tear's appear clock, walks to its post on the shipped leash, and takes no target and no damage until its emergence clock ends. Where a patrol returns from is authored ground first (a `spawn` marker at a bay mouth, a dock face, a seam mouth) and a tear only where the composer authored nothing. Rift interiors keep instant waves until rooms are shaped (O134).

**O275** — A supply chest stands behind full-height cover or inside a bay, never on open lane ground; the session seed picks the sites, not the coordinates. It always pays one item at the completion floor (Exceptional where the item level allows it, Uncommon below) plus the currency floor. Opening one is a director verb.

**O276** — Fernhall carries a fourth yard off the entry plaza's west flank with its own door and its own rift definition, level beside the entry yard's, referenced by no mission beat, completing on its generic terminator; it carries its own pocket. A yard's rift definition is authored beside the yard, never derived from another's.

**O277** — The world is lit by one low warm sun and a cool sky, and the air has depth: haze begins inside play range, not beyond it, and the frame is anti-aliased. The runtime rig in BreakerWorldBasics is the lighting authority until a map authors a light (O42); its numbers are O2 and judged by photograph.

**O278** — Buildings are built to the player: a storey is 3 m, a door 2.2 m, a step under 45 cm, and a tile is never stretched past its own proportions — a wall is stacked and tiled, not scaled. Every deck and dock the composer places is reachable on foot by a stair the movement component can climb.

**O279** — A surface shows what it is made of: a kit piece keeps the material it was imported with, and a composer slab wears a tiled ground material with a world-aligned grain, tinted by role. The builder's flat colour is the fallback for a piece that carries no material, never the default.

**O280** — Every prop the composer places is a real mesh from the committed kit, sized to the player; the supply chest is a crate with a lid, not a cube, and keeps its gold band (O179).

**O281** — A body at rest idles, a body moving walks, a body struck flinches: every enemy rig ships its idle, walk and hit cycles. Modifier rolls mix the session into the seed, so the same body never wears the same set two runs running; Fernhall's pocket elites roll modifiers like everyone else.

**O282** — Every combat moment has a visible fallback that reads at play distance: a muzzle has a flash, an impact throws sparks, a tear turns, a cast has a shape — built from primitives in code until the ASSETS-5 Niagara systems land (O190).
