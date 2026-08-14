#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "BreakerAbility_Rot.generated.h"

class ABreakerZoneActor;
class UBreakerAttributeSet;

// C3 Rot (Class-Kits §2.2, Ability-Implementation-Spec §5.3): 25 Mana, no
// cooldown. "4 m radius zone at the aim point, 6s duration. Enemies inside take
// Poison and have their Armour reduced by a flat 40. Zones are the Void
// Whisperer's whole grammar."
//
// The ability is deliberately thin: everything durable about it lives in
// ABreakerZoneActor, because Support's Suppress, Gunsmith's Disruptor, every
// boss telegraph and every environmental hazard need the same volume. What is
// Rot's and only Rot's is the aim solve, the payload, and the anti-stack call.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Rot : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Rot();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Pure rule: where the puddle lands. A zone dropped at the point the trace
    // hit is right for a floor; a zone dropped at the far end of a trace that
    // hit NOTHING must still be placed somewhere the player expects, which is
    // the end of the reticle line and not the caster's feet.
    UFUNCTION(BlueprintPure, Category="Rot")
    static FVector AimPoint(const FVector& ViewLocation, const FVector& ViewDirection, float MaximumRangeCm, bool bTraceHit, const FVector& HitLocation);

    // Class-Kits §2.2 C3: 4 m radius, 6 s.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float RadiusCm = 400.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float DurationSeconds = 6.0f;
    // O2 PLACEHOLDER: the doc gives radius and duration and nothing else. The
    // cadence is a shape, not balance.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0.05")) float TickIntervalSeconds = 1.0f;
    // O2 PLACEHOLDER: Rot's own per-tick damage. Class-Kits describes Rot as a
    // Poison-and-armour zone rather than a damage zone, so this defaults to
    // zero and the damage arrives through the Poison it applies. Raise it in
    // the editor to make the puddle bite directly.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float ZoneDamagePerTick = 0.0f;
    // Class-Kits §2.2 C3: a FLAT 40. Flat and never percentage is the ruling —
    // it is what protects the boss armour cap (Master 7.10.5).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float FlatArmorReduction = 40.0f;
    // O2 PLACEHOLDER: the Poison payload. No doc supplies these. Per-tick
    // damage is the item-level-1 number and rides the weapon scalar (O35).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float PoisonDamagePerTick = 5.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float PoisonDuration = 4.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0.05")) float PoisonTickInterval = 1.0f;
    // How far out the puddle can be placed. O2 PLACEHOLDER.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float MaximumRangeCm = 2500.0f;
    // Vertical reach of the volume; see FBreakerZoneSpec::HalfHeightCm.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float HalfHeightCm = 250.0f;
};
