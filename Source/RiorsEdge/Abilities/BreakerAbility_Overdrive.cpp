#include "Abilities/BreakerAbility_Overdrive.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerMomentumComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"

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

    // The ignition, drawn once: a violet burst — violet is the ultimates'
    // colour (BreakerUIStyle) — a body glow, a real light so the room
    // answers, and six radial strokes low at the feet. The 8s window itself
    // stays the HUD bar's job; a world aura pinned to the cast point would be
    // a lie three steps later on the game's fastest class. Figures O2
    // PLACEHOLDER. (Server-vs-client caveat in BreakerEffectRenderer.h.)
    if (UWorld* EffectWorld = Character->GetWorld())
    {
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(EffectWorld))
        {
            const FVector Centre = Character->GetActorLocation();
            const FVector Feet = Centre - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight() * 0.8f);
            BreakerFX::FEffectTiming BurstTiming;
            BurstTiming.DurationSeconds = 0.55f;
            BurstTiming.FadeInSeconds = 0.02f;
            BurstTiming.FadeOutSeconds = 0.40f;
            // The glow sits at the FEET, not the body centre: a primitive
            // wrapped around the camera paints the whole screen violet for
            // its lifetime — the probe's second photograph was a full-frame
            // wash — and an ignition should light the room, not blind the
            // player igniting it. The blink light stays at the body so the
            // surroundings still answer.
            Effects->AddGlow(Feet, 70.0f, BreakerUI::Violet, 3.6f, BurstTiming);
            Effects->AddBlinkLight(Centre, 650.0f, BreakerUI::Violet, 3600.0f, BurstTiming);
            for (int32 Index = 0; Index < 6; ++Index)
            {
                const FVector Out = FRotator(0.0f, 60.0f * Index, 0.0f).Vector();
                Effects->AddStroke(Feet + Out * 40.0f, Feet + Out * 150.0f, 4.5f, BreakerUI::Violet, 2.8f, BurstTiming, 0.03f * Index);
            }
        }
    }

    // ---- Keystone branches ---------------------------------------------
    // Spec D1: the non-parametric half of each rewrite is an honest named C++
    // branch guarded by the variant's tag. Each branch is one named condition.
    if (Variant.KeystoneTag == BreakerAbilityTags::Keystone_Swift_Bloodrhythm)
    {
        // Class-Kits F12: every weapon hit refunds Momentum, and the ultimate
        // ends immediately after HitTimeoutSeconds without a landed hit.
        //
        // This is the ONE variant for which the ability does not end at the
        // bottom of ActivateAbility: it has to outlive activation to hear
        // OnHitDealt and to run its own clock. The instancing policy is
        // InstancedPerActor (BreakerGameplayAbility), so `this` is a stable
        // listener.
        //
        // The timeout is a RESET one-shot timer rather than a poll, so the
        // resolution is exact and no polling interval has to be invented — an
        // invented interval would be an authored value, which O2 forbids.
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            BloodrhythmTimeoutSeconds = Variant.HitTimeoutSeconds;
            BoundCombat = Combat;
            Combat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Overdrive::HandleBloodrhythmHit);
            bBloodrhythmActive = true;
            ArmBloodrhythmTimeout();
            // The window's natural end also has to end the ABILITY, or a
            // Bloodrhythm Overdrive would linger active for up to one timeout
            // past its own expiry, still refunding Momentum after every entry
            // it pushed had already come down. Whichever of the two timers
            // fires first wins; the teardown is idempotent, so a second firing
            // is harmless.
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(
                    BloodrhythmWindowEndHandle, this, &UBreakerAbility_Overdrive::HandleBloodrhythmTimeout,
                    Duration, /*bLoop=*/false);
            }
        }
        else
        {
            // FEEL-FIRST / loud failure: without a combat component the refund
            // and the timeout both silently do not exist, and the player would
            // experience Bloodrhythm as a keystone that changed nothing. Say so.
            UE_LOG(LogTemp, Warning,
                TEXT("Overdrive/Bloodrhythm: no UBreakerCombatComponent on %s, so the per-hit refund and the no-hit ")
                TEXT("timeout are both inert. The keystone will read as doing nothing."), *GetNameSafe(Character));
        }
    }
    else if (Variant.KeystoneTag == BreakerAbilityTags::Keystone_Swift_TerminalVelocity)
    {
        // Class-Kits K12: unlimited dash charges and no wall-ride timer for the
        // duration. An availability rewrite, never a speed rewrite — Master 5.4
        // still forbids self-acceleration and wall rides still generate no
        // speed, which is why this variant's SpeedMultiplier is 1.0.
        //
        // "Unlimited dash charges" is expressed as a SUSPENDED COOLDOWN rather
        // than a charge pool, because O40(a) rules the single-directional-dash-
        // on-cooldown model FINAL — the multi-charge model it reads like is the
        // historical Godot one. Suspending the cooldown is what that sentence
        // means under the model the game actually ships. Both suspensions are
        // keyed to the window and self-expire, so there is no teardown path
        // that can strand a player with a permanent free dash.
        if (UBreakerCharacterMovementComponent* Movement = Character->GetBreakerMovement())
        {
            Movement->PushDashCooldownSuspension(WindowKey(), Duration);
            Movement->PushWallRideTimerSuspension(WindowKey(), Duration);
        }
    }
    else if (Variant.KeystoneTag == BreakerAbilityTags::Keystone_Swift_StandingWave)
    {
        // Class-Kits M12: Momentum freezes entirely — "no gain, no loss" — and
        // shots behave as if fired at point-blank.
        //
        // The freeze REPLACES the base row's loop override rather than adding a
        // second one: both are keyed to WindowKey(), so this push overwrites
        // the doubled-generation entry pushed above. That is the intent, and it
        // is why the freeze is applied here rather than by suppressing the base
        // push — the base row must stay exactly as authored for every other
        // variant. Decay stays suspended (no loss) and generation goes to zero
        // (no gain), which is the sentence stated as two flags.
        if (UBreakerMomentumComponent* Momentum = Character->FindComponentByClass<UBreakerMomentumComponent>())
        {
            Momentum->PushLoopOverride(WindowKey(), /*bSuspendDecay=*/true, /*GenerationMultiplier=*/0.0f, Duration);
        }
        // "Shots behave as if fired at point-blank regardless of distance."
        // The override makes the distance falloff resolve to 1.0 and touches
        // nothing else about the shot — spread, pellets, the trace and the
        // recoil/aim contract are all unchanged.
        //
        // GAP, recorded rather than invented: the same Class-Kits sentence also
        // says "projectile speed treatment", and the projectile path carries no
        // falloff or speed concept this can reach. The range half ships; the
        // projectile half does not exist to be wired.
        if (UBreakerWeaponComponent* Weapon = Character->FindComponentByClass<UBreakerWeaponComponent>())
        {
            Weapon->PushRangeTreatmentOverride(WindowKey(), Duration);
        }
    }

    // For every variant but Bloodrhythm the window is the ability's whole
    // lifetime: nothing ticks on the ability, every entry it pushed self-expires
    // on its own duration, so the ability ends immediately and the state
    // component owns the clock. Bloodrhythm is the exception and stays active,
    // because it has to hear hits and run its own timeout.
    if (!bBloodrhythmActive)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UBreakerAbility_Overdrive::HandleBloodrhythmHit(const FBreakerHitContext& Hit)
{
    if (!bBloodrhythmActive) return;

    // "Every weapon hit refunds Momentum." The refund goes through GrantMomentum
    // rather than the ordinary generation path deliberately: the generation
    // budget is a per-second cap on how fast fighting pays, and a keystone
    // refund that the cap could eat would be invisible on exactly the build
    // that earned it — Overdrive already doubles generation, so the cap is
    // where an aggressive Swift lives during the window.
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerMomentumComponent* Momentum = Character->FindComponentByClass<UBreakerMomentumComponent>())
        {
            Momentum->GrantMomentum(BloodrhythmRefundPerHit);
        }
    }

    // The clock restarts from THIS hit, which is what makes the rewrite an
    // aggression gate rather than a shorter window.
    ArmBloodrhythmTimeout();
}

void UBreakerAbility_Overdrive::ArmBloodrhythmTimeout()
{
    UWorld* World = GetWorld();
    if (!World || BloodrhythmTimeoutSeconds <= 0.0f) return;

    // SetTimer on an existing handle restarts it, so a hit during the window
    // resets rather than stacks.
    World->GetTimerManager().SetTimer(
        BloodrhythmTimeoutHandle, this, &UBreakerAbility_Overdrive::HandleBloodrhythmTimeout,
        BloodrhythmTimeoutSeconds, /*bLoop=*/false);
}

void UBreakerAbility_Overdrive::HandleBloodrhythmTimeout()
{
    // Class-Kits F12: the ultimate ENDS, it does not merely stop paying. Every
    // window entry has to come down early, because each was pushed with the
    // full duration and would otherwise outlive the ultimate that owns it —
    // the one teardown path in this ability that expiry does not cover for us.
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
        {
            State->CloseWindow(WindowKey());
        }
        if (UBreakerMomentumComponent* Momentum = Character->FindComponentByClass<UBreakerMomentumComponent>())
        {
            Momentum->PopLoopOverride(WindowKey());
        }
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->RemoveOutgoingModifier(OutgoingModifierKey());
        }
        if (UBreakerCharacterMovementComponent* Movement = Character->GetBreakerMovement())
        {
            Movement->PopSpeedMultiplier(WindowKey());
        }
    }

    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Overdrive::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // Unbind and disarm on EVERY exit, including cancellation and the natural
    // end of the window, not only the timeout path. An InstancedPerActor
    // ability is reused for the next cast, so a surviving binding would make
    // the SECOND Overdrive refund Momentum whether or not the player still
    // holds the keystone.
    if (bBloodrhythmActive)
    {
        bBloodrhythmActive = false;
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Overdrive::HandleBloodrhythmHit);
        }
        BoundCombat.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(BloodrhythmTimeoutHandle);
            World->GetTimerManager().ClearTimer(BloodrhythmWindowEndHandle);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
