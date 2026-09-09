# Owner playtest: the opening hour

Five items from one session. Three landed, one was refused by its own design and returned as a different request, one is still not reproduced.

## The opening equip window (O263)

"I played for 20 minutes and got some items and cool stuff but I can't equip anything because my level." RequiredLevelFor was identity — an ilvl 7 drop asked for character level 7 — so the first hour's drops were inventory clutter. Item levels 1 to 10 now ask for level 1. The rule stays derived and clamped, so this is a floor moving and not a save migration; the requirement test now walks the whole window and pins its far edge, where item level 11 asks in full again.

## The jump-then-slide chain (O264)

"If you jump then slide you'll heavily keep your momentum when sprinting and it makes everything feel off."

The engine of it is BoostedSpeedCeiling. A landing left it untouched by design, and the tick's surviving branch is max(ceiling, current speed) — a ratchet that only ever rises while a direction is held. So sprint, jump, slide, jump compounded: each slide-jump re-armed the boost with a fresh grant, and no landing in between ever started its decay. A landing now latches the bleed. D1(a) still holds — the boost glides down over AboveCapDecaySeconds rather than vanishing, and an air dash still lands with its momentum — but the chain converges instead of ratcheting, because only a fresh grant re-arms it.

Motion cannot be photographed, so this was traced (`-BreakerMoveTrace`, Gym, `Saved/Logs/movetrace.log`). The scripted slide-jump at t=7.90 goes in at 1107.6 and out at 775.3, the airborne ceiling rises to 990 by t=8.13 and holds through the fall, and at the landing on t=8.83 the ceiling reads 0.0 — the latch running, and resolving to the sentinel because this chain happened to converge exactly at the 990 resting cap. LIMIT OF THIS TRACE, stated plainly: the script never lands with the ceiling ABOVE the resting cap, which is the case the report is actually about. The mechanism is demonstrated; the magnitude at a high landing is not, and needs a trace leg that dashes and then jumps.

Two adjacent findings are recorded at the site and not repaired, because one report moves one dial: bSlideOwnsLanding exempts a merely REQUESTED slide from the landing cost entirely, so crouch held in the air pays no toll at any fall speed when the stated intent is only a floor at SlideEntrySpeed; and GetMaxSpeed while sliding returns max(SprintSpeed * multiplier, Velocity.Size2D()), a cap that reads its own current speed.

## The level-up cue

"Could we add a sound and a visual for leveling up." The banner already existed — one 400x88 plate in a corner that also carries the objective line — and nothing in the game made a noise when the player grew.

A seventh audio verb: two stepped pure tones a fourth apart, D5 into A5, 0.55 s. It is the only cue in the bank that rises, the only one with no noise term, and the only one that steps rather than glides; every other verb either falls, carries grit, or sits under 200 Hz. Overridable with level_up.wav like the rest. The visual is a gold ground ring at the player's feet, strokes arriving around the circle over 0.14 s so it reads as a mark being made — O179 files gold as reward and puts self-anchored draws at the feet, where the camera is guaranteed not to stand in them. Both fire from the seam that already knew a level was gained, one cue per event and never per level, so two levels from one kill stay one banner and one sound.

Adding the voice exposed a real defect beside it: ApplyVolumeSettings iterated a hand-kept list of twelve components, so the new cue was routed nowhere and ignored the player's volume settings entirely. It now iterates the actor's own audio components, and the routing test still pins the roster by name and by count.

## Caster cooldowns — refused, and correctly

"Skills also need base cooldowns because there are none." Authored, built, and reverted inside the same cycle. "Mana IS the cooldown" is the Caster's defining rule, defended at four sites: two assertion loops in the ability tests, the census, and UBreakerCasterAbility's constructor, which nulls the cooldown effect class so an accidentally authored number cannot quietly grow one. Five authored cooldowns turned four tests red, which is the defence working exactly as designed.

The owner's answer on being shown this: Mana stays the cooldown, and what the spells actually need is a CAST TIME — they should not appear instantly. That is a different mechanism and it is not built. Cleave's AnimationLockSeconds is a post-effect lock on re-activation, not a pre-effect delay, and no Caster ability has either. It is on the desk as its own block with the rulings it needs first.

## The Act I chain — still not reproduced, now much narrower

The chain is reachable end to end through real visible choices in a real world. The Act I+II loop probe walks it: `dialogue KESS — FORGE KEEPER: What would heat the Forge? -> Quest.KessSalvage.Offered` fires immediately after `Quest.FirstContract.TurnedIn`, and the run reaches Quest.Breach with four cumulative Doctrine points.

The owner reports that choice was ABSENT on his screen. The probe reads GetVisibleChoices directly and says so itself — it is not a Slate input test. So every layer except the plate is now cleared, and the remaining suspect is presentation: that node offers six choices, and BuildFrame puts the body in an SScrollBox with no visible scrollbar and the "1 – N CHOOSE" footer inside the scrolling region, so a plate that overflows hides both the later choices and the count that would tell you they exist. Unconfirmed — the plate has not been photographed in that state, and no capture fixture seeds those flags.

## Suite

Build and full suite: 864 passing, 3 expected red, 0 unexpected. The three out-of-band bands are the pre-existing enumerated set.
