# Prototype destinations — 2026-09-09

Red Basin and Station Zero now have real World packages and travel entries. Each has three fixed-level districts,18 mixed native guards,three physical recoverable supply rewards,two return gates,and a local map. Red Basin levels12/14/16; Station Zero24/26/28. Higher-level visitors do not raise ordinary enemies or cache item levels.

Native validation covers package availability,actual guarded-cache completion and normal equipment pickup,one-time rewards,ground continuity and standing capsule clearance,stable objective markers,return handler binding,safe arrival beyond detection range,and actual lethal damage followed by normal safe respawn. It does not establish encounter balance or human travel input feel.

Presentation uses existing dressing plus editable O2 primitive geometry. Red Basin has scorched crop rows,a ruined barn frame and an impact rim. Station Zero has laboratory benches,specimen gardens and a containment tower. These remain visibly rough blockouts with exposed outer floor edges,plain surfaces and repetitive cover. Campaign beats,distinct return Rifts,final terrain/materials/audio and human pacing remain open.

All four1920x1080 frames inspected for each region:
- Red Basin: seat/captures/red-basin-scenery-0737/Saved/Screenshots/breaker_00..03.png
- Station Zero: seat/captures/station-zero-scenery-0736/Saved/Screenshots/breaker_00..03.png

Capture root is C:/Users/Administrator/Documents/Codex/2026-09-07/read-agentsgpt-md-and-fully-familiarize. Explicit map launch with -BreakerAutoPlay=<Id> -BreakerCaptureTour -BreakerCaptureScenery -BreakerScreenshots=4 and a fresh absolute UserDir. Scenery mode freezes simulation after arrival; these are placement photographs,not combat evidence. Earlier live Red Basin capture caught unsafe origin arrival and death; safe start moved to shared approach and real respawn validated. Frozen tour initially retained the old camera cache; explicit cache refresh after each pawn teleport fixes the instrument. Station Zero's southwest bench and final low cover obstructed the diagonal approach; both were moved and original capsule assertions retained.

The photographs exposed an unrelated campaign instruction in the HUD. Prototype tracker now reads the local map's real cache objective. Supply prompts remain visually crowded at some views; focus/range/occlusion refinement is separate follow-up.

Final build/full suite847 passing,3 expected failures,0 unexpected. Two1280x720 frames in seat/captures/prototype-objective-0739 confirm the real 0/3 supply objective and its second instruction fit the tracker.

## Station Zero priority hunt

Station Zero now asks for the Containment Custodian in the final laboratory,
using the existing fixed-level28 Warden. The other guards and supply lockers
are optional. Its actual accepted death commits a distinct journal flag once;
ordinary enemy XP/loot pays normally, with no extra scripted economy reward.
Both existing travel gates remain available. Completion persists on revisits
and changes the objective to returning to Anchor13.

Native checks cover wrong-target refusal, real target death/reward, repeated
lethal refusal, one-time flag, native in-memory save/reload, marker retirement,
optional unopened caches and actual return-handler binding. They do not
simulate crossing maps or establish the encounter's human difficulty.
The quest registry intentionally grows49→50 flags; its explicit census assertion
was updated after the first full suite caught the stale count.

All four1280x720 frames inspected:
- seat/captures/station-hunt-hud-0917: two real HUD views; hunt and optional-cache lines fit.
- seat/captures/station-hunt-map-0918: two real map views; target marker, fixed-level detail and route footprint fit.

The overhead target remains the native Warden name; Containment Custodian is
its authored map identity. This is a solo destination beat, not a new main
campaign chapter or shared multiplayer quest system.

Station follow-up final build/census/full suite:855 passing,3 expected reds,0 unexpected.

## Red Basin recorder recovery and extraction

A specialized console in Burned Homestead now starts a carried survey-recorder
objective. Bring it to the Impact Basin extraction relay, then use either real
Anchor return gate. Two journal receipts persist both stages; this is not a
backpack item and does not mint additional XP, loot or currency. Fixed regional
guards and recoverable supply caches remain optional.

The authoritative action checks same world/destination, live player, range,
static visibility and current sequence stage. Normal nearby-NPC selection and
the focused prompt share eligibility. Duplicate and out-of-order claims refuse;
the two destination flags are separate from existing campaign rewards.

Independent review caught the original extraction console inside a solid rim.
Its district-relative Y moved-900→-650; native console/body and player approach
clearance plus visibility assertions verify the actual geometry. Quest census
now expects52 registered flags. Lifecycle coverage uses actual interactions,
native in-memory archive/reload at both steps, no duplicate economy delivery,
marker retirement and unchanged native return bindings. Cross-map travel and
human campaign pacing are not simulated by that test.

Four1280x720 screenshots inspected, all in the task workspace:
- seat/captures/basin-recovery-0935: two standing views; focused recovery prompt and objective fit.
- seat/captures/basin-extraction-0936: two standing views; actual prior recovery enabled the extraction prompt and changed objective.

Capture mode uses a fresh disposable UserDir, native nearby recovery action
for extraction readiness, actual floor/capsule placement and existing combat
freeze. No journal flag injection or artificial HUD. These are static UI/
placement checks. Simple console bodies, exposed horizons/empty extraction
backdrop and distant redundant TALK labels remain visible polish gaps.

Red Basin final build/census/full suite:856 passing,3 expected reds,0 unexpected.
