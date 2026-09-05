# Lane ownership

Loaded every session. Directory decides the owner; the file's own comments
decide the crossing contract.

| Lane | Owns | Does not touch without a declared crossing |
|---|---|---|
| KIT | `Movement/ Abilities/ Characters/ Classes/` | `Combat/` (damage submission is FIELD's), `UI/` |
| FIELD | `Combat/` | `Game/` (spawning), `UI/` (the enemy bar TU is the one exception — O130) |
| GROUND | `Game/ Playtest/ Interaction/` | `Combat/`, `Items/` |
| GLASS | `UI/ Audio/` | `Abilities/` (KIT owns *when* a cue fires — O178) |
| LEDGER | `Items/ Progression/ Save/` | `Abilities/` (LEDGER writes nodes, KIT writes what they trigger) |
| NAV | `AI/` (new) | `Combat/` behaviour hooks stay FIELD's; NAV supplies locomotion under them |
| DATA | `Data/` (new), `Scripts/` | Each content library it migrates, once, one commit per library |

- Four shared HUD members are a declared crossing by rule (O155):
  `EnemyBlips`, `DrawnLabelBounds`, `LastFocusBarEnemy`, `LastFocusBarTime`.
- A crossing is stated in the commit message: which member, which direction,
  which lane is the other end.
- Two lanes never edit the same translation unit in one cycle. A file over
  3,000 lines is a legitimate split-by-owner work item.
- `.claude/OWNER_SEAT` (gitignored) marks the owner's seat: with it present, the
  protect-docs hook stands down so the design seat can edit protected docs.
