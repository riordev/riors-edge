#include "Abilities/BreakerAbility_Overdrive.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"

UBreakerAbility_Overdrive::UBreakerAbility_Overdrive()
{
    FallbackAbilityId = TEXT("Swift.Overdrive");
    // Spec §4.7: an 8s global state change is not worth predicting, and a
    // mispredicted ultimate is the worst possible feel.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ultimate_Overdrive.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Swift_Overdrive.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Overdrive::WindowKey()
{
    return TEXT("Window.Swift.Overdrive");
}

FName UBreakerAbility_Overdrive::OutgoingModifierKey()
{
    return TEXT("Overdrive");
}

bool UBreakerAbility_Overdrive::MeetsUltimateThreshold(float CurrentResource, float Threshold)
{
    return Threshold <= 0.0f || CurrentResource >= Threshold;
}

void UBreakerAbility_Overdrive::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    // The threshold is the definition's cost — a full bar. Checked explicitly
    // before committing so the ultimate never half-fires on a partial bar even
    // if a future cost-reduction affix lowers what CheckCost demands: an
    // ultimate is all-or-nothing.
    const float Threshold = Definition ? Definition->ResourceCost : 100.0f;
    if (!Character || !MeetsUltimateThreshold(GetCurrentClassResource(), Threshold))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // CommitAbility spends the definition's cost through the shared SetByCaller
    // cost effect and applies no cooldown effect, because Overdrive authors no
    // cooldown at all: the cost is the cooldown (spec D3).
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ---- Spec D1 variant resolution -----------------------------------
    // The owner's own tag container is the only input. A keystone node grants
    // a passive infinite GE carrying its Keystone.Swift.* tag; removing the
    // node removes the GE, removes the tag, and reverts the ultimate with no
    // bespoke revert code.
    FGameplayTagContainer OwnerTags;
    if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
        ASC->GetOwnedGameplayTags(OwnerTags);
    }
    const FBreakerAbilityVariant Variant = Definition
        ? Definition->ResolveVariant(OwnerTags)
        : FBreakerAbilityVariant();
    const float Duration = Variant.WindowDuration > 0.0f
        ? Variant.WindowDuration
        : (Definition ? Definition->WindowDuration : 8.0f);

    // ---- Base effect ---------------------------------------------------
    // Three simultaneous state changes, all keyed to the same window and all
    // self-expiring, so there is no teardown path that can leave the player in
    // a permanent power state: the loop stops draining and pays double, the
    // run line speeds up, and shots land harder. GAP: the Redline floor still
    // needs PushMomentumFloor on the Momentum component.
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    if (UBreakerMomentumComponent* Momentum = Character->FindComponentByClass<UBreakerMomentumComponent>())
    {
        Momentum->PushLoopOverride(WindowKey(), /*bSuspendDecay=*/true, LoopGenerationMultiplier, Duration);
    }
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        // Expiry is the teardown: the modifier chain prunes itself, so nothing
        // has to survive the ability instance to pop it. RemoveOutgoingModifier
        // stays available for an early exit (Bloodrhythm's no-hit timeout).
        Combat->PushOutgoingModifier(OutgoingModifierKey(), /*FlatBonus=*/0.0f, OutgoingMoreMultiplier, Duration);
    }
    if (UBreakerCharacterMovementComponent* Movement = Character->GetBreakerMovement())
    {
        if (Variant.SpeedMultiplier > 1.0f)
        {
            // Composes multiplicatively with the gear and tree multipliers and
            // expires on its own; no teardown path to get wrong.
            Movement->PushSpeedMultiplier(WindowKey(), Variant.SpeedMultiplier, Duration);
        }
    }

    // ---- Keystone branches ---------------------------------------------
    // Spec D1: the non-parametric half of each rewrite is an honest named C++
    // branch guarded by the variant's tag. Each branch is one named condition.
    if (Variant.KeystoneTag == BreakerAbilityTags::Keystone_Swift_Bloodrhythm)
    {
        // Class-Kits F12: every weapon hit refunds 1 Momentum, and the ultimate
        // ends immediately after HitTimeoutSeconds without a landed hit.
        // GAP: both halves need SI-8's attacker-side OnHitDealt. The timeout
        // value is already resolved (Variant.HitTimeoutSeconds) and the SI-9
        // streak API is the intended bookkeeping for the "did I hit recently"
        // read, so this branch becomes a delegate binding, not new structure.
    }
    else if (Variant.KeystoneTag == BreakerAbilityTags::Keystone_Swift_TerminalVelocity)
    {
        // Class-Kits K12: unlimited dash charges and no wall-ride timer for the
        // duration. An availability rewrite, never a speed rewrite — Master 5.4
        // still forbids self-acceleration and wall rides still generate no
        // speed, which is why this variant's SpeedMultiplier is 1.0.
        // GAP: needs PushDashChargeOverride / PushWallRideDurationOverride on
        // the movement component (spec §4.7). Deliberately not invented here:
        // both need the dash-charge model that does not exist yet.
    }
    else if (Variant.KeystoneTag == BreakerAbilityTags::Keystone_Swift_StandingWave)
    {
        // Class-Kits M12: Momentum freezes entirely and shots behave as if
        // fired at point-blank. GAP: the freeze needs the Momentum loop
        // override; the range treatment needs
        // UBreakerWeaponComponent::PushRangeTreatmentOverride (Weapons/, owned
        // elsewhere).
    }

    // The window is the ability's whole lifetime here; nothing ticks on the
    // ability itself, so it ends immediately and the state component owns the
    // duration. Bloodrhythm's early exit becomes a real EndAbility once its
    // hit event exists, at which point this ability stays active for the window.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
