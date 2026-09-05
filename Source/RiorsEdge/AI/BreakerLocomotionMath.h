#pragma once

#include "CoreMinimal.h"

// Enemy locomotion, the rule without the world (NAV-1).
//
// An archetype's TickEngagedBehaviour answers "which way, how fast" every
// frame: the arrival ring, the ranged advance/hold/retreat band, the Warden's
// plant, the Skirmisher's relocation, the lunges — all of them are a direction
// and a speed scale. That stays the goal selector. Locomotion only decides HOW
// the direction is honoured: by steering straight along it, as every enemy
// did before the navmesh existed, or by asking the navmesh for a path to the
// target because the straight line is blocked.
//
// PATH ONLY WHEN THE CLOSING LINE IS BLOCKED. Steering is what the archetypes
// were tuned on — the melee weave, the strafe cadence, the retreat — and a
// path following component ignores per-frame input while it holds a path, so
// pathing everywhere would silently delete every lateral behaviour. The rule
// therefore paths in exactly one case: the behaviour is closing on its target,
// the target is further than the acceptance radius, and world geometry stands
// between them. The moment the line clears, steering resumes and the
// behaviour's full direction is honoured again.

enum class EBreakerLocomotionMode : uint8
{
    // No direction: decelerate and hold (the Warden's plant, a HELD body).
    Idle,
    // Closing on a target the straight line cannot reach: navmesh path.
    Path,
    // Every other direction: steer along it with a swept move.
    Steer,
};

namespace BreakerLocomotionMath
{
    // A direction counts as "closing on the target" inside this cone. 35
    // degrees admits the melee weave (a lateral sinusoid folded into the chase
    // vector) and the ranged advance, and excludes a strafe (90) and a retreat
    // (180). O2 PLACEHOLDER.
    constexpr float PathAlignCos = 0.81915f;   // cos 35
    // A chase re-plans when its goal has moved this far since the last path
    // request, so a strafing player does not cost a path per frame. O2
    // PLACEHOLDER.
    constexpr float ReplanDistanceCm = 150.0f;
    // The path hands back to the behaviour at this fraction of AttackRange, so
    // the arrival ring (AttackRange x ArrivalInnerRatio) is always the
    // behaviour's to govern, never the path's. O2 PLACEHOLDER.
    constexpr float AcceptanceRatio = 0.5f;

    inline EBreakerLocomotionMode ChooseMode(const FVector& Direction, const FVector& ToTarget,
        bool bHasTarget, bool bClosingLineBlocked, float DistanceToTarget, float AcceptanceRadius,
        float AlignCos = PathAlignCos)
    {
        if (Direction.IsNearlyZero()) return EBreakerLocomotionMode::Idle;
        if (!bHasTarget || !bClosingLineBlocked) return EBreakerLocomotionMode::Steer;
        if (DistanceToTarget <= AcceptanceRadius) return EBreakerLocomotionMode::Steer;
        const float Align = FVector::DotProduct(Direction.GetSafeNormal2D(), ToTarget.GetSafeNormal2D());
        return Align >= AlignCos ? EBreakerLocomotionMode::Path : EBreakerLocomotionMode::Steer;
    }

    // The goal override (NAV-2). An archetype that wants to be SOMEWHERE — a
    // firing flank the Lattice can see the player from — hands the mover a
    // goal instead of the player, and the alignment cone does not apply: the
    // direction it hands is the straight line to that goal, and the question
    // "is this direction closing on the target" was only ever there to keep
    // strafes and retreats out of the path follower. A goal is never a
    // strafe. So the rule is the closing rule with the cone removed: hold on
    // a zero direction, steer while the line is clear or the body has
    // arrived, path when world geometry stands between the body and the goal.
    inline EBreakerLocomotionMode ChooseGoalMode(const FVector& Direction, bool bGoalLineBlocked,
        float DistanceToGoal, float AcceptanceRadius)
    {
        if (Direction.IsNearlyZero()) return EBreakerLocomotionMode::Idle;
        if (!bGoalLineBlocked) return EBreakerLocomotionMode::Steer;
        if (DistanceToGoal <= AcceptanceRadius) return EBreakerLocomotionMode::Steer;
        return EBreakerLocomotionMode::Path;
    }

    inline bool ShouldReplan(const FVector& LastGoal, const FVector& NewGoal, bool bMoveIdle,
        float ReplanDistance = ReplanDistanceCm)
    {
        if (bMoveIdle) return true;
        return FVector::DistSquared2D(LastGoal, NewGoal) > ReplanDistance * ReplanDistance;
    }

    inline float MaxSpeed(float MoveSpeed, float SpeedScale)
    {
        return FMath::Max(0.0f, MoveSpeed * FMath::Max(0.0f, SpeedScale));
    }

    inline float AcceptanceRadius(float AttackRange, float Ratio = AcceptanceRatio)
    {
        return FMath::Max(0.0f, AttackRange * Ratio);
    }

    // The arrival angle (NAV-3). Two closers that both walk the line to the
    // player arrive on the same point of the arrival ring and stack; each
    // instead walks to a point on the ring rotated off its own bearing by a
    // signed offset, so a pair splits and comes in from two angles. The sign
    // is read from the per-enemy desync seed (PatrolPhase) with the same rule
    // the Lattice's strafe uses, so a pack seeded in steps of 1.3 alternates
    // and no second seed is authored.
    inline float ArrivalSign(float Phase)
    {
        return FMath::Fmod(FMath::Abs(Phase), 2.0f) < 1.0f ? 1.0f : -1.0f;
    }

    // A 150 cm body spacing (two capsule radii plus a hand's width) on the
    // 260 cm arrival ring subtends a chord of 2 x asin(75 / 260) = 33.5
    // degrees, so a pair split by +/-30 (60 between them) stands about two
    // spacings apart. O2 PLACEHOLDER.
    constexpr float ArrivalOffsetDeg = 30.0f;

    // The point on the ring of RingRadius around Target at the target-to-pawn
    // bearing, rotated by Sign x OffsetDeg about the vertical. Planar: the
    // goal keeps Target's Z and the ground snap owns the rest. A pawn standing
    // on the target has no bearing; the goal falls on the ring's +X point so
    // it is still a point on the ring rather than the target itself.
    inline FVector ArrivalGoal(const FVector& Target, const FVector& Pawn, float RingRadius, float Sign, float OffsetDeg)
    {
        FVector Bearing = (Pawn - Target).GetSafeNormal2D();
        if (Bearing.IsNearlyZero()) Bearing = FVector::ForwardVector;
        const FVector Rotated = FRotator(0.0f, Sign * OffsetDeg, 0.0f).RotateVector(Bearing);
        return FVector(Target.X + Rotated.X * RingRadius, Target.Y + Rotated.Y * RingRadius, Target.Z);
    }
}
