#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Engine/HitResult.h"
#include "BreakerAbility_Skim.generated.h"

// S3 Skim (Class-Kits §1.2). The proof ability for the Phase 0 chain: it
// grants from the loadout, binds to a slot, spends Momentum, starts a cooldown,
// and moves the character.
//
// Skim routes through UBreakerCharacterMovementComponent::TryRedirect: a pure
// rotation of existing horizontal velocity, no speed floor, no bonus, and no
// dash charge consumed (Class-Kits S3, "Not a dash ... It redirects").
//
// "Usable airborne once per airtime" (Class-Kits §1.2 S3) is enforced HERE
// rather than by the spec's ConsumeAirborneAction counter on the movement
// component (Ability-Implementation-Spec §4.3): the ability is InstancedPerActor,
// so the instance counts its own airborne activations and resets the count off
// ACharacter::LandedDelegate — no Movement/ edit, same observable rule. K7 Skim
// Discipline's second half raises the ceiling to two (Class-Kits §1.4 K7:
// "Skim may be used twice per airtime instead of once"). A refused airborne
// cast ends before CommitAbility, so it costs nothing and starts no cooldown.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Skim : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Skim();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Pure rule, exposed for tests: the horizontal impulse direction for a view
    // rotation. Returns a normalized, strictly horizontal vector; falls back to
    // world forward when the view is exactly vertical.
    static FVector HorizontalDirectionForView(const FRotator& ViewRotation);

    // Key for the post-redirect burst on the movement component's temporary
    // multiplier stack.
    static FName BurstKey();

    // Hard Stop (K7 Skim Discipline grants S4 Hard Stop: "cancels all velocity
    // instantly"). Until Hard Stop is its own equippable ability with its own
    // cost and cooldown, the node's verb is folded onto Skim: with the node
    // owned, aiming steeply down turns the redirect into a dead stop. One
    // named condition, no new input, and it reads as a real node payoff.
    static bool ShouldHardStop(bool bHasSkimDiscipline, float ViewPitchDegrees, float PitchThresholdDegrees);

    // Once/twice per airtime (Class-Kits §1.2 S3 base rule; §1.4 K7 raises it:
    // "Skim may be used twice per airtime instead of once"). Pure, so the
    // without-node ceiling of 1 is pinned alongside the bought one.
    static int32 MaxAirborneUses(bool bHasSkimDiscipline);

    // K10 Spend to Live, cost half. The shipped node text is the spec here:
    // "it costs twice the Momentum" (Progression/BreakerProgressionLibrary.cpp,
    // Swift.Kinetic.SpendToLive). Class-Kits §1.4 K10's absolute 30 -> 60 is
    // authored against S4 as its own ability; while Hard Stop rides Skim's
    // committed cost, the doubling is the half that transcribes. Applies only
    // to a cast that will actually resolve as Hard Stop — a plain redirect is
    // bit-identical with the node owned.
    static float HardStopCostMultiplier(bool bWouldHardStop, bool bHasSpendToLive);

    // Hard Stop's protective window, as a multiplier for the incoming-damage
    // chain (UBreakerCombatComponent::PushIncomingDamageModifier; 0.0 = immune).
    // Base: Class-Kits §1.2 S4, "0.6s of Damage Reduction While Airborne
    // treatment on the ground" — the affix does not exist in the item layer
    // yet, so the fraction below is an O2 PLACEHOLDER standing in for its
    // rolled value. With K10 Spend to Live: "Hard Stop's window becomes true
    // immunity" (node text; Class-Kits §1.4 K10 "full damage immunity for its
    // 0.6s"), which is the chain's own 0.0.
    static float HardStopIncomingMultiplier(bool bHasSpendToLive, float ReductionFraction);

    // Key for Hard Stop's entry on the incoming-damage chain.
    static FName HardStopWindowKey();

    // Live cost: Skim's authored cost, doubled when the cast will resolve as
    // Hard Stop under Spend to Live (see HardStopCostMultiplier). CheckCost
    // and ApplyCost both read through this, so an unaffordable Hard Stop is
    // refused rather than discounted.
    virtual float GetResourceCost() const override;

    // O2 PLACEHOLDERS, all three.
    // The redirect alone reads as "nothing happened" (owner feedback): a short
    // burst makes the new run line visibly accelerate. It composes on the same
    // multiplicative stack as gear and Overdrive and expires on its own.
    static constexpr float BurstSpeedMultiplier = 1.25f;
    static constexpr float BurstSeconds = 0.8f;
    // Steeply down: the look angle nobody holds while running a line.
    static constexpr float HardStopPitchDegrees = -50.0f;

    // Class-Kits §1.2 S4 / §1.4 K10, quoted: the window is 0.6s in both the
    // base and the Spend to Live form. Strictly shorter than Skim's 3s
    // cooldown, so two windows can never overlap and the timed removal below
    // can never strip a newer window's entry.
    static constexpr float HardStopWindowSeconds = 0.6f;
    // O2 PLACEHOLDER: stands in for the Damage Reduction While Airborne affix
    // value (Class-Kits §1.2 S4), which has no row in Items/BreakerAffixLibrary
    // yet. Replaced by the equipped roll the day the affix exists.
    static constexpr float HardStopDamageReductionFraction = 0.30f;

private:
    // Landing resets the airborne-use count; bound lazily on first activation
    // (the instance has no BeginPlay of its own).
    UFUNCTION() void HandleLanded(const FHitResult& Hit);

    // The cast would resolve as Hard Stop right now: node owned and the view
    // aimed steeply down. Shared by GetResourceCost and ActivateAbility so the
    // price checked is the price of the verb that actually runs.
    bool WouldResolveAsHardStop() const;

    int32 AirborneUsesThisAirtime = 0;
    bool bLandedResetBound = false;
};
