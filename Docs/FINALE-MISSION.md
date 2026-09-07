# The final reconstruction

After the Survivor rescue, the Researcher appears at Anchor. Their assignment
leads to the Stripped Earth: a former civic system dismantled for its parts.
Three finite Vestige formations defend a working fragment. Killing the guards
does not recover it; the player must reach and interact with the actual rig.

Returning to Anchor with the fragment lets the Researcher reconstruct the
route to the Winning Earth. This is an intact settlement with no enemy
population. The alternate self uses the player's mannequin body and an
unarmed idle, has no combat component, and must be met in that actual world.
Rior is not present.

After returning to Anchor, the reconstructed device offers SEAL or HOLD.
It refuses completion below level 50 and explains the requirement. The two
epilogues differ in text; rewards and existing Rift access are identical.
The device records one exclusive choice together with the completion flag
before notifying reward listeners or requesting persistence. Repeating or
reversing the choice cannot pay again.

## Progression

Four explicit benchmark identities span three acts: Act1.Fernhall,
Act2.Breach, Act3.Survivor and Act3.Finale. Each pays exactly two Doctrine
points. The finale brings the cumulative entitlement to eight, independently
of branch commitment. Gaining levels alone never pays Doctrine.

The mission uses actual destination-map checks for travel, a guarded physical
fragment interaction, and a guarded alternate-self meeting. No generic kill
counter or dialogue-only extraction grants later mission credit. The final
reward is two Exceptional items at item level 50 (O2 content tuning).

## Validation tools

`Scripts/ue-loop-probe.ps1 -Finale` starts with isolated empty saves and runs
the campaign through real dialogue, enemy deaths, travel and interactions.
It accelerates damage and relocates the test player. At the final device it
first verifies under-level refusal, then explicitly awards the XP needed for
level 50 to exercise completion and two subsequent map loads. That final XP
setup is a test fixture, not evidence of natural leveling duration or balance.
`-Photos` captures the actual authored spaces during the run.

The two map shells are created using Unreal's
`-run=BreakerCensus -CreateFinaleMaps`; existing map assets are never overwritten.
Runtime layouts remain in `BreakerFinaleEarthBuilder`, while text and ordered
mission data remain in the existing JSON files. Current world art is a blockout.
