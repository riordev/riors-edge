---
paths:
  - "Source/RiorsEdge/Progression/**"
  - "Source/RiorsEdge/Items/**"
  - "Source/RiorsEdge/Abilities/BreakerAbilityDefinition.cpp"
  - "Data/**"
---

# Content authoring (LEDGER, DATA)

- Aggregation law: `(Base + ΣFlat) × (1 + ΣIncreased/100) × ΠMore`. Three
  additive pools (Weapon, Ability, Shared); delivery decides the pool (O55).
- One More ceiling, 1.30³, spanning every pool; applicable gear and tree
  Mores share the strongest three slots. Aberrant or Unwritten rewrites may
  author a More; ordinary affixes never do (O221, `Docs/DECISIONS.md`).
- Core follows the owner's full 22-wedge roster, including its explicit flat,
  Increased, rate and defensive-chance lanes. Do not reject those nodes as
  affix-only percentages (O234–O235; `Docs/spec/core-wheel.md`). Core owns
  generic axes, not named class resources, ability identities or ultimates.
- Do not author a node against a stat target with no aggregation lane or a
  condition nothing evaluates. Check `Docs/STATE.md` "Silent nodes" first.
- Preserve save-facing tree/node identities for re-themes (O103). An explicitly
  authorized replacement needs its own frozen, atomic, idempotent migration;
  the accepted Core replacement has `Save.CoreLayoutMigration` and
  `Save.HistoricalTravelMigration` contracts in `Docs/spec/core-wheel.md`.
  Ordinary unresolved removed nodes use O180's load-time refund rule; do not
  substitute that fallback for authored historical purchase costs. Save enums
  remain append-only.
- Rarity ladder: Standard, Uncommon, Exceptional, Aberrant (stacking: Focused
  or Modified), Unwritten (singular, one major rewrite). Legendary is a
  separate axis (O32). Rewrite equip caps are three minor plus one major;
  a legendary's authored pair occupies that major slot (O68,
  `Docs/spec/items-and-crafting.md`). Existing over-budget special items are
  grandfathered: budgets constrain new rolls, never strip saved affixes
  (O248, `Docs/DECISIONS.md`).
- One currency: Riftglass, account-wide (O51). No vendor economy.
- Once content lives in `Data/`, a magnitude change is a data change with no
  C++ diff. If it needs a compile, the migration is not done.
