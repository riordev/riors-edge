# Stage 4 · Death beat and banners

> Palette amendment applied: bg/panel steps olive (hue 118, chroma 0.03): bg-0 #0E1103 · bg-1 #151808 · bg-2 #1D2010 · panel-0 #262918 · panel-1 #2F3221 · panel-2 #393D2B. Text #EDEBE3 / #B8B6AC / #8E8D80 / #5C5C52; borders #454840 / #5C6056 / #8A8F84; sys #E3DFD2; gold #E6B33A; weapon #E8842B; harm #D9402F; movement verb #6FC3E8; ultimate #A98BEA; teal band #2FB8A6. Faces: Barlow Condensed 600/700 (OFL), Source Sans 3 400/600 (OFL), Sometype Mono 500/700 (OFL). Grid 4/8/12/16/24/40/64; radii 0/2/4; identity rail 4 px left; status rail 2 px top; hit target 44. Focus = 2 px border-high + one panel step. Disabled = hatch 135°, 6/2 px, text #5C5C52.

## Death beat (killing hit → live input = 3.2 s)
0.0 hit · weapon lowers 0.0–0.3 · camera drops 0.6 m and tilts 12° toward ground 0.0–0.8 while frame desaturates to 0 · one low audio cue on the hard cut at 0.8 (asset unspecified; room audio continues under black) · black 0.8–2.0 · fade-in 2.0–2.4 with input live on the first visible frame at 2.0 · the extra 0.8 s (2.4–3.2) is the HUD line and the focused control settling before the player is expected to act; input is accepted from 2.0.

## Death screen
Two lines display 700 64/64 and body 20/28 at (96, 400). Controls at (96, 560): RETRY THE RIFT focused (gold #E6B33A, 56 tall, 400 wide), RETURN TO ANCHOR secondary panel-0. Variants: campaign (gear kept, unlimited); endgame (DEATHS REMAINING n OF m, tally cells 16×8); endgame-terminal (budget spent, rift closes, RETURN the only control, retry painted). Boss: line 2 reads "The encounter resets." World behind at 40% value.

## Banners
Wave clear: top centre (760, 96) 400×64, slides down 16 px 160 ms, holds 1.6 s, out up 120 ms. Rift complete: centre band (0, 440) full width 200 tall, slides from left, holds 2.4 s. Level up: right rail (1480, 200) 400×88, slides from right, holds 2.0 s. Priority rift-complete → level-up → wave-clear; arrivals staggered 300 ms; positions disjoint. Colours: sys ink on bg-1 plate, 4 px identity rail (system sys, reward gold for level up).
