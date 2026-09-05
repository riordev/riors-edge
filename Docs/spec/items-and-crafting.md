# Items and crafting

## What this system is for

Gear is the entire endgame. The level cap is a hard stop, so an item is the
only thing left that can change a character after 50 — which makes the item
card, not the HUD, the most important screen in the game.

It fails when a rarity is an arithmetic event rather than a build event. An
item with more lines than the last one is a bigger number; an item that changes
what the player does is a decision. If the top of the ladder reads as "a rare
with a bonus", the chase has nothing at the end of it.

## The rules

**Item level is hybrid**, and the zone is the authority for how good its drops
are, not the individual enemy:

    ItemLevel = clamp(ZoneLevel + TierBonus + Variance, 1, 120)

An enemy-level fallback exists so content with no authored zone still drops.

**Rarity gates affix COUNT and a tier ceiling. It does not gate magnitude.**
Magnitude lives strictly inside tier ranges, which is what keeps the More
budget intact while the ladder grows.

**A rarity can only roll when both the drop's item level is at or above that
rarity's unlock AND the killed monster's rank is at or above its minimum. A
gated-out rarity's weight is zero and its share redistributes across what
remains.** Redistribution rather than a reroll: a reroll preserves the shape of
the table and relabels illegal results, where zeroing means a low-level trash
kill genuinely has a different distribution.

**The rarity gates do not scale with the item-level ceiling.** They pace the
player's introduction to rarity, and that introduction happens across the
campaign however far the tier ladder runs. Scaling them proportionally would
mean finishing the campaign having never seen the top two tiers — the ladder
introduced only after the content that teaches it.

**Most kills drop nothing.** Drop chance is a step of its own, by rank, ahead
of the rarity roll. Kill count is not item count.

**Rarity and legendary are different axes.** A legendary is a field naming an
authored item with a fixed slot, guaranteed affixes and a hand-authored
rewrite. Every legendary rolls at top rarity; most top-rarity drops are not
legendaries. Do not conflate a tier of the ladder with an identity.

**A rewrite is a field on the item, never derived from its rarity.** An item
earns a rewrite when it is rolled one. Deriving it from rarity would hand one
to every existing item in every save and every test fixture.

**No item rule may author a More.** The budget is three, the trees already
offer more options than that, so a fourth from an item is either dead weight or
a quiet nerf to the three the player chose. Mores stay a tree instrument.

**Affix breadth is an invariant, not an aspiration.** Every slot can raise
weapon damage; every slot can raise ability damage; every slot carries a
conditional line of its own; and no two slots offer the same set. Per-slot
identity is the design — two players hunting damage on boots and on a necklace
must be hunting different lines.

**Conditional lines roll roughly twice the unconditional line**, because they
pay nothing while standing still. That trade is what makes building around a
movement state a decision rather than a strictly better version of the line.

**A stat stays out of the pool until something can consume it.** A line that
cannot pay is dead content wearing a number, and the pool must lie to nobody.

**The stash is account-wide and transfer is Anchor-gated.** Characters are
builds, gear is an account asset, and a run cannot mutate account state.

**Equip caps are enforced at equip time and again at save load.** If the save
layer does not enforce them they are advisory, and they are the constraint that
carries the whole endgame.

## The model

### The ladder as it ships

| Rarity | Affixes | Tier cap | Qualitative rule |
|---|---|---|---|
| Standard | 1–2 | T4 | — |
| Uncommon | 2–3 | T2 | — |
| Exceptional | 3–5 | T-1 | — |
| Aberrant | 4–6 | T-1 | Focused: one affix rolls a tier better |
| Unwritten | 5–6 | T-1 | One rolled rewrite, drawn from a pool of four |

Equip caps: three Aberrant, one non-legendary Unwritten, one legendary, as
three separate axes. A legendary does not draw against the Unwritten cap.

### The ladder as intended

The four rollable rewrites are all invisible. Every one changes what the
aggregation obeys; none changes what the player does or what the screen shows.
They are not weak, they are **misfiled** — minor-rewrite content sitting in the
top slot, which is why the top rarity reads as a rare with a bonus.

**Aberrant becomes the stacking tier.** Up to three equipped, each rolling one
of two flavours:

- **Focused** — a raised tier ceiling with a floor. The magnitude pick. Does
  not consume a rewrite slot.
- **Modified** — one minor rewrite, no raised ceiling. The exploration pick.

The four existing rewrites migrate down into Aberrant's minor pool. They are
the right size for something worn three of.

**The fifth tier becomes the singular tier.** One equipped, and it carries
**one major rewrite plus generic affixes stronger than an ordinary item's**, so
it lands on par with a good Aberrant rather than above it. It is not an
upgrade, it is a decision — the rewrite is what you came for, and the stats are
what stop taking it from being a sacrifice.

**The minor/major line, as a test a person can apply to a card:**

> A **minor** rewrite changes the terms of a rule the player is already
> obeying. A **major** rewrite changes the shape of what happens on screen, or
> the contents of the loadout.

"Conditional affixes always pay" is minor. "Projectiles split toward enemies"
is major. "Ultimate sealed, third ability slot unlocked" is major. It has to be
checkable by reading the card, because it must hold across hundreds of authored
items.

**Major rewrites come in three kinds, and the pool needs all three:**

| Kind | Changes | Example shape |
|---|---|---|
| **Delivery** | How the damage arrives | Projectile split; area centred on the caster |
| **Economy** | What pays for what | A sealed ultimate buying a third ability slot; a resource inversion |
| **Rule** | What a mechanic obeys | The four existing rewrites' category |

Weight the pool toward delivery and economy. **A major rewrite must be
observable: if a player cannot tell it is equipped without opening the
character sheet, it belongs in the minor pool.**

**Every major rewrite pays an authored forfeit** — something the item takes
away, printed as a line on the card, never a smaller number somewhere else. The
best forfeits are the rewrite's own downside: "area always centres on you" is a
rewrite and a forfeit in one line.

**Rewrite caps replace the item-count cap:** three minor, from Modified
Aberrants, plus one major. A legendary's authored pair occupies the major slot
rather than sitting beside it, so legendaries compete with rolled top-tier
items instead of stacking with them.

**Minor rewrites are class-tagged, and a class-tagged item drops for every
class regardless of who is playing.** That is a rule, not an oversight. Finding
a rewrite for a class you are not playing is the game telling you what else
exists, and it is the raw material of a second character; smart loot would
delete both. Nobody is to "fix" this later.

**Stacking is not prohibited.** Three minor rewrites on the same mechanic is a
legal and intended build — a player who wants to stack three area rewrites and
play a strange area character is buildcrafting, the same way a keystone that
excludes them from something is a choice they get to make. Each minor rewrite
carries a tag naming the mechanic it touches, but the tag is for authoring and
reporting, never prohibition: it exists so same-tag rewrites are written to
compose into a coherent shape rather than three unrelated effects landing on
one mechanic. If a stack lands outside the band, the fix is the rewrite's
magnitude, never a ban on the combination.

### The drop pipeline

Three steps where a naive implementation has one:

1. **Drop chance**, by monster rank. Trash rarely, elites reliably,
   modifier-bearing more so, bosses always.
2. **Rarity gate**, by item level and rank, weights zeroed before the roll.
3. **Rarity roll**, weighted over whatever survived.

The drop decision and the rarity draw from independent sub-seeds. Sharing a
stream makes "dropped at all" and "dropped well" the same coin, which shows up
as the rare tiers clustering in the same seeds.

### The roll pipeline

Rarity, then affix count from the rarity's range, then slot-legal weighted
selection with no duplicates, then a tier per affix — item-level gated, rarity
capped, walking up one step at a time so the top tiers stay earned — then a
value inside the tier band. **The rewrite and the legendary draw come last**,
after every affix, so no previously recorded roll moves when the pool grows.

Archetype leans are weights, never filters: a lean makes a line likelier on the
gun it suits and never makes another impossible.

### Defence

Three lines, sized together so a committed defensive build meaningfully
outlasts a bare one:

- **Physical damage reduction** — the flat mitigation line.
- **Ailment avoidance** — a deterministic pre-immunity roll at the application
  door, capped; ticks never re-roll, and a refusal broadcasts a tell so the
  player can see the stat working.
- **Elemental resistance** — per element, capped, and held out of the pool
  until enemies deal elemental damage.

### The Forge

Minimal item agency, deliberately not an economy. No vendor, no bench
progression, no item-derived materials.

**One currency: Riftglass** — vitrified breach edge, chipped from closed rifts.
Account-wide and scalar. One ladder means the economy can be read in one line
and can never invert against itself. The balance lives in the account save;
a character file written before that folds in exactly once through a journal
in the account file — record the slot and amount, zero and stamp the
character file, credit and clear — so a crash after any write replays to the
same total. The fold is not a migration step: a step is pure on one file and
this moves value between two.

| Verb | Moves | Keeps |
|---|---|---|
| **Temper** | One affix, one tier better | Everything else |
| **Reforge** | Every affix value, within its band | Ids and tiers |
| **Attune** | Which affixes | Count and tiers |

Rarity still caps crafting, or crafting erases rarity's meaning. A refused
craft costs nothing — a partial spend is how one refusal becomes a lost
currency bug report. Temper re-derives the value at the new tier rather than
scaling the old one, so a temper is always exactly what that tier is worth.
Reforge draws from the same distribution the drop pipeline uses. Attune keeps a
legendary's signature and its rewrite: those lines are its identity, and a
craft that could roll them away turns a build-defining item into a lottery
ticket. Salvage pays; discard does not.

## Boundaries

This spec owns the item, the ladder, the roll, and the Forge. It does not own:

- the aggregation law, the tier value curve, or the More ceiling — **power and
  scaling**;
- where an affix's value lands in the damage order — **combat**;
- what a tree node may author — **progression and trees**;
- which rewrites are authored, and what they do to a class — **classes and
  abilities**;
- where an item is found — **content and modes**;
- how a card is laid out and what it may print — **art and UI**.

## Asserted invariants

| Invariant | Test |
|---|---|
| No item rule authors a More, on any aggregated attribute, including delivery and economy rewrites | `Items.Rules.NeverAuthorsAMore` |
| Every slot can raise weapon damage and ability damage, and every slot has a conditional line | `Items.Affixes.Breadth` |
| Trash cannot roll a gated rarity, exhaustively across the item-level range | `Items.Drops.TrashCannotRollAberrant` |
| A seed reproduces the drop decision, the rarity and the item; the chance step does not bias the rarity step | `Items.Drops.Determinism` |
| The projected loot rate matches what the pipeline actually produces | `Items.Drops.LootPerHour` |
| The weapon and item-level ceilings are equal | `Items.TierLadder` |
| Items rolled before the ladder widened keep their rolled values | `Items.LegacyItemsSurviveTheWiderLadder` |
| Tempering reaches the top of the tier spike and is worth the authored value | `Items.Forge.TemperReachesTheSpike` |
| A craft on an equipped item moves the composed attribute | `Items.Forge.Loop` |
| An on-kill affix pays through a real kill and stops exactly on unequip | `Items.Affixes.OnKillReachesGameplay` |
| Equip caps hold at equip time and at save load | `Items.Equipment.PerAxisCaps` |
| A crash after any one of the fold's three writes replays to the balance credited exactly once | `Save.RiftglassFold.CrashAfterJournal`, `Save.RiftglassFold.CrashAfterCharacterWrite`, `Save.RiftglassFold.CrashAfterCredit` |
| The roster sweep journals every unfolded balance in one write and credits nothing | `Save.RiftglassFold.Sweep` |
| A three-minor same-tag stack lands inside its band ceiling | `Progression.RuleBandImpact.MinorStack` |
| A major rewrite, or a legendary's authored pair, lands inside its band ceiling | `Progression.RuleBandImpact.Major` |
| The band holds for a build wearing a top-tier item | `Progression.PowerBand.Singular` |

Two consequences of the intended structure, recorded because they invalidate
tests that pass today. `NeverAuthorsAMore` extends to the delivery and economy
kinds — a rewrite making every projectile hit twice is a More in a costume, and
the existing test only walks rule-kind rewrites. And rewrite impact needs two
ceilings rather than one: the current single ceiling was derived for one
rewrite, and the tier-step rewrite already measures above it. Band fixtures
need a build wearing a top-tier item, since that tier is deliberately off the
power ladder and must still land in the band, and a same-tag triple stack
rather than only a mixed one — three-of-a-kind is the intended extreme and
therefore the case the ceiling has to hold against.

## Open

- The zone-level table, and the mapping from content difficulty to tier bonus.
- What the generic affixes on a top-tier item are worth, given they must land
  it on par with a good Aberrant rather than above one.
