#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// INPUT MODE ARITHMETIC — world-free. The two rules the sprint and aim keys
// obey once the settings screen can make either a hold or a toggle, and the
// one rule the look axis obeys once aiming has its own sensitivity. No pawn,
// no input component, no weapon: ABreakerCharacter is the thin caller and
// RiorsEdge.Input.HoldToggle / RiorsEdge.Settings.ScopedSensitivity.AimOnly
// prove the rules here.
// ---------------------------------------------------------------------------
namespace BreakerInputMode
{
    // The next engaged state of a two-state verb (sprinting, aiming) after
    // one input edge.
    //   HOLD:   engaged exactly while the key is down — the state IS the edge.
    //   TOGGLE: a press flips the state; a release is ignored, so two presses
    //           return to where they started.
    inline bool NextEngaged(bool bEngaged, bool bPressedEdge, bool bToggle)
    {
        if (!bToggle)
        {
            return bPressedEdge;
        }
        return bPressedEdge ? !bEngaged : bEngaged;
    }

    // The look-axis gain: the base sensitivity, multiplied by the scoped
    // multiplier only while the sights are up. Not aiming is the identity
    // whatever the multiplier holds.
    inline float LookGain(float Sensitivity, float Scoped, bool bAiming)
    {
        return Sensitivity * (bAiming ? Scoped : 1.0f);
    }
}
