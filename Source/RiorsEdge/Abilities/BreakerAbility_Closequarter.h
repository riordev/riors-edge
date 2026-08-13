#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "BreakerAbility_Closequarter.generated.h"

// C2 Closequarter (Class-Kits §2.2, Ability-Implementation-Spec §5.2): 35 Mana,
// no cooldown. "Blink to the target under the crosshair within 12 m, arriving
// 2 m short of it. Not a dash and not a grapple — instantaneous, no travel, no
// tether, no velocity carried. Landing refunds 15 Mana if the target is at or
// below 40% health."
//
// Verb-compliance note (Class-Kits §6.3): this stays an ability occupying a
// loadout slot. It is not base kit and no node may grant it outside SB7.
//
// DEVIATION FROM SPEC: §5.2 asks for
// UBreakerCharacterMovementComponent::TryBlinkTo. The blink is done here with a
// swept SetActorLocation, which has the same contract the spec describes
// ("stops at the last non-penetrating position") without this agent editing
// Movement/. When a second blink consumer appears, lift BlinkTo onto the
// movement component unchanged.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Closequarter : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Closequarter();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Pure rule: the arrival point is StandoffCm short of the target along the
    // approach vector. Returns the caster's own location when the target is
    // already closer than the standoff — a blink must never push the player
    // backwards away from what they aimed at.
    UFUNCTION(BlueprintPure, Category="Closequarter")
    static FVector ArrivalPoint(const FVector& CasterLocation, const FVector& TargetLocation, float StandoffCm);

    // Pure rule: Class-Kits §2.2 C2, the refund gate. SB10 moves the threshold
    // to 100%, which is a data change to RefundHealthFraction, not a branch.
    UFUNCTION(BlueprintPure, Category="Closequarter")
    static bool ShouldRefund(float TargetHealthFraction, float Threshold);

    // Class-Kits §2.2 C2: 12 m reach, arriving 2 m short.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0")) float MaximumRangeCm = 1200.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0")) float StandoffCm = 200.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0", ClampMax="1")) float RefundHealthFraction = 0.4f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0")) float RefundMana = 15.0f;
};
