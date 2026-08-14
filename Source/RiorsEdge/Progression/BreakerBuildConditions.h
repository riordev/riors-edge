#pragma once

#include "CoreMinimal.h"
#include "BreakerBuildConditions.generated.h"

// ---------------------------------------------------------------------------
// Conditional power, keyed off the movement pillar.
// ---------------------------------------------------------------------------
// Power-Curve.md §"Choices over accumulation": "This game's pillar is movement,
// so conditional damage that keys off the movement state (airborne, sliding,
// wall-riding, at Redline Momentum) is the obvious place for build identity to
// live, and it is currently absent."
//
// A condition is a PREDICATE ON LIVE STATE, not a stat. Both power layers own
// affix/node lines that only pay out while their condition holds, and both need
// the same answer at the same instant, so the evaluation lives here once rather
// than twice. This header consumes Movement/, Classes/ and Abilities/ — it never
// edits them; every read below is an existing public accessor.
//
// The composition rule is unchanged and still LOCKED: a conditional line is an
// ordinary Increased percentage that joins the ONE additive bucket for its stat
// while it is active and is absent while it is not. Nothing here is a second
// multiplier.
UENUM(BlueprintType)
enum class EBreakerBuildCondition : uint8
{
    // Always active. The default, so an ordinary line needs no annotation.
    Always,
    // UBreakerCharacterMovementComponent::IsFalling().
    Airborne,
    // UBreakerCharacterMovementComponent::IsSliding().
    Sliding,
    // UBreakerCharacterMovementComponent::IsWallRiding().
    WallRiding,
    // UBreakerMomentumComponent::GetMomentumState() == Redline. Swift only by
    // construction — the momentum loop is inert for every other class, so a
    // Redline line on a Tank is dead weight the player can see and avoid.
    Redline,
    // Within RecentDashSeconds of UBreakerCharacterMovementComponent
    // ::GetLastDashTime().
    RecentlyDashed,
    Count UMETA(Hidden)
};

// The set of conditions true for one actor at one instant. A bitmask rather
// than a bool array so a layer can cheaply notice "nothing changed" and skip a
// resubmission — the components re-derive their whole contribution on any
// change, which is only affordable because most frames change nothing.
struct RIORSEDGE_API FBreakerBuildConditionState
{
    static constexpr int32 ConditionCount = static_cast<int32>(EBreakerBuildCondition::Count);

    // O2 PLACEHOLDER: how long "recently dashed" lasts. Long enough to cover a
    // burst of fire after the dash, short enough that it is not just "on".
    static constexpr float RecentDashSeconds = 3.0f;

    FBreakerBuildConditionState() = default;

    // Always is true by definition, so an empty state still pays unconditional
    // lines. That is what lets one code path serve both.
    bool IsActive(EBreakerBuildCondition Condition) const
    {
        if (Condition == EBreakerBuildCondition::Always) return true;
        return (Mask & Bit(Condition)) != 0;
    }

    void Set(EBreakerBuildCondition Condition, bool bActive)
    {
        if (Condition == EBreakerBuildCondition::Always) return;
        if (bActive) Mask |= Bit(Condition);
        else Mask &= ~Bit(Condition);
    }

    bool operator==(const FBreakerBuildConditionState& Other) const { return Mask == Other.Mask; }
    bool operator!=(const FBreakerBuildConditionState& Other) const { return Mask != Other.Mask; }

    // Reads the live movement/momentum state off an actor. A null actor, or one
    // with none of those components, yields the empty state — which is exactly
    // what a test rig with no world wants, and why every conditional line is
    // simply absent there instead of erroring.
    static FBreakerBuildConditionState EvaluateForActor(const AActor* Actor);

    // Every condition on at once. Used by tests and by the tooltip path that
    // wants to show what a line would be worth when it is live.
    static FBreakerBuildConditionState All();

private:
    static uint8 Bit(EBreakerBuildCondition Condition) { return static_cast<uint8>(1u << static_cast<uint8>(Condition)); }

    uint8 Mask = 0;
};
