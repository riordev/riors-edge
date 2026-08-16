# Build Profiles — the shipped math, played on paper (2026-08-16)

**Computed against commit `f2f5a54` (HEAD at time of writing).** Every constant below is
traceable to a file at that commit; nothing is invented. Where a value is an O2 placeholder in
code it is a placeholder here too. Where the code has no number, the line says **UNKNOWN**.

## 0. The shared math (verified in code)

| Piece | Formula as shipped | Source |
|---|---|---|
| Weapon base | `ArchetypeBase × 1.09^(ilvl−1)` (`ItemLevelDamageGrowth = 0.09`, ilvl clamp 1..120) | `Weapons/BreakerWeaponMath.cpp` `ItemLevelDamageScalar`, `Weapons/BreakerWeaponComponent.h:256` |
| Source multiplier | `DamageMultiplier attribute = (1 + AddedDamage/100) × (1 + ΣIncreased%/100) × MoreProduct`, More product globally clamped to `1.30³ = 2.197`, ≤3 sources ≤1.30 each | `Attributes/BreakerAttributeAggregation.cpp` `Compose`/`ComposedMoreProduct`; Added lands as Flat on base 1.0: `Items/BreakerEquipmentComponent.cpp:896` |
| Tree floor | +0.25% Increased per committed point (`IncreasedDamagePerSpentPoint = 0.25`) | `Progression/BreakerProgressionComponent.h:103` |
| Crit | expected factor `1 + chance × (mult − 1)`; base 0.05 / 1.5 | `Attributes/BreakerAttributeSet.h:31-32`, `Combat/BreakerDamageLibrary.cpp` |
| Monster HP | `220 × 1.09^(AL−1) × rank × archetype`; rank {1, 3, 2.5, 25}; ModifierBearing also `× (1 + 0.35(N−1))` per extra modifier | `Combat/BreakerMonsterChassis.h`, `BreakerEnemyModifiers.h:166`, `BreakerEnemy.cpp:198` |
| Monster damage | `14 × 1.055^(AL−1) × rank {1, 1.5, 1.25, 2}` (ranged archetype authors 16) | `BreakerMonsterChassis.h:65,78` |
| Armor | `a/(a+100)` capped at 80%; block mitigates 50% on proc; dodge negates | `Combat/BreakerDamageLibrary.cpp:3-6,85` |
| Boss | archetype health ×0.35 (`BreakerBossEnemy.cpp:31`), frontal armor 90 → ×0.50 at 33% HP (`BreakerBossPhases.h:93`), rear arc armor ×0.0 (`BreakerWardenEnemy.h:61`), exposed during order raises and permanently in phase 3 |
| XP | `XpToNext(L) = 240 × L^1.45`; kill = `12 × rank{1,4,7,40} × (1 + 0.08(AL−1))` | `Progression/BreakerExperience.h` |
| Points | Class = min(L,30), Core = min(L,50); slice lump 10/12 is an *advance* on those | `BreakerProgressionLibrary.h:189-197`, `BreakerProgressionComponent.cpp` |
| Swift channels | pierce ×0.70/penetration (kill skips step w/ Overpenetration), chain ×0.50, ricochet ×0.65; Running +1 pierce, Redline +1 chain, airborne +1.0 projectile, sliding +0.5 (both require ≥Running) | `BreakerWeaponComponent.h:417-429`, `MomentumChannelBonus` |
| Ability scaling (O35) | every kit ability multiplies by the equipped weapon's `ItemLevelDamageScalar` (`GetScaledBaseDamage` or `AbilityDamageScalarFor`) | `Abilities/BreakerGameplayAbility.cpp:69`, per-ability cpps |

**Snapshot scaffolding used everywhere below** (gym content at character level):

| | L10 (AL10, ilvl 10) | L25 (AL25, ilvl 25) | L50 (AL50, ilvl 50) |
|---|---|---|---|
| ilvl scalar `S = 1.09^(ilvl−1)` | 2.172 | 7.911 | 68.22 |
| Best rollable tier (`BestTierForItemLevel`) | **T11** | **T10** | **T6** |
| Trash HP | 478 | 1,740 | 15,008 |
| Elite HP (×3) | 1,433 | 5,221 | 45,024 |
| 2-mod pack anchor HP (×2.5×1.35) | 1,613 | 5,874 | 50,652 |
| Boss HP (×25×0.35) | 4,181 | 15,229 | 131,320 |
| Trash hit (melee 14 base) | 22.7 | 50.6 | 193.0 |
| Elite hit (×1.5) | 34.0 | 75.9 | 289.5 |
| Boss hit (×2) | 45.3 | 101.2 | 386.1 |
| Spent points → floor | 22 pts → +5.5% | 50 pts → +12.5% | 80 pts → +20% |

**Gear model** (stated so the numbers are reproducible; `ValueForTier` =
`V12 × (V1/V12)^(p^1.25)`, p = (12−T)/11 — verified against the Core.Health table in
`BreakerAffixLibrary.h`):

- **L10**: 8 Standard/Uncommon items, ~14 lines at T11. Offense allocation used: 2× Weapon
  Damage (3.39% ea), 1× Added Damage (1.13), 1 archetype-lean line, 2× Health (28.7 ea), 2× Armour (6.8 ea).
- **L25**: ~22 lines at T10 + one **Aberrant** carrying one special line at T10. 3× Weapon Damage
  (4.02 ea), 2× Added (1.33 ea), 1× Crit Chance (1.30 pts), 3× Health (34.75), 3× Armour (8.1).
- **L50**: ~30 lines at T6 + one **Anomalous** (signature line at T6 + a rule rewrite, whose
  effect is class/UNQUANTIFIED here). 5× Weapon Damage (9.49 ea), 3× Added (3.08 ea), 2× Crit
  Chance (2.80 ea), 1× Crit Damage (13.25), 3× Health (91.7), 5× Armour (19.6).

**Baseline source multiplier** (unconditional, before class/conditional lines):

| | Added | Increased (gear + core tree + floor) | Crit factor | **Multiplier** |
|---|---|---|---|---|
| L10 | 1.011 | 6.8 + 9 (Cyclic 3 + Salvo 6) + 5.5 = **21.3%** | 1.025 | **1.257** |
| L25 | 1.027 | 12.1 + 15 (…+ Penetrance 4, partial) + 12.5 = **39.6%** | 1.032 | **1.479** |
| L50 | 1.092 | 47.5 + 22 (Cyclic 3, Salvo 6, Penetrance 4, ReactionChain 6, CalledShot 3) + 20 = **89.5%** | 1.067 | **2.209** |

(Core-tree node values from `Progression/BreakerProgressionLibrary.cpp` lines 246–524, all O2.)

**Shared survivability** (identical for all five classes — no class authors HP, armor, dodge or
block; base 100 HP `BreakerAttributeSet.cpp:14`, dodge/block base 0 `BreakerCombatComponent.h:212-215`,
and **no dodge or block affix exists in any pool**):

| | HP | Armor → mitigation | EHP | Trash hits to die | Elite | Boss |
|---|---|---|---|---|---|---|
| L10 | 157 | 13.6 → 12.0% | 179 | **7.9** | 5.3 | 3.9 |
| L25 | 204 | 24.3 → 19.6% | 254 | **5.0** | 3.3 | 2.5 |
| L50 | 375 | 98 → 49.5% | 743 | **3.9** | 2.6 | 1.9 |

Archetype sustained body-DPS bases (per second, ilvl 1, from the prototype table in
`BreakerWeaponComponent.cpp:330-534`): Rifle **240**, SMG 195, Burst ~172 (3 rounds per
0.507s cycle), Sidearm 147, MG 128, Shotgun 113 (8×10 pellets), Sniper 180 per-shot-limited,
Rocket 82.5. Weak-point mults 1.75 / 1.5 / 1.9 / 1.8 / 1.4 / 1.35 / 2.0 / 1.0 respectively.

---

## 1. Swift (Momentum)

Loop constants (`Classes/BreakerMomentumComponent.h`): sprint (1100 cm/s,
`BreakerCharacterMovementComponent.h:120`) generates lerp(6@750, 10@1250) = **8.8/s**; slide
12/s; airborne 8/s (3s credit); dash +10 (≥1s apart); global cap 25/s. Decay 15/s under 400
speed, 6/s under 750, 0 above; **walking (700) generates nothing** — the 750 threshold sits
above walk speed. States: Running ≥ 1/3 bar, Redline ≥ 2/3 (`StateForFraction`). Skim 15 / 3s,
Lead 40 / 10s, Overdrive 100 (freezes decay, doubles gen, pushes a **×1.25 outgoing More** for
8s — `BreakerAbility_Overdrive.h:76`, the only More window in the game).

**Loop rhythm in numbers**: 0 → Redline = 7.6s of clean sprint (66.7 / 8.8), ~4–5s mixing
slides and a dash. Skim costs 1.7s of sprint income; a dash-Skim chain (−15 +10) is net −5.
Redline holds itself while moving ≥750; one second stationary costs 15 (≈1.7s of sprint to win back).

### Build A — "Redline Courier" (lean-in: SMG, Frenzy, Redline conditionals)
Adds Redline-conditional Increased: L10 gear 5.0 + node 6 = +11; L25 Aberrant **Redline
Chorus** T10 13.1 + gear 5.9 + nodes 11 = +30; L50 Redline Chorus T6 29.4 + gear 13.1 +
nodes 23 = +65.5. SMG Bleed (25%/bullet, 36 total over 3s, `BreakerWeaponComponent.cpp:370-373`)
adds ~4% sustained (stacking behavior vs refresh: governed by the status component; treated as one live instance here).

| Redline up | Body DPS | Trash | Elite | Boss (frontal blend 90→45) |
|---|---|---|---|---|
| L10 | 581 | 0.82s | 2.5s | 13.7s |
| L25 | 2,772 | 0.63s | 1.9s | 10.4s |
| L50 | 39,540 | 0.38s | 1.14s | 5.8s |

**Airborne at Running+ doubles this** (+1.0 projectile = a full second pellet, multishot is not
falloff'd): jump-spam sustained ≈ ×1.5, apex bursts ×2 — L50 airborne bursts ~74k DPS. Pierce
(Running +1) and chain (Redline +1 at ×0.50) add nothing vs a lone target and roughly +50–80%
effective damage into a 3-deep pack lane.

**How it plays**: sprint-slide into a pack, hit Redline inside ~5s, and hold it by never
stopping; every shot punches through one body and arcs half its damage onward, and every jump
literally doubles the gun. The power spike is the moment Redline conditionals stack (L25, when
Redline Chorus lands). It chases Redline-uptime lines and, at 50, a second Redline conditional —
not fire rate, whose T6 roll (6.4%) is a third of one Weapon Damage line.

### Build B — "Deadstill Marksman" (spicy: Sniper, Marksman, Stationary crit)
Aberrant **Deadstill Protocol** (+crit damage while Stationary: T10 +18.7, T6 +35.6 pts) plus
crit gear. L50: 14% chance, 1.99 mult → crit factor 1.138.

| Stationary | Body DPS | Weak-point DPS | Trash | Boss frontal blend |
|---|---|---|---|---|
| L10 (no special yet) | 494 | 988 | 0.97s | 16.1s |
| L25 | 2,129 | 4,258 | 0.82s | 13.6s |
| L50 | 28,930 | 57,860 | 0.52s | 8.0s (≈4s if weak points held) |

**The tension the math exposes**: standing still is Settled — Momentum decays at 15/s, so this
build has no channels, no Skim economy, and its own class resource fights its damage condition.
Also ADS with the sniper multiplies move speed by 0.50 (`ArchetypeRecoilProfile`), which is 550
cm/s — below the 750 generation threshold, so **a scoped Swift can never gain Momentum**. It
plays like a different class wearing Swift's HUD; that is either a great identity hybrid or a
trap, and the playtest should decide which.

---

## 2. Caster (Mana)

Loop (`Classes/BreakerManaComponent.h`): 100 bar starts full; passive regen **6/s**; hit-driven
income capped at another 6/s; Overcast floor **−20**, generation ×2 while negative, +15%
incoming damage. Costs (`BreakerAbilityDefinition.cpp`): Cleave 20, Rot 25, Closequarter 35,
Siphon 30, Fracture 30, Resonance 40, Unmake 80 (free casts, generation suspended). No cooldowns.

**Loop rhythm**: full → Overcast = 4× Rot (100→0), a 5th is refused (−25 < −20), one Cleave
fits (0→−20). Recovery: −20→0 at 12/s = 1.7s, then 0→100 at 6/s = 16.7s ≈ **18.4s to full**;
firing the gun (capped +6/s) halves the second leg. Sustained cast budget = 6–12 mana/s → one
25-cost cast every 2.1–4.2s.

### Build A — "Void Whisperer" (lean-in: DoT, Sidearm as applicator)
Rot: poison 5/tick × S × DoT multiplier (`BreakerAbility_Rot.h:52`; **tick interval lives in
the status spec — UNKNOWN in this report**, per-tick figures given). Siphon 14/tick × S, heals a
LeechFraction. Gear DoT: L25 +6.7%, L50 2×15.6% (+VW tree DoT nodes, present in code but
magnitudes not extracted here). Per-tick at L50: Rot **447**, Siphon **955** (before boss's
3-stack DoT cap, `BreakerBossEnemy.h:124`).

Sustained output is regen-limited: at L50 a Rot every ~4s plus the sidearm
(147 × 68.22 × 2.209 ≈ 22.2k DPS) — **the gun is >90% of the build's damage**; the spells are
armor-strip (Rot) and sustain (Siphon), not the payload.

### Build B — "Entropy Debt gunmage" (spicy: Rifle + Anomalous **Entropy Debt**)
Entropy Debt: +Increased Weapon Damage **while resource ≤ 0** (T6 = 73.0%; headline T1 = 200%).
The rotation: cast to −20, and each time regen touches 0, Cleave again (20) — the bank
oscillates in [−20, 0] on a 1.7s doubled-regen cycle, so **condition uptime ≈ 100%**, at the
price of permanent Overcast (+15% damage taken).

| | Body DPS (gun + Cleave rider) | Trash | Elite | Boss frontal blend |
|---|---|---|---|---|
| L25 (no Anomalous yet — Failsafe var.) | ~2,930 at ≤35% HP | 0.59s | 1.8s | 9.9s |
| L50 | **54,600** (50.1k rifle + 4.5k Cleave) | 0.27s | 0.82s | **4.2s** |

**How it plays**: the class loop is played *backwards* — mana is kept empty on purpose, casting
is a metronome for the debt window, and the rifle does the killing. It is the highest sustained
single-target number in the game off one affix line. The build chases nothing else; every other
line is filler next to a 73-point bucket entry.

---

## 3. Gunsmith (Scrap)

Loop (`Classes/BreakerScrapComponent.h`): accumulates from 0, **no regen, no decay**; kill +12
(× killing proc coefficient), reload +4 (only if a round was fired), full-magazine dump +8;
global cap 15/s; destruction refunds 50% of cost. Costs: Turret 40, Ammo Crate 30, Mines 35,
Disruptor 45, Field Assembly 100; Sidearm Rig / Overhaul are free on 10s/18s cooldowns.
Deployable caps: 4 total, 2 per type (`Combat/BreakerDeployable.h:130`).

**Loop rhythm**: a Turret costs **3⅓ kills** (or 10 reloads); the MG's magazine-dump cycle
(120 rounds ≈ 10.3s of fire) banks 12 (8 dump + 4 reload) plus kills. Two turrets ≈ 7 kills of
ledger. Field Assembly = ~8.3 kills.

### Build A — "Lane Engineer" (lean-in: Machinegun + 2 Turrets + Disruptor)
Turret: 0.6 × owner's scaled weapon base per 1.0s shot, proc coefficient 0.5, 150 HP, 18m
(`BreakerDeployable.h:138-145`). With the MG equipped: per-turret DPS = 0.6 × 11 × S.
(**Assumption flagged**: turret damage shown without the player's Increased bucket — whether
deployable fire routes `ApplyOutgoingModifiers` was not verified.)

| | Player MG DPS | +2 turrets | Trash | Elite | Boss frontal blend |
|---|---|---|---|---|---|
| L10 | 350 | 379 | 1.26s | 3.8s | 21.0s |
| L25 | 1,501 | 1,605 | 1.08s | 3.3s | 18.1s |
| L50 | 19,335 | 20,235 | 0.74s | 2.2s | 11.4s |

**How it plays**: the ledger makes the first minutes of every session poor (0 Scrap at spawn)
and mid-fight rich; turrets are ~5% of output — presence and proc utility, not damage. The felt
loop is dump-reload-place. Spike: first double-turret at ~7 kills. Chases: kill-tempo (its
damage IS its income) and, honestly, a rifle — the MG lean costs it ~47% body DPS vs the rifle
baseline for the fantasy.

### Build B — "Claymore Courier" (spicy: Sidearm Rig + Mine Cluster burst)
Sidearm Rig: next magazine +8 flat/shot (× S, `BreakerGunsmithAbilities.h:53`) and +1 pierce.
Mines: 3 × 1.2 × scaled base, 3m blast (`BreakerDeployable.h:155-160`). At L50 with Sidearm:
rigged magazine = 14 rounds × (21+8) × 68.22 × mult ≈ 61k in ~2s, mines 3 × 1.2 × 1,433 ×
2.209 ≈ 11.4k per 35 Scrap. A burst-window class in a game whose bosses punish burst least
(health-gated phases, no enrage): effective, spiky, ammo-hungry.

---

## 4. Tank (Grit)

Loop (`Classes/BreakerGritComponent.h`): accumulates from 0; +1 Grit per 2% max HP lost
post-mitigation (shield damage at half rate), damage source token-bucket capped 10/s (bucket
holds one second — a single big hit pays ≤10, `BreakerGritComponent.cpp:288-299`); proximity
(≤5m, count-independent) 1.5/s; melee kill +10; combat entry +15; block proc +6 (0.4s ICD);
global cap 20/s. Decay 5/s after a 6s lapse with no contact. Costs: Rend 25/6s, Bloodline
40/12s, Anchor 30/10s, Provoke 35/12s, Breach 30/8s, Ground Zero 45/10s, Hold 100 (caps
per-hit damage, ×3 generation, 10s).

**Loop rhythm (hits-to-full)**: entry 15, then a spaced elite hit ≥1s apart pays the full
10-point bucket → **~6 real hits + proximity ≈ 8–9s of honest contact from 15 to 100**. Rend
every 6s costs 25 — sustainable at ~4.2/s against an 11.5/s contact income.

### Build A — "Leech Brawler" (lean-in: melee sustain)
Rend = 1.3 × scaled weapon base + heal (`BreakerTankAbilities.h:38`). **The coefficient reads
the equipped weapon's per-PELLET base**, so with the thematic shotgun a Rend swing is
1.3 × 10 × S — with a sniper equipped it is 1.3 × 72 × S, 7.2× larger for the same button.
Shotgun shown (the fantasy loadout):

| | Body DPS (gun + Rend) | Trash | Elite | Boss frontal blend |
|---|---|---|---|---|
| L10 | 315 | 1.52s | 4.6s | 25.2s |
| L25 | 1,350 | 1.29s | 3.9s | 21.4s |
| L50 | 17,400 | 0.86s | 2.6s | 13.2s |

Survivability: **identical to every other class** (same 743 EHP at 50) until abilities fire —
the Tank's toughness is Anchor Point (a 20%-of-max-HP cover panel), Hold's per-hit cap, and
Rend/Bloodline sustain, not stats. At AL50 an elite still two-shots him between cooldowns.

### Build B — "Riftplate Bombardier" (spicy: Anomalous **Riftplate** + Breach/Ground Zero)
Riftplate at T6: **+101 flat Armour** (headline T1 400), bill −10% move speed. Armor 199 →
66.6% mitigation → **EHP 1,122** (5.8 trash hits — the only build in the game whose
hits-to-die at 50 beats its own L10 self). Breach Charge 1.5× and Ground Zero 1.8× weapon base
on 8/10s cooldowns; rocket self-damage feeds Grit at the 0.25 self rate under its own 3/s
bucket — rocket-jumping pays ~2 Grit a jump, deliberately worthless as an engine.

---

## 5. Support (Charge)

Loop (`Classes/BreakerChargeComponent.h`): accumulates from 0, no regen, no decay in combat;
out-of-combat clamp decays to 60. Income: +1 per 3% of target max HP healed/shielded
(overheal pays 0), **+1 per 2% of a MARKED target's max HP dealt as damage**, buff uptime 2/s
(boolean, count-independent), cleanse +4, self-heal sub-cap 6/s, global cap 18/s. Costs:
Patch 25/6s (heals 25% of target max), Mark 20/5s (target takes ×1.15, 10s), Cadence 30/8s,
Metronome 35/9s, Purge 30/10s, Suppress 40, Conduit 100 (abilities free, 15m, generation
continues).

**Loop rhythm (heal-events to cap)**: Patch pays 8.3 Charge (25%/3%) and costs 25 — **healing
is net −16.7 Charge per cast**; the bar is filled by Mark: one killed trash = 100% of its max
= **+50 Charge** (metered through the 18/s cap over ~2.8s), so **two trash kills = full bar**.
Against the boss the percent basis self-balances: at L50 build DPS ≈ 41.6k vs 2% of boss max
(2,626) → 15.8 Charge/s, just under the cap — a boss fight banks ~1 full bar every ~7s.

### Build A — "Warden Marksman" (lean-in: Rifle + Mark, solo)
Everything below is baseline rifle × the Mark multiplier (×1.15, uptime near 100% given
5s cd / 10s duration and self-sustaining cost):

| | Body DPS vs marked | Trash | Elite | Boss frontal blend |
|---|---|---|---|---|
| L10 | 763 | 0.63s | 1.9s | 10.4s |
| L25 | 3,229 | 0.54s | 1.6s | 9.0s |
| L50 | 41,616 | 0.36s | 1.08s | **5.5s** |

Quietly the game's second-best boss build: Mark is the only party-neutral damage multiplier any
class owns, and its Charge economy is the only resource loop that gets *richer* on bosses.

### Build B — "Metronome Conductor" (spicy: SMG + Metronome ramp + Cadence)
Metronome: +2 flat damage per consecutive-hit stack, streak resets after 1.0s without a hit
(`BreakerSupportAbilities.h:136-138`; **stack cap UNKNOWN — not read in this pass**). At the
SMG's 15 hits/s the ramp is instant and ×S-scaled riders on a 195/s chassis add up; Conduit
(free abilities, 20s) every ~2 packs. Playable rhythm-DPS, but the honest number is: without a
stack cap in hand this build cannot be priced, and it should be pinned before a playtest leans
on it.

---

## 6. Cross-class report

### TTK spread (body-shot, sustained, realistic condition uptime; boss = frontal 90→45 blend)

| Build | L10 trash/elite/boss | L25 trash/elite/boss | L50 trash/elite/boss |
|---|---|---|---|
| Swift A (Redline SMG) | 0.82 / 2.5 / 13.7 | 0.63 / 1.9 / 10.4 | 0.38 / 1.1 / 5.8 |
| Swift B (Deadstill Sniper) | 0.97 / 2.9 / 16.1 | 0.82 / 2.5 / 13.6 | 0.52 / 1.6 / 8.0 |
| Caster A (VW Sidearm DoT) | ~1.1 / 3.4 / 19 | ~0.95 / 2.9 / 16 | ~0.65 / 2.0 / 10 |
| Caster B (Entropy Rifle) | 0.72 / 2.2 / 12.0 | 0.59 / 1.8 / 9.9 | **0.27 / 0.82 / 4.2** |
| Gunsmith A (MG + Turrets) | 1.26 / 3.8 / 21.0 | 1.08 / 3.3 / 18.1 | 0.74 / 2.2 / 11.4 |
| Gunsmith B (Rig burst, avg) | ~0.9 / 3.0 / 17 | ~0.8 / 2.6 / 15 | ~0.55 / 1.7 / 9 |
| Tank A (Shotgun Leech) | **1.52 / 4.6 / 25.2** | **1.29 / 3.9 / 21.4** | **0.86 / 2.6 / 13.2** |
| Tank B (Riftplate Rifle) | 0.72 / 2.2 / 12.0 | 0.59 / 1.8 / 9.9 | 0.41 / 1.2 / 6.4 |
| Support A (Mark Rifle) | 0.63 / 1.9 / 10.4 | 0.54 / 1.6 / 9.0 | 0.36 / 1.1 / 5.5 |
| Support B (Metronome SMG) | ~0.8 / 2.4 / 13 | ~0.7 / 2.0 / 11 | ~0.5 / 1.4 / 7 |

Best/worst spread: **2.4× at L10 and L25, 3.1× at L50** — past the 2× line at 50, and the axis
is almost entirely **which gun you hold**, not which class you are. Rifle-carriers cluster at
the top regardless of class; shotgun/MG carriers sit at the bottom (archetype base DPS spread
is itself 2.1× — Rifle 240/s vs Shotgun 113/s — and the rifle also has the second-best
weak-point multiplier at 1.75).

### Where the power actually comes from (L50 baseline rifle, body)

```
ilvl base curve   ██████████████████████████████████████  ×68.2   (16,373 of 36,152 pre-band)
Increased bucket  ██████████                              ×1.895  (gear 47.5 + tree 22 + floor 20)
Weak-point aim    ████████ (potential)                    ×1.35–2.0, skill-gated
Conditionals      ████ (build-dependent)                  +30–73 into the same bucket
Added flat        █                                       ×1.092
Crit              █                                       ×1.067
More product      (empty)                                 ×1.00 passive; ×1.25 only inside Overdrive
Swift channels    ██████████ (Swift only, airborne)       ×2.0 multishot; pierce/chain AoE only
```

Of the multiplier *band* (the part builds control), the Increased bucket is ~86%, Added ~6%,
crit ~5%, Mores ~0%. Choices inside the bucket are real; choices between buckets are not.

### Three most broken-feeling interactions

1. **Entropy Debt is always-on for three classes.** `ResourceDepleted` is `fraction ≤ 0`
   (`BreakerBuildConditions.cpp:260`) and Grit/Scrap/Charge **start at 0 and stay there if you
   simply never spend** — a Tank, Gunsmith or Support who ignores their class loop wears a
   +30%→+200% (T-range) unconditional damage line. The Caster oscillation rotation (cast at 0,
   ride doubled regen back from −20) gives ~100% uptime *while using* the loop. One affix
   outbids entire class kits.
2. **Airborne multishot at Running.** +1.0 whole projectile (`MomentumChannelBonus`) is a
   full ×2 on every target including bosses, gated only on "be moving and jump" — no falloff,
   no per-target cap, and it stacks with sliding's +0.5 banked fraction. It is worth more than
   every damage node in the Swift trees combined.
3. **Mark's percent-of-max Charge on trash.** +1 per 2% of *target* max means one trash kill
   pays 50 Charge at any area level; two kills = a full ultimate. Conduit (free abilities,
   generation continues) then lets Patch/Mark spam pay for the *next* bar. On bosses it
   self-sustains just under the 18/s cap. It never breaks the cap, but it makes Support the
   only class whose ultimate cadence *improves* as content density rises.

### Three flattest / most boring numbers

1. **The More product.** No tree node authors a Damage More (the only `MorePercent` in the
   fallback trees is VW Long Dark's DoT More, which `AggregateStats` explicitly drops with a
   warning). The O3/O34 ceiling of 2.197 is defended by three layers of clamps and the live
   maximum anywhere is Overdrive's ×1.25 for 8 seconds. A whole rule system guarding an empty room.
2. **Crit.** Base 5%/1.5×; the affix ceilings (9 pts chance, 40 pts damage at T1) move the
   expected factor from 1.025 to ~1.14 in the best dedicated build. Crit cannot anchor a build;
   it is a rounding error next to one T6 Weapon Damage line.
3. **Caster spellcasting as damage.** Casts are priced in regen (6–12/s) while guns are priced
   in nothing: sustained spell throughput ≈ 4% of rifle DPS at 50 (Cleave: 36 base/3.3s vs
   rifle 240 base/s), even though both scale by the same ilvl curve (O35 works — the *rates*
   don't). Rot/Siphon matter as armor-strip and sustain; "Caster" as a damage identity is the
   rifle with extra steps.

---

## 7. Findings, ranked by playtest distortion

1. **Boss fights are 2–5× under the O18 band and shrinking.** Blended-armor boss TTK: ~12s at
   L10, ~10s at L25, 4.2–13.2s at L50 (best/worst build) vs O18's 20–45s. `w = g = 0.09` holds
   the *baseline* flat, but the multiplier band (1.26 → 2.21) plus conditionals plus weak-point
   play all land on top. Either the boss rank multiplier (25×0.35) or the band needs to move
   before boss pacing can be playtested honestly.
2. **The defense curve is inverted.** Hits-to-die falls 7.9 → 5.0 → 3.9 (trash) and 5.3 → 2.6
   (elite) from L10 to L50: monster damage grows ×8.5 while the Health affix grows ×3.2 and
   nothing else on gear defends (no dodge, no block, no DR-percent affix beyond PhysicalDR's
   small line). Riftplate is the only defensive scaling in the game. Playtesters at 50 will
   report "everything one-shots me" and they will be arithmetically correct.
3. **`ResourceDepleted` + accumulate-from-zero classes** (finding 6.1 above) — one Anomalous
   line that is passively best-in-slot for Tank/Gunsmith/Support and trivially loopable on
   Caster. Cheapest fix candidates: gate on "has spent recently" or exclude bank-at-zero classes.
4. **Weapon archetype, not class, decides TTK.** 2.1× base-DPS spread across the table with the
   rifle also owning a top weak-point mult; every class's best build holds a rifle. Melee
   coefficients (Rend/Cleave/Breach/Ground Zero) read the **per-pellet** base, so the shotgun —
   the thematic melee-adjacent gun — is the *worst* stat stick (10 vs the sniper's 72; a
   sniper-wielding Tank swings 7.2× harder). Two distortions from one table.
5. **Swift's airborne ×2** (finding 6.2) — will dominate any DPS reading taken from a Swift
   playtest; instrument grounded and airborne DPS separately or the whole class reads 50% high.
6. **Aberrant identity is invisible at mid-level.** At ilvl 25 specials roll T10 ≈ 12–15% of
   their headline values (Riftburn 15.9% of a 130% headline; Terminal Velocity 15.6/110). The
   "build-bender" moment the rarity exists for doesn't arrive until deep T-levels; the L25
   playtest will report Aberrants as ordinary items with an extra line.
7. **Momentum vs aiming/walking.** Walk (700) and every heavy-ADS state (sniper 0.50, MG 0.45
   aim-move multipliers → ≤550 cm/s) sit under the 750 generation threshold: a Swift who aims
   is locked out of their resource. Deliberate tension or trap — needs a ruling before the
   Marksman branch is playtested.
8. **The More/crit systems read as dead content** (findings 6-flat 1 & 2): three clamps, a
   global ceiling, an audit — protecting ×1.25-for-8s and a 1.067 crit factor. Skill-screen
   copy promising "More multipliers" will test as placebo.
9. **XP pacing**: ~24,300 XP to L10 against on-level trash paying 12–21 → on the order of 1,500
   trash-kill-equivalents solo (elites ×4, packs ×7 soften this). If the gym pretest expects
   L10 in one session, `BaseXpPerLevel = 240` (O2) is the knob.
10. **UNKNOWNs that block sharper numbers**: DoT tick intervals for Rot/Siphon status specs;
    Metronome stack cap; whether deployable (turret/mine) damage passes through the owner's
    `ApplyOutgoingModifiers`; SMG bleed stacking-vs-refresh policy; VW/Spellblade/Multispell
    node magnitudes (present in `BreakerProgressionLibrary.cpp`, not extracted here). None
    change the rankings above; all change second-decimal DPS.

*Every magnitude above inherits the code's own O2 PLACEHOLDER status. This document measures
the shape that ships at `f2f5a54`, not a balance verdict.*
