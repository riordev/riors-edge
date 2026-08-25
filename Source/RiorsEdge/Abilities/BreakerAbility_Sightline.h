#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerAbility_Sightline.generated.h"

// S5 Sightline (Class-Kits §1.2, landed under O175). "Next shot fired within
// 2s pierces all targets in a line and cannot be blocked by cover-state
// enemies. Pierce here is a granted rule, distinct from the Pierce affix."
//
// The pierce grant rides the weapon's existing channel stack
// (PushShotChannelBonus, the Sidearm Rig seam): a very large count for one
// shot. "All targets" is honest about two limits the weapon rule keeps: the
// per-target 0.70 falloff still applies (the grant is a COUNT, not a damage
// rewrite), and world geometry still stops the round — all targets in a
// clear line, never through walls.
//
// RECORDED ABSENT, not faked (O175): the "cannot be blocked by cover-state
// enemies" clause. Nothing on the shot path consults a cover state — a
// Skirmisher's InCover is a position behind ordinary geometry, not a
// collision or damage rule — so there is no blocker for this clause to
// bypass. The one actor-attached shot blocker in the game is the Warden's
// shield; scoping the clause to it is the candidate when a ruling wants the
// sentence to pay.
//
// NAME NOTE: the Marksman node Swift.Marksman.Sightline currently grants a
// flat +2 Pierce as an explicit stand-in for this ability (its own comment
// says it comes out the day the grant goes in). Retiring the stand-in is a
// LEDGER edit, recorded in the O175 session report.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Sightline : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Sightline();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Window.Swift. prefix: the HUD's family scan draws the 2s bar for free.
    static FName WindowKey();
    // Key for the weapon's shot-channel stack.
    static FName ChannelKey();

    // O2 PLACEHOLDER: "all targets" as a count. 64 is unreachable in any
    // shipped arena (the falloff makes the deep tail negligible long before
    // it), chosen finite so the channel arithmetic stays ordinary integers.
    static constexpr int32 AllTargetsPierceCount = 64;

private:
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    UFUNCTION() void HandleWindowEnded(FName Key);

    bool bDelegatesBound = false;
};
