# Decisions

Live rulings, one line each. A superseded ruling is deleted; git holds it.
Numbers are permanent and never reused.

## Rulings

**O1** — Stamina does not exist and may not be re-added. Block and dodge are passive chance layers; Parry is the only defensive input and carries its own short cooldown.

**O2** — Every number is a placeholder until measured, and a new constant is flagged as one at its declaration. When two implementations are possible, prefer the one that authors nothing.

**O3** — More multipliers compose as an unordered product; a build holds at most three; they are authored only on Core constellation Convergence or Keystone nodes.

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

**O18** — Seed targets: trash a little under a second, elite around three, boss twenty to forty-five unless a special enemy claims the exception; time-to-die four to five seconds bare, substantially higher invested.

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

**O43** — One Class Point per level through 30, one Core Point per level through 50. The slice's opening grant is an advance on that entitlement, not a separate pool.

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

**O73** — The campaign is post-slice.

**O74** — The single More ceiling spans the ability-damage lane. There is no per-lane ceiling.

**O75** — An elite's loot floor rises one rarity step at three modifiers.

**O76** — Affixes own the raw defensive chances and percentages; trees own defensive rule changes and quality. I-frame duration stays tree-only.

**O77** — The shipped keystone rewrites are reconciled to the class More ledger's design.

**O78** — Block is rolled and its reduction applied at the same step, before armour.

**O79** — Facing-dependent armour applies on every armoured enemy.

**O80** — A minimal stagger model exists: a binary interrupt state, a resistance stat, and a per-enemy immunity flag.

**O81** — The top pack-composition tier is renamed off the canon location's word.

**O82** — The death budget favours solo: solo carries two, and a party's scales with its size.

**O83** — A campaign rift's enemy level is clamped to a small margin above the player's.

**O84** — Shield renders above health, so depletion reads downward.

**O85** — The nameplate policy is a per-mode asset from day one.

**O86** — Commitment is to a **doctrine**: one per character, changed only at the Forge. A doctrine grants a visible identity as well as mechanics — a Caster committed to Void Whisperer *is* a Void Whisperer, on the character sheet and to other players in the Anchor.

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

**O106** — Shield magnitude is affix-owned and the defence triad becomes a quartet. The affix does not ship until the damage-versus-defence retune lands, because a new defensive line while that ratio is 3.76 is defence scaling up to meet damage, which O91 forbids. Until then a shield doctrine pays in rules.

**O107** — Multishot, pierce, chain and ricochet live in the rule tier, so they need no doctrine axis and no rename. The commit that deletes a class's branch trees lands their replacement rule nodes in the same commit.

**O108** — A doctrine keystone is paid by its own doctrine's axis. Frenzy's stacks on consecutive hits to one target, not on weak points, which vacates weak point for Marksman so no two doctrines share an axis.

**O109** — Out of combat a resource may CONVERGE on a midpoint, draining from above and regenerating from below. Decay is barred in safe states — the menu, the Forge, the Anchor — which is what the enumeration always said; "combat-state gated" over-generalised it. Convergence changes where the bar rests, never how fast fighting pays it.

**O110** — A doctrine's self-targeted duplication is additive. "Twice at full value" is one bucket counted twice, never a multiplier: a doubled multiplicative buff is an unbudgeted More arriving through a doctrine, which O95 forbids, and the global clamp would silently eat it — leaving the keystone doing nothing for the solo player it was written for.
## Open


**O111** — The class-currency migration refunds nothing and reads nothing. Class Points are one per level to 30, so spent plus unspent is derivable from the payload alone, and O27 deletes the freed points rather than folding them: v5 to v6 clears the class rank array, zeroes the class wallet, clears any commitment naming one of the fifteen retiring branch ids from a frozen list, and stamps. No cost table and no node library.

**O112** — The node-shape band is unpinned pending re-derivation. Sixty percent ranked minors means sixty percent unconditional stat lines, which O76 gives to affixes outright, so the authored target cannot be met without breaking another rule. The measured fourteen percent is honest and must not drive authoring until a band is derived for a tree whose percentages live on gear.
- The class layer's shape. Under exploration: doctrines of 4-5 transformative nodes at an 8-10 point budget, adjacency replacing investment gates, and the freed points moving to the Core budget.
- The zone-level table, and the mapping from content difficulty to tier bonus.
- Whether 134 items per hour is the right drop rate. It sits mid-band and has never been argued about, and it governs whether a 300-400 hour chase reads as generous or as grinding.
- Whether the at-cap band's 8-10x was ever derived or was seeded. It is the only reason that section is pinned as a target rather than a measurement.
- Where the permanent class choice is made, once the Anchor is authored.
- Who owns audio.
- What replaces the shared pool as the tension between the two defensive constellations.
- Whether weapon archetype outweighing class is intended identity or a table to flatten.
- What becomes of the node ability-grant path. After the one redundant grant retires it has three readers on the node card and no writers.
- Whether the quartermaster has an endgame role. Its stock is finite by construction — one token per unlockable — so it empties once the last ability is bought and is a levelling-window service thereafter.
- Whether the ability pool gets a flat line. O54 names three Increased pools and settles nothing about the flat half, and Added Damage bids Flat into the weapon lane alone — so an ability build's flat layer is structurally 1.000 against a weapon build's 1.154 at the cap and 1.550 at endgame. Flat multiplies against the whole Increased bucket, so this is the half of the parity gap that widens fastest with gear depth.
- Whether deployable stats snapshot at placement or read live, and — the same question wearing its other face — whether a deployable composes the outgoing More chain. It does not today, so a live ability window reaches every ability except a turret, which makes the one multiplier rule the canon actually enforces advisory in one place. The two are entangled: a deployable that snapshots at placement is *right* to miss a window opened afterwards, and one that reads live is not.
- Whether enemies deal elemental damage at all.
- Where Anomalies, Raids, Dungeons and Conquest sit relative to the five canon locations.
- Whether endgame tiers are capped or unbounded.
- Whether one Anchor is the settlement layer, or a network.
- Whether the ending's premise contradicts an endgame made of rifts.
- Whether rift-archetype first-clears are two grouped grants or eight individual ones.
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
