#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "BreakerCasterAbility.generated.h"

class UBreakerManaComponent;

// Shared base for every Caster ability (Class-Kits §2.1).
//
// Two class-wide rules live here so no individual ability restates them:
//
//  1. NO COOLDOWNS. Mana *is* the cooldown. Caster definitions author no
//     cooldown tag and no cooldown seconds; the base's ApplyCooldown already
//     no-ops in that case, and this class asserts the intent in one place.
//  2. UNMAKE REWRITES THE PRICE. While the Unmake window is open, every
//     Caster ability costs its authored cost multiplied by the window's
//     payload — 0 for the base ultimate, 0.5 under the Long Dark keystone
//     (Class-Kits §2.2). CheckCost and ApplyCost both read GetResourceCost, so
//     the free window affects affordability and spend identically.
//
// OVERCAST IS NOT YET REACHABLE — read this before "fixing" it. Class-Kits
// §2.1 allows a Caster to cast down to −20 Mana. `UBreakerManaComponent`
// implements that bank faithfully, but `UBreakerAttributeSet::PreAttributeChange`
// still clamps ClassResource to [0, Max], and GAS costs are GameplayEffects,
// which go through exactly that clamp. So a GAS-routed overdraft silently
// resolves to "spend down to zero" — strictly *better* for the player than the
// design intends, which is worse than refusing. Until the `ClassResourceFloor`
// attribute from Ability-Implementation-Spec §1.8 (D8) exists, this class keeps
// the strict rule: a cast the bank cannot fully pay for is refused. When the
// floor lands, CanCastAt below becomes the floor-aware rule and nothing else
// changes. Do NOT add a second, non-GAS spend path (spec D3).
UCLASS(Abstract)
class RIORSEDGE_API UBreakerCasterAbility : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerCasterAbility();

    // The Unmake window key on the SI-9 state component. Shared, so the
    // ultimate and the abilities it discounts cannot drift apart.
    static FName UnmakeWindowKey();

    // Pure rule: the price of a cast given the authored cost and the live
    // Unmake scalar (1.0 when no window is open). Never negative.
    static float CostUnderWindow(float AuthoredCost, float WindowScalar);

    // Pure rule: may this cast happen? Strict today; floor-aware when D8 lands.
    static bool CanCastAt(float CurrentMana, float Cost);

    virtual float GetResourceCost() const override;

    UFUNCTION(BlueprintPure, Category="Abilities") UBreakerManaComponent* GetManaComponent() const;
};
