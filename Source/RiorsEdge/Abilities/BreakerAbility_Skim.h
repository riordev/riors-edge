#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "BreakerAbility_Skim.generated.h"

// S3 Skim (Class-Kits §1.2). The proof ability for the Phase 0 chain: it
// grants from the loadout, binds to a slot, spends Momentum, starts a cooldown,
// and moves the character.
//
// Skim routes through UBreakerCharacterMovementComponent::TryRedirect: a pure
// rotation of existing horizontal velocity, no speed floor, no bonus, and no
// dash charge consumed (Class-Kits S3, "Not a dash ... It redirects").
//
// GAP: "usable airborne once per airtime" is not enforced yet — it needs the
// spec's ConsumeAirborneAction counter on the movement component, which K7
// Skim Discipline also raises to 2. Cost and cooldown bound the ability in the
// meantime.
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

    // O2 PLACEHOLDERS, all three.
    // The redirect alone reads as "nothing happened" (owner feedback): a short
    // burst makes the new run line visibly accelerate. It composes on the same
    // multiplicative stack as gear and Overdrive and expires on its own.
    static constexpr float BurstSpeedMultiplier = 1.25f;
    static constexpr float BurstSeconds = 0.8f;
    // Steeply down: the look angle nobody holds while running a line.
    static constexpr float HardStopPitchDegrees = -50.0f;
};
