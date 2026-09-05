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
- One More ceiling, 1.30³, spanning every pool; at most three Mores, Core
  Convergence/Keystone only (O3, O34, O95). No item rule authors a More.
- Trees own rules and conditions; affixes own raw percentages (O76). A node
  that reads as a flat percentage is misfiled.
- Do not author a node against a stat target with no aggregation lane or a
  condition nothing evaluates. Check `Docs/STATE.md` "Silent nodes" first.
- Tree ids and node ids never move (O103). A removed node refunds on load
  (O180). Save enums are append-only.
- Rarity ladder: Standard, Uncommon, Exceptional, Aberrant (stacking: Focused
  or Modified), Unwritten (singular, one major rewrite). Legendary is a
  separate axis (O32).
- One currency: Riftglass, account-wide (O51). No vendor economy.
- Once content lives in `Data/`, a magnitude change is a data change with no
  C++ diff. If it needs a compile, the migration is not done.
