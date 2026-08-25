#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerAbility_Siphon.generated.h"

class UBreakerCombatComponent;

// C4 Siphon (Class-Kits §2.2, Ability-Implementation-Spec §5.4): 30 Mana, no
// cooldown. "5s channel on one target: deals Void damage over time and heals
// the caster for a portion. Channel breaks on the caster taking damage above a
// threshold. The class's only self-heal and the branch's solo answer."
//
// The heal routes through UBreakerCombatComponent::ApplyHealing — the shared
// contract — and never writes the health attribute. That is what lets overheal
// reporting, healing-modifier affixes and Support's Charge loop see it.
//
// NOT EXTRACTED: the spec asks for a UBreakerGameplayAbility_Channel base.
// Siphon is the only channel in the project today, so the loop lives here; the
// pure rules below (the per-tick heal, the break test) are static and testable
// and are exactly what a base class would hold when a second channel appears.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Siphon : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Siphon();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // Pure rule: what one tick heals, from what one tick dealt. A PORTION of
    // damage dealt, not a flat number, so the heal follows the caster's
    // offensive scaling instead of becoming irrelevant at level 50.
    UFUNCTION(BlueprintPure, Category="Siphon")
    static float HealForTick(float DamageDealt, float LeechFraction);

    // Pure rule: does this incoming hit break the channel? VW6 Drain raises the
    // threshold, so the comparison is against a FRACTION of max health and the
    // fraction is a data row, not a constant.
    UFUNCTION(BlueprintPure, Category="Siphon")
    static bool ShouldBreakChannel(float HealthDamageTaken, float MaxHealth, float BreakThresholdFraction);

    static FName ChannelWindowKey();

    // Class-Kits §2.2 C4: a 5 s channel.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siphon", meta=(ClampMin="0")) float ChannelSeconds = 5.0f;
    // Everything below is O2 PLACEHOLDER: GAP [O2] in spec §5.4 says the damage
    // per tick, the tick interval, the heal portion and the break threshold are
    // all unspecified.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siphon", meta=(ClampMin="0.05")) float TickIntervalSeconds = 0.5f;   // O2 PLACEHOLDER
    // The item-level-1 number; each tick rides the equipped weapon's
    // item-level scalar (O35).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siphon", meta=(ClampMin="0")) float DamagePerTick = 14.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siphon", meta=(ClampMin="0", ClampMax="2")) float LeechFraction = 0.4f;   // O2 PLACEHOLDER
    // VW6 Drain moves this to 0.15 and then 0.30 — a data row, not a branch.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siphon", meta=(ClampMin="0", ClampMax="1")) float BreakThresholdFraction = 0.05f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siphon", meta=(ClampMin="0")) float MaximumRangeCm = 3000.0f;
    // The channel also breaks if the target walks out of this. A channel that
    // follows a target across the map is not what "on one target" means.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siphon", meta=(ClampMin="0")) float BreakDistanceCm = 3600.0f;

protected:
    // One channel tick: damage the target through the contract, heal the caster
    // through the contract. Public-ish for the timer; not part of the API.
    void TickChannel();
    void StopChannel();

    UFUNCTION() void HandleCasterDamaged(const FBreakerDamageResult& Result);

private:
    TWeakObjectPtr<AActor> ChannelTarget;
    TWeakObjectPtr<UBreakerCombatComponent> BoundCasterCombat;
    FTimerHandle ChannelTimer;
    FTimerHandle ChannelEndTimer;
    int32 TicksDelivered = 0;
    // The beam's effect handle (0 = none). Ended in StopChannel, which every
    // exit path funnels through.
    int32 BeamHandle = 0;
};
