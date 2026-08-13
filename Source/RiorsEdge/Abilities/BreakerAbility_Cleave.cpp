#include "Abilities/BreakerAbility_Cleave.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerMeleeSweep.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

namespace
{
    // Salts the Bleed roll away from the damage roll so the two stay
    // independent while both remain reproducible on the server.
    constexpr uint32 BreakerCleaveBleedSalt = 0x51EAF00Du;
}

UBreakerAbility_Cleave::UBreakerAbility_Cleave()
{
    FallbackAbilityId = TEXT("Caster.Cleave");
    // Spec §5.1: it deals damage in a volume, so it never runs on a client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Cleave.GetTag());
    // The animation lock IS the self-block: while the ability is still active
    // it owns State.Ability.Cleave, and that tag blocks its own re-activation.
    ActivationBlockedTags.AddTag(BreakerAbilityTags::State_Ability_Cleave.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Cleave::SwingWindowKey()
{
    return TEXT("Window.Caster.Cleave");
}

float UBreakerAbility_Cleave::SwingDamage(float WeaponDamage, float Coefficient, float UnarmedFallback)
{
    const float Base = WeaponDamage > 0.0f ? WeaponDamage : FMath::Max(0.0f, UnarmedFallback);
    return FMath::Max(0.0f, Base * FMath::Max(0.0f, Coefficient));
}

float UBreakerAbility_Cleave::AnimationLockFor(bool bHasEdgework, float AuthoredLockSeconds)
{
    return bHasEdgework ? 0.0f : FMath::Max(0.0f, AuthoredLockSeconds);
}

void UBreakerAbility_Cleave::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FVector ViewLocation = Character->GetActorLocation();
    FRotator ViewRotation = Character->GetControlRotation();
    if (const AController* Controller = Character->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    // The arc is horizontal — a melee swing is not a downward cone, and
    // aiming at the floor must not shorten the reach.
    FVector Forward = ViewRotation.Vector();
    Forward.Z = 0.0;
    if (!Forward.Normalize())
    {
        Forward = Character->GetActorForwardVector();
    }

    FBreakerMeleeSweepParams Params;
    // Sweep from the pawn, not from the camera: 3 m is a body reach.
    Params.Origin = Character->GetActorLocation();
    Params.Forward = Forward;
    Params.RangeCm = RangeCm;
    Params.ArcDegrees = ArcDegrees;

    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();

    float WeaponDamage = 0.0f;
    if (const UBreakerWeaponComponent* Weapon = Character->FindComponentByClass<UBreakerWeaponComponent>())
    {
        if (const UBreakerWeaponDefinition* Definition = Weapon->GetActiveDefinition())
        {
            WeaponDamage = Definition->Damage;
        }
    }
    const float BaseDamage = SwingDamage(WeaponDamage, WeaponDamageCoefficient, UnarmedDamage);

    UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
    const TArray<AActor*> Targets = UBreakerMeleeSweep::SweepTargets(World, Character, Params);

    int32 TargetIndex = 0;
    for (AActor* Target : Targets)
    {
        UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!TargetCombat)
        {
            continue;
        }

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage;
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        // Damage.Melee is what the Spellblade 1.30x More and Melee Damage %
        // affixes gate on; without it a melee hit is indistinguishable from a
        // bullet downstream.
        Damage.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
        Damage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Cleave.GetTag());
        Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
        Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
        Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
        Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(TargetIndex) + static_cast<uint32>(World->GetTimeSeconds() * 1000.0));
        Damage.SourceLocation = Params.Origin;
        Damage.bHasSourceLocation = true;
        Damage.SetInstigator(Character);
        if (OwnerCombat)
        {
            OwnerCombat->ApplyOutgoingModifiers(Damage);
        }
        TargetCombat->ReceiveDamage(Damage);

        // Class-Kits §2.2 C1: Bleed at a 100% base chance — no roll at all.
        ApplyCleaveBleed(Target, SourceAttributes, TargetIndex);
        ++TargetIndex;
    }

    const bool bHasEdgework = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
        && ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_Edgework.GetTag());
    const float Lock = AnimationLockFor(bHasEdgework, AnimationLockSeconds);

    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        // Published for the HUD and for cosmetics; the ability's own lock is
        // the GAS activation, not this window.
        State->StartWindow(SwingWindowKey(), FMath::Max(Lock, 0.05f));
    }

    if (Lock <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    TWeakObjectPtr<UBreakerAbility_Cleave> WeakThis(this);
    World->GetTimerManager().SetTimer(LockTimer, FTimerDelegate::CreateLambda([WeakThis]()
    {
        if (UBreakerAbility_Cleave* Ability = WeakThis.Get())
        {
            Ability->EndAbility(Ability->CurrentSpecHandle, Ability->CurrentActorInfo, Ability->CurrentActivationInfo, true, false);
        }
    }), Lock, false);
}

void UBreakerAbility_Cleave::ApplyCleaveBleed(AActor* Target, const UBreakerAttributeSet* SourceAttributes, int32 Salt) const
{
    UBreakerStatusComponent* Status = Target ? Target->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    if (!Status || BleedDamagePerTick <= 0.0f || BleedDuration <= 0.0f)
    {
        return;
    }

    FBreakerStatusApplicationSpec Spec;
    Spec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Bleed"), false);
    Spec.BaseDamagePerTick = BleedDamagePerTick;
    Spec.Duration = BleedDuration;
    Spec.TickInterval = FMath::Max(0.05f, BleedTickInterval);
    Spec.Snapshot.SourcePower = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    Spec.Snapshot.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
    Spec.Snapshot.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
    Spec.Snapshot.DamageOverTimeMultiplier = SourceAttributes ? SourceAttributes->GetDamageOverTimeMultiplier() : 1.0f;

    const ABreakerCharacter* Character = GetBreakerCharacter();
    FRandomStream Stream(static_cast<int32>(HashCombine(HashCombine(GetTypeHash(Character), static_cast<uint32>(Salt)), BreakerCleaveBleedSalt)));
    // Snapshot criticals: one roll at application decides every tick of this
    // application, exactly as the weapon path does it.
    Spec.Snapshot.bRolledCritical = Stream.FRand() < Spec.Snapshot.CriticalChance;

    Status->ApplyStatus(Spec, EBreakerDamageFamily::Physical, const_cast<ABreakerCharacter*>(Character));
}
