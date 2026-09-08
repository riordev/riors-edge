# Progression and trees

## What this system is for

To give a character an identity that is spent for rather than found, and to
make the spending a series of refusals. A tree that a player can finish is a
checklist; a tree that offers far more than a budget can buy is a set of
decisions, and the decisions are the product.

It fails in two ways, and the project has shipped both. A node that reads as a
flat percentage is doing the affix layer's job and teaches the player that the
tree is a slower way to get gear. A node that grants a tag nothing consumes
costs a point and produces no observable change, which is worse — it is a
promise the game does not keep.

## The rules

**Three layers, and each owns a different kind of change.**

- **Class** owns resource loops and identity mechanics — behaviour no other
  class can produce.
- **Core** scales generic axes across distinct stages and rewrites rules.
  **Doctrines** change class behavior and ability identity.
- **Affixes** scale verbs the player already owns, and own the raw
  percentages. They make existing actions better, cheaper, faster or longer;
  they do not introduce actions.

**Item rules are a fourth layer that also rewrites rules.** Special-rarity
damage perks share the same More budget as trees. A tree rewrite is a build
committed to; an item rewrite is a build found.

**Parry is the only tree-granted verb.** Walk, sprint, two jumps, slide, vault,
mantle, passive block and passive dodge are base kit. Dash is innate to Swift.
Trees improve movement verbs; gear never grants one.

**Core choices span stages, rather than repeating one damage bucket.**
Base, Increased, More, Site, Rate, Multiplicity and Stream give lanes distinct
jobs. Ranked generic magnitudes are permitted; doctrine identity stays separate.

**Core sets the axis; the class layer changes what the axis does.** Three
permitted class-layer forms, and one forbidden:

| Form | What it does |
|---|---|
| **Conversion** | Makes one investment count as another. Worthless without the Core investment it converts. |
| **Condition change** | Widens the window an axis pays in. |
| **Rule rewrite** | Changes the shape of an axis rather than its magnitude. |
| **Forbidden** | Authoring a magnitude on a stat target a Core node on the same axis also authors. |

Two trees that both offer "crit" are not more knobs. They are one knob with two
handles, and the decision collapses to which number is bigger. The forbidden
form is mechanically checkable and is asserted, not left to judgement.

**Subclass commitment empowers rather than excludes.** Committing to a branch
unlocks that branch's keystone tier and its identity presentation; ordinary
nodes of every branch stay freely purchasable. One commitment per character,
changed only at the Forge. There are no mutually exclusive tiers anywhere.

**A character holds at most two Core keystones and one doctrine keystone,
ever.** This is the structural decision the point budgets exist to produce. On
the Core side it is arithmetic on the pool: a third keystone costs more than the
pool holds. On the doctrine side it is the shape of the wallet — eight points,
four picks of two, and the keystone is one of them.

**Tree More multipliers belong to Core convergences.** The strongest three
applicable gear/tree Mores share one ceiling; no slot is reserved for either
source. New Core convergences author at most1.26x each. A doctrine authors none — its
keystone earns its place by rewriting the ultimate, not by multiplying a number,
and a doctrine spending one of eight points on a multiplier would be buying what
Core already sells.

**A doctrine may not author a magnitude on a generic damage pool unless it is
conditional on that doctrine's own axis.** Increased Damage, Weapon Damage and
Ability Damage are Core's. Where a doctrine's rule carries a magnitude, that
magnitude is conditional on the axis the doctrine is about, or it sits on a stat
target no Core wheel authors. A doctrine spending any of its eight points on
generic damage has spent its identity on what Core does with two hundred.

**Adding a stat target does not make it pay.** A target needs an aggregation
lane, and the register of which targets have one is hand-maintained on purpose
and flips in the same commit as the lane, never before. Every entry declares
its bucket; an entry without one cannot be wired, because the lane would have
to guess.

**A condition is a predicate on live state, never a property of a hit.**
Whether a hit crit, hit a weak point, or killed is an outcome of an event and
belongs to the hook payload. How the damage was delivered belongs to the
stat-target partition. This line is what keeps the condition budget affordable
and stops two systems answering one question.

**Conditions compose with AND only.** No OR — that is two effects, and a
tooltip has to print both lines anyway. No NOT — "not airborne" and "grounded"
differ on ladders, in water and mid-teleport, and the player cannot tell which
they bought. Where a complement is genuinely wanted it gets its own entry.

**Target-side conditions resolve on the target side**, at the one site that
knows both actors, and are Increased-bucket only. A target-conditional More is
not supported by rule.

**Dead conditions and dead stat targets are loud.** A requirement naming
something that cannot be evaluated warns once — once per condition, not per
call, because a conditional effect is evaluated many times a second and the
same line at that rate trains people to filter the channel. The project has
shipped content that compiled, purchased, displayed and did nothing quietly
four times; loudness is the standing answer.

**Ids never move; a re-theme is a display change.** A tree id and a node id are
save data — a character's branch commitment is stored as a tree id — so renaming
a doctrine renames only what the screen prints. A re-theme that moved ids would
strand every commitment on an id that no longer resolves, and it would do it
silently, because the save is not corrupt: it points at nothing.

**A node that needs a primitive the game lacks is recorded, never faked.** The
same rule the ability layer already carries: the absence is written at the
node's own site and the node ships honestly short. A keystone is where this
bites, because a keystone is a rule rewrite by definition — if an enum entry
unblocks it, it was a minor with the wrong label, and it is not what the
vocabulary ordering is waiting for.

**Nodes may be purchasable while inert.** Buying the node that reads a system
before buying the system is a legitimate pattern and the tree should support it.

**Every world-content progression point is one-time, permanent, spread across
the campaign, unmissable, solo-reachable, and free of traversal mastery.** No
repeatable source exists — a repeatable source is an infinite power track.

## The model

### Density — the ratio is a target, not an accident

**Density counts offered points, not nodes.** Doctrine density retains its
own target. Core density is derived from the authored major/minor roster
against65 spendable points; its deliberately wider shape is not a3–5x overrun.

**Composition follows the major/minor shapes.** Ranked lanes supply generic
axes; notables change stage, convergences combine lanes, and major keystones
pair a rule rewrite with a forfeit. Core owns no named class resource, ability
identity or ultimate. Magnitudes are tuning placeholders; the shape is design.

**The gate is vocabulary, not count, and the ordering is explicit: widen the
stat-target and condition vocabularies first, then author to the ratio.**
Authoring more nodes against a narrow vocabulary produces more near-identical
nodes. A tree reads small when it is repetitive, and count without vocabulary
makes it worse, not better.

Measure offered points and ranked-node composition independently for each tree.

### The Core tree

**The wheel is one tree every class shares, its points level-driven (O211).**
Six domain sectors — weapon, ability, status, defence, movement, utility — hold their
constellations as wedges and the wheel never hides a sector; a constellation
with a silent node draws dark and unselectable until its plumbing exists
(O212). Class flavour on the Core is a lit recommendation, never a lock. No
bead is a travel node: every rank carries a magnitude. Core respec is free
until level N and costs Riftglass after; Doctrine respec stays the Forge's (O213).

The first gateway is freely chosen; later gateways require any owned node in
either neighboring wedge. Major wedges have11 nodes and26
offered points; minor wedges have6 nodes and13 offered points.
The full roster has11 majors and11 minors:22 wedges,187 nodes and429 offered
points against65 spendable, a6.6x ratio. All named wedges remain in the design.

```
Gateway (1)
  Lane A: Minor 3 ranks (3) -> Notable (2)     = 5
  Lane B: Minor 3 ranks (3) -> Notable (2)     = 5
  Lane C: Minor 3 ranks (3) -> Notable (2)     = 5
  Link A-B (1), Link B-C (1)                   = 2
  Convergence (3)  requires two lanes complete
  Keystone (5)     requires 18 spent here
                                       TOTAL   = 26

Minor: Gateway(1), Lane A(3+2), Lane B(3+2), Convergence(2) =13
Minor convergence requires both completed lanes.
```

Each ranked lane node costs one point per rank, up to three. A notable requires
rank one of its lane; ranks two and three are optional deepening. Crossing a
lane costs three points; fully owning it costs five. Major convergence requires two notables;
its keystone requires convergence and18 points already spent in that wedge.
The keystone's five-point purchase cannot count toward its own gate. Its
minimum23-point path permits two for46 and rejects three for69 against65.

**A Core keystone is expensive enough that two is a character and three is not
a choice.** That is the intent, and it is arithmetic on the POOL: whatever a
keystone's gate and charge come to, three must exceed 65, and two must leave
enough that the character is still playing a build rather than servicing two
decisions.

The local spending gate belongs to Core. Doctrine commitment and tier rules
remain separate from Core wedge investment.
The census exports gateway entries, adjacency, counted lane groups and local spending gates.
Physical reduction, elemental buildup resistance and ailment avoidance add percentage
points before their existing caps. Health and armour Increased scale their own pools.
Cast/channel rate scales authored clocks: deployment delay and Siphon tick spacing
snapshot at activation; costs and channel duration remain unchanged. Instant abilities
need an authored clock before cast rate can affect them. Overclock adds half the
cooldown recovery bonus to each rate; it does not feed the converted bonus back into itself.
The complete named roster and runtime contracts live in [Core wheel](core-wheel.md).

Links exist so a player can reach a second lane's notable without completing
the first lane — the only way to build a two-notable splash with no keystone.

### The doctrines

Three doctrines per class, one committed to per character, changed only at the
Forge. A doctrine is the subclass: committing grants a visible identity as well
as mechanics. A doctrine offers **12 nodes** with tier gates at
0 / 2 / 4 / 6 invested. The keystone is an ordinary tier-4 node: gated at 6
invested, costing 2, with no cornerstone gate of its own.

**Every doctrine node costs two points to reach its last rank**, so eight
divides into exactly four picks and nothing is stranded. That is the shape the
budget is for, and it is why the keystone costs what every other pick costs: it
competes with the tier-4 rewrites for the last of the four rather than sitting
above them.

**A doctrine's budget is eight points, earned two at each of four campaign benchmarks.**
Commitment selects the branch and pays nothing. Eight against roughly twenty-four
offered leaves most of a doctrine declined.

Spellblade's No Distance costs two points after six invested and Momentum Transfer.
It raises Closequarter's authored cost to50 before ordinary cost modifiers and permits
the existing arrival refund against any target health; it grants no damage More.
Reprisal costs two after six invested and Bloodprice: a surviving passive block
grants one free Cleave within two seconds. A refused cast keeps the opportunity;
committing consumes it before callbacks. Death, expiry and respec clear it.
Multispell's Interference costs two after six invested and Resonance; it selects
Resonance's authored fixed-per-status curve with its three-status threshold bonus.

The eight-point budget leaves one two-point tier-four choice after six earlier
points. It does not buy a complete doctrine; prerequisites still gate each choice.

**A doctrine authors no More multiplier.** Core and eligible gear rewrites compete
for the three slots. A doctrine pays in conversion, conditions and rule rewrites.

### The axes

**GUNS, ABILITIES and DEFENCE.** Defence is a Core axis because the offensive
taxonomy has no home for the live defensive and mobility nodes, and pushing
them to gear collides with trees improving movement verbs.

**Minions are a class axis, not a Core one.** A universal minion cluster is
dead for four of five classes, and the alternative — every class gets
deployables — is a much larger change that collides with the class that owns
them.

**Bleed and poison are one AILMENT axis.** They are one mechanic wearing two
tags, and splitting them makes a single weapon carry an entire axis alone.

### Points

Two pools and no third. **Core is 65**: one Core Point per level to 50, plus
roughly fifteen from world content across the campaign. The slice's opening
grant is an advance on that entitlement rather than a separate pool.

**Doctrine is 8**, paid two at a time on four main-story benchmarks, which are
mission Unlock beats rather than levels; a settled counter means no character is
paid twice. A respec zeroes the wallet rather than refunding it — the points
belong to the commitment, not to the character.

## Boundaries

This spec owns the layers, the tree shapes, the vocabularies and the budgets.
It does not own:

- the aggregation law, the More ceiling or the bands — **power and scaling**;
- what an affix may roll — **items and crafting**;
- what a class ability does, or its resource loop — **classes and abilities**;
- where a condition's underlying state is computed — **combat**;
- what grants a world-content point — **content and modes**;
- how a node card and a board are drawn — **art and UI**.

## Asserted invariants

| Invariant | Test |
|---|---|
| Every keystone is reachable at the shipped point entitlement, not a test grant | `Abilities.KeystoneReachability` |
| Every keystone tag a node grants has a consumer, and every variant row's tag is granted by some node | `Abilities.KeystoneGrantsAreRead` |
| Points per level match the entitlement, and the opening grant is an advance on it | `Progression.LevelPointEntitlement` |
| The stat-target lane register matches what the aggregator actually consumes | `Progression.ConditionVocabulary.StatTargets` |
| Condition and stat-target enum values are pinned against reordering | `Progression.ConditionVocabulary.StatTargets` |
| A dead condition or unpaid target warns rather than failing silently | `Progression.ConditionVocabulary.Evaluability` |
| Target-side conditions resolve in the additive bucket and never as a More | `Combat.TargetRiders.*` |
| No doctrine node authors an unconditional magnitude on a generic damage pool | `Progression.AxisOverlap` |
| Commitment unlocks the keystone tier and leaves ordinary nodes free | `Progression.BranchCommitment` |
| Every tree's deepest node is purchasable on that tree's own shipped grant | `Progression.TreeDepthIsReachable` |
| A Forge respec restores the pre-purchase composition exactly | `Progression.RespecRestoresAttributes` |
| Offered-to-spendable ratio per tree stays inside its band | `Progression.TreeDensity.Offered` |
| Node-shape composition per tree stays inside its band | `Progression.TreeDensity.Composition` |

## Open

- The class branch tier shape: the shipped four-tier gating, or the 26-point
  five-tier target. Decided together with density.
- Where the Core tree's hub sits, or whether it keeps one — a hub that is one
  of the axes privileges that axis.
- Whether the conditional-payout ratio differs by condition count. The stated
  direction is that it barely should.
