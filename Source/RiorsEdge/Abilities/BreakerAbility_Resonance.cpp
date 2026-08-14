#include "Abilities/BreakerAbility_Resonance.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

UBreakerAbility_Resonance::UBreakerAbility_Resonance()
{
    FallbackAbilityId = TEXT("Caster.Resonance");
    // Spec §5.6: it mutates another pawn's status list, so it never runs on a
    // client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Resonance.GetTag());
    SetAssetTags(Tags);
}

void UBreakerAbility_Resonance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World)
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

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerResonance), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaximumRangeCm;
    AActor* Target = nullptr;
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams))
    {
        Target = Hit.GetActor();
    }

    UBreakerStatusComponent* Status = Target ? Target->FindComponentByClass<UBreakerStatusComponent>() : nullptr;
    const int32 DistinctCount = Status ? Status->GetDistinctStatusTypeCount() : 0;

    // Target acquisition and the status count both happen BEFORE the commit,
    // for the reason Closequarter states: charging 40 Mana for a cast that
    // provably cannot do anything is a dead key, not a risk the design asked
    // for. Detonating an unstatused target is exactly that — the whole ability
    // is "consume what is there", and there is nothing there.
    if (DistinctCount <= 0 || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Read the count, then take the statuses away, then damage. Consuming
    // BEFORE the damage matters: the burst can kill, and a death that fires
    // while the status list is still populated would let a listener detonate a
    // corpse's statuses a second time.
    if (bConsumeStatuses)
    {
        Status->ConsumeAllStatuses();
    }
    else
    {
        // MS8: the statuses survive at half duration instead of dying.
        Status->ScaleRemainingDurations(DurationScalarWhenNotConsuming);
    }

    // O35: the detonation's authored parameters are item-level-1 numbers; the
    // burst rides the equipped weapon's item-level scalar. Applied to the
    // RESULT of the curve, not its parameters, so the §2.7.5 ratio bound is
    // untouched at every item level (a common scalar cancels out of the ratio).
    const float BaseDamage = UBreakerStatusConsumption::DetonationDamage(DistinctCount, Detonation, Curve)
        * AbilityDamageScalarFor(Character);
    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();

    if (UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>())
    {
        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage;
        // O5: Elemental stays the pipeline family until the resistance model
        // lands; Void is carried on the type tag so nothing needs rewriting
        // when Rift/Entropy/Void resistances arrive.
        Damage.DamageFamily = EBreakerDamageFamily::Elemental;
        Damage.DamageTypeTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Void"), false);
        Damage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Resonance.GetTag());
        Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
        Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(World->GetTimeSeconds() * 1000.0));
        Damage.SourceLocation = Character->GetActorLocation();
        Damage.bHasSourceLocation = true;
        // The burst is one hit from the caster: kill credit, Mana generation
        // and every on-hit affix hang off this being a normal damage request.
        Damage.SetInstigator(Character);
        if (UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            OwnerCombat->ApplyOutgoingModifiers(Damage);
        }
        TargetCombat->ReceiveDamage(Damage);
    }

    if (RefundManaPerStatus > 0.0f)
    {
        if (UBreakerManaComponent* Mana = GetManaComponent())
        {
            // A payback, not generation: metered through the 20/s cap it would
            // read as broken, exactly as Closequarter's refund would.
            Mana->GrantMana(UBreakerStatusConsumption::RefundForConsumed(DistinctCount, RefundManaPerStatus, Detonation.MaximumCountedStatuses), true);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
