#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeAggregation.h"
#include "BreakerAttributeSet.generated.h"

#define BREAKER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class RIORSEDGE_API UBreakerAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UBreakerAttributeSet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Vitals") FGameplayAttributeData Health;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, Health)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Vitals") FGameplayAttributeData MaxHealth;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Shield, Category="Defense") FGameplayAttributeData Shield;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, Shield)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxShield, Category="Defense") FGameplayAttributeData MaxShield;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MaxShield)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor, Category="Defense") FGameplayAttributeData Armor;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, Armor)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ClassResource, Category="Resources") FGameplayAttributeData ClassResource;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, ClassResource)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxClassResource, Category="Resources") FGameplayAttributeData MaxClassResource;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MaxClassResource)
    // How far below zero the class resource may be driven (spec D8). ZERO for
    // every class and every character by default, which is exactly the old
    // [0, Max] clamp — Swift's Momentum, and every test written against it,
    // cannot tell this attribute exists. Only Caster opens it, to −20, so
    // Overcast is reachable at all; SB4/MS10 deepen it later. It is authored
    // (never negative-summed by gear), so it is deliberately NOT part of the
    // aggregated set — see the note on the aggregation block below.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ClassResourceFloor, Category="Resources") FGameplayAttributeData ClassResourceFloor;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, ClassResourceFloor)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalChance, Category="Offense") FGameplayAttributeData CriticalChance;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, CriticalChance)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CriticalMultiplier, Category="Offense") FGameplayAttributeData CriticalMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, CriticalMultiplier)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageMultiplier, Category="Offense") FGameplayAttributeData DamageMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, DamageMultiplier)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DamageOverTimeMultiplier, Category="Offense") FGameplayAttributeData DamageOverTimeMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, DamageOverTimeMultiplier)

    // The character's composed walk speed in cm/s. The movement component owns
    // the authored base (it is EditAnywhere there, not here) and publishes it
    // through SetAggregatedAttributeBase, so this attribute is the real number
    // rather than a parallel one that happens to be close.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MoveSpeed, Category="Movement") FGameplayAttributeData MoveSpeed;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, MoveSpeed)
    // Multiplier-shaped, base 1.0, exactly like DamageMultiplier. These three
    // exist so gear and tree movement percentages share ONE additive bucket
    // instead of being multiplied together in the movement component — the last
    // instance of the bug class the damage pass already fixed.
    // DashCooldownReduction is a DIVISOR: x1.20 means a 20% shorter cooldown.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_SlideSpeedMultiplier, Category="Movement") FGameplayAttributeData SlideSpeedMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, SlideSpeedMultiplier)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_AirControlMultiplier, Category="Movement") FGameplayAttributeData AirControlMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, AirControlMultiplier)
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DashCooldownReduction, Category="Movement") FGameplayAttributeData DashCooldownReduction;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, DashCooldownReduction)

    // Rounds per minute multiplier. Base 1.0. Consumed by the weapon
    // component's fire timing, so a Fire Rate affix changes cadence rather
    // than being a number on a card.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_FireRateMultiplier, Category="Weapon") FGameplayAttributeData FireRateMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, FireRateMultiplier)

    // Ability cost scale. Base 1.0; Resource Efficiency drives it down. Floored
    // in PreAttributeChange well above zero, because a cost of zero would make
    // every ability free and silently delete the resource loop that is the
    // entire ergonomic of the Caster class.
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ResourceCostMultiplier, Category="Resource") FGameplayAttributeData ResourceCostMultiplier;
    BREAKER_ATTRIBUTE_ACCESSORS(UBreakerAttributeSet, ResourceCostMultiplier)

    // --- Unified attribute application path -------------------------------
    // This attribute set is the ONE owner of the true base value for every
    // attribute in EBreakerAggregatedAttribute. Equipment and progression (and
    // anything added to EBreakerAttributeContributor later) submit a complete
    // contribution and never write those attributes directly; every submission
    // re-derives all of them from bases + all contributions, so order does not
    // matter and removal is exact. See BreakerAttributeAggregation.h.

    // Snapshots the currently authored values as the true bases. Idempotent:
    // whichever component runs first captures pristine values and every later
    // call is a no-op, which is precisely what stops one layer from baking the
    // other layer's bonus into "its" base.
    void CaptureAttributeBases();
    bool HasCapturedAttributeBases() const { return Aggregator.HasCapturedBases(); }

    // Replaces one layer's contribution and re-applies everything.
    void ApplyAttributeContribution(EBreakerAttributeContributor Contributor, const FBreakerAttributeContribution& Contribution);
    // Equivalent to submitting an empty contribution: the layer stops
    // contributing and the composed values return exactly to what they would
    // be had it never contributed at all.
    void ClearAttributeContribution(EBreakerAttributeContributor Contributor);

    // --- Class-resource floor (spec D8) -----------------------------------
    // ClassResourceFloor is deliberately outside the aggregator. The fold is
    // (Base + flat) * (1 + Increased) * More over the Equipment/Progression
    // contributors; with a base of 0 every percentage term is annihilated, so
    // "Increased floor" would silently do nothing, and no gear affix or skill
    // node authors a floor anyway — it is set by the class-resource loop
    // component that owns the mechanic (Mana for Caster), one writer, one
    // authored value. Aggregation is for attributes several layers bid on.
    //
    // Sets the floor (clamped at or below zero) and, when the floor is raised,
    // lifts a bank that is now below it. That is what makes a class change or a
    // respec unable to strand a character in permanent debt.
    void ApplyClassResourceFloor(float NewFloor);
    // Null-safe write of the bank itself, for the resource-loop components:
    // routes through the ability system in play and writes the attribute data
    // with the same clamp policy when there is none (a standalone test).
    // This is NOT an ability spend path — ability costs stay GameplayEffects
    // (spec D3); this is the generation/regen write the loop components have
    // always done, made testable.
    void ApplyClassResource(float NewValue);
    // Null-safe writes of the two vitals, for the same reason and by the same
    // route as ApplyClassResource above. Used by the healing path in
    // UBreakerCombatComponent so a heal is exercisable in automation without a
    // world and an ability system — the generated setters ensure() when there
    // is no owning ASC, which is what made the whole vitals path untestable.
    // NOT a damage path: damage still goes through ReceiveDamage, and nothing
    // outside the combat component may call these.
    void ApplyHealth(float NewValue);
    void ApplyShield(float NewValue);

    // Publishes an authored base that this class cannot know on its own.
    // MoveSpeed is the case that forced it: WalkSpeed is EditAnywhere on
    // UBreakerCharacterMovementComponent, so an attribute-set constant would go
    // stale the moment the owner retunes it — and a composed MoveSpeed that
    // disagrees with the speed the character actually walks at is precisely the
    // "attribute that lies to the player" failure mode.
    //
    // Captures first (idempotent), so a caller running before any contributor
    // cannot leave the other bases uncaptured, then overrides the one base and
    // re-derives everything.
    void SetAggregatedAttributeBase(EBreakerAggregatedAttribute Attribute, float Value);

    float GetAttributeBase(EBreakerAggregatedAttribute Attribute) const { return Aggregator.GetBase(Attribute); }
    float GetComposedAttribute(EBreakerAggregatedAttribute Attribute) const { return Aggregator.Compose(Attribute); }
    const FBreakerAttributeAggregator& GetAttributeAggregator() const { return Aggregator; }

protected:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_Shield(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxShield(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_ClassResource(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MaxClassResource(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_ClassResourceFloor(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_CriticalChance(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_CriticalMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_DamageMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_DamageOverTimeMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_SlideSpeedMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_AirControlMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_DashCooldownReduction(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_FireRateMultiplier(const FGameplayAttributeData& OldValue) const;
    UFUNCTION() void OnRep_ResourceCostMultiplier(const FGameplayAttributeData& OldValue) const;

private:
    FBreakerAttributeAggregator Aggregator;

    // Re-derives every aggregated attribute from the captured bases and the
    // current contributions. The only place those attributes are written by
    // the aggregation path.
    void RecomputeAggregatedAttributes();
    // Routes through the ability system when there is one, and writes the
    // attribute data directly (same clamp policy) when there is not, so the
    // aggregation is exercisable in tests without an ability system.
    void WriteAttributeValue(const FGameplayAttribute& Attribute, FGameplayAttributeData& Data, float NewValue);
    // Null-safe replacement for GetOwningAbilitySystemComponent(), which
    // CastChecked's the outer to an actor.
    UAbilitySystemComponent* FindOwningAbilitySystemSafe() const;
};
