# Owner rulings pending ratification

**Written 2026-08-16.** These are rulings the owner made in chat on
2026-08-15/16 that exist nowhere in `Docs/Design/Decisions.md`. Each is phrased
so the owner can transcribe it as an O-entry verbatim, or overturn it. Nothing
in this file is law until it lands in the ledger — under O28, the ledger is the
only authority. Agents: do not edit `Decisions.md` yourself (R1); this file is
the staging area, not a second ledger.

## Rulings made in chat, awaiting transcription

- **P1 — Premise.** Rior's Edge is a **looter shooter with ARPG elements**, with
  movement as a **core pillar** — NOT "movement-driven" as older docs and
  HANDOFF §1 phrase it. Story structure is **multi-area** per
  `Docs/Design/Campaign-And-Story.md`, not a single arena.
- **P2 — The three-map split.** `Lvl_FrontEnd` (boot/title) → `Lvl_Anchor`
  (hub) → `Lvl_Gym`, travel as `OpenLevel`
  (`Interaction/BreakerTravelPoint.cpp`), and gym-as-fallback for unnamed maps
  (`Game/BreakerGameInstance.cpp:33-43`). Agent-built under a chat brief
  (HANDOFF §7 A8); needs ratification or overturning.
- **P3 — Runtime lighting as the provisional A6 answer.**
  `Game/BreakerWorldBasics.cpp` spawns sun/sky/atmosphere (and the front end's
  boot floor) only when a map has no authored directional light, so an authored
  lit map silently supersedes it. Provisional until A6 is ruled.
- **P4 — Points per level (A1).** `XP-And-Pacing.md` §4 implemented as ruled:
  1 Class Point/level to 30, 1 Core Point/level to 50, the slice lump
  reinterpreted as an advance on the entitlement — level 11 makes the first
  keystone affordable (`Progression/BreakerProgressionComponent.cpp:468`).
  This **supersedes A2** (the keystone-budget contradiction) by levelling past
  it rather than moving either O2-frozen number.
- **P5 — Loot slot-draw salt.** The drop slot is drawn from a salted stream
  (`UBreakerLootLibrary::RollDropSlot`), ending the archetype==slot collision;
  all eight weapon archetypes now drop. **Historical drop seeds re-rolled** —
  unavoidable, since the old outcomes were the bug (HANDOFF §8 T12 caveat).
- **P6 — All five classes get built.** Owner authorization, 2026-08-16 chat:
  build Gunsmith, Tank and Support as **O2-placeholder implementations**
  (perceptible placeholders per R2), and author **Aberrant unique-modifier
  affixes and Anomalous signature affixes** — the seat O11 reserved.
- **P7 — Swift identity redirection.** Swift's identity is
  "multishot, pierce, chain, ricochet, movement, manipulation of projectiles
  with your momentum" (owner's words). Existing Swift tree/kit content should
  be read against this direction, not the older movement-only framing.
- **P8 — Save wipe.** Performed 2026-08-16: save data moved (not deleted) to
  `Saved/SaveGames_wiped_2026-08-16` per the owner's fresh-boot request.

## Still-open contradictions only the owner can rule

- **O25's stale third-jump line** (`Decisions.md:89`) still records Swift's
  third jump as unimplemented; it is built. Append-only means only the owner
  corrects the ledger.
- **D33** — `Decisions.md:14` says pending questions live at the bottom; the
  pending section is at `:128`, mid-file, with O29–O40 after it. A fresh
  reader who jumps to the bottom concludes nothing is pending.
- **A6 final** — runtime-built vs asset-authored lighting (P3 is provisional).
- **A7** — does the authored Anchor map replace or dress the runtime hub
  builder? (`Anchor-Hub-Layout-Brief.md:129-133`; 24 anchor_hub meshes are
  committed with no ruling.)
- **A10** — vendor duplication: Kess and the Quartermaster exist in both the
  gym camp and the hub, offering the same `Quest.FirstContract` from two maps.
- **A12** — `GymAreaLevel = 10` vs the rarity gates (Aberrant 25 /
  Anomalous 40): as shipped, no gym playtest can produce an Aberrant,
  Anomalous, legendary or item rule. Both numbers O2-frozen.
