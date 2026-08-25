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

    // Once/twice per airtime (Class-Kits §1.2 S3 base rule; §1.4 K7 raises it:
    // "Skim may be used twice per airtime instead of once"). Pure, so the
    // without-node ceiling of 1 is pinned alongside the bought one. K7's other
    // half — "Grants S4 Hard Stop" — no longer touches this file: Hard Stop is
    // its own ability (O177, UBreakerAbility_HardStop) and the pitch-gated
    // modal branch that used to stand in for it is retired.
    static int32 MaxAirborneUses(bool bHasSkimDiscipline);

    // O2 PLACEHOLDERS, both.
    // The redirect alone reads as "nothing happened" (owner feedback): a short
    // burst makes the new run line visibly accelerate. It composes on the same
    // multiplicative stack as gear and Overdrive and expires on its own.
    static constexpr float BurstSpeedMultiplier = 1.25f;
    static constexpr float BurstSeconds = 0.8f;

private:
    // Landing resets the airborne-use count; bound lazily on first activation
    // (the instance has no BeginPlay of its own).
    UFUNCTION() void HandleLanded(const FHitResult& Hit);

    int32 AirborneUsesThisAirtime = 0;
    bool bLandedResetBound = false;
};
