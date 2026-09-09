# Owner playtest: pace, Cleave reach, Core board, Act I chain

Four items came from one session of play. Three landed; the fourth is a defect this pass could not reproduce and is recorded with the evidence that exists.

## Base walk speed

"I feel so clunky." WalkSpeed moves 595 to 640, +7.6%. It is the only speed dial that moved: SprintSpeed stays at 990, which narrows the sprint gain from 1.66x to 1.55x, and the crouch cap and Momentum's ground gates are fractions of WalkSpeed and follow it without further edits. One dial per report is deliberate — the gravity history in the same constructor is the record of what happens when two move at once. The shipped-configuration test, the walk-speed footfall pin and the additive-composition comment were all restated at 640.

## Cleave reach

"Cleave's range is way too short." RangeCm moves 450 to 650 in Data/abilities.json. The comment it replaced cited a Class-Kits section that no longer exists in Docs/spec and a 3 m number the shipped data has not matched for some time; it now records the authored value and the reason.

A real geometric shortfall sits under the complaint and is recorded at the site rather than repaired: UBreakerMeleeSweep::IsInsideArc reads centre to centre. The overlap that feeds it is a sphere against the target's collision, so a body already touching the sphere is rejected when its actor origin sits past RangeCm — the swing lands short of what the player sees by both capsule radii, roughly 80 cm. Repairing it means the arc test taking a target radius, an API change across every future melee source. The authored range carries the difference until a second melee verb makes that change worth making.

The out-of-reach fixture in CleaveWorldOcclusion shipped as a literal 500 beside a 450 reach and went red the moment the authored number moved. It now derives its distance from the authored reach, which is what the assertion was always about.

## Core constellations board

"The skill tree is absolutely unreadable (the core one)." Confirmed from frames before and after, four each at 1920x1080 through the Anchor capture with -BreakerCaptureMenu=SKILLTREES -BreakerCaptureBoard=CORE.

Every text block on that canvas was gated behind `!bRoleLayout`, and the shipped Core tree is the role layout. The overview therefore drew 187 identical markers and named none of them: no wedge name, no sector name, no hub mark, and sector divisions stroked at one canvas unit — a fifth of a pixel at the fit zoom, so they were drawn and never arrived. The right-hand CONSTELLATIONS list was the only thing on the screen that said what any of it was.

The names are back on the role layout, authored in canvas units divided by the fit scale so they arrive at a fixed screen size. Twenty-two names on one radius leave 414 canvas units of arc each, which is an 87-pixel budget for CONSTITUTION; they alternate between two radii so each has twice that. Sector names sit in the 620-unit hub hole, CORE marks the centre, and the sector strokes scale with the fit. They are labels, not buttons: the markers already open their wedge on click and the list already offers a named button for each.

The focused single-constellation view is untouched — every added block is gated on `!bFocused`.

What the frames cannot show: the harness cannot move a mouse, so hover, the node-detail panel and wheel-zoom legibility are unverified. Node markers themselves are still small grey squares at the fit zoom; the atlas now says where you are, and whether the markers read at a glance is the owner's hands, not a screenshot.

## Act I chain after the Quartermaster turn-in

"The quest chain breaks and [doesn't] get started after you turn the first one in to the Quartermaster." Not reproduced, and not repaired.

The shipped data is intact. Walking Act1.Fernhall beat by beat against Data/quests.json, Data/dialogue.json and Data/missions.json: every beat's completion flag has an available dialogue choice that sets it, from the Quartermaster's Job through KessSalvage, Pattern and Deeper to Deeper.TurnedIn. The entry override after FirstContract.TurnedIn resolves to Done, whose forward choice reaches Start; Kess's Salvage offer requires exactly FirstContract.TurnedIn and is reachable from both Start and Returned. Every flag named in dialogue and missions is registered except the four synthesized Mission.<id>.Arrived names, which are correct. The reported screenshot shows the tracker had advanced to SPEAK TO THE KESS — FORGE KEEPER, which is the right beat.

So the break is in what happens at runtime, not in the graph, and the five-minute box closed without a candidate. The question that would settle it is in the desk.

## Suite

Build and full suite completed: 864 passing, 3 expected red, 0 unexpected. The three out-of-band bands (uncalled-generation, power-band-atcap, rewrite-impact) are the pre-existing enumerated set.
