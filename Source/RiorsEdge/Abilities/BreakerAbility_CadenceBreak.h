#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerAbility_CadenceBreak.generated.h"

// S2 Cadence Break. AUTHORED AS ITS OWN ABILITY CLASS, not a rewrite of an
// existing Swift ability, because the design defines it as its own verb:
// Class-Kits §1.2 gives it its own kit row (S2, Frenzy, 35 Momentum, 8s,
// granted by F7 per §1.3), and Ability-Implementation-Spec §4.2 maps it to its
// own GA ("Cadence Break (Frenzy, granted by F7) ... LocalPredicted ...
// Window.Swift.CadenceBreak for 3s"). The fallback registry had no
// `Swift.CadenceBreak` row — the gap both Slipcut Mastery and Second Wind
// record in Progression/BreakerProgressionLibrary.cpp; the row now exists
// alongside this class.
//
// §1.2 S2, transcribed: "Instantly completes the current reload and grants a
// 3s state: each consecutive hit on the same target adds a stacking flat
// damage bonus (10 stacks max, resets on miss or target swap). Explicitly the
// *flat* bucket, not Increased — it must not double-dip with Damage Ramp."
//
// What is live here:
//  * The 3s window (SI-9 state component, key Window.Swift.CadenceBreak).
//  * The streak, driven off the weapon's own OnShot (spec §4.2: "OnShot fires
//    on misses too, so binding OnShot and checking !bHit covers the weapon
//    case"), with the stacking rule as a pure static.
//  * The flat bonus, through UBreakerCombatComponent::PushOutgoingModifier's
//    FlatBonus lane — the chain adds it to BaseDamage BEFORE the Increased
//    bucket, which is exactly the "flat, not Increased" clause. More stays 1.0.
//  * F9 Second Wind (Node_SecondWind, granted by Swift.Frenzy.SecondWind):
//    "Cadence Break's stack no longer breaks when you change targets. Only a
//    full second without a hit resets it" (node text; Class-Kits §1.3 F9).
//    Read per shot, so a respec changes the rule mid-window honestly.
//
//  * The reload half: "Instantly completes the current reload", through
//    UBreakerWeaponComponent::CompleteReloadImmediately (the §4.2 hook,
//    landed with O175) — the normal reload's gates and economy compressed to
//    one frame, authority-side.
//
// The unlock path is the quartermaster token (O176), not a node grant — the
// node ability-grant path is retiring (DECISIONS' open note records it with
// readers and no writers), so Slipcut Mastery's F7 grant clause stays a rule
// tag and the loadout entry is bought.
UCLASS()
class RIORSEDGE_API UBreakerAbility_CadenceBreak : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_CadenceBreak();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // The window key on the SI-9 state component (spec §4.2 names it).
    static FName WindowKey();
    // This ability's entry on the attacker's outgoing-modifier chain.
    static FName ModifierKey();

    // The stacking rule, pure (Class-Kits §1.2 S2 base; §1.3 F9 rewrite).
    //  Base:        a miss resets to 0; a target swap resets to 1 (the new
    //               target's first hit); a same-target hit increments.
    //  Second Wind: only a full second without a HIT resets (to 1, counting
    //               the hit that arrived after the gap); swaps and misses no
    //               longer reset — "Only a full second without a hit resets
    //               it" is the node's whole text and "only" is load-bearing.
    //  Both cap at MaxStacks.
    static int32 NextStackCount(int32 CurrentStacks, bool bHit, bool bSameTarget, float SecondsSinceLastHit, bool bHasSecondWind);

    UFUNCTION(BlueprintPure, Category="CadenceBreak") int32 GetStacks() const { return Stacks; }

    // Class-Kits §1.2 S2: "10 stacks max."
    static constexpr int32 MaxStacks = 10;
    // Class-Kits §1.3 F9: "a full second without a hit."
    static constexpr float SecondWindResetSeconds = 1.0f;
    // O2 PLACEHOLDER — spec §4.2 GAP: "the flat bonus per stack is not
    // specified. Structure is complete; the magnitude is a placeholder."
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="CadenceBreak", meta=(ClampMin="0")) float FlatDamagePerStack = 4.0f;   // O2 PLACEHOLDER

private:
    // Streak driver, bound once per instance to the weapon's OnShot.
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    // One teardown path: natural expiry and CloseWindow both land here.
    UFUNCTION() void HandleWindowEnded(FName Key);

    void ClearStacks();

    bool bDelegatesBound = false;
    int32 Stacks = 0;
    TWeakObjectPtr<AActor> StreakTarget;
    double LastHitTime = -1000.0;
};
