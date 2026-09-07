# Ability tuning

Edit `Data/abilities.json` in a text editor. Find an ability by its `id`, such as `Caster.Cleave` or `Caster.Rot`. Save valid JSON, then fully close and relaunch the game/editor: this file is loaded once per process. Numeric edits require no C++ rebuild. Keep every existing ID and key; do not add comments or trailing commas to JSON.

Each row exposes `resourceCost`, `cooldownSeconds`, and `windowDuration`. Its `numbers` object contains that ability's supported base values. Ultimate rows and their `variants` are in the same file; keep keystone IDs intact.

| Value | Meaning |
| --- | --- |
| `RangeCm`, `RadiusCm`, `HalfHeightCm`, `MaximumRangeCm` | Distance in centimetres; 100 cm = 1 metre |
| `DamagePerTick`, `PoisonDamagePerTick`, `ImpactDamage`, `UnarmedDamage` | Base damage before applicable character and target modifiers |
| `WeaponDamageCoefficient` | Multiplier of the ability's weapon damage input, not flat damage |
| `TickInterval`, `TickIntervalSeconds`, `PoisonTickInterval` | Seconds between ticks; halving the interval doubles ticks per second |
| `Duration`, `DurationSeconds`, `PoisonDuration` | Duration in seconds |
| `ArcDegrees` | Width of a melee swing in degrees |

Use only the keys already present on the row. Some abilities have no `numbers` entries yet; those have only the row-level cost/cooldown/window knobs available here.

## Caster examples

Cleave: `RangeCm` is now 450 (4.5 metres). `ArcDegrees` controls width, `WeaponDamageCoefficient` controls the hit, and `Bleed*` values control its damage over time.

Rot: `ZoneDamagePerTick` is direct zone damage; `PoisonDamagePerTick` is the applied poison's damage per tick. `TickIntervalSeconds` controls zone damage/status-application ticks, while `PoisonTickInterval` controls poison damage. These are separate clocks. Changing poison from 5 damage every 1 second to 2.5 damage every 0.5 seconds preserves its base damage per second while making ticks more frequent. Application delay and repeated refreshes still affect what a short visit to the zone receives.

After an edit, check a fresh cast on the same enemy with the same gear. Change one value at a time. Invalid data is reported in the game log; fallback values should not be used to judge a tuning experiment.