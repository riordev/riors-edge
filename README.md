# Rior's Edge / Project Breaker

**Last reconciled against: O32**

An Unreal Engine 5.8 first-person, movement-driven ARPG looter shooter.

Movement is part of character building rather than a fixed utility layer:
weapons, affixes, legendaries, abilities and skill nodes are meant to interact
with momentum, dash, slide, wall movement and gravity. Grapple and tether are
explicitly excluded. The level cap is 50 with no post-cap character power, so
all endgame power comes from gear — specifically from **gear depth**: item
level runs to 120 and the affix tier ladder is back-loaded (O29).

This is a private repository. Its readers are the owner and the agents working
on it — so this file is a routing table, not a pitch.

## Start here

| If you are… | Read |
|---|---|
| An agent about to change anything | **`CONTEXT.md`** — current state, next actions, workflow facts |
| Looking for what was DECIDED and why | **`Docs/Design/Decisions.md`** — the append-only O-ledger, supreme authority |
| Setting up a machine | `Docs/Setup.md` |
| Asking "what is this milestone" | `CONTEXT.md` → *Current milestone and next actions* |

Everything else routes through those four.

## Authority chain

When two documents disagree, the higher one wins. This order is ruled by O28:

1. `Docs/Design/Decisions.md` — the O-ledger. Append-only. Owner-authored.
2. `CONTEXT.md` — current state and the operative plan.
3. `Docs/Design/Design-Overview.md` — maps the design space.
4. The per-domain docs under `Docs/`.

`Docs/Design/Master-Sheet-Import.txt` is superseded and historical. So is
`Docs/Roadmap.md`, which now points at `CONTEXT.md`.

## Reference docs

- `Docs/Architecture.md` — which layer owns what (C++ / Blueprint / Data Asset / GAS)
- `Docs/Layer-Ownership.md` — which layer owns verbs, scaling and identity
- `Docs/Combat-Foundation.md` — damage order, armour, shields, critical DoTs
- `Docs/Weapon-Foundation.md` — hitscan flow, archetypes, recoil, falloff, round presentation
- `Docs/Item-Foundation.md` — item level, affix tiers, loot rolls, and the stat aggregation rule
- `Docs/Movement-Design.md` — the movement verbs and the weight pass
- `Docs/Playtest-Gym-v1.md` — what the gym spawns and how to reach each part of it
- `Docs/Playtest-Feedback-Log.md` — every owner playtest and what was done about it
- `Docs/Design/Power-Curve.md` — the O27 scaling architecture and O29's endgame answer
- `Docs/Design/Level-Design.md` — the spatial grammar, and the editor delete list
- `Docs/Design/UI-*.md` — the FIELDPLATE visual system, each with an implementation-status section
- `Docs/Vertical-Slice.md` — slice scope and definition of done
- `Docs/Godot-Mechanics-Audit.md` — the prior prototype, as read-only reference

## Conventions that bite

- Never hand-edit `.uasset` or `.umap`. Move and rename only inside the editor.
- **`Lvl_FirstPerson` is World Partition with external actors.** The `.umap` is
  ~14 KB and contains no actor names; every actor is its own `.uasset` under
  `Content/__ExternalActors__/`. Grepping the map for an actor returns a
  confident false negative. Historical `.uasset` blobs in git are **LFS
  pointers**, not assets.
- Never commit `Binaries`, `DerivedDataCache`, `Intermediate` or `Saved`.
- Every unplaytested number is flagged `O2 PLACEHOLDER` in a comment. Ruling O2
  freezes value authoring, so an unflagged constant reads as balanced when it
  is not.
- All content has a C++ fallback registry, so a clean clone plays with no
  assets. Data Assets replace fallbacks one-for-one later.
- Builds fail while the editor is open (Live Coding lock). Close it first.
- **The suite is expected to be RED.** 215 tests, 213 pass, and
  `RiorsEdge.Progression.PowerBand` and `RiorsEdge.Progression.RuleBandImpact`
  fail by design pending an owner ruling. Do not fix them.
- **The project can photograph itself**, and reading your own captures is
  expected of any visual work. It cannot move a mouse, so hover states and
  zoom/pan gestures are permanently outside what it can verify. Switch list in
  `CONTEXT.md`.
