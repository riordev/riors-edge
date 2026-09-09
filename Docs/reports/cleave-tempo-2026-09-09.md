# Cleave starter tempo

Cleave uses12 Mana instead of20, with a0.30-second activation lock/window instead of0.45. Both are editable O2 placeholders in Data/abilities.json. WeaponDamageCoefficient remains1.5; range, arc, Bleed and all other class costs are unchanged. Melee damage and status application occur synchronously before the lock, and the visual stroke lasts0.22 seconds, so the shorter lock truncates neither.

First native trial, seconds10–20 against the same level/area/item setup:

| Level | Earlier Cleave DPS | Trial Cleave DPS | Trial rifle DPS | Cleave/rifle |
|---|---:|---:|---:|---:|
| 1 | 110.115 | 150.944 | 206.930 | 0.729 |
| 20 | 577.972 | 779.145 | 1050.555 | 0.742 |

Accepted casts increase13→23 per ten seconds in that interval. This is an improvement, not completed starter parity. Ordinary critical samples vary across runs. A fixed saturated Bleed stream does not scale with every extra swing; direct versus periodic accepted damage needs separate measurement before another coefficient change. Cleave remains Weapon-pool delivery, while the earned Fracture diagnostic independently exercises Ability delivery.

Tradeoffs: the higher recovery and melee income operate in negative Mana, which retains15% increased incoming damage (25% with Long Debt). Multiple targets and Follow Through refunds benefit from the cheaper cost; its rank-two debt refund can match the new price. Free-cast effects save less per activation. These interactions need encounter play, not a blanket ability-damage increase.

Coupled tests use the new actual quote and lock. Prepared's status-income boundary is reached with an already-purchased Fracture from a positive bank; starting cheaper Cleave at negative Mana would correctly refuse casting. The test retains the same−19 boundary and exact doubled status-income assertion. Censored diagnostic rows explicitly report opening-window availability and avoid treating kill-awarded ammo as weapon consumption. No progression/balance pins change.

Final build/census/full suite:852 passing,3 expected failures,0 unexpected. Independent review covered timing, debt/refund tradeoffs and coupled assertions. Repeated native output still shows23 accepted casts in seconds10–20; damage varies with ordinary critical samples.
