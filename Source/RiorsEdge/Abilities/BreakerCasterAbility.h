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
//  3. OVERCAST IS REACHABLE (spec D8, landed). `ClassResourceFloor` on the
//     attribute set is 0 for every class; `UBreakerManaComponent` opens it to
//     the Overcast floor while the owner is a Caster, and
//     `UBreakerAttributeSet::PreAttributeChange` clamps ClassResource to
//     [Floor, Max]. Ability costs stay ordinary GameplayEffects — there is no
//     second, non-GAS spend path (spec D3) — so a cast now genuinely drives the
//     bank negative. CheckCost below is overridden to compare against the floor
//     rather than zero, and it REFUSES a cast that would breach the floor
//     rather than letting the clamp truncate the spend: a partial spend would
//     be a silent discount, which is worse for the design than a refused cast.
//     While the bank is below zero nothing may be cast at all, per Class-Kits
//     §2.1; the bank climbs back out on the Mana component's doubled
//     generation.
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

    // Pure rule: may this cast happen? Floor-aware (spec D8). A default floor
    // of 0 is exactly the strict rule this replaced.
    static bool CanCastAt(float CurrentMana, float Cost, float Floor = 0.0f);

    virtual float GetResourceCost() const override;
    // Spec D8: compare against ClassResourceFloor rather than zero. This is
    // also the first time a Caster ability's affordability rule is actually on
    // the GAS activation path — CanCastAt existed but nothing called it, so the
    // base class's strict rule was what ran.
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    UFUNCTION(BlueprintPure, Category="Abilities") UBreakerManaComponent* GetManaComponent() const;
};
