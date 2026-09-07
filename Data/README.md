# Ability tuning

Edit `Data/abilities.json` in a text editor. Find an ability by its `id`, such as `Caster.Cleave` or `Caster.Rot`. Save valid JSON, then fully close and relaunch the game/editor: this file is loaded once per process. Numeric edits require no C++ rebuild. Keep every existing ID and key; do not add comments or trailing commas to JSON.

Gear's `Ability.AddedPower` row in `Data/affixes.json` controls Added Ability Power. Its initial 1–11 tier anchors are O2 placeholder tuning. This percentage increases the ability lane's base multiplier before Increased bonuses; it does not change weapon damage. Ability-delivered damage-over-time snapshots that multiplier when applied.

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

`Data/caster-resource.json` holds Caster resource-node tuning. All values are **O2 PLACEHOLDER tuning**, loaded once per process with compiled defaults if validation fails. `StatusApplicationMana` is 2 for a new status type on a target. Seep's rank multipliers are 1.5 and 2. These grants scale by the application's proc coefficient (0–1) and share the existing 6 Mana/second conditional-income cap. Reapplying an existing type pays nothing unless Follow Through applies to that Cleave Bleed.

Attrition refunds 4/8 Mana at ranks 1/2 when a victim dies carrying your damaging status, regardless of who lands the killing blow. Multiple statuses on that victim pay once per caster. Expired statuses and zero-proc applications do not pay.

In `abilities.json`, Cleave's `FollowThroughRankOneKillRefund` / `FollowThroughRankTwoKillRefund` are 3/6 Mana for a direct Cleave kill. Resonance's `PaymentRankOneManaPerStatus` / `PaymentRankTwoManaPerStatus` are 2/4 per distinct detonated status, up to six; its base refund remains zero. These four values are also **O2 PLACEHOLDER tuning**. Payment still pays when the Resonance node preserves statuses at half duration, but zero-proc secondary statuses never earn refunds.

Explicit kill/detonation refunds bypass the conditional-income cap. They retain the existing resource maximum, generation suspension during Unmake, and doubled income while Mana is negative. These are initial tuning values, not validated build-balance targets.

Close doubles landed weapon-hit Mana within 600/900 cm at ranks 1/2. Debt extends the Overcast floor by 10/20 Mana. Bloodprice heals 10%/20% of actual melee health and shield damage while your Mana is negative. Patience doubles passive regeneration after 4/2 seconds without firing; a missed shot also restarts that wait. Variance raises income for a newly applied status type to 2×/3×; its bonus adds to Seep rather than multiplying Seep. These values live in `caster-resource.json` and are O2 placeholders.

Siphon's `DrainRankOneThreshold` / `DrainRankTwoThreshold` in `abilities.json` raise the incoming health-damage threshold that interrupts the channel to 10%/15% of maximum health. The unpurchased threshold stays 5%. All three are fractions (0.10 means 10%), not damage amounts.

Rot's StandingWaterRankOneManaPerSecond / StandingWaterRankTwoManaPerSecond grant 2/4 Mana per second while a living enemy occupies your Rot, sharing the conditional-income cap. More zones or enemies do not multiply this stream. ZoneworkAdditionalArmorReduction adds 20 flat armour strip against an already damaged-over-time target. WellspringSelfPlacementRadiusCm (150 cm) and WellspringMinimumGroundNormalZ (0.7) define an intentional ground cast near your feet; that zone follows you and later self-casts refresh it. These five values are O2 placeholders in abilities.json.

Closequarter's MomentumTransferRankOneSeconds / MomentumTransferRankTwoSeconds give 2/3 seconds after a successful targeted arrival to land your next melee hit with that target's block and dodge bypassed. Chain's rank reach is currently an editor-only StatusComponent default (600/900 cm), not an abilities.json key. These are O2 placeholders.

## Gunsmith placement timing

`BaseDeployCastSeconds` in the Turret, Ammo Crate, Mine Cluster and Disruptor
rows is the ordinary placement delay, currently 1 second (O2 placeholder).
Purchased Dead Ground makes Mine Cluster and Disruptor instant while Dry or
Stocked, and doubles the delay while Surplus. The band is read when casting
starts. Pressing the same slot again cancels a pending placement. The target
must still have valid ground, range and line of sight when the cast finishes;
payment and cooldown start only when placement succeeds.
