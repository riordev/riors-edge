# The map is fast travel

M opens the local map from anywhere, and the map now travels (O265). From any instance it offers exactly one destination, the hub; from the Anchor it offers the ordinary registry. Travel is refused in combat, in both directions, so it cannot be used to escape a fight.

The rule is pure and world-free in `UI/BreakerMapTravelRules.h`, proved on arrays rather than on a loaded level: an instance offers exactly one destination and it is the hub even when that instance's own registry would not have carried it; the hub offers the rest of the registry and never itself; the rule narrows by location and can never widen what the registry already refused; and a world with no authored hub offers nothing rather than a button that travels to None.

The key binding is the shape `I` and `C` already use — a toggled full-screen modal, legal while paused. Travel calls `HandleHubTravelSelected` directly, which is the same verb a travel point calls and the same one the death screen reaches through `ReturnToAnchor`, so the project still has exactly one travel path. That call is a declared crossing, GLASS into GROUND, recorded at the declaration: the map decides WHICH destinations it offers and never how travelling happens. Travel runs before `ResumeFromMenu`, matching the travel screen's own order — travel is legal while the menu holds the pause, and resuming first would unpause a world about to be torn down.

Combat uses the character's existing `IsInResourceCombat` window rather than a second notion of "in combat".

## What the frames show, and what they do not

Two 1920x1080 captures of the map screen. The first put the travel block at the bottom of the body and it pushed BACK off the plate entirely — the whole point of the screen is to leave, so travel moved above the canvas and the canvas came down from 620 to 520. The second frame fits: objective, TRAVEL, ANCHOR 13, canvas, legend, BACK, no scrollbar.

A HARNESS FINDING came out of this and is on the desk. `-BreakerCaptureMenu=` fires its frames on `Lvl_FrontEnd`, before `-BreakerAutoPlay` finishes travelling: the log carries "Skipping the title menu; travelling to Lvl_Anchor" and the Anchor world never loads in that run. The plate-capture path waits on `IsArrivalCoverUp`; the plain menu path does not. So both of these frames are the front end, which is correctly not the hub — the instance branch is photographed and the HUB BRANCH IS NOT. The rule behind it is unit-tested either way, but no frame yet shows the Anchor's own list, and that is the harness's fault rather than the screen's.

The map canvas is empty in both frames because nothing had been discovered yet. That is pre-existing and unrelated.

## What this cycle deliberately did not touch

The death screen was in the same batch and is not in this commit. It has no capture fixture, and per the finding above the menu capture path photographs the front end, so nobody has seen it in its rift state. Guessing at a screen nobody has looked at is how the unphotographable screens in this project shipped broken; the fixture comes first.

The "marked enemies appear before the quest" report turned out not to be a crediting bug — `NotifyEnemyKilled` already refuses progress unless the quest is Active. Those are the ordinary elite population the quest later refers to. The real defect underneath it is that NOTHING in the project repopulates, verified by search: no timer, no respawn, no repopulate path outside the gym wave driver. That is one gap producing three of the owner's complaints and it is the next system block.

## Suite

Build and full suite: 865 passing, 3 expected red, 0 unexpected.
