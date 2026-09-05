#pragma once

#include "CoreMinimal.h"

// The named-body fit: where an imported placeholder mesh sits on an enemy's
// capsule, derived from the mesh's own bounds and its own rig rather than
// authored per mesh. Pure maths, no actor and no world, so the rule is
// provable without a spawned enemy — the same split ABreakerNPC::ApplyBodyMesh
// does inline, extracted here because enemies apply it from three call paths
// (BeginPlay, the editor property, the Breaker.EnemyBody preview command) and
// a rule with three callers belongs in one tested place.
namespace BreakerEnemyBody
{
    struct FBreakerBodyFit
    {
        float Scale = 1.0f;
        FVector RelativeLocation = FVector::ZeroVector;
        FRotator RelativeRotation = FRotator::ZeroRotator;
    };

    // Which way a biped mesh faces, read from its own rig: a left bone and its
    // right-hand twin (shoulders, upper arms, upper legs) span the body's
    // RIGHT axis in mesh space, and in Unreal's frame (X forward, Y right,
    // Z up, left-handed) Cross(Right, Up) is forward — Cross(+Y, +Z) = +X.
    // The pair is flattened to the ground plane first so a slouched or
    // asymmetric ref pose cannot tilt the answer. A pair closer than a
    // centimetre spans nothing and returns Zero: the caller records the gap
    // rather than guessing an axis.
    constexpr double BodyBilateralMinimumSpanCm = 1.0; // O2 PLACEHOLDER

    inline FVector BodyForwardAxisFromBilateralBones(const FVector& LeftPos, const FVector& RightPos)
    {
        FVector Right = RightPos - LeftPos;
        Right.Z = 0.0;
        if (Right.SizeSquared() < BodyBilateralMinimumSpanCm * BodyBilateralMinimumSpanCm)
        {
            return FVector::ZeroVector;
        }
        Right.Normalize();
        return FVector::CrossProduct(Right, FVector::UpVector);
    }

    // The mesh fills the capsule's height exactly: an imported body reads as
    // the same combatant the primitives did, whatever units it was authored
    // in (Quaternius ships metres-ish; the blockout GLBs shipped 100x). The
    // mesh's own forward axis (BodyForwardAxisFromBilateralBones, or Zero when
    // the rig offers none) is yawed onto the actor's +X, so a body authored
    // facing +Y in Blender stands looking where the actor looks. The bounds
    // ORIGIN is cancelled at the fitted scale THROUGH that yaw — a relative
    // transform scales, then rotates, then translates, so a pivot off the
    // bounds centre turns with the mesh before the location lands — and the
    // bounds centre comes to rest on the capsule centre: an authored pivot at
    // the feet or a baked scene offset both come out standing in the right
    // place, whichever way the mesh was authored to face.
    inline FBreakerBodyFit FitBodyToCapsule(const FVector& BoundsOrigin,
                                            const FVector& BoundsExtent,
                                            const float CapsuleHalfHeight,
                                            const FVector& MeshForwardAxis = FVector::ZeroVector)
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
        if (!MeshForwardAxis.IsNearlyZero())
        {
            // The yaw that carries the mesh's forward onto +X: the negative of
            // the heading the axis already has. Pitch and roll stay zero — a
            // body stands on its feet whatever its rig's forward reads.
            Fit.RelativeRotation = FRotator(0.0f, -static_cast<float>(MeshForwardAxis.GetSafeNormal2D().Rotation().Yaw), 0.0f);
        }
        Fit.RelativeLocation = -Fit.RelativeRotation.RotateVector(BoundsOrigin * Fit.Scale);
        return Fit;
    }
}
