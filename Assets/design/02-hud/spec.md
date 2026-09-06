# Rior's Edge · HUD spec · 1920×1080

Origin top-left. All positions in px. Tokens per `01 Tokens and Type`. Positions never change between states; only content does. Safe gutter 40 px. No gradients, blur, glow or shadow anywhere; hatch = `repeating-linear-gradient(135deg, A 0 6px, B 6px 8px)` (flat stripes).

## Vitals — bottom-left, 480 wide at (40, 936)
| Element | Pos / size | Colour | State rules |
|---|---|---|---|
| Health value | (40, 936) · numeric 700 32/32 | sys-full #E3DFD2 | harm-full #D9402F under 20 % |
| Max value | after value, 8 gap · numeric 500 13/16 +0.08em | text-2 #B8B6AC | static |
| Shield layer | (40, 976) 480×6 · 1 px border-mid | fill text-2 #B8B6AC on bg-1 | drains instantly, recovers linear |
| Health bar | (40, 990) 480×16 · 1 px border-mid | fill sys-full on bg-1 | harm: fill drains 0 ms; chip (hatch harm-dim/harm-full 4/2 px) holds 400 ms, recovers 600 ms linear. 1 px bg-0 tick at 20 % |
| Resource track | (40, 1010) 480×8 · 1 px border-low | fill text-2; mark triangle 12×8 at fill edge; notch 1×14 at 70 % | banked: fill + mark go sys-full, 8×8 gold-full cell at right end. Five class behaviours on same footprint (see sheet) |

## Near-death frame — full screen
| Element | Pos / size | Colour | State rules |
|---|---|---|---|
| Edge frame | inset 0, border 8 px | harm-full | visible < 20 % health; border pulses 8→16→8 px over 1.6 s ease-in-out, loop |
| Corner brackets | 64×64, 4 px L-shapes at 24 px from each corner | harm-full | visible with frame, do not pulse |

## Abilities — bottom-centre, 232 wide at (844, 936), bottoms on y = 1024
| Element | Pos / size | Colour | State rules |
|---|---|---|---|
| Ability 1 | (844, 960) 64×64 · r 2 · 1 px border-mid | bg-1; top rail 2 px verb-move #6FC3E8; mark 16×16 at (24, 28); key numeric 700 13 at top-right 6/6 | READY: whole tile verb colour, mark + key bg-0. COOLDOWN: tile bg-1, panel-2 #393D2B fill drains bottom-up (height = remaining %), mark text-3, key text-2, no digits. LOCKED: hatch bg-2/panel-0, rail + mark border-low, key text-4 |
| Ultimate | (916, 936) 88×88 · r 2 | rail 4 px ult #A98BEA; mark 24×24 at (32, 36); key at 8/10 | charges with panel-2 fill rising; READY = whole tile ult-full, mark + key bg-0 |
| Ability 2 | (1012, 960) 64×64 | rail verb by kind (weapon = wpn-full #E8842B, heal = gold-full #E6B33A, taunt = harm-full, movement = verb-move) | as Ability 1 |

## Weapon — bottom-right, right edge x = 1880
| Element | Pos / size | Colour | State rules |
|---|---|---|---|
| Magazine | right-aligned, top 944 · numeric 700 48/48 | sys-full | wpn-full under 25 % of magazine |
| Reserve | left of magazine, 12 gap, baseline aligned · numeric 500 24/24 | text-2 | static |
| Ammo rail | (1640, 1000) 240×4 on border-low | rest: magazine fill in sys-full | reload: becomes progress 0→100 left→right in wpn-full, then snaps back to magazine fill |
| Weapon name | display 600 20 above magazine | text-1 | only on swap, 1.2 s, slides 16 px up along the rail axis and fades |

## Centre
| Element | Pos / size | Colour | State rules |
|---|---|---|---|
| Crosshair | 40×40 box at (940, 520); four ticks 2×12, gap 16 | sys-full | SPREAD: gap 16→40 with movement/fire, 60 ms out, 200 ms back. ADS: ticks collapse (80 ms) to 2×2 dot + 24 px ring 1 px |
| Hit | four diagonals 12×2 from tick corners | wpn-full | 80 ms |
| Kill | diagonals + 6×6 centre square | sys-full | 200 ms |
| Weak-point kill | diagonals 16×2 + 8×8 diamond | gold-full | 320 ms |
| Damage numbers | world-space at impact · numeric 700 | weapon wpn-full 32; crit gold-full 48; absorbed text-3 32 "0"; rift damage rift-hot #35E8FF 32 | pop 60 ms, settle 120 ms, rise 24 px over 700 ms, fade last 300 ms; crit ×1.5 size, +400 ms hold; aggregate per target per 250 ms |

## Periphery
| Element | Pos / size | Colour | State rules |
|---|---|---|---|
| Rift / zone name | (40, 40) · display 600 24/28 | sys-full | changes on zone |
| Wave cells | (40, 76) 16×8 cells, 4 gap | done text-2, current sys-full, upcoming border-low | count = waves in encounter |
| Countdown | after cells, 16 gap · numeric 500 16/16 | text-2 | to next wave; empty out of combat |
| Quest line | right edge 1880, top 40, max 320 wide · body 400 14/20 right-aligned | text-2 | one line |
| Interact / loot plate | world-space, 40 tall, bg-1, 1 px border-low, 4 px left rail | rail + tally (4×12 cells, 2 gap) in rarity colour; key tile 32×32 panel-2 r 4; name display 600 20 | rarity carried by rail AND tally; Unwritten adds 1 px teal-1 frame |
| Enemy bar (stage 3 owns) | world-space above body, 6 tall | sys-full on bg-1, border-mid | — |

## Not on the combat HUD
- XP and level: pause-screen header and the level-up banner (stage 4).
- Minimap: deleted. Enemy state is read off the enemy.
- Weapon name at rest, ability digits, resource state words, boss bar at top (lives on the boss).

## Key labels
Keyboard: numeric 700 13 in the tile corner. Gamepad, drawn geometry in sys-full: LB 20×8 bar; RB bar + 4×16 right cap; ultimate = both, 2 px apart; triggers = bar + 2 px underline; face buttons = 20 px ring 2 px with 4×4 dot at N/E/S/W; stick click = ring in ring.
