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
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"
#include "UObject/UObjectIterator.h"

namespace
{
    // Salts the echo's snapshot critical roll away from every other roll made
    // off the same caster hash, so Cascade's crits stay independent of the
    // application that triggered them while remaining server-reproducible.
    constexpr uint32 BreakerUnmakeCascadeSalt = 0xCA5CADEu;
}

bool UBreakerCascadeEchoListener::IsEchoActive() const
{
    const auto* Character = Caster.Get();
    const auto* Actor = Target.Get();
    if (!bActive || !IsValid(Character) || !IsValid(Actor)
        || Character->IsActorBeingDestroyed() || Actor->IsActorBeingDestroyed()) return false;
    const auto* SourceCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
    const auto* TargetCombat = Actor->FindComponentByClass<UBreakerCombatComponent>();
    if (!SourceCombat || SourceCombat->IsDead() || !TargetCombat || TargetCombat->IsDead()) return false;
    const auto* ASC = Character->GetAbilitySystemComponent();
    const auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    return ASC && ASC->HasMatchingGameplayTag(BreakerAbilityTags::Keystone_Caster_Cascade.GetTag())
        && State && State->IsWindowActive(UBreakerCasterAbility::UnmakeWindowKey());
}

void UBreakerCascadeEchoListener::HandleTargetDeath() { ++Generation; }

void UBreakerCascadeEchoListener::HandleStatusApplied(const FBreakerActiveStatus& Status)
{
    if (!IsEchoActive()) return;
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
    // O222 elemental entries are buildup verbs, not proc-zero carried statuses.
    // Draw across them without fabricating an earned Rot/Erased/Unstable mark.
    FBreakerCycleEntry Entry;
    bool bFoundPhysical = false;
    const int32 Length = Cycle->GetCycleLength();
    for (int32 Index = 0; Index < Length; ++Index)
    {
        Entry = Cycle->PeekNextEntry(0);
        Cycle->AdvanceCycle();
        if (Entry.Element == EBreakerElement::None && Entry.DamageFamily == EBreakerDamageFamily::Physical
            && Entry.Spec.StatusTag.IsValid())
        {
            bFoundPhysical = true;
            break;
        }
    }
    if (!bFoundPhysical || !IsEchoActive()) return;

    FBreakerStatusApplicationSpec Echo = UBreakerAbility_Unmake::MakeCascadeEchoSpec(
        Entry.Spec, UBreakerGameplayAbility::AbilityDamageScalarFor(CasterCharacter));
    Echo.BaseDamagePerTick = UBreakerGameplayAbility::AbilityBaseDamageFor(CasterCharacter, Echo.BaseDamagePerTick);

    // Snapshot NOW, at the application that triggered the echo — Fracture's
    // own contract: one critical roll per application decides every tick.
    const UBreakerAttributeSet* SourceAttributes = CasterASC ? CasterASC->GetSet<UBreakerAttributeSet>() : nullptr;
    const UBreakerCombatComponent* OwnerCombat = CasterCharacter->FindComponentByClass<UBreakerCombatComponent>();
    Echo.Snapshot.SourcePower = UBreakerCombatComponent::ComposeDotSourcePower(SourceAttributes, OwnerCombat,
        EBreakerDamageDelivery::Ability);
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
    const TWeakObjectPtr<UBreakerCascadeEchoListener> WeakListener(this);
    const uint64 QueuedGeneration = Generation;
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakListener, QueuedGeneration, Echo, Family]()
    {
        const auto* Listener = WeakListener.Get();
        if (!Listener || Listener->Generation != QueuedGeneration || !Listener->IsEchoActive()) return;
        if (AActor* EchoTarget = Listener->Target.Get())
        {
            if (UBreakerStatusComponent* TargetStatus = EchoTarget->FindComponentByClass<UBreakerStatusComponent>())
            {
                TargetStatus->ApplyStatus(Echo, Family, Listener->Caster.Get());
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
    EndCascadeListening();
    CascadeWorld = World;
    CascadeCaster = Character;
    if (!World || !Character) return;
    CascadeSpawnHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::BindCascadeActor));
    if (auto* Combat = Character->GetCombat())
        Combat->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleCascadeOwnerDeath);
    if (auto* ASC = Character->GetAbilitySystemComponent())
        CascadeKeystoneHandle = ASC->RegisterGameplayTagEvent(BreakerAbilityTags::Keystone_Caster_Cascade.GetTag(),
            EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleCascadeKeystoneChanged);
    // Bind an ear to every status-bearing actor in this world. The status
    // layer's application event carries no target pointer, so each binding is
    // a small listener object that remembers whose event it is hearing.
    // Native spawned actors are added by the scoped world delegate above.
    for (TObjectIterator<UBreakerStatusComponent> It; It; ++It)
    {
        UBreakerStatusComponent* Component = *It;
        if (!IsValid(Component) || Component->GetWorld() != World || !Component->GetOwner())
        {
            continue;
        }
        BindCascadeActor(Component->GetOwner());
    }

    // THE CHAIN, IN FIRE ORDER: one leg per armed listener, walking caster to
    // first target to second and on, in exactly the order the loop above
    // armed them, each leg a beat after the last so the cascade visibly
    // TRAVELS. Endpoints are captured at arming — a placeholder honesty gap
    // for targets that move during the stagger. Figures O2 PLACEHOLDER.
    // (Server-only ability, cosmetic call — see BreakerEffectRenderer.h.)
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        const FVector Lift(0.0f, 0.0f, 60.0f);
        FVector Previous = Character->GetActorLocation() + Lift;
        BreakerFX::FEffectTiming LegTiming;
        LegTiming.DurationSeconds = 0.45f;
        LegTiming.FadeInSeconds = 0.03f;
        LegTiming.FadeOutSeconds = 0.3f;
        int32 LegIndex = 0;
        for (const UBreakerCascadeEchoListener* Listener : CascadeListeners)
        {
            const AActor* LegTarget = Listener ? Listener->Target.Get() : nullptr;
            if (!LegTarget) continue;
            const FVector Next = LegTarget->GetActorLocation() + Lift;
            const float Delay = 0.08f * LegIndex;
            Effects->AddStroke(Previous, Next, 4.0f, BreakerUI::Cyan, 2.4f, LegTiming, Delay);
            Effects->AddGlow(Next, 30.0f, BreakerUI::Cyan, 2.4f, LegTiming, Delay);
            Previous = Next;
            ++LegIndex;
        }
    }
}

void UBreakerAbility_Unmake::BindCascadeActor(AActor* Actor)
{
    auto* Character = CascadeCaster.Get();
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !Character || Actor == Character
        || !CascadeWorld.IsValid() || Actor->GetWorld() != CascadeWorld.Get()) return;
    auto* Component = Actor->FindComponentByClass<UBreakerStatusComponent>();
    if (!Component || CascadeListeners.ContainsByPredicate([Actor](const auto& Listener)
        { return Listener && Listener->Target.Get() == Actor; })) return;
    auto* Listener = NewObject<UBreakerCascadeEchoListener>(this);
    Listener->Target = Actor;
    Listener->Caster = Character;
    Component->OnStatusApplied.AddDynamic(Listener, &UBreakerCascadeEchoListener::HandleStatusApplied);
    if (auto* Combat = Actor->FindComponentByClass<UBreakerCombatComponent>())
        Combat->OnDeath.AddDynamic(Listener, &UBreakerCascadeEchoListener::HandleTargetDeath);
    CascadeListeners.Add(Listener);
}

void UBreakerAbility_Unmake::HandleCascadeOwnerDeath() { EndCascadeListening(); }

void UBreakerAbility_Unmake::HandleCascadeKeystoneChanged(FGameplayTag Tag, int32 Count)
{
    if (Count <= 0) EndCascadeListening();
}

void UBreakerAbility_Unmake::EndCascadeListening()
{
    // Invalidate before removing any delegate: already queued next-tick work
    // belongs to these listeners, never to a later cast of the same ultimate.
    for (UBreakerCascadeEchoListener* Listener : CascadeListeners)
        if (Listener) { Listener->bActive = false; ++Listener->Generation; }
    if (auto* World = CascadeWorld.Get(); World && CascadeSpawnHandle.IsValid())
        World->RemoveOnActorSpawnedHandler(CascadeSpawnHandle);
    CascadeSpawnHandle.Reset();
    if (auto* Character = CascadeCaster.Get())
    {
        if (auto* Combat = Character->GetCombat())
            Combat->OnDeath.RemoveDynamic(this, &ThisClass::HandleCascadeOwnerDeath);
        if (auto* ASC = Character->GetAbilitySystemComponent(); ASC && CascadeKeystoneHandle.IsValid())
            ASC->RegisterGameplayTagEvent(BreakerAbilityTags::Keystone_Caster_Cascade.GetTag(),
                EGameplayTagEventType::NewOrRemoved).Remove(CascadeKeystoneHandle);
    }
    CascadeKeystoneHandle.Reset();
    CascadeWorld.Reset();
    CascadeCaster.Reset();
    for (UBreakerCascadeEchoListener* Listener : CascadeListeners)
    {
        if (!Listener)
        {
            continue;
        }
        if (const AActor* TargetActor = Listener->Target.Get())
        {
            if (auto* Combat = TargetActor->FindComponentByClass<UBreakerCombatComponent>())
                Combat->OnDeath.RemoveDynamic(Listener, &UBreakerCascadeEchoListener::HandleTargetDeath);
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

    // The ultimates' violet ignition (Overdrive's precedent, the one moment
    // base and Long Dark Unmake were still invisible), feet-anchored per the
    // camera law. Figures O2 PLACEHOLDER. (Server-only, cosmetic — see
    // BreakerEffectRenderer.h.)
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        const FVector Centre = Character->GetActorLocation();
        const FVector Feet = Centre - FVector(0.0f, 0.0f, Character->GetSimpleCollisionHalfHeight() * 0.8f);
        BreakerFX::FEffectTiming BurstTiming;
        BurstTiming.DurationSeconds = 0.55f;
        BurstTiming.FadeInSeconds = 0.02f;
        BurstTiming.FadeOutSeconds = 0.40f;
        Effects->AddGlow(Feet, 70.0f, BreakerUI::Violet, 3.6f, BurstTiming);
        Effects->AddBlinkLight(Centre, 650.0f, BreakerUI::Violet, 3600.0f, BurstTiming);
        for (int32 Index = 0; Index < 6; ++Index)
        {
            const FVector Out = FRotator(0.0f, 60.0f * Index, 0.0f).Vector();
            Effects->AddStroke(Feet + Out * 40.0f, Feet + Out * 150.0f, 4.5f, BreakerUI::Violet, 2.8f, BurstTiming, 0.03f * Index);
        }
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
