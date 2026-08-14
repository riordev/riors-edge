# The elements — Rift, Entropy, Void

Last reconciled against: O28

Authority: **O5** (per-element resistances, applied after armour and before
shields) and **O19** (the elements are Rift / Entropy / Void; "Time" is
renamed; Void Whisperer IS the Void specialist; Rift damage takes a hotter,
whiter cyan; **saturated teal is a property of objects, not of damage**).
Fiction comes from `Docs/Design/Story-Source.md` §1.5 and §1.6.

Everything below is **AUTHORED** unless marked TRANSCRIBED. Every number is an
`O2 PLACEHOLDER`.

## The rule this document is written against

An element must be a **rule**, not a percentage. Three reasons, all of them
already load-bearing in this project:

1. **O3 caps composed More multipliers at three.** If an element is "your
   damage but tinted", it competes for a budget that is already spent, and the
   honest version of it is a fourth More. Elements that change *what happens*
   cost nothing against that ceiling.
2. **O27 rules that choices beat accumulation.** Three interchangeable damage
   types are accumulation wearing three hats.
3. The project has already shipped one stat that was structurally incapable of
   doing anything (`ElementalDamageReduction`, still inert, still deliberately
   absent from the affix pool). A fourth damage number nobody can feel would be
   the same failure with better marketing.

So each element below owns a **verb** no other element has.

## Where they come from

TRANSCRIBED, §1.6: *closing a rift does not seal a door. It closes a gap in
time. The timeline behind it, and everyone in it, never existed.*

That single sentence is the whole element system. A rift is a hole in **time**,
not in space, so the three elements are the three things a hole in time does:
it **opens** (Rift), it **decays what is cut off from its source** (Entropy),
and it **erases** (Void). Severance — the degradation that turns a refugee into
a hostile — is Entropy happening slowly to a person, which is why the element
set and the enemy taxonomy are the same idea at two scales.

That derivation matters practically: it means the elements do not need to be
explained to the player in a tutorial. They are already the plot.

---

## RIFT — the breach. *Verb: displace.*

**Fiction.** The doorway itself, and the force of something arriving that
should not be here.

**Damage identity.** The only element that **moves things** — the player, the
target, or both. It is the element that belongs to the movement pillar, and the
one that a movement build reads as an extension of its own kit rather than as a
damage type.

**Ailment — DISPLACED.** The target is pulled toward, or pushed from, a point.
Not a stun and not a knockback that removes control: enemies keep acting while
being *moved*, so the effect is positional rather than a soft crowd-control
button. Positional effects compose with the passive-defence ruling (O1) that
governs everything here — the player cannot dodge on command, so an element
that rearranges the fight is worth more to them than one that briefly stops it.

**Resistance behaviour.** Rift resistance reduces the *magnitude of the
displacement* as well as the damage. A heavy enemy is not immune, it is harder
to move — which keeps the stat legible instead of binary.

**On consumption** (`Resonance`, and the status-consumption verbs that landed
in `Combat/`): consuming Displaced converts the remaining displacement into
damage at the destination, so it rewards having moved the target somewhere
first. The obvious build is "put them where I want them, then collect."

**Colour.** Hotter, whiter cyan than the UI's cyan, per O19. Never the
suppression teal — that is an object colour and a rift is the one place the
distinction would be easiest to blur and most damaging to blur.

---

## ENTROPY — severance as a weapon. *Verb: accelerate.*

**Fiction.** What happens to anything cut off from a timeline that no longer
exists. The Altered die of this slowly. Entropy damage does it quickly.

**Damage identity.** The element that operates on **duration**. It does not
mainly deal damage itself; it changes how fast everything else on the target
resolves. This is the renamed "Time" element and the rename earns its keep —
"Time" suggests stopping things, "Entropy" correctly suggests running them
down.

**Ailment — SEVERED.** All damage-over-time on the target ticks faster.
Deliberately a multiplier on *the rest of the kit* rather than a DoT of its own,
which makes Entropy the element with the strongest opinion about what else you
brought. This is the mechanical peer of the existing `Long Debt` node, which
doubles DoT tick frequency while Overcast — so the shape is already proven in
the codebase and already survives the snapshot rules.

**The snapshot rule it must obey** (TRANSCRIBED, O10): tick interval is part of
the DoT snapshot, with discrete steps. So SEVERED applies at application time
and a DoT already ticking does not retroactively speed up. That is not a
limitation to design around — it is what makes application ORDER matter, which
is exactly the sequencing skill Multispell is built to reward.

**Resistance behaviour.** Entropy resistance reduces the acceleration, not the
damage — a high-Entropy-resist target is one your DoT build fights at normal
speed rather than one your DoT build cannot hurt.

**On consumption.** Consuming Severed spends all remaining accelerated
duration at once: every DoT on the target resolves its remaining ticks
instantly. Enormous with a loaded target, worthless on a clean one — a
detonator that demands setup rather than a button that always pays.

**Colour.** Not yet ruled. Recommend a desaturated amber/bone, so it reads as
decay and cannot be confused with Rift's cyan or Void's violet. **OPEN.**

---

## VOID — erasure. *Verb: remove.*

**Fiction.** What is left where a timeline never existed. Not destruction —
absence.

**Damage identity.** The element that **takes things away**: armour,
resistances, buffs, an enemy modifier, a shield. Void is the answer to "this
target has a property I do not want it to have."

**Ailment — UNMADE.** Strips defensive properties for a duration rather than
adding damage. Note the existing precedent and its reason: `Rot` applies a
**flat** armour reduction, not a percentage, explicitly to protect the boss
cap. Void follows that rule — flat removal, never percentage — because a
percentage strip is a stealth More multiplier and O3 is already spent.

**Why Void ships first.** TRANSCRIBED, Class-Kits §2.4: Void, Bleed and Poison
are physical or armour-facing and can ship **now**; the Rift and Entropy lines
are blocked on the resistance model that does not exist. Void is therefore the
only one of the three that is buildable today, which is consistent with O19
making Void Whisperer the specialist branch — the class content and the element
availability agree by accident, and that is worth not disturbing.

**Resistance behaviour.** Void resistance reduces the *duration* of the strip,
not its magnitude. Halving how long you are exposed is legible; halving how
much armour you lost is not.

**On consumption.** Consuming Unmade makes the removal permanent for the rest
of the encounter on that target. The one effect in the set that persists, which
is the correct privilege for the element of erasure.

**Colour.** Violet (`#B866FF`), already the ultimate/Void colour in the
Fieldplate palette and already carried by Void lash in the ability icon spec.

---

## Reactions — three elements, three pairs

Multispell "rotates all three; Void Whisperer masters one" (O19). Reactions are
what rotating is FOR. Three elements give exactly three pairs, which is small
enough to be memorised and large enough to be a rotation — and it is the reason
the element count is three rather than four.

| Pair | Reaction | What it does |
|---|---|---|
| Rift + Entropy | **COLLAPSE** | The displacement accelerates: the target is moved the remaining distance instantly, and arrives having taken its pending DoT ticks. |
| Entropy + Void | **DISSOLVE** | The strip lasts as long as the accelerated DoTs do, so removal and damage expire together instead of needing separate tracking. |
| Void + Rift | **EVICT** | The target is displaced *and* loses the property that made its position matter — the anti-anchor answer to an enemy that is dangerous because of where it is standing. |

Rules the matrix must obey, all of which already exist:

- **One reaction per target per 0.5s** (`MS11 Conductor's Rule` already states
  this, and already converts suppressed reactions into Mana rather than a
  silent clamp — keep that shape).
- **A reaction may not itself apply a status**, or the matrix recurses. The
  spread in `MS4 Chain` is already proc-coefficient 0 for the same reason.
- **Reactions are rules, never percentages** — see the top of this document.

## What must be built before any of this ships

Stated plainly because three separate docs currently describe elements as
BLOCKED and none of them lists the blockers in one place:

1. **The resistance model.** O5 places per-element resistance after armour and
   before shields in the damage order. `EBreakerStatTarget::ElementalDamageReduction`
   exists, is inert, and is deliberately kept out of the affix pool so it lies
   to nobody. It needs to become per-element, and `Docs/Damage-Pipeline.md`
   needs the step inserted at the ruled position.
2. **A damage family per element.** Today `EBreakerDamageFamily::Elemental` is
   one bucket. Rift/Entropy/Void need to be distinguishable at the request, or
   resistances have nothing to key on.
3. **The reaction matrix as a Data Asset**, not code — it is content and it
   will be retuned constantly.
4. **Status consumption**, which **now exists** (`ConsumeStatus`,
   `ConsumeAllStatuses`, distinct-count, `OnStatusConsumed`), so the "on
   consumption" clause of each element above has a real hook to attach to. This
   was the blocker that cleared most recently.
5. **The Elements constellation's nodes are authored physical-only** and carry
   the elemental rule as a tag, so they light up when the above lands without
   being rewritten. That was deliberate; do not re-author them.

## OPEN QUESTIONS

1. **Does the player deal elemental damage at all before the resistance model
   exists?** Void can ship (armour-facing). Rift and Entropy cannot be resisted
   yet, so shipping them early means shipping them unresistable. Recommend: ship
   Void, hold the other two, and do not fake it.
2. **Entropy's colour.** Recommended desaturated amber/bone above. Needs a
   ruling because it interacts with the Fieldplate palette and with Orange,
   which already means "cost / warning" on the HUD.
3. **Do enemies use elements against the player?** The Vestige family is the
   obvious carrier and it would give the resistance stat a reason to exist on
   gear. If they do not, elemental resistance is a stat with no incoming damage
   to resist, which is the inert-stat failure again.
4. **Does Severance-the-fiction and SEVERED-the-ailment sharing a name help or
   confuse?** It is thematically exact — the ailment is literally what is
   killing the Altered — but a player reading "Severed" on an enemy that is
   already called Altered may not connect them.
