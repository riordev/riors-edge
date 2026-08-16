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
// SB7 "Blink" (Class-Kits §2.3): with the node owned, "Closequarter may be
// cast with no target to blink 12 m in the aim direction." The untargeted cast
// reuses the targeted blink's machinery — same swept teleport, same
// zero-velocity arrival, same MaximumRangeCm (the node's 12 m IS C2's 12 m) —
// so the verb feels identical with and without a target. No target means no
// refund: the refund gate reads target health, and there is none.
//
// Edgework's Closequarter half (Class-Kits §2.2): "during Unmake, Closequarter
// has no range limit within line of sight." Gated on BOTH the keystone tag AND
// the live Unmake window — the tag alone is permanent from node purchase, and
// gating Cleave's half on the tag alone already shipped one bug (D10). The
// line trace itself is the line-of-sight check: an unlimited-range blink still
// needs the target under the crosshair with nothing in between.
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

    // Pure rule, SB7: an untargeted cast is legal only when nothing was under
    // the crosshair AND the Blink node is owned. With a target the targeted
    // path always wins — the node adds a fallback, it never replaces the verb.
    UFUNCTION(BlueprintPure, Category="Closequarter")
    static bool ShouldBlinkUntargeted(bool bTargetFound, bool bHasBlinkNode);

    // Pure rule, SB7: the untargeted destination is the full range along the
    // aim direction — where the targeted blink would have gone had a target
    // stood at maximum range (minus its standoff, which needs a target to be
    // measured from). A degenerate aim direction goes nowhere rather than
    // somewhere surprising.
    UFUNCTION(BlueprintPure, Category="Closequarter")
    static FVector UntargetedBlinkDestination(const FVector& CasterLocation, const FVector& AimDirection, float RangeCm);

    // Pure rule, Edgework: the trace range for this cast. Only the two-part
    // gate (keystone tag held AND Unmake window live) lifts the limit —
    // mirror of UBreakerAbility_Cleave::AnimationLockFor's combined flag.
    UFUNCTION(BlueprintPure, Category="Closequarter")
    static float EffectiveRangeCm(bool bEdgeworkDuringUnmake, float AuthoredRangeCm, float UnrestrictedRangeCm);

    // Class-Kits §2.2 C2: 12 m reach, arriving 2 m short.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0")) float MaximumRangeCm = 1200.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0")) float StandoffCm = 200.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0", ClampMax="1")) float RefundHealthFraction = 0.4f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0")) float RefundMana = 15.0f;

    // Edgework's "no range limit within line of sight". 1 km stands in for
    // unlimited: far past any playable sightline, so it reads as no limit at
    // all, while still bounding the trace. O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Closequarter", meta=(ClampMin="0")) float UnrestrictedRangeCm = 100000.0f;
};
