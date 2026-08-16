#include "Abilities/BreakerSupportAbilities.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerChargeComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerStatusComponent.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

namespace BreakerSupportAbilityLocal
{
    // Prefixed for the unity build, per house rule.

    // The target's maximum health, for percentage-of-target healing (§U1) and
    // marked-damage generation (§1.1). Null-safe: 0 when unreadable.
    float BreakerSupportTargetMaxHealth(const AActor* Target)
    {
        if (const ABreakerCharacter* Breaker = Cast<ABreakerCharacter>(Target))
        {
            const UBreakerAttributeSet* Attributes = Breaker->GetAttributes();
            return Attributes ? Attributes->GetMaxHealth() : 0.0f;
        }
        if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Target))
        {
            if (const UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
            {
                if (const UBreakerAttributeSet* Attributes = ASC->GetSet<UBreakerAttributeSet>())
                {
                    return Attributes->GetMaxHealth();
                }
            }
        }
        return 0.0f;
    }
}

// ---------------------------------------------------------------------------
// The Support base
// ---------------------------------------------------------------------------

FName UBreakerSupportAbility::ConduitWindowKey() { return TEXT("Window.Support.Conduit"); }

float UBreakerSupportAbility::CostUnderConduit(float AuthoredCost, float WindowScalar)
{
    return FMath::Max(0.0f, AuthoredCost * FMath::Max(0.0f, WindowScalar));
}

float UBreakerSupportAbility::GetResourceCost() const
{
    const float Authored = Super::GetResourceCost();
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UBreakerAbilityStateComponent* State = Character ? Character->FindComponentByClass<UBreakerAbilityStateComponent>() : nullptr;
    if (!State) return Authored;
    // Payload 1.0 when no window is open; 0.0 under base CONDUIT ("costs
    // nothing"); 1.0 under Triage/Blackout, which replace casting rather than
    // enabling it. CheckCost and ApplyCost both read through here, so
    // affordability and the spend cannot disagree (the Caster precedent).
    const float Scalar = State->GetWindowPayload(ConduitWindowKey(), 1.0f);
    return CostUnderConduit(Authored, Scalar);
}

AActor* UBreakerSupportAbility::ResolveAllyTarget(ABreakerCharacter* Caster, float RangeCm)
{
    // §3: the ally under the crosshair, or SELF with no target. With no party
    // layer this trace can only ever find another spawned ABreakerCharacter
    // (a second PIE pawn), so solo it resolves to self every time — the case
    // §2 makes first-class. One path, no self-discount, no self-bonus.
    if (!Caster) return nullptr;
    UWorld* World = Caster->GetWorld();
    if (!World) return Caster;

    FVector ViewLocation = Caster->GetActorLocation();
    FRotator ViewRotation = Caster->GetControlRotation();
    if (const AController* Controller = Caster->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerSupportAim), false, Caster);
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * RangeCm, ECC_Pawn, QueryParams))
    {
        if (ABreakerCharacter* Ally = Cast<ABreakerCharacter>(Hit.GetActor()))
        {
            return Ally;
        }
    }
    return Caster;
}

void UBreakerSupportAbility::RefreshBuffUptime(ABreakerCharacter* Character)
{
    if (!Character) return;
    UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>();
    const UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>();
    if (!Charge || !State) return;
    // A BOOL by construction: two live buffs pay exactly what one pays.
    const bool bAnyBuff = State->IsWindowActive(UBreakerAbility_Cadence::WindowKey())
        || State->IsWindowActive(UBreakerAbility_Metronome::WindowKey());
    Charge->SetAnyBuffActive(bAnyBuff);
}

// ---------------------------------------------------------------------------
// U1 — PATCH
// ---------------------------------------------------------------------------

UBreakerAbility_Patch::UBreakerAbility_Patch()
{
    FallbackAbilityId = TEXT("Support.Patch");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_Patch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AActor* Target = ResolveAllyTarget(Character, TargetRangeCm);
    UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const float TargetMaxHealth = BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Target);
    if (TargetCombat && TargetMaxHealth > 0.0f)
    {
        // §U1: a percentage of the TARGET'S maximum health, so it is equally
        // meaningful on a Tank and on a Caster. Overheal is discarded — the
        // heal result reports it separately and the Charge wiring credits
        // only the effective half (the loop's single most load-bearing rule).
        FBreakerHealRequest Heal;
        Heal.Amount = TargetMaxHealth * HealFractionOfTargetMax;
        Heal.SetHealer(Character);
        TargetCombat->ApplyHealing(Heal);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// U2 — PURGE
// ---------------------------------------------------------------------------

UBreakerAbility_Purge::UBreakerAbility_Purge()
{
    FallbackAbilityId = TEXT("Support.Purge");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_Purge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AActor* Target = ResolveAllyTarget(Character, TargetRangeCm);
    if (UBreakerStatusComponent* Status = Target ? Target->FindComponentByClass<UBreakerStatusComponent>() : nullptr)
    {
        // Distinct TYPES, never stacks — a 10-stack Bleed cleansed is one
        // status removed for generation (§1.1's cleanse row).
        const int32 DistinctRemoved = Status->GetDistinctStatusTypeCount();
        Status->ConsumeAllStatuses();
        if (DistinctRemoved > 0)
        {
            if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
            {
                Charge->NotifyStatusCleansed(DistinctRemoved);
            }
        }
        // The 3s status-immunity window is RECORDED ABSENT (see the header):
        // no immunity primitive exists to grant. §U2 calls the immunity the
        // whole value, so this gap is the loudest one in the kit.
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// U3 — CADENCE
// ---------------------------------------------------------------------------

UBreakerAbility_Cadence::UBreakerAbility_Cadence()
{
    FallbackAbilityId = TEXT("Support.Cadence");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Cadence::WindowKey() { return TEXT("Window.Support.Cadence"); }

void UBreakerAbility_Cadence::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const float Duration = Definition ? Definition->WindowDuration : 8.0f;
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    // Being a BUFF, it drives the count-independent uptime source (§U3) —
    // that, plus the HUD window, is the whole shipped payload; the tempo half
    // is the header's recorded gap.
    RefreshBuffUptime(Character);
    bCadenceActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        if (CurrentActorInfo) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }), Duration, false);
}

void UBreakerAbility_Cadence::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bCadenceActive)
    {
        bCadenceActive = false;
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
            RefreshBuffUptime(Character);
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// U4 — METRONOME
// ---------------------------------------------------------------------------

UBreakerAbility_Metronome::UBreakerAbility_Metronome()
{
    FallbackAbilityId = TEXT("Support.Metronome");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Metronome::WindowKey() { return TEXT("Window.Support.Metronome"); }
FName UBreakerAbility_Metronome::OutgoingModifierKey() { return TEXT("Metronome"); }

void UBreakerAbility_Metronome::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    UBreakerCombatComponent* Combat = Character ? Character->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!World || !Combat || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const float Duration = Definition ? Definition->WindowDuration : 8.0f;
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    RefreshBuffUptime(Character);
    Stacks = 0;
    LastHitTime = -1000.0;
    BoundCombat = Combat;
    Combat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Metronome::HandleHitDealt);
    bMetronomeActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMetronome(); }), Duration, false);
}

void UBreakerAbility_Metronome::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bMetronomeActive) return;
    UWorld* World = GetWorld();
    if (!World) return;
    const double Now = World->GetTimeSeconds();
    // §U4: a full second without a hit resets the ramp. PER HOLDER — solo the
    // Support's own ramp is the only one, and it is not shared or averaged.
    if (Now - LastHitTime > StreakGapSeconds)
    {
        Stacks = 0;
    }
    LastHitTime = Now;
    Stacks = FMath::Min(Stacks + 1, MaximumStacks);
    if (UBreakerCombatComponent* Combat = BoundCombat.Get())
    {
        // Re-pushing the key replaces the entry, so the ramp reads as one
        // growing flat contribution, never a stack of modifiers.
        Combat->PushOutgoingModifier(OutgoingModifierKey(), FlatDamagePerStack * Stacks, 1.0f, StreakGapSeconds);
    }
}

void UBreakerAbility_Metronome::CloseMetronome()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Metronome::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bMetronomeActive)
    {
        bMetronomeActive = false;
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Metronome::HandleHitDealt);
            Combat->RemoveOutgoingModifier(OutgoingModifierKey());
        }
        BoundCombat.Reset();
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
            RefreshBuffUptime(Character);
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// U5 — MARK
// ---------------------------------------------------------------------------

UBreakerAbility_Mark::UBreakerAbility_Mark()
{
    FallbackAbilityId = TEXT("Support.Mark");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Mark::IncomingModifierKey() { return TEXT("Support.Mark"); }

void UBreakerAbility_Mark::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !World)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Aim resolution BEFORE commit: marking nothing refuses the cast rather
    // than eating 20 Charge — the loop's ignition must not misfire on a whiff.
    FVector ViewLocation = Character->GetActorLocation();
    FRotator ViewRotation = Character->GetControlRotation();
    if (const AController* Controller = Character->GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerMarkAim), false, Character);
    AActor* Target = nullptr;
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * TargetRangeCm, ECC_Pawn, QueryParams))
    {
        if (Cast<ABreakerEnemy>(Hit.GetActor())) Target = Hit.GetActor();
    }
    UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    if (!TargetCombat || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const float Duration = Definition ? Definition->WindowDuration : 10.0f;

    // The paint, on the shared mark surface the weapon already reads for its
    // marked-target treatment, so the two mark consumers cannot disagree.
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->SetMark(Target, Duration);
    }
    // "Takes more damage from ALL SOURCES including allies" is structural: the
    // keyed incoming modifier sits on the TARGET, so every damage request from
    // anyone passes through it (§U5).
    TargetCombat->PushIncomingDamageModifier(IncomingModifierKey(), MarkedDamageMultiplier);

    MarkedTarget = Target;
    if (UBreakerCombatComponent* OwnCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        BoundCombat = OwnCombat;
        OwnCombat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Mark::HandleHitDealt);
    }
    bMarkActive = true;
    World->GetTimerManager().SetTimer(MarkTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseMark(); }), Duration, false);
}

void UBreakerAbility_Mark::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bMarkActive || Hit.Target != MarkedTarget.Get() || !Hit.Target) return;
    // §1.1: damage YOU deal to your mark generates, at +1 per 2% of the
    // TARGET'S maximum health — a boss pays the same total for the same
    // fraction, so the bar is bounded and predictable.
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
        {
            Charge->NotifyMarkedTargetDamage(Hit.Result.HealthDamage + Hit.Result.ShieldDamage,
                BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Hit.Target));
        }
    }
}

void UBreakerAbility_Mark::CloseMark()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Mark::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bMarkActive)
    {
        bMarkActive = false;
        if (AActor* Target = MarkedTarget.Get())
        {
            if (UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>())
            {
                TargetCombat->RemoveIncomingDamageModifier(IncomingModifierKey());
            }
        }
        MarkedTarget.Reset();
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Mark::HandleHitDealt);
        }
        BoundCombat.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(MarkTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// U6 — SUPPRESS
// ---------------------------------------------------------------------------

UBreakerAbility_Suppress::UBreakerAbility_Suppress()
{
    FallbackAbilityId = TEXT("Support.Suppress");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_Suppress::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
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
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerSuppressAim), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * AimRangeCm;
    const FVector Center = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams)
        ? Hit.ImpactPoint : TraceEnd;

    // §U6: a zone that deals NO damage at all — BaseDamage 0 is a legal and
    // useful zone, the zone actor's own comment names Suppress as the case.
    FBreakerZoneSpec Spec;
    Spec.ZoneTag = FGameplayTag::RequestGameplayTag(TEXT("Zone.Support.Suppress"), false);
    Spec.RadiusCm = RadiusCm;
    Spec.Duration = Definition ? Definition->WindowDuration : 6.0f;
    Spec.TickInterval = 1.0f;
    Spec.ZoneColor = FLinearColor(0.55f, 0.35f, 0.85f);   // violet; teal is reserved (O19)

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    ActiveZone = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams);
    if (ActiveZone)
    {
        ActiveZone->ConfigureZone(Spec, Character);
        ActiveZone->OnOccupantEntered.AddDynamic(this, &UBreakerAbility_Suppress::HandleOccupantEntered);
        ActiveZone->OnOccupantExited.AddDynamic(this, &UBreakerAbility_Suppress::HandleOccupantExited);
        ActiveZone->OnZoneExpired.AddDynamic(this, &UBreakerAbility_Suppress::HandleZoneExpired);
    }

    // The ability ends now; the zone owns the 6s. The 10s cooldown guarantees
    // no second Suppress races this zone's teardown handlers.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UBreakerAbility_Suppress::HandleOccupantEntered(AActor* Occupant)
{
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Occupant))
    {
        Enemy->ApplyModifierMovementProfile(SlowMultiplier, -1.0f);
        SlowedEnemies.AddUnique(Enemy);
    }
}

void UBreakerAbility_Suppress::HandleOccupantExited(AActor* Occupant)
{
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Occupant))
    {
        Enemy->ApplyModifierMovementProfile(1.0f, -1.0f);
        SlowedEnemies.Remove(Enemy);
    }
}

void UBreakerAbility_Suppress::HandleZoneExpired()
{
    for (const TWeakObjectPtr<AActor>& Slowed : SlowedEnemies)
    {
        if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Slowed.Get()))
        {
            Enemy->ApplyModifierMovementProfile(1.0f, -1.0f);
        }
    }
    SlowedEnemies.Reset();
    ActiveZone = nullptr;
}

// ---------------------------------------------------------------------------
// ULTIMATE — CONDUIT
// ---------------------------------------------------------------------------

UBreakerAbility_Conduit::UBreakerAbility_Conduit()
{
    FallbackAbilityId = TEXT("Support.Conduit");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Conduit::BlackoutModifierKey() { return TEXT("Conduit.Blackout"); }
FName UBreakerAbility_Conduit::DownbeatModifierKey() { return TEXT("Conduit.Downbeat"); }

void UBreakerAbility_Conduit::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const UBreakerAbilityDefinition* Definition = GetAbilityDefinition();
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    const float Threshold = Definition ? Definition->ResourceCost : 100.0f;
    if (!Character || !World || GetCurrentClassResource() < Threshold || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FGameplayTagContainer OwnerTags;
    if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
        ASC->GetOwnedGameplayTags(OwnerTags);
    }
    const FBreakerAbilityVariant Variant = Definition ? Definition->ResolveVariant(OwnerTags) : FBreakerAbilityVariant();
    const float Duration = Variant.WindowDuration > 0.0f ? Variant.WindowDuration : 12.0f;

    const bool bTriage = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Triage"), false);
    const bool bDownbeat = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Downbeat"), false);
    const bool bBlackout = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Support.Blackout"), false);

    // The window CARRIES the cost scalar (0.0 base and Downbeat, 1.0 Triage
    // and Blackout — the variant rows author it), so the ultimate and the
    // abilities it discounts cannot drift apart. Generation deliberately
    // CONTINUES: a well-played window partially refunds itself, bounded by
    // cooldowns (§3.1).
    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindowWithPayload(ConduitWindowKey(), Duration, Variant.AbilityCostMultiplier);
    }
    bConduitActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseConduit(); }), Duration, false);

    if (bTriage)
    {
        // §3.1 TRIAGE: a continuous healing field, every valid target — solo,
        // the set is the Support, unconditionally. The one-lethal-hit-save per
        // target is RECORDED ABSENT (no lethal-prevention hook exists).
        World->GetTimerManager().SetTimer(TriageTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { HandleTriagePulse(); }), 1.0f, /*bLoop=*/true);
    }
    else if (bDownbeat)
    {
        // §3.1 DOWNBEAT: free casts stay, cadence effects double (the cadence
        // payload is the recorded Cadence gap, so the doubling has nothing to
        // double yet), and every buffed target adds FLAT weapon damage — solo,
        // one buffed target: the Support.
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->PushOutgoingModifier(DownbeatModifierKey(), DownbeatFlatDamagePerBuffedTarget, 1.0f, Duration);
        }
    }
    else if (bBlackout)
    {
        // §3.1 BLACKOUT: marks and suppresses every enemy in radius INSTEAD
        // of casting abilities, and the marks are YOURS for generation.
        UBreakerCombatComponent* OwnCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Enemy = *It;
            if (!Enemy) continue;
            UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
            if (!EnemyCombat || EnemyCombat->IsDead()) continue;
            if (FVector::DistSquared(Character->GetActorLocation(), Enemy->GetActorLocation()) > RadiusCm * RadiusCm) continue;
            EnemyCombat->PushIncomingDamageModifier(BlackoutModifierKey(), BlackoutMarkedDamageMultiplier);
            Enemy->ApplyModifierMovementProfile(BlackoutSlowMultiplier, -1.0f);
            BlackoutTargets.Add(Enemy);
        }
        if (OwnCombat)
        {
            BoundCombat = OwnCombat;
            OwnCombat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Conduit::HandleBlackoutHit);
        }
    }
}

void UBreakerAbility_Conduit::HandleTriagePulse()
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character) return;
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        const float MaxHealth = BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Character);
        Combat->ApplyHealingAmount(MaxHealth * TriageHealFractionPerSecond, Character, FGameplayTag());
    }
}

void UBreakerAbility_Conduit::HandleBlackoutHit(const FBreakerHitContext& Hit)
{
    if (!bConduitActive || !Hit.Target || !BlackoutTargets.Contains(Hit.Target)) return;
    if (ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UBreakerChargeComponent* Charge = Character->FindComponentByClass<UBreakerChargeComponent>())
        {
            Charge->NotifyMarkedTargetDamage(Hit.Result.HealthDamage + Hit.Result.ShieldDamage,
                BreakerSupportAbilityLocal::BreakerSupportTargetMaxHealth(Hit.Target));
        }
    }
}

void UBreakerAbility_Conduit::CloseConduit()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Conduit::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bConduitActive)
    {
        bConduitActive = false;
        ABreakerCharacter* Character = GetBreakerCharacter();
        if (Character)
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(ConduitWindowKey());
            }
            if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
            {
                Combat->RemoveOutgoingModifier(DownbeatModifierKey());
            }
        }
        for (const TWeakObjectPtr<AActor>& Target : BlackoutTargets)
        {
            if (AActor* Enemy = Target.Get())
            {
                if (UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>())
                {
                    EnemyCombat->RemoveIncomingDamageModifier(BlackoutModifierKey());
                }
                if (ABreakerEnemy* AsEnemy = Cast<ABreakerEnemy>(Enemy))
                {
                    AsEnemy->ApplyModifierMovementProfile(1.0f, -1.0f);
                }
            }
        }
        BlackoutTargets.Reset();
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Conduit::HandleBlackoutHit);
        }
        BoundCombat.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
            World->GetTimerManager().ClearTimer(TriageTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
