#include "Abilities/BreakerAbility_Slipcut.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Engine/World.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"
#include "Weapons/BreakerWeaponComponent.h"

UBreakerAbility_Slipcut::UBreakerAbility_Slipcut()
{
    FallbackAbilityId = TEXT("Swift.Slipcut");
    // The window must open the frame the key lands — a cadence verb on a
    // round trip reads as input lag (spec §4.3's argument).
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Slipcut.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Swift_Slipcut.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Slipcut::WindowKey()
{
    return TEXT("Window.Swift.Slipcut");
}

FName UBreakerAbility_Slipcut::CadenceKey()
{
    return TEXT("Slipcut.Cadence");
}

float UBreakerAbility_Slipcut::WindowSeconds(float BaseSeconds, int32 ActiveCooldowns, bool bHasMastery)
{
    if (!bHasMastery)
    {
        return BaseSeconds;
    }
    return BaseSeconds + ExtensionPerCooldownSeconds * FMath::Max(0, ActiveCooldowns);
}

int32 UBreakerAbility_Slipcut::CountActiveAbilityCooldowns() const
{
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const IAbilitySystemInterface* AbilityOwner = Cast<const IAbilitySystemInterface>(Character);
    const UAbilitySystemComponent* ASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
    if (!ASC)
    {
        return 0;
    }
    // The Swift cooldown vocabulary, enumerated from the native tag header —
    // the cooldown GE grants exactly these. Overdrive authors none (its cost
    // is its cooldown) so it is absent by construction, not by omission.
    const FGameplayTag CooldownTags[] = {
        BreakerAbilityTags::Cooldown_Class_Swift_Slipcut.GetTag(),
        BreakerAbilityTags::Cooldown_Class_Swift_Skim.GetTag(),
        BreakerAbilityTags::Cooldown_Class_Swift_Lead.GetTag(),
        BreakerAbilityTags::Cooldown_Class_Swift_CadenceBreak.GetTag(),
        BreakerAbilityTags::Cooldown_Class_Swift_HardStop.GetTag(),
        BreakerAbilityTags::Cooldown_Class_Swift_Sightline.GetTag(),
    };
    int32 Count = 0;
    for (const FGameplayTag& Tag : CooldownTags)
    {
        if (ASC->HasMatchingGameplayTag(Tag))
        {
            ++Count;
        }
    }
    return Count;
}

UBreakerWeaponComponent* UBreakerAbility_Slipcut::FindWeapon() const
{
    const ABreakerCharacter* Character = GetBreakerCharacter();
    return Character ? Character->FindComponentByClass<UBreakerWeaponComponent>() : nullptr;
}

void UBreakerAbility_Slipcut::HandleReloadChanged(bool bReloading)
{
    // "Ends early on reload" (§1.2 S1): the reload START is the end — the
    // player traded the rest of the window for the magazine. CloseWindow
    // broadcasts OnWindowEnded, so the teardown below runs exactly once
    // whichever way the window dies.
    if (!bReloading)
    {
        return;
    }
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !Character->HasAuthority())
    {
        return;
    }
    if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
    {
        if (State->IsWindowActive(WindowKey()))
        {
            State->CloseWindow(WindowKey());
        }
    }
}

void UBreakerAbility_Slipcut::HandleWindowEnded(FName Key)
{
    if (Key != WindowKey())
    {
        return;
    }
    if (UBreakerWeaponComponent* Weapon = FindWeapon())
    {
        // The explicit pop re-arms a held automatic trigger at true cadence;
        // the push's expiry is only the safety net behind this line.
        Weapon->PopFireRateMultiplier(CadenceKey());
    }
}

void UBreakerAbility_Slipcut::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UBreakerWeaponComponent* Weapon = Character ? Character->FindComponentByClass<UBreakerWeaponComponent>() : nullptr;
    if (!Character || !Weapon)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // F7's inputs, read BEFORE the commit so the fresh cooldown this cast is
    // about to start never counts toward its own window.
    const UBreakerProgressionComponent* Progression = Character->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bHasMastery = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SlipcutMastery.GetTag());
    const int32 ActiveCooldowns = CountActiveAbilityCooldowns();

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    const float BaseSeconds = Definition ? Definition->WindowDuration : 0.4f;   // §1.2 S1: 0.4s
    const float Duration = WindowSeconds(BaseSeconds, ActiveCooldowns, bHasMastery);

    // Bound once per instance (InstancedPerActor, no BeginPlay of its own).
    if (!bDelegatesBound)
    {
        Weapon->OnReloadChanged.AddDynamic(this, &UBreakerAbility_Slipcut::HandleReloadChanged);
        if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
        {
            State->OnWindowEnded.AddDynamic(this, &UBreakerAbility_Slipcut::HandleWindowEnded);
        }
        bDelegatesBound = true;
    }

    // Cadence is server fact (the fire timers live there); the window state
    // rides beside it so the HUD bar and the teardown share one clock.
    if (Character->HasAuthority())
    {
        // Duration doubles as the lazy-expiry safety net; the deterministic
        // end is HandleWindowEnded's explicit pop.
        Weapon->PushFireRateMultiplier(CadenceKey(), CadenceMultiplier, Duration);
        if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
        {
            State->StartWindow(WindowKey(), Duration);
        }
    }

    // The cast moment, drawn once: a cadence verb is a weapon moment, so it
    // wears the weapon family's orange (BreakerUIStyle), not movement cyan.
    // A window-length world aura is deliberately NOT drawn — a Swift at speed
    // leaves a world-placed primitive behind in a step, and the window state
    // is the HUD bar's job. Figures O2 PLACEHOLDER. (Server-vs-client caveat
    // recorded once in BreakerEffectRenderer.h.)
    if (UWorld* World = Character->GetWorld())
    {
        if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
        {
            const FVector Chest = Character->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
            const FVector Aim = Character->GetControlRotation().Vector();
            BreakerFX::FEffectTiming SnapTiming;
            SnapTiming.DurationSeconds = 0.22f;
            SnapTiming.FadeInSeconds = 0.0f;
            SnapTiming.FadeOutSeconds = 0.16f;
            Effects->AddGlow(Chest + Aim * 60.0f, 30.0f, BreakerUI::Orange, 3.0f, SnapTiming);
            // Two short rails bracketing the aim line: the cadence opening.
            const FVector Side = FVector::CrossProduct(Aim, FVector::UpVector).GetSafeNormal();
            Effects->AddStroke(Chest + Side * 35.0f + Aim * 20.0f, Chest + Side * 25.0f + Aim * 110.0f, 3.5f, BreakerUI::Orange, 2.6f, SnapTiming);
            Effects->AddStroke(Chest - Side * 35.0f + Aim * 20.0f, Chest - Side * 25.0f + Aim * 110.0f, 3.5f, BreakerUI::Orange, 2.6f, SnapTiming);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
