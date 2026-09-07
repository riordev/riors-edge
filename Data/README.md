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

Rot: `ZoneDamagePerTick` (5) and `TickIntervalSeconds` (0.5) control direct Entropy hits. Accepted hits build toward Rot; the zone no longer applies Poison. The three old Poison tuning keys are removed.

After an edit, check a fresh cast on the same enemy with the same gear. Change one value at a time. Invalid data is reported in the game log; fallback values should not be used to judge a tuning experiment.

## Caster resource nodes

`Data/caster-resource.json` holds Caster resource-node tuning. All values are **O2 PLACEHOLDER tuning**, loaded once per process with compiled defaults if validation fails. `StatusApplicationMana` is 2 for a new status type on a target. Seep's rank multipliers are 1.5 and 2. These grants scale by the application's proc coefficient (0–1) and share the existing 6 Mana/second conditional-income cap. Reapplying an existing type pays nothing unless Follow Through applies to that Cleave Bleed.

Attrition refunds 4/8 Mana at ranks 1/2 when a victim dies carrying your damaging status, regardless of who lands the killing blow. Multiple statuses on that victim pay once per caster. Expired statuses and zero-proc applications do not pay.

In `abilities.json`, Cleave's `FollowThroughRankOneKillRefund` / `FollowThroughRankTwoKillRefund` are 3/6 Mana for a direct Cleave kill. Resonance's `PaymentRankOneManaPerStatus` / `PaymentRankTwoManaPerStatus` are 2/4 per distinct detonated status, up to six; its base refund remains zero. These four values are also **O2 PLACEHOLDER tuning**. Payment still pays when the Resonance node preserves statuses at half duration, but zero-proc secondary statuses never earn refunds.

Explicit kill/detonation refunds bypass the conditional-income cap. They retain the existing resource maximum, generation suspension during Unmake, and doubled income while Mana is negative. These are initial tuning values, not validated build-balance targets.

Close doubles landed weapon-hit Mana within 600/900 cm at ranks 1/2. Debt extends the Overcast floor by 10/20 Mana. Bloodprice heals 10%/20% of actual melee health and shield damage while your Mana is negative. Patience doubles passive regeneration after 4/2 seconds without firing; a missed shot also restarts that wait. Variance raises income for a newly applied status type to 2×/3×; its bonus adds to Seep rather than multiplying Seep. These values live in `caster-resource.json` and are O2 placeholders.

Siphon's `DrainRankOneThreshold` / `DrainRankTwoThreshold` in `abilities.json` raise the incoming health-damage threshold that interrupts the channel to 10%/15% of maximum health. The unpurchased threshold stays 5%. All three are fractions (0.10 means 10%), not damage amounts.

`Data/statuses.json` defines Void as a non-damaging debuff: `durationSeconds`
is 4, `armorReductionPercent` is 20, and `healingReductionPercent` is 25
(O2 placeholders). Siphon's successful damage ticks and Fracture's third cycle
position apply it. Refreshing extends the effect without stacking its reductions.
Changing `dealsPeriodicDamage` changes status behavior, so keep it false for Void.

Sequence's `SequenceWindowSeconds`, `SequenceCooldownSeconds`,
`SequenceRankOneMana` and `SequenceRankTwoMana` in `caster-resource.json` are
6, 10, 10 and 15 (O2 placeholders). Three different accepted applications to
the same living target pay the lump sum immediately. This node uses its
per-target cooldown instead of the ordinary 6 Mana/second income meter;
suspension, resource capacity and Overcast rules still apply. The weakest
application's proc coefficient scales the payout; zero-proc echoes never count.

Rot's StandingWaterRankOneManaPerSecond / StandingWaterRankTwoManaPerSecond grant 2/4 Mana per second while a living enemy occupies your Rot, sharing the conditional-income cap. More zones or enemies do not multiply this stream. ZoneworkAdditionalArmorReduction adds 20 flat armour strip against an already damaged-over-time target. WellspringSelfPlacementRadiusCm (150 cm) and WellspringMinimumGroundNormalZ (0.7) define an intentional ground cast near your feet; that zone follows you and later self-casts refresh it. These five values are O2 placeholders in abilities.json.

Closequarter's MomentumTransferRankOneSeconds / MomentumTransferRankTwoSeconds give 2/3 seconds after a successful targeted arrival to land your next melee hit with that target's block and dodge bypassed. Chain's rank reach is currently an editor-only StatusComponent default (600/900 cm), not an abilities.json key. These are O2 placeholders.

## Support Cadence

Cadence's `AuraRadiusCm` is 500. Section adds 200/400 cm at ranks 1/2;
`DetachedBatonRadiusCm` is 800 before that addition. `ReloadTempoMultiplier`
and `SwapTempoMultiplier` are 1.25, making each action take its authored
duration divided by 1.25. These bonuses do not multiply across Supports.
Downbeat's active Conduit doubles the bonus above 1, producing 1.5 at these
defaults. `ConductingTailSeconds` is 2. All are O2 placeholder values in
`abilities.json`, loaded once per process.

## Support Metronome

Metronome's `RecipientRadiusCm` selects recipients at cast, currently 500 cm
(O2). `FlatDamagePerStack`, `MaximumStacks` and `StreakGapSeconds` tune each
holder's independent weapon ramp. Conduit's `DownbeatFlatDamagePerBuffedTarget`
uses the live unique count of your Cadence and Metronome recipients.

## Support Triage and Attending

Triage uses Conduit's existing `RadiusCm` (1500) and
`TriageHealFractionPerSecond` (0.04). The boundary follows the caster; nearby
living players receive healing and one lethal-hit rescue per cast. Rescue
leaves at most 1 health (O2 C++ default), and cannot be reset by leaving or
reviving. Attending credits actual restored health at the heal's proc weight;
rank two also refreshes the real Mark effect, preserving any longer remainder.

## Support Mark

Mark's `PaintedAllyYieldMultiplier` is 0.5 (O2), paid only at Painted rank two.
Own and allied marked damage also use their actual proc coefficient; zero
proc and unsuccessful hits generate no Charge. This does not grant kill credit.

## Tank landing and stagger

Ground Zero resolves its hit when the plunge physically lands. Its damage
reads measured fall distance, with the ordinary 12-metre cap extended to
25 metres by Terminal Descent. The two distance keys in `abilities.json`
replace the previous fall-speed proxy.

Ordinary fall harm is currently tuned on the Character Movement component:
`SafeFallDistanceCm` (600) and `FallDamageHealthFractionPerMeter` (0.05).
These are O2 defaults; the fall deals shield-first environmental damage.
Kinetic Recovery's authored timing is three seconds to land and 1.5 seconds
of stagger immunity. It does not reduce the blast's takeoff self-hit.

`StaggerResistance` lives on Combat components, from 0 to 1; it shortens
stagger duration. Enemies also have an explicit `bStaggerImmune` switch.
Warden's `SlamStaggerSeconds` is 0.5 (O2). These component defaults currently
require an editor property change or C++ rebuild rather than a JSON edit.

## Gunsmith placement timing

`BaseDeployCastSeconds` in the Turret, Ammo Crate, Mine Cluster and Disruptor
rows is the ordinary placement delay, currently 1 second (O2 placeholder).
Purchased Dead Ground makes Mine Cluster and Disruptor instant while Dry or
Stocked, and doubles the delay while Surplus. The band is read when casting
starts. Pressing the same slot again cancels a pending placement. The target
must still have valid ground, range and line of sight when the cast finishes;
payment and cooldown start only when placement succeeds.

## Special gear damage

`Aberrant.ReserveSurge` in `affixes.json` grants ability More damage while your
class resource is low. Its tier anchors run from 3% at T12 to 8% at T1 (O2
placeholders). It consumes the shared strongest-three gear/tree damage-source
budget and carries `Downside.Riftburn`, reducing Increased weapon damage.
The paired bill is 12% at ordinary tiers; existing tier extrapolation also
scales that downside at T0 and T-1. Ordinary affixes cannot grant damage More.

## Damage Ramp

`Weapon.DamageRamp` in `affixes.json` is a Primary prefix. Its per-stack
Increased weapon damage anchors are 0.5% at T12 and 2% at T1 (O2 placeholders).
The current tier extrapolation produces 4.4% at T0 and 7.2% at T-1; these are
not the old eight-tier draft's values. Ten stacks is the maximum. Each damaging
shot earns one stack for later shots; purchased Redline Trigger earns two at
Redline. A miss, swap, removal or death clears the streak. Projectile weapons
earn stacks on impact, and pellets or multiple blast victims count once.

### Entropy tuning
`elements.json` controls the first elemental status. All five numbers are O2 placeholders: threshold is 10% of target maximum health; buildup is the raw Entropy share times proc eligibility and one minus resistance. Four seconds without a qualifying hit clears incomplete buildup. At threshold, Rot snapshots half the triggering raw Entropy share over four seconds, ticking every 0.5 seconds. Active Rot cannot stack, refresh or be directly applied by carried payloads. Its ticks cannot build another Rot. Armour and shared incoming modifiers affect its ticks once; resistance never reduces damage.

`Weapon.EntropyConversion` in `affixes.json` is an ordinary Primary prefix. It converts 20–60% across the ordinary tier ladder, capped at 100% including special tiers. Only the remaining physical share receives physical gear reduction. Hitscan and projectiles snapshot conversion when fired. Legacy `Status.Void` armour/healing reduction is removed; Siphon retains its channel damage and healing, and Fracture cycles Bleed, Poison, then Entropy. The Entropy position converts its real impact and earns Rot only at threshold.
Vestige melee uses `vestigeMeleeEntropyFraction` (0.5) and Vestige chassis use `vestigeEntropyResistancePercent` (25). Both are O2 tuning in `elements.json`; zero disables the respective contribution. Other families retain their authored attacks. These values do not change base melee damage. Enemy readouts show ENTROPY buildup or ROT seconds beneath their health bars; incoming buildup and Rot duration appear beside player vitals.