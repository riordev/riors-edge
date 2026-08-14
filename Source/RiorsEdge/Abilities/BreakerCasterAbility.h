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

    // Pure rule: the FULL price composition, resource efficiency included.
    //
    //     cost = Authored * CostMultiplier * WindowScalar
    //
    // Order is irrelevant to the arithmetic (it is a product) and is stated
    // this way because the two factors mean different things: CostMultiplier is
    // the player's gear/tree efficiency, WindowScalar is Unmake rewriting the
    // price of the class. They MULTIPLY rather than fight, and the composition
    // is deliberately free of any division, so:
    //   * Unmake's 0 scalar makes a cast free no matter how efficient the
    //     player is — efficiency cannot make "free" cheaper than free, and
    //     nothing anywhere divides by a scalar that may be zero;
    //   * Long Dark's 0.5 scalar and a 20% efficiency roll read 0.4x, not
    //     0.5x-or-0.8x-whichever-ran-last.
    // Efficiency is floored (see MinimumResourceCostMultiplier) so no stack of
    // affixes can reach a free Caster by the gear route; only Unmake, a
    // deliberate ultimate, may set the price to zero.
    static float ComposeResourceCost(float AuthoredCost, float CostMultiplier, float WindowScalar);

    // Costs may be reduced, never eliminated, by the affix layer. Defence in
    // depth: the aggregated attribute is floored on its own side too, and a
    // cost of exactly zero would make Mana stop being the cooldown, which is
    // the one thing the whole class is built on.
    static constexpr float MinimumResourceCostMultiplier = 0.10f;

    // The live resource-efficiency multiplier for this ability's owner. Read
    // fresh on every call and never cached: the player re-gears mid-fight, and
    // a cached cost would quote a price the bank is not being charged.
    UFUNCTION(BlueprintPure, Category="Abilities") float GetResourceCostMultiplier() const;

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
