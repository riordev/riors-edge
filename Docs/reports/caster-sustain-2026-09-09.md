# Native Caster sustain diagnostic

Actual level/area/item 1 and 20 characters use ordinary class choice, registered equipment grants, the same deterministic legal Standard rifle at each depth, native Mana recovery, weapon reloads, traveling projectiles and accepted target health loss. Fracture is purchased with an earned level token. No Core/Doctrine allocations or affix edits are added. Each row runs 60 seconds plus five seconds for pending delivery; an opening Rot is optional.

| Level | Delivery | DPS 0–10s | DPS 10–20s | DPS 50–60s | Casts in 60s |
|---|---|---:|---:|---:|---:|
| 1 | Rifle | 213.735 | 205.693 | 0 (no ammo) | 0 |
| 1 | Cleave | 105.166 | 110.115 | 110.115 | 78 |
| 1 | Rifle + opening Rot | 223.570 | 208.168 | 0 (no ammo) | 0 |
| 1 | Cleave + opening Rot | 116.547 | 108.259 | 106.403 | 77 |
| 20 | Rifle | 1114.421 | 1050.555 | 0 (no ammo) | 0 |
| 20 | Cleave | 542.848 | 577.972 | 568.392 | 78 |
| 20 | Rifle + opening Rot | 1188.247 | 1103.245 | 0 (no ammo) | 0 |
| 20 | Cleave + opening Rot | 629.446 | 558.812 | 549.233 | 77 |
| 20 | Fracture | 1900.975 | 1267.318 | 1263.306 | 38 |
| 20 | Fracture + opening Rot | 1772.642 | 1267.318 | 1072.808 | 38 |

Cleave uses the Weapon pool by design; Fracture exercises Ability delivery. Cleave/rifle ratio in seconds10–20 is0.535 at level1 and0.550 at level20. This identifies a starter-kit shortfall; it does not establish a universal ability deficit. The existing allocation-parity gauge remains unchanged.

Every rifle row spends its normal150 rounds and exhausts ammunition during seconds20–30. Late rifle zeros and whole-minute totals must not be used as sustained-throughput parity evidence. No pickups or ammo injections occur. Targets are stationary, at3m, and use the shipped crowd-probe boss chassis; none dies during measurement. Level20 has unspent progression and is not a representative developed build. Enemy retaliation, movement, aim difficulty, multiple targets, encounter drops and audio/visual feel are outside this diagnostic.

All opening Rot zones deliver their12 boundary ticks, affect actual occupancy and release/destroy normally. A separate real-clock HUD test confirms successive half-second Rot ticks merge only inside the existing merge interval, preserve number birth time, start a new group afterward, and remain separate from Bleed. This fixes a test that previously advanced status time while leaving HUD world time at zero; production feedback tuning is unchanged.

Build/full suite:852 passing,3 expected failures,0 unexpected. The real-clock assertion uses double-precision world time to match the engine API. No production numbers or measurement pins changed.
