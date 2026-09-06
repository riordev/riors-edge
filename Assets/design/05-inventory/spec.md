# Stage 5 · Item card, inventory, swap picker

> Palette amendment applied: bg/panel steps olive (hue 118, chroma 0.03): bg-0 #0E1103 · bg-1 #151808 · bg-2 #1D2010 · panel-0 #262918 · panel-1 #2F3221 · panel-2 #393D2B. Text #EDEBE3 / #B8B6AC / #8E8D80 / #5C5C52; borders #454840 / #5C6056 / #8A8F84; sys #E3DFD2; gold #E6B33A; weapon #E8842B; harm #D9402F; movement verb #6FC3E8; ultimate #A98BEA; teal band #2FB8A6. Faces: Barlow Condensed 600/700 (OFL), Source Sans 3 400/600 (OFL), Sometype Mono 500/700 (OFL). Grid 4/8/12/16/24/40/64; radii 0/2/4; identity rail 4 px left; status rail 2 px top; hit target 44. Focus = 2 px border-high + one panel step. Disabled = hatch 135°, 6/2 px, text #5C5C52.

Untinted (base olive, like the HUD). World behind at 40%.

## Layout (1920×1080)
Header 0,0 1920×88 bg-1, tabs 44 tall with 2 px sys top rail when active. Equipment column (40, 128) 400 wide, 8 slots 64 tall. Backpack (480, 128) 5×5 cells 160×128, gap 4, ruled 1 px border-low. Item card (1320, 128) 560 wide: name display 700 24, rarity tally 5 cells 8×8 + word in numeric 13, sections REWRITE (Aberrant/Unwritten) or SIGNATURE (named Legendary) → prefixes → suffixes, tier badge column 32 px, compare marks UP (triangle 12×8 sys) / DOWN (triangle text-3) / EQUAL (8 px square border-mid). Card follows hover on mouse, focus on pad; compares to same slot; weapons compare to Primary unless Secondary focused.

## Swap picker
Modal 960×400 centred, three candidate rows 96 tall, focused row border-high + panel-1, SWAP gold, TAKE OFF panel-0. Which piece a swap removes is the equipment component's rule; the UI offers, never decides. Limit counter 44 tall with rarity rail and tally. Modal in 160 ms slide 16 px along rail axis; out 100 ms.

## Controls (default / hover / pressed / focused / disabled)
fill panel-0 / panel-1 / bg-2 / panel-1 / hatch; border 1 border-low / 1 border-mid / 1 border-mid / 2 border-high / 1 border-low; ink text-2 / text-1 / text-1 / text-1 / #5C5C52. Primary: gold / gold+#6A5118 border / #6A5118 / gold+border-high / hatch. Pressed translateY 1 px.
