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

## Accepted damage split and direct coefficient

The native diagnostic now records actual victim-side accepted health damage,
separating non-periodic hits from DoT. Element/status subtotals overlap those
categories; they must not be added together. Shield-only loss and overkill
are excluded. The observer never changes damage, health or clocks. Its
non-periodic category includes bursts/reactions, not only raw weapon impact.

Before coefficient tuning, seconds10–20 contained:

| Level | Cleave direct health | Cleave Bleed health | Casts | Rifle DPS | Cleave DPS |
|---|---:|---:|---:|---:|---:|
| 1 | 890.818 | 618.624 | 23 | 214.663 | 150.944 |
| 20 | 4502.421 | 3193.206 | 23 | 1063.329 | 769.566 |

That is about41% periodic damage whose existing tick clock does not accelerate
with casts. A coefficient change1.5→2.4 predicts1.6times only the direct part,
roughly204/1040DPS. Actual rerun with native critical variation:

| Level | Direct health | Bleed health | Rifle DPS | Cleave DPS | Ratio |
|---|---:|---:|---:|---:|---:|
| 1 | 1365.921 | 618.624 | 212.189 | 198.454 | 0.935 |
| 20 | 7203.874 | 3193.206 | 1080.892 | 1039.705 | 0.962 |

Both still cast23 times, end at-2.6Mana, and pay ten Bleed ticks. Range, arc,
resource cost, animation lock, Bleed values and unarmed fallback are unchanged.
This supports provisional close-range starter competitiveness under the owner
ruling; it does not prove whole-game parity. Cleave remains weapon-derived
and Weapon-pool scoped, so this does not redefine the independent Ability-pool
gauge. Fracture remains the separate earned Ability-pool diagnostic.

Independent review confirmed Reprisal still consumes one charge and the
Spellblade melee More scales normally. Overpressure's40% splash scales with
stronger accepted direct hits, so clustered/multitarget builds need playtest
attention; no child recursion or proc guards changed. Rifle ammunition runs
out later in these fixtures, so late totals remain unsuitable parity evidence.

Raw before log: Saved/Logs/caster-split-before.log. After measurements are in
the09:26UTC full suite log. Canonical census now emits an O2 PLACEHOLDER
annotation beside every ability row: arbitrary prior comments were stripped
by export, so the declaration previously did not survive regeneration.
Only Cleave's WeaponDamageCoefficient magnitude changed in this block.

Final build/census/full suite:855 passing,3 expected reds,0 unexpected.
