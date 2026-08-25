#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "BreakerAbility_HardStop.generated.h"

// S4 Hard Stop (Class-Kits §1.2, landed under O175/O177). "Cancels all
// velocity instantly and grants 0.6s of Damage Reduction While Airborne
// treatment on the ground. The counter-intuitive Swift ability: dumping
// Momentum to stop is a real tactical option."
//
// Until O177 this verb lived inside Skim as a pitch-gated modal branch behind
// the K7 node — no slot, Skim's price, and it stole Skim's cast from anyone
// aiming down. It is now the designed thing: its own equippable ability with
// S4's own cost and cooldown, unlocked at the quartermaster like any
// non-starter. K10 Spend to Live's two halves ride HERE: the cast costs twice
// the Momentum, and the window becomes true immunity.
UCLASS()
class RIORSEDGE_API UBreakerAbility_HardStop : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_HardStop();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Key for the window's entry on the incoming-damage chain, and (with the
    // Window.Swift. prefix, below) the HUD's duration bar.
    static FName IncomingModifierKey();
    static FName WindowKey();

    // K10 Spend to Live, cost half. The shipped node text is the spec:
    // "it costs twice the Momentum" (Progression/BreakerProgressionLibrary.cpp,
    // Swift.Kinetic.SpendToLive). Now that Hard Stop owns S4's authored 30,
    // the doubling is the doc's own 30 -> 60.
    static float CostMultiplier(bool bHasSpendToLive);

    // The protective window, as a multiplier for the incoming-damage chain
    // (UBreakerCombatComponent::PushIncomingDamageModifier; 0.0 = immune).
    // Base: Class-Kits §1.2 S4, "0.6s of Damage Reduction While Airborne
    // treatment on the ground" — the affix does not exist in the item layer
    // yet, so the fraction below is an O2 PLACEHOLDER standing in for its
    // rolled value. With K10 Spend to Live: "Hard Stop's window becomes true
    // immunity" (node text; §1.4 K10 "full damage immunity for its 0.6s"),
    // which is the chain's own 0.0.
    static float IncomingMultiplier(bool bHasSpendToLive, float ReductionFraction);

    // Live cost: S4's authored 30, doubled under Spend to Live. CheckCost and
    // ApplyCost both read through this, so an unaffordable doubled cast is
    // refused rather than discounted.
    virtual float GetResourceCost() const override;

    // Class-Kits §1.2 S4 / §1.4 K10, quoted: the window is 0.6s in both
    // forms. Strictly shorter than the 6s cooldown, so two windows can never
    // overlap and the timed removal can never strip a newer window's entry.
    static constexpr float WindowSeconds = 0.6f;
    // O2 PLACEHOLDER: stands in for the Damage Reduction While Airborne affix
    // value (Class-Kits §1.2 S4), which has no row in Items/BreakerAffixLibrary
    // yet. Replaced by the equipped roll the day the affix exists.
    static constexpr float DamageReductionFraction = 0.30f;

private:
    bool OwnerHasSpendToLive() const;
};
