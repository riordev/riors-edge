# Rior's Edge · Nameplates and boss bar spec

Scale s = 1.0 at ≤12 m, linear to 0.5 at ≥35 m, floor 0.5. Maximum draw 60 m. Plates hide with the body (head occluded = plate hidden); boss bar and name draw through occlusion. Beyond 60 m: Standard/Elite nothing; Champion a 4×4 sys-full contact dot at head height; Boss its bar at s = 0.5, no name.
Pixel floors: no plate under 5 px tall; fill inside borders ≥ 3 px; boss bar ≥ 12 px incl. 2 px border. Borders never scale (1 px, boss 2 px).

## Rank by silhouette
| Rank | Bar (s=1) | Bar (s=0.5) | Name | Extra geometry |
|---|---|---|---|---|
| Standard | 96×6, bg-1, 1 px border-mid, sys-full fill | 48×5 | display 600 16 text-2; dropped at 20 m | none |
| Elite | as Standard | as Standard | as Standard | halo disc under body: 2 px sys-full ellipse, width = body width × 1.6, height 28 → 14 |
| Champion | 160×8 | 80×5 | display 600 20 text-2; dropped at 20 m | two 10×10 sys-full diamonds 4 px off bar ends (6 px at floor) |
| Boss | 640×24, 2 px border-mid, world-space above body | 320×12 | display 700 32 teal-2 #6ADFC9; 20 at floor; never dropped ≤60 m | 3 phase marks 4×32 bg-0 with 1 px sys-full edges at 33.3 / 66.6 %; phase pips 24×6 (16×4) below: done text-2, current sys-full, upcoming border-low |

Plate sits 12·s px above the head. Column: marks row, name, bar; gaps 6·s px.

## Bar states
- Fill sys-full drains at 0 ms. Chip: harm hatch (harm-dim/harm-full 4/2 px) holds 400 ms, recovers 600 ms linear. Hidden at ≥25 m.
- Shield: 2 px text-2 line along the top of the fill, width = shield %. Hidden at ≥25 m.
- Health-band ticks deleted; the chip hatch supersedes the eight-band rule. Boss phase marks are independent of health bands.

## Names
Teal is boss-only. Every other name is text-2 #B8B6AC. No ELITE/CHAMPION label; no band-state text; no numbers.

## Modifier marks · 16 px cell, sys-full, drawn geometry, never coloured
Row above the name, 8·s px apart (min 4), order of application. Active state = 2 px sys-full underline 2 px below the cell (never a fill inversion). Stripped mark leaves in 100 ms along the row axis. Body geometry is the first tell, the plate mark the second; the halo is rank, not a modifier.
| Modifier | Shape |
|---|---|
| Warded | hollow square 12, 2 px stroke |
| Volatile | filled diamond 10 (rot 45°) |
| Fleetfoot | filled right-pointing triangle 12×14 |
| Anchored | inverted T: 12×3 base + 2×12 stem |
| Splitting | two horizontal bars 12×3, 6 px gap |
| Warding Aura | ring 14, 2 px stroke |
| Reflective | open chevron, 3 px stroke, apex up-left |
| Phasing | two vertical bars 3×12, 6 px gap |
| Cascading | three ascending steps 4×4, 4×8, 4×12 |
| Wakeful | hollow square 12 with 4 px centre dot |
Collision audit at 12 px: only one ring, one filled diamond, one triangle, one stem, one chevron, one stepped mass; the two squares differ by the centre dot; the two bar pairs differ by orientation.
