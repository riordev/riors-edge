#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Engine/TimerHandle.h"
#include "BreakerAbility_Overdrive.generated.h"

struct FBreakerAbilityVariant;
struct FBreakerHitContext;
class UBreakerCombatComponent;

// Swift's ultimate (Class-Kits §1.2): 100 Momentum, a full bar, and no
// cooldown — the cost *is* the cooldown. Base: for 8s Momentum does not decay
// and generation is doubled against a raised per-second cap, with the player
// locked at minimum Redline.
//
// Keystone rewrites follow spec D1: the three Swift keystones each grant a
// passive GE carrying one Keystone.Swift.* tag; this ability reads its own
// owner's tag container at activation, resolves the matching variant row from
// the ability definition, applies the row's parameters, and then runs a named
// C++ branch for the part of the rewrite that is not parametric.
//
// ALL THREE REWRITES ARE NOW LIVE AND REACHABLE. Until 2026-08-14 the three
// branches were empty stubs and only one of the three tags was granted by any
// node, so two of the rewrites could not be held by any character and the third
// resolved to nothing — the third-jump failure class, repeated. Overpressure
// and Culling now carry the Kinetic and Marksman tags (Progression/), and
// RiorsEdge.Abilities.KeystoneReachability fails the day that stops being true.
//
// REMAINING GAPS:
//  - The Redline floor still needs UBreakerMomentumComponent::PushMomentumFloor
//    (spec §4.7). Decay suspension and doubled generation are live through
//    PushLoopOverride.
//  - Standing Wave's "projectile speed treatment" half (Class-Kits M12) is not
//    built; the range half is. See the branch comment.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Overdrive : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Overdrive();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // The window key on the SI-9 state component, and the key the temporary
    // movement speed multiplier is pushed under.
    static FName WindowKey();

    // Pure rule, exposed for tests: an ultimate needs the whole bar. Separate
    // from the base IsAffordable so the "full bar" reading is explicit and can
    // diverge (Maximum Resource affixes raise the ceiling).
    static bool MeetsUltimateThreshold(float CurrentResource, float Threshold);

    // The key the outgoing damage modifier is pushed under on the owner's
    // combat component.
    static FName OutgoingModifierKey();

    // Class-Kits §1.2: "all Momentum generation is doubled".
    static constexpr float LoopGenerationMultiplier = 2.0f;
    // Class-Kits F12 quotes "refunds 1 Momentum" per hit. O2 PLACEHOLDER: the
    // magnitude is the design doc's, not this file's, and it is deliberately
    // granted OUTSIDE the generation budget (see the call site) so the cap
    // cannot silently eat the whole keystone.
    static constexpr float BloodrhythmRefundPerHit = 1.0f;   // O2 PLACEHOLDER
    // O2 PLACEHOLDER. The old "4th More" self-flag is RESOLVED by O34:
    // temporary ability windows ARE Mores and count within the ONE
    // aggregator-derived budget (FBreakerAttributeAggregator::
    // ComposedMoreCeiling), so this window competes with the three branch
    // keystones (F12/K12/M12) for the same headroom instead of stacking a
    // fourth on top. On a build already holding three tree Mores near the
    // ceiling, Overdrive buys little damage — that competition is the choice
    // O27 wants, not a defect. The value stays per-source legal (at or under
    // the shared single-More ceiling, asserted in the ability tests).
    static constexpr float OutgoingMoreMultiplier = 1.25f;

private:
    // Bloodrhythm (Class-Kits F12) is the only variant that outlives
    // ActivateAbility: it listens for hits and runs a resettable no-hit clock.
    UFUNCTION() void HandleBloodrhythmHit(const FBreakerHitContext& Hit);
    UFUNCTION() void HandleBloodrhythmTimeout();
    void ArmBloodrhythmTimeout();

    // Guards every Bloodrhythm-only path. The ability is InstancedPerActor and
    // therefore REUSED across casts, so this must be false for a plain Overdrive
    // even on a character who cast a Bloodrhythm one a moment ago.
    bool bBloodrhythmActive = false;
    float BloodrhythmTimeoutSeconds = 0.0f;
    FTimerHandle BloodrhythmTimeoutHandle;
    FTimerHandle BloodrhythmWindowEndHandle;
    // Weak, because the binding has to be removed against the SAME component it
    // was added to, and the character can be destroyed mid-window.
    TWeakObjectPtr<UBreakerCombatComponent> BoundCombat;
};
