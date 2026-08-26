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

## The travel label and its prompt now say the same words (raised, not blocking)

`BreakerPlaytestHUD.cpp` line ~1742 calls `GetPromptLabel()` as ordered, which
removes the contradiction — the door no longer says TRAVEL over F ENTER RIFT.
Worth knowing what it leaves, because I was half-wrong about this earlier and
the correction matters: I said reusing the prompt getter would "print the verb
twice". It does — but it **already did** at every ordinary gate, which prints
TRAVEL over F TRAVEL today. The redundancy is pre-existing, not introduced, and
the one-line fix is a strict improvement either way.

- The NPC block beside it is **noun then verb**: `GetDisplayName()` over a
  literal `F TALK`. The travel block is now **verb then verb**.
- `ABreakerTravelPoint` already has a `DisplayName` UPROPERTY with no getter, so
  the noun exists and is simply unreachable from the HUD. A `GetDisplayName()`
  on that actor would restore the NPC idiom — which is what GROUND originally
  proposed before the cheaper fix was ruled.
- **Question:** leave it verb-over-verb, or ask GROUND for the name getter so
  travel points read like NPCs do? Purely presentational, nothing waits on it,
  and I would not open another lane's header for it without the ask.

