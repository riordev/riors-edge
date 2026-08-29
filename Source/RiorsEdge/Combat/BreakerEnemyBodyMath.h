#pragma once

#include "CoreMinimal.h"

// The named-body fit: where an imported placeholder mesh sits on an enemy's
// capsule, derived from the mesh's own bounds rather than authored per mesh.
// Pure maths, no actor and no world, so the rule is provable without a
// spawned enemy — the same split ABreakerNPC::ApplyBodyMesh does inline,
// extracted here because enemies apply it from three call paths (BeginPlay,
// the editor property, the Breaker.EnemyBody preview command) and a rule with
// three callers belongs in one tested place.
namespace BreakerEnemyBody
{
    struct FBreakerBodyFit
    {
        float Scale = 1.0f;
        FVector RelativeLocation = FVector::ZeroVector;
    };

    // The mesh fills the capsule's height exactly: an imported body reads as
    // the same combatant the primitives did, whatever units it was authored
    // in (Quaternius ships metres-ish; the blockout GLBs shipped 100x). The
    // bounds ORIGIN is cancelled at the fitted scale so the bounds centre
    // lands on the capsule centre — an authored pivot at the feet or at a
    // baked scene offset both come out standing in the right place.
    inline FBreakerBodyFit FitBodyToCapsule(const FVector& BoundsOrigin,
                                            const FVector& BoundsExtent,
                                            const float CapsuleHalfHeight)
    {
        FBreakerBodyFit Fit;
        // A degenerate mesh (empty bounds) keeps scale 1 rather than dividing
        // by zero into an infinity the renderer turns into a vanished body.
        const float MeshHalfHeight = static_cast<float>(BoundsExtent.Z);
        if (MeshHalfHeight <= UE_KINDA_SMALL_NUMBER || CapsuleHalfHeight <= UE_KINDA_SMALL_NUMBER)
        {
            return Fit;
        }
        Fit.Scale = CapsuleHalfHeight / MeshHalfHeight;
        Fit.RelativeLocation = -BoundsOrigin * Fit.Scale;
        return Fit;
    }
}
