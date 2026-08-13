#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "BreakerAbility_Cleave.generated.h"

class UBreakerAttributeSet;

// C1 Cleave (Class-Kits §2.2, Ability-Implementation-Spec §5.1): 20 Mana, no
// cooldown. "Short forward melee arc, 3 m, physical damage scaled by weapon
// damage. Applies Bleed at a 100% base chance."
//
// The Caster's only melee verb, and the only melee damage submission in the
// project. Everything it needs beyond the shared ability base is the arc sweep
// (UBreakerMeleeSweep) and the existing status component.
//
// The animation lock is a real GAS lock, not a cosmetic delay: the ability
// stays active for AnimationLockSeconds and blocks its own re-activation
// through ActivationBlockedTags. That is what the Edgework keystone removes
// (Class-Kits §2.2), so the duration is variant-readable and never hardcoded
// at the call site.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Cleave : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Cleave();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Pure rule: the swing's base damage from the equipped weapon's damage.
    // "Scaled by weapon damage" means exactly that — an unarmed Caster swings
    // for the fallback, never for zero.
    UFUNCTION(BlueprintPure, Category="Cleave")
    static float SwingDamage(float WeaponDamage, float Coefficient, float UnarmedFallback);

    // Pure rule: Edgework removes the animation lock entirely (Class-Kits §2.2).
    UFUNCTION(BlueprintPure, Category="Cleave")
    static float AnimationLockFor(bool bHasEdgework, float AuthoredLockSeconds);

    static FName SwingWindowKey();

    // Class-Kits §2.2 C1: 3 m.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0")) float RangeCm = 300.0f;
    // O2 PLACEHOLDER: no design doc gives the base arc. SB8 Edge widens it to
    // 180, so the base must be narrower than that; 120 is a shape, not balance.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0", ClampMax="360")) float ArcDegrees = 120.0f;
    // O2 PLACEHOLDER: "scaled by weapon damage" with no coefficient authored.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0")) float WeaponDamageCoefficient = 1.5f;
    // O2 PLACEHOLDER: a Caster with no weapon component still has a melee verb.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0")) float UnarmedDamage = 20.0f;
    // O2 PLACEHOLDER: the lock is the melee risk. Class-Kits names it (Edgework
    // removes it) but never times it.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0")) float AnimationLockSeconds = 0.45f;
    // Class-Kits §2.2 C1: Bleed at a 100% base chance. The magnitude and
    // duration are O2 PLACEHOLDER — no doc supplies them for this application.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0")) float BleedDamagePerTick = 6.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0")) float BleedDuration = 4.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cleave", meta=(ClampMin="0.05")) float BleedTickInterval = 1.0f;

private:
    void ApplyCleaveBleed(AActor* Target, const UBreakerAttributeSet* SourceAttributes, int32 Salt) const;

    FTimerHandle LockTimer;
};
