# Bloodline ends on owner death

Bloodline now cancels its paid ability window immediately when its owner dies. Cleanup removes its own outgoing-hit and death listeners and clears the window timer; the hit callback also refuses dead owners. Revival cannot resume healing from the old cast. Living behavior and the authored gear-dependent payout are unchanged.

The native fixture uses an actual Tank, earned level-20 unlock token, ordinary ability equipment and unmodified generated LifeOnKill gloves. Grit is earned by actual Character proximity detection and the normal loop, with world/player/resource clocks advancing together and no healing or resource grants. It pays the actual Bloodline quote, observes healing on an accepted nonlethal rifle hit, kills the owner, restores vitals, and observes another accepted nonlethal rifle hit with no former payout or rearmed window. The ordinary target is level100 solely to survive the two observations; AI is disabled, so this is lifecycle acceptance, not encounter balance. A nonexistent combat-health accessor in the initial fixture was corrected to the target's actual ASC attributes.

The targeted runtime test passed. Full-suite results are reported in STATE. Exsanguinate's dead-hit refusal shares the guarded callback, but its separate paid Doctrine route was not exercised by this fixture. No art or audio change is claimed.
