# Stage 6 · Core wheel and doctrine board (wireframe fidelity)

> Palette amendment applied: bg/panel steps olive (hue 118, chroma 0.03): bg-0 #0E1103 · bg-1 #151808 · bg-2 #1D2010 · panel-0 #262918 · panel-1 #2F3221 · panel-2 #393D2B. Text #EDEBE3 / #B8B6AC / #8E8D80 / #5C5C52; borders #454840 / #5C6056 / #8A8F84; sys #E3DFD2; gold #E6B33A; weapon #E8842B; harm #D9402F; movement verb #6FC3E8; ultimate #A98BEA; teal band #2FB8A6. Faces: Barlow Condensed 600/700 (OFL), Source Sans 3 400/600 (OFL), Sometype Mono 500/700 (OFL). Grid 4/8/12/16/24/40/64; radii 0/2/4; identity rail 4 px left; status rail 2 px top; hit target 44. Focus = 2 px border-high + one panel step. Disabled = hatch 135°, 6/2 px, text #5C5C52.

Geometry for the wheel is in 06-core-wheel-geometry.json (radii, angles, sizes, label and stick rules). World behind at 40% in gutters; board opaque bg-1.

## Shared
Header 0,0 1920×88; tabs CORE WHEEL / DOCTRINE BOARD; held/spent counter. Board (40, 152) 1424×888. Detail rail (1488, 152) 392 wide: held tally 16×8 cells, node name display 700 24, body 16/24, now/after numeric 16 with compare mark, BUY gold 44 tall, REFUND painted when nothing to refund. Zoom levels 1.0 and 0.6, around focused node.

## Node ladder
unspent panel-0 fill, 1 px border-mid, text-2 label · reachable panel-1, 2 px border-high, text-1 · spent sys-full · keystone 2 px border-mid, taken gold-full · refused/forfeit hatch. Held points read as intent: tally in rail, unspent ground never faded.

## Doctrine board
Adjacency lattice on a 96 px pitch; size = type: rule-rewrite prose cards 368×160, percentage one-liners 176×56; 32 px empty icon slots; links 2 px, inked when both ends spent.
