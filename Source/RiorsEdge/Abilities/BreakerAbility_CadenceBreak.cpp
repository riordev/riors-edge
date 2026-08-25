#include "Abilities/BreakerAbility_CadenceBreak.h"

#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Engine/World.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"

UBreakerAbility_CadenceBreak::UBreakerAbility_CadenceBreak()
{
    FallbackAbilityId = TEXT("Swift.CadenceBreak");
    // Spec §4.2: LocalPredicted — the reload completion and window start must
    // feel immediate on the autonomous proxy; the stacks themselves are a
    // server-side fact (HandleShot is authority-gated).
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_CadenceBreak.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Swift_CadenceBreak.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_CadenceBreak::WindowKey()
{
    // Spec §4.2 names the window Window.Swift.CadenceBreak.
    return TEXT("Window.Swift.CadenceBreak");
}

FName UBreakerAbility_CadenceBreak::ModifierKey()
{
    return TEXT("CadenceBreak.Flat");
}

int32 UBreakerAbility_CadenceBreak::NextStackCount(int32 CurrentStacks, bool bHit, bool bSameTarget, float SecondsSinceLastHit, bool bHasSecondWind)
{
    const int32 Current = FMath::Clamp(CurrentStacks, 0, MaxStacks);
    if (bHasSecondWind)
    {
        // F9, transcribed from the node text: "Cadence Break's stack no longer
        // breaks when you change targets. Only a full second without a hit
        // resets it." "Only" is load-bearing — the miss reset goes too; a miss
        // simply fails to advance the hit clock, and it is the clock that
        // resets. (Spec §4.2 task 5 says merely "skip the target-swap reset";
        // the node text and Class-Kits §1.3 F9 both say "only", and the design
        // doc is the authority.)
        if (!bHit)
        {
            return Current;
        }
        const bool bGapReset = SecondsSinceLastHit > SecondWindResetSeconds;
        return FMath::Min(bGapReset ? 1 : Current + 1, MaxStacks);
    }
    // Base rule (Class-Kits §1.2 S2): "resets on miss or target swap." The
    // swap's first hit is itself a consecutive hit on the new target, so a
    // swap resets to 1, not 0.
    if (!bHit)
    {
        return 0;
    }
    return FMath::Min(bSameTarget ? Current + 1 : 1, MaxStacks);
}

void UBreakerAbility_CadenceBreak::ClearStacks()
{
    Stacks = 0;
    StreakTarget = nullptr;
    LastHitTime = -1000.0;
}

void UBreakerAbility_CadenceBreak::HandleShot(const FBreakerShotResult& Shot)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !Character->HasAuthority())
    {
        return;
    }
    UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!State || !State->IsWindowActive(WindowKey()))
    {
        return;
    }
    // Hitscan only, the Momentum loop's own convention: a rocket's shot record
    // carries no pellets and is neither a hit nor a miss.
    if (Shot.GetPelletCount() <= 0)
    {
        return;
    }

    const UBreakerProgressionComponent* Progression = Character->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bHasSecondWind = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_SecondWind.GetTag());
    const UWorld* World = Character->GetWorld();
    const double Now = World ? World->GetTimeSeconds() : 0.0;

    const bool bSameTarget = Shot.bHit && Shot.HitActor && Shot.HitActor == StreakTarget.Get();
    Stacks = NextStackCount(Stacks, Shot.bHit, bSameTarget, static_cast<float>(Now - LastHitTime), bHasSecondWind);
    if (Shot.bHit)
    {
        StreakTarget = Shot.HitActor;
        LastHitTime = Now;
    }

    // The bonus rides the chain's FLAT lane (spec §4.2: "It must land in the
    // flat sum stage, before the additive Increased bucket ... the design note
    // explicitly warns against double-dipping with Damage Ramp"). More stays
    // 1.0. Expiry mirrors the window remainder as a safety net; the window's
    // end removes it deterministically either way.
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        if (Stacks > 0)
        {
            Combat->PushOutgoingModifier(ModifierKey(), Stacks * FlatDamagePerStack, 1.0f, State->GetWindowRemaining(WindowKey()));
        }
        else
        {
            Combat->RemoveOutgoingModifier(ModifierKey());
        }
    }
}

void UBreakerAbility_CadenceBreak::HandleWindowEnded(FName Key)
{
    if (Key != WindowKey())
    {
        return;
    }
    ClearStacks();
    if (const ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->RemoveOutgoingModifier(ModifierKey());
        }
    }
}

void UBreakerAbility_CadenceBreak::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // "Instantly completes the current reload" (§1.2 S2), through the weapon's
    // own reload path compressed to one frame — CompleteReloadImmediately
    // routes StartReload's gates and FinishReload's economy, so the snap and a
    // timed reload cannot disagree about what a reload pays. Authority-side,
    // like the ammo it rewrites. NOTE the ordering: a live Slipcut window ends
    // on reload START by design, so a Cadence Break cast mid-Slipcut trades
    // the cadence window for the magazine — that is a real choice, not a bug.
    if (Character->HasAuthority())
    {
        if (UBreakerWeaponComponent* Weapon = Character->FindComponentByClass<UBreakerWeaponComponent>())
        {
            Weapon->CompleteReloadImmediately();
        }
    }

    // Bound once per instance (InstancedPerActor, no BeginPlay of its own).
    if (!bDelegatesBound)
    {
        if (UBreakerWeaponComponent* Weapon = Character->FindComponentByClass<UBreakerWeaponComponent>())
        {
            Weapon->OnShot.AddDynamic(this, &UBreakerAbility_CadenceBreak::HandleShot);
        }
        if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
        {
            State->OnWindowEnded.AddDynamic(this, &UBreakerAbility_CadenceBreak::HandleWindowEnded);
        }
        bDelegatesBound = true;
    }

    // A re-cast refreshes rather than inherits: stacks earned under the last
    // window do not carry into this one (§1.2 S2 grants "a 3s state", not a
    // rolling one). StartWindow itself already replaces the remaining time.
    ClearStacks();
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        Combat->RemoveOutgoingModifier(ModifierKey());
    }

    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    const float Duration = Definition ? Definition->WindowDuration : 3.0f;   // Class-Kits §1.2 S2: 3s
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
