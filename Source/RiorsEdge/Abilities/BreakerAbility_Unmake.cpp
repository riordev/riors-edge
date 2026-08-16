#include "Abilities/BreakerAbility_Unmake.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerManaComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusCycleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
    // Salts the echo's snapshot critical roll away from every other roll made
    // off the same caster hash, so Cascade's crits stay independent of the
    // application that triggered them while remaining server-reproducible.
    constexpr uint32 BreakerUnmakeCascadeSalt = 0xCA5CADEu;
}

void UBreakerCascadeEchoListener::HandleStatusApplied(const FBreakerActiveStatus& Status)
{
    ABreakerCharacter* CasterCharacter = Caster.Get();
    AActor* TargetActor = Target.Get();
    UWorld* World = TargetActor ? TargetActor->GetWorld() : nullptr;
    if (!CasterCharacter || !World)
    {
        return;
    }

    UAbilitySystemComponent* CasterASC = nullptr;
    if (const IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(CasterCharacter))
    {
        CasterASC = AbilityInterface->GetAbilitySystemComponent();
    }
    const bool bCascadeHeld = CasterASC
        && CasterASC->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_Cascade.GetTag());
    const UBreakerAbilityStateComponent* State = CasterCharacter->FindComponentByClass<UBreakerAbilityStateComponent>();
    const bool bWindowActive = State && State->IsWindowActive(UBreakerCasterAbility::UnmakeWindowKey());
    const bool bInstigatedByCaster = Status.Instigator.Get() == CasterCharacter;
    if (!UBreakerAbility_Unmake::ShouldCascadeEcho(bCascadeHeld, bWindowActive, bInstigatedByCaster, Status.Spec.ProcCoefficient))
    {
        return;
    }

    // "The next status in Fracture's cycle" — DRAWN, not peeked: the echo
    // consumes a cycle position exactly as a Fracture cast does, so Cascade
    // sequences breadth instead of stamping one status repeatedly, and the HUD
    // cycle readout stays honest about what the next cast will apply.
    UBreakerStatusCycleComponent* Cycle = UBreakerStatusCycleComponent::FindOrAdd(CasterCharacter);
    if (!Cycle || Cycle->GetCycleLength() <= 0)
    {
        return;
    }
    const FBreakerCycleEntry Entry = Cycle->PeekNextEntry(0);
    Cycle->AdvanceCycle();
    if (!Entry.Spec.StatusTag.IsValid())
    {
        return;
    }

    FBreakerStatusApplicationSpec Echo = UBreakerAbility_Unmake::MakeCascadeEchoSpec(
        Entry.Spec, UBreakerGameplayAbility::AbilityDamageScalarFor(CasterCharacter));

    // Snapshot NOW, at the application that triggered the echo — Fracture's
    // own contract: one critical roll per application decides every tick.
    const UBreakerAttributeSet* SourceAttributes = CasterASC ? CasterASC->GetSet<UBreakerAttributeSet>() : nullptr;
    const UBreakerCombatComponent* OwnerCombat = CasterCharacter->FindComponentByClass<UBreakerCombatComponent>();
    Echo.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(SourceAttributes, OwnerCombat);
    Echo.Snapshot.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Echo.Snapshot.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    Echo.Snapshot.DamageOverTimeMultiplier = SourceAttributes ? SourceAttributes->GetDamageOverTimeMultiplier() : 1.0f;
    FRandomStream Stream(static_cast<int32>(HashCombine(
        HashCombine(GetTypeHash(CasterCharacter), static_cast<uint32>(World->GetTimeSeconds() * 1000.0)),
        BreakerUnmakeCascadeSalt)));
    Echo.Snapshot.bRolledCritical = Stream.FRand() < Echo.Snapshot.CriticalChance;

    // Applied NEXT TICK, never inside the broadcast that triggered it: the
    // echo lands on the same component that is mid-broadcast, and mutating
    // ActiveStatuses under other listeners' feet is the re-entrancy bug this
    // one line prevents. One frame of latency is imperceptible; the snapshot
    // above already fixed the numbers at the moment that counts.
    const EBreakerDamageFamily Family = Entry.DamageFamily;
    TWeakObjectPtr<AActor> WeakTarget = TargetActor;
    TWeakObjectPtr<ABreakerCharacter> WeakCaster = CasterCharacter;
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakTarget, WeakCaster, Echo, Family]()
    {
        if (AActor* EchoTarget = WeakTarget.Get())
        {
            if (UBreakerStatusComponent* TargetStatus = EchoTarget->FindComponentByClass<UBreakerStatusComponent>())
            {
                TargetStatus->ApplyStatus(Echo, Family, WeakCaster.Get());
            }
        }
    }));
}

UBreakerAbility_Unmake::UBreakerAbility_Unmake()
{
    FallbackAbilityId = TEXT("Caster.Unmake");
    // Spec §4.7's reasoning applies unchanged: a multi-second global state
    // change is not worth predicting, and a mispredicted ultimate is the worst
    // possible feel.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ultimate_Unmake.GetTag());
    ActivationBlockedTags.AddTag(BreakerAbilityTags::State_Ultimate_Unmake.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Unmake.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Unmake::GenerationSuspensionKey()
{
    return TEXT("Unmake");
}

float UBreakerAbility_Unmake::ResolveCostScalar(float VariantCostMultiplier)
{
    return FMath::Clamp(VariantCostMultiplier, 0.0f, 1.0f);
}

float UBreakerAbility_Unmake::ResolveDuration(float VariantDuration, float DefinitionDuration)
{
    if (VariantDuration > 0.0f)
    {
        return VariantDuration;
    }
    return FMath::Max(0.0f, DefinitionDuration);
}

bool UBreakerAbility_Unmake::ShouldCascadeEcho(bool bCascadeHeld, bool bWindowActive, bool bInstigatedByCaster, float AppliedProcCoefficient)
{
    return bCascadeHeld && bWindowActive && bInstigatedByCaster && AppliedProcCoefficient > 0.0f;
}

FBreakerStatusApplicationSpec UBreakerAbility_Unmake::MakeCascadeEchoSpec(FBreakerStatusApplicationSpec CycleSpec, float DamageScalar)
{
    CycleSpec.BaseDamagePerTick *= FMath::Max(0.0f, DamageScalar);
    // "at proc coefficient 0" — the load-bearing clause (Class-Kits §2.2,
    // Master 7.10.1). The echo generates no Mana, feeds no reaction, and above
    // all does not trigger Cascade again.
    CycleSpec.ProcCoefficient = 0.0f;
    return CycleSpec;
}

void UBreakerAbility_Unmake::BeginCascadeListening(UWorld* World, ABreakerCharacter* Character)
{
    // Bind an ear to every status-bearing actor in this world. The status
    // layer's application event carries no target pointer, so each binding is
    // a small listener object that remembers whose event it is hearing.
    // KNOWN LIMIT (header note): actors spawned after this scan echo nothing
    // until the next Unmake.
    for (TObjectIterator<UBreakerStatusComponent> It; It; ++It)
    {
        UBreakerStatusComponent* Component = *It;
        if (!IsValid(Component) || Component->GetWorld() != World || !Component->GetOwner())
        {
            continue;
        }
        // Never the caster's own component: the only statuses that land there
        // are enemy applications, which fail the instigator gate anyway, and a
        // self-echo would hand the Caster their own DoT.
        if (Component->GetOwner() == Character)
        {
            continue;
        }
        UBreakerCascadeEchoListener* Listener = NewObject<UBreakerCascadeEchoListener>(this);
        Listener->Target = Component->GetOwner();
        Listener->Caster = Character;
        Component->OnStatusApplied.AddDynamic(Listener, &UBreakerCascadeEchoListener::HandleStatusApplied);
        CascadeListeners.Add(Listener);
    }
}

void UBreakerAbility_Unmake::EndCascadeListening()
{
    for (UBreakerCascadeEchoListener* Listener : CascadeListeners)
    {
        if (!Listener)
        {
            continue;
        }
        if (const AActor* TargetActor = Listener->Target.Get())
        {
            if (UBreakerStatusComponent* Component = TargetActor->FindComponentByClass<UBreakerStatusComponent>())
            {
                Component->OnStatusApplied.RemoveDynamic(Listener, &UBreakerCascadeEchoListener::HandleStatusApplied);
            }
        }
    }
    CascadeListeners.Empty();
}

void UBreakerAbility_Unmake::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    FGameplayTagContainer OwnerTags;
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(OwnerTags);
    }
    const FBreakerAbilityVariant Variant = Definition
        ? Definition->ResolveVariant(OwnerTags)
        : FBreakerAbilityVariant();

    const float Duration = ResolveDuration(Variant.WindowDuration, Definition ? Definition->WindowDuration : 6.0f);
    const float CostScalar = ResolveCostScalar(Variant.AbilityCostMultiplier);

    if (Duration <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // The window IS the ultimate: every Caster ability reads its payload as
    // the cost scalar through UBreakerCasterAbility::GetResourceCost.
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindowWithPayload(UnmakeWindowKey(), Duration, CostScalar);
    }

    // "Mana generation is suspended" (Class-Kits §2.2). Keyed push/pop so an
    // early end reverts exactly its own entry.
    if (UBreakerManaComponent* Mana = GetManaComponent())
    {
        Mana->PushGenerationSuspension(GenerationSuspensionKey());
    }

    // Cascade (Class-Kits §2.2): the reaction ultimate. Only when the resolved
    // variant IS the Cascade row — the resolver already required the keystone
    // tag, so this cannot arm off a foreign keystone or an untagged owner.
    if (Variant.KeystoneTag == BreakerAbilityTags::Keystone_Caster_Cascade.GetTag())
    {
        BeginCascadeListening(World, Character);
    }

    TWeakObjectPtr<UBreakerAbility_Unmake> WeakThis(this);
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateLambda([WeakThis]()
    {
        if (UBreakerAbility_Unmake* Ability = WeakThis.Get())
        {
            Ability->EndAbility(Ability->CurrentSpecHandle, Ability->CurrentActorInfo, Ability->CurrentActivationInfo, true, false);
        }
    }), Duration, false);
}

void UBreakerAbility_Unmake::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // Cascade's ears come off with the window, on every exit path including
    // cancel and death — the same unconditional-teardown rule as the window
    // and the Mana suspension below.
    EndCascadeListening();
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UWorld* World = Character->GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
        // Teardown is unconditional, including on cancel and on death: a
        // Caster left with free casts because the ultimate was interrupted is
        // the worst possible failure mode of this design.
        if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
        {
            State->CloseWindow(UnmakeWindowKey());
        }
        if (UBreakerManaComponent* Mana = Character->GetMana())
        {
            Mana->PopGenerationSuspension(GenerationSuspensionKey());
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
