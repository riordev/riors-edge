#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "BreakerAbility_Lead.generated.h"

// S6 Lead (Class-Kits §1.2): 40 Momentum, 10s cooldown. "Marks the target under
// the crosshair for 6s. Shots that hit the mark from more than 25 m away are
// treated as weak-point hits regardless of impact location."
//
// GAP — the mark is currently *tracked*, not *consumed*. The spec's
// UBreakerMarkComponent (§4.6) does not exist, and the distance-gated
// weak-point forcing has to happen in the outgoing-damage builder, which lives
// under Combat/ and is owned elsewhere. The faithful part that is buildable
// here: the ability traces the target under the crosshair, records it, and
// opens a 6s window on the SI-9 state component, with the 25 m gate expressed
// as a pure rule (ShouldTreatAsWeakPoint) ready for the damage builder to call.
// When UBreakerMarkComponent lands, this ability applies a mark instead of
// holding the target itself, and the damage builder calls the same rule.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Lead : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Lead();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // The window key on the SI-9 state component.
    static FName WindowKey();

    // Pure rule, exposed for tests and for the eventual damage-builder call:
    // a hit on a marked target counts as a weak point only beyond the range
    // gate. The gate is what stops Lead being a free crit engine at close
    // quarters (Class-Kits §1.2 S6).
    static bool ShouldTreatAsWeakPoint(bool bTargetIsMarked, float DistanceCm, float MinimumRangeCm);

    UFUNCTION(BlueprintPure, Category="Abilities") AActor* GetMarkedTarget() const { return MarkedTarget.Get(); }

    // Class-Kits §1.2 S6: 25 m.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lead", meta=(ClampMin="0")) float MarkMinimumRangeCm = 2500.0f;
    // O2 PLACEHOLDER: no design doc states the targeting trace length. 12 km is
    // "as far as the player can see" for the gym, not a balance number.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Lead", meta=(ClampMin="0")) float MarkTraceDistanceCm = 12000.0f;

private:
    TWeakObjectPtr<AActor> MarkedTarget;
};
