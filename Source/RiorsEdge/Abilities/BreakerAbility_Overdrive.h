#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "BreakerAbility_Overdrive.generated.h"

struct FBreakerAbilityVariant;

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
// GAPS (all outside Abilities/ and Movement/):
//  - The Redline floor still needs UBreakerMomentumComponent::PushMomentumFloor
//    (spec §4.7). Decay suspension and doubled generation are live through
//    PushLoopOverride.
//  - Bloodrhythm's per-hit refund and 1.5s no-hit early exit need SI-8's
//    OnHitDealt attacker-side event.
//  - Terminal Velocity needs PushDashChargeOverride / PushWallRideDurationOverride.
//  - Standing Wave needs PushRangeTreatmentOverride on the weapon component.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Overdrive : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Overdrive();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

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
};
