#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "Combat/BreakerStatusConsumption.h"
#include "BreakerAbility_Resonance.generated.h"

// C6 Resonance (Class-Kits §2.2, Ability-Implementation-Spec §5.6): 40 Mana,
// no cooldown. "Detonates every status currently on the target for a burst of
// damage, consuming them. Damage scales with the NUMBER of distinct status
// types, not their stacks — an explicit anti-stacking rule."
//
// This is the ability that makes status a build axis rather than a
// damage-over-time footnote, and it is the reason UBreakerStatusComponent grew
// a consumption verb at all. All the arithmetic lives in
// UBreakerStatusConsumption so the §2.7.5 bound (6 statuses is at most 2.2x
// 2 statuses) is provable with no world and no actor.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Resonance : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Resonance();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // O266's wind-up moved two things to the cast start, and they are the same
    // two things.
    //
    // THE REFUSAL. This ability has always declined to charge for a target
    // carrying nothing — "the whole ability is consume what is there, and there
    // is nothing there". With a wind-up that check sat AFTER the Mana was
    // spent, so a mis-aimed press became a 40 Mana fine. It runs here instead.
    //
    // THE SNAPSHOT, owner-ruled: the count is taken at cast start, like a
    // damage-over-time source snapshots its own. Without it the wind-up eats
    // the payload it is paid from — statuses tick down during the cast and
    // expire, so the same press that would have detonated six types detonates
    // four, and the ability gets weaker the longer it takes to fire. What is
    // CONSUMED is still whatever is live at the landing; only the count the
    // damage is scaled by is frozen.
    virtual bool PrepareCast() override;

private:
    // The trace and the count, in one place because the cast start and the
    // no-cast-time path must not be able to ask the question differently.
    bool AcquireDetonationTarget(AActor*& OutTarget, int32& OutDistinctTypes, int32& OutRefundable) const;

    TWeakObjectPtr<AActor> CastSnapshotTarget;
    int32 CastSnapshotDistinctTypes = 0;
    int32 CastSnapshotRefundable = 0;
    bool bCastSnapshotValid = false;

public:

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance") FBreakerDetonationParams Detonation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance") EBreakerDetonationCurve Curve = EBreakerDetonationCurve::Linear;
    // MS8's rewrite: do not consume, halve the remaining durations instead.
    // A data field rather than a branch, so the node flips one value.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance") bool bConsumeStatuses = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0", ClampMax="1")) float DurationScalarWhenNotConsuming {}; // O246: authored in Data/abilities.json.
    // MS5 Payment: Mana per distinct status consumed. Zero at base.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0")) float RefundManaPerStatus {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resonance", meta=(ClampMin="0")) float MaximumRangeCm {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, Category="Resonance|Mana") float PaymentRankOneManaPerStatus {}; // O246: authored in Data/abilities.json. // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, Category="Resonance|Mana") float PaymentRankTwoManaPerStatus {}; // O246: authored in Data/abilities.json. // O2 PLACEHOLDER
};
