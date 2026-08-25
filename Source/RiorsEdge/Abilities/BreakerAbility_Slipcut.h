#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerAbility_Slipcut.generated.h"

// S1 Slipcut (Class-Kits §1.2, landed under O175). "0.4s window in which
// every weapon hit has its cadence cost halved (fires at 2x rate, consumes
// ammo normally). Ends early on reload. Rewards holding a full magazine into
// the window." The design's Swift starter alongside Skim (O176) and Frenzy's
// ignition — F7 Slipcut Mastery and the branch's cadence nodes rewrite it.
//
// The cadence rewrite reaches the weapon through ONE seam:
// UBreakerWeaponComponent::PushFireRateMultiplier, composed into the same
// GetFireRateMultiplier every fire-timing site reads, so the window can never
// speed the trigger and not the burst (or vice versa). "Consumes ammo
// normally" is free — the magazine debit sits in FireOnce, untouched by
// cadence. "Ends early on reload" binds the weapon's own reload-start
// broadcast; an instant reload (Cadence Break's snap) counts, because a
// reload happened.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Slipcut : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Slipcut();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Window.Swift. prefix: the HUD's family scan draws the bar for free.
    static FName WindowKey();
    // Key for the weapon's fire-rate multiplier stack.
    static FName CadenceKey();

    // Pure rule: the window's length. F7 Slipcut Mastery ("Slipcut's window
    // widens for each ability cooldown running"): +ExtensionPerCooldownSeconds
    // for each of the owner's ability cooldowns active at cast. Counted
    // BEFORE the commit, so Slipcut's own fresh cooldown never pays itself.
    static float WindowSeconds(float BaseSeconds, int32 ActiveCooldowns, bool bHasMastery);

    // O2 PLACEHOLDERS, both. The multiplier is §1.2 S1's own "fires at 2x
    // rate" — quoted, but the doc is deleted and no live ruling pins it, so
    // it keeps the placeholder banner until measured.
    static constexpr float CadenceMultiplier = 2.0f;
    // F7's figure from the deleted §1.3: 0.15s per running cooldown.
    static constexpr float ExtensionPerCooldownSeconds = 0.15f;

private:
    UFUNCTION() void HandleReloadChanged(bool bReloading);
    UFUNCTION() void HandleWindowEnded(FName Key);

    // Active Swift ability cooldowns on the owner, counted off the ASC's
    // granted cooldown tags — the same tags the cooldown GE carries.
    int32 CountActiveAbilityCooldowns() const;

    UBreakerWeaponComponent* FindWeapon() const;

    bool bDelegatesBound = false;
};
