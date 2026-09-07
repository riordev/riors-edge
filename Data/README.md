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

Rot: `ZoneDamagePerTick` is direct zone damage; `PoisonDamagePerTick` is the applied poison's damage per tick. `TickIntervalSeconds` controls zone damage/status-application ticks, while `PoisonTickInterval` controls poison damage. These are separate clocks. Rot now applies poison on entry and deals 2.5 base damage every 0.5 seconds per stack (5 per second). Its zone reapplies once per second; ordinary status stacking and refresh rules still apply. Halving a tick interval requires halving per-tick damage to preserve base DPS. Earlier application, stack ramps, and proc effects can still change total combat output.

After an edit, check a fresh cast on the same enemy with the same gear. Change one value at a time. Invalid data is reported in the game log; fallback values should not be used to judge a tuning experiment.

## Caster resource nodes

`Data/caster-resource.json` holds shared status income. All five values are **O2 PLACEHOLDER tuning**, loaded once per process with compiled defaults if validation fails. `StatusApplicationMana` is 2 for a new status type on a target. Seep's rank multipliers are 1.5 and 2. These grants scale by the application's proc coefficient (0–1) and share the existing 6 Mana/second conditional-income cap. Reapplying an existing type pays nothing unless Follow Through applies to that Cleave Bleed.

Attrition refunds 4/8 Mana at ranks 1/2 when a victim dies carrying your damaging status, regardless of who lands the killing blow. Multiple statuses on that victim pay once per caster. Expired statuses and zero-proc applications do not pay.

In `abilities.json`, Cleave's `FollowThroughRankOneKillRefund` / `FollowThroughRankTwoKillRefund` are 3/6 Mana for a direct Cleave kill. Resonance's `PaymentRankOneManaPerStatus` / `PaymentRankTwoManaPerStatus` are 2/4 per distinct detonated status, up to six; its base refund remains zero. These four values are also **O2 PLACEHOLDER tuning**. Payment still pays when the Resonance node preserves statuses at half duration, but zero-proc secondary statuses never earn refunds.

Explicit kill/detonation refunds bypass the conditional-income cap. They retain the existing resource maximum, generation suspension during Unmake, and doubled income while Mana is negative. These are initial tuning values, not validated build-balance targets.
