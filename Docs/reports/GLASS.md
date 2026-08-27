# GLASS

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## Weapon fire is ONE clip for every archetype — is the override mine to build?

The owner's complaint list opens with *"all of the sound is bad … does not sound
like real guns"*, and ORDERS splits it: the weapon's BEHAVIOUR is KIT's, **the
cue is GLASS's**. It is not in my numbered list, so I have not built anything.

The structural half of "does not sound like real guns" is measurable and it is
not the sample's quality: `PlayWeaponFire()` takes no argument, so **a sidearm, a
rifle and a shotgun fire the identical clip**. No amount of asset work fixes
archetype sameness.

- The fix is the shape ruling 2 already blessed for abilities, one level over:
  `weapon_fire_<archetype>.wav` -> `weapon_fire.wav` -> synth, resolved lazily
  and cached, exactly as `PlayAbilityCast` resolves per ability. No new verb, no
  generic `PlaySound`, no asset field — the archetype enum already exists.
- It costs the owner nothing until he authors a file, and it means the files he
  authors land somewhere rather than needing plumbing first.
- **Question:** is that mine to build now, or does an override dimension on an
  existing verb need its own ruling the way a new verb does? Ruling 2 covered
  abilities explicitly and I will not read it as covering weapons by analogy.

## Telegraph audio: the sweep exists, but it does not measure the layer I would add

ORDERS holds enemy telegraph audio until FIELD's sweep exists. It does now
(`d43bbb7`), so reporting what it does and does not settle rather than treating
the gate as lifted.

- **What it gives me:** the awake-set bound. `t = 1.14 + 0.0864 N + 0.002584 N^2`
  puts 60 fps at N=63 and 120 fps at N=38, so a telegraph system should assume
  the awake set is bounded well below 100 in practice, and a positioned voice per
  awake body needs pooling and a hard cap — the effect renderer's light shape.
- **What it does not give me, and this is the important half:** the sweep is
  explicitly HALF A FIGHT — no hit reactions, damage numbers, flashes, death
  effects or player fire. Audio lives in exactly the half that was not measured,
  so the sweep bounds the BODY COUNT a telegraph system must serve and says
  nothing about what a telegraph system COSTS.
- **Question:** does the hold lift on the sweep existing, or on a measurement
  that actually contains the cosmetic half? I would keep holding — designing
  against the body count alone is how it gets authored twice, which is the
  reason the hold exists.

## The reward is paid where the player cannot see it (GROUND's walk, reported not built)

GROUND walked Anchor -> Fernhall -> a rift door -> completion and found this on
my surface: **Riftglass is drawn on the Anchor's HUD and not on the combat
HUD**, but the payout fires inside the rift at completion. So the number a run
pays changes on a readout only visible after the player has left the run and
travelled home. The completion moment is the one moment the reward is about,
and it currently shows nothing about it.

- I have NOT built this. ORDERS item 1 says *do not invent the reward summary*,
  LEDGER owns what was paid, and my banner deliberately carries no figure.
  GROUND's observation does not change who owns the number — it strengthens the
  case that someone should show it.
- O168 put the broadcast at the latch so the reward reads "standing in the rift
  you just beat" rather than during a loading screen. Nothing reads there.
- **Question:** should the completion banner carry the payout? If yes, I need a
  seam from LEDGER stating what was paid — I will not recompute it, because two
  lanes deriving one number is how they come to disagree. If no, the answer is
  probably that Riftglass belongs on the combat HUD too, which is a social-trim
  decision and also not mine.

