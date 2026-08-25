#include "Abilities/BreakerAbility_Sightline.h"

#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/World.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerAbility_Sightline::UBreakerAbility_Sightline()
{
    FallbackAbilityId = TEXT("Swift.Sightline");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Sightline.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Swift_Sightline.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Sightline::WindowKey()
{
    return TEXT("Window.Swift.Sightline");
}

FName UBreakerAbility_Sightline::ChannelKey()
{
    return TEXT("Sightline.Pierce");
}

void UBreakerAbility_Sightline::HandleShot(const FBreakerShotResult& Shot)
{
    // "NEXT shot": the first real hitscan discharge inside the window
    // consumes it. A rocket's shot record carries no pellets and is not a
    // discharge of the line this ability grants — the Momentum loop's own
    // convention.
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !Character->HasAuthority())
    {
        return;
    }
    if (Shot.GetPelletCount() <= 0)
    {
        return;
    }
    if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
    {
        if (State->IsWindowActive(WindowKey()))
        {
            // CloseWindow broadcasts OnWindowEnded, which pops the channel
            // bonus below — one teardown path for the shot, the timeout and
            // any future cancel.
            State->CloseWindow(WindowKey());
        }
    }
}

void UBreakerAbility_Sightline::HandleWindowEnded(FName Key)
{
    if (Key != WindowKey())
    {
        return;
    }
    if (const ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerWeaponComponent* Weapon = Character->FindComponentByClass<UBreakerWeaponComponent>())
        {
            Weapon->PopShotChannelBonus(ChannelKey());
        }
    }
}

void UBreakerAbility_Sightline::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerWeaponComponent* Weapon = Character ? Character->FindComponentByClass<UBreakerWeaponComponent>() : nullptr;
    if (!Character || !Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Bound once per instance (InstancedPerActor, no BeginPlay of its own).
    if (!bDelegatesBound)
    {
        Weapon->OnShot.AddDynamic(this, &UBreakerAbility_Sightline::HandleShot);
        if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
        {
            State->OnWindowEnded.AddDynamic(this, &UBreakerAbility_Sightline::HandleWindowEnded);
        }
        bDelegatesBound = true;
    }

    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    const float Duration = Definition ? Definition->WindowDuration : 2.0f;   // §1.2 S5: "within 2s"

    if (Character->HasAuthority())
    {
        // The granted rule, on the channel stack the tree's Pierce lanes and
        // Sidearm Rig already compose through — additive, so a build's own
        // pierce is not overwritten, merely irrelevant for one shot. Duration
        // is the lazy-expiry safety net; HandleShot/HandleWindowEnded is the
        // deterministic end.
        Weapon->PushShotChannelBonus(ChannelKey(), 0.0f, AllTargetsPierceCount, 0, 0, Duration);
        if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
        {
            State->StartWindow(WindowKey(), Duration);
        }
    }

    // The line, shown once at cast: a thin gold stroke down the aim
    // direction — gold is the weak-point/reward family (BreakerUIStyle), and
    // a pierce-everything shot is a weak-point promise. World-placed, so it
    // marks where the line WAS opened rather than tracking the aim; the
    // 2s window itself is the HUD bar's job. Figures O2 PLACEHOLDER.
    // (Server-vs-client caveat recorded once in BreakerEffectRenderer.h.)
    if (UWorld* World = Character->GetWorld())
    {
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            const FVector Eye = Character->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
            const FVector Aim = Character->GetControlRotation().Vector();
            BreakerFX::FEffectTiming LineTiming;
            LineTiming.DurationSeconds = 0.40f;
            LineTiming.FadeInSeconds = 0.05f;
            LineTiming.FadeOutSeconds = 0.30f;
            // Offset off the weapon side so the caster sees a LINE, not the
            // end-on dot a camera-origin stroke collapses to (the Lead
            // photograph's lesson, applied here by principle).
            const FVector Side = FVector::CrossProduct(Aim, FVector::UpVector).GetSafeNormal();
            Effects->AddStroke(Eye + Aim * 110.0f + Side * 25.0f - FVector(0.0f, 0.0f, 18.0f),
                Eye + Aim * 2200.0f, 2.5f, BreakerUI::Gold, 2.8f, LineTiming);
            Effects->AddGlow(Eye + Aim * 130.0f, 22.0f, BreakerUI::Gold, 3.0f, LineTiming);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
