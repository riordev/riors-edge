# Stage 1 · Tokens and type

> Palette amendment applied: bg/panel steps olive (hue 118, chroma 0.03): bg-0 #0E1103 · bg-1 #151808 · bg-2 #1D2010 · panel-0 #262918 · panel-1 #2F3221 · panel-2 #393D2B. Text #EDEBE3 / #B8B6AC / #8E8D80 / #5C5C52; borders #454840 / #5C6056 / #8A8F84; sys #E3DFD2; gold #E6B33A; weapon #E8842B; harm #D9402F; movement verb #6FC3E8; ultimate #A98BEA; teal band #2FB8A6. Faces: Barlow Condensed 600/700 (OFL), Source Sans 3 400/600 (OFL), Sometype Mono 500/700 (OFL). Grid 4/8/12/16/24/40/64; radii 0/2/4; identity rail 4 px left; status rail 2 px top; hit target 44. Focus = 2 px border-high + one panel step. Disabled = hatch 135°, 6/2 px, text #5C5C52.

## Amendment · screen tints (chroma 0.04, same L; panel steps only)
| tint | hue | bg-0 | bg-1 | bg-2 | panel-0 | panel-1 | panel-2 | screen |
|---|---|---|---|---|---|---|---|---|
| moss | 140 | #061404 | #0C1B09 | #142310 | #1C2C18 | #253521 | #30402B | Dialogue |
| clay | 45 | #1C0A02 | #241105 | #2D190C | #362213 | #402B1C | #4B3627 | Forge |
| ochre | 85 | #170E00 | #1F1503 | #271D0A | #302613 | #392F1C | #453A27 | Stash |
| slate | 250 | #05101F | #0B1727 | #131F2F | #1C2838 | #253141 | #303C4D | reserved (rooms only) |
| heather | 320 | #170919 | #1E1020 | #261828 | #2F2131 | #382A3A | #443546 | reserved |
| class verb (Swift) | 225 | #04121B | #0A1923 | #12212B | #1B2A34 | #24333D | #2F3E49 | Class select, follows selection |

Text, rails, accents, borders and the hatch keep olive values on tinted screens. Tints are for rooms only: Forge, Stash, Dialogue, Class select. HUD, inventory, nameplates, banners, menus, settings, trees and control sheets untinted. text-3 passes 4.5:1 on all tinted bg steps and panel-0; not on panel-1/2.

World behind modals: scene at 40% value (multiply 0.4, no blur), plates opaque. Applies to inventory, stash, settings, both trees, death. Stand-ins at 40%: sky #3E413E, ground #1F210A.
