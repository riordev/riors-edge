#include "Abilities/BreakerAbility_Siphon.h"

#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerUIStyle.h"

UBreakerAbility_Siphon::UBreakerAbility_Siphon()
{
    FallbackAbilityId = TEXT("Caster.Siphon");
    // Spec §5.4: it damages another pawn on a timer, so it never runs on a
    // client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    ActivationOwnedTags.AddTag(BreakerAbilityTags::State_Ability_Siphon.GetTag());
    // The channel blocks its own recast. Spec §4.2 asks for
    // BlockAbilitiesWithTag Ability.Class on channels; that would block the
    // whole class, which is a rule about the ability system's tag vocabulary
    // and not about Siphon, so this blocks only itself until the vocabulary
    // exists. Recasting Siphon mid-channel is the case that must not silently
    // start a second overlapping drain.
    ActivationBlockedTags.AddTag(BreakerAbilityTags::State_Ability_Siphon.GetTag());

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Siphon.GetTag());
    SetAssetTags(Tags);
}

FName UBreakerAbility_Siphon::ChannelWindowKey()
{
    return TEXT("Window.Caster.Siphon");
}

float UBreakerAbility_Siphon::HealForTick(float DamageDealt, float LeechFraction)
{
    return FMath::Max(0.0f, DamageDealt) * FMath::Max(0.0f, LeechFraction);
}

bool UBreakerAbility_Siphon::ShouldBreakChannel(float HealthDamageTaken, float MaxHealth, float BreakThresholdFraction)
{
    if (HealthDamageTaken <= 0.0f) return false;
    // A threshold of zero means ANY damage breaks it, which is a legal and
    // meaningful authoring choice, so it must not be read as "no threshold".
    if (MaxHealth <= 0.0f) return true;
    // "above a threshold" — the boundary does NOT break, matching the
    // at-or-below convention Closequarter's refund already uses.
    return HealthDamageTaken > MaxHealth * FMath::Max(0.0f, BreakThresholdFraction);
}

void UBreakerAbility_Siphon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    // Acquisition before the commit: a channel with nothing to drain is a dead
    // key, not a risk (the Closequarter precedent).
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerSiphon), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaximumRangeCm;
    AActor* Target = nullptr;
    if (World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams) && Hit.GetActor())
    {
        if (Hit.GetActor()->FindComponentByClass<UBreakerCombatComponent>()) Target = Hit.GetActor();
    }

    if (!Target || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ChannelTarget = Target;
    TicksDelivered = 0;

    // The break condition is a binding, not a poll: the caster's combat
    // component already announces every hit it takes, and polling health would
    // miss a hit that was healed back before the next tick.
    if (UBreakerCombatComponent* CasterCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        BoundCasterCombat = CasterCombat;
        CasterCombat->OnDamageReceived.AddDynamic(this, &ThisClass::HandleCasterDamaged);
    }

    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        // Published for the HUD; the ability's own lifetime is the GAS
        // activation, not this window.
        State->StartWindow(ChannelWindowKey(), ChannelSeconds);
    }

    // THE BEAM. Anchored to both actors so it tracks caster and target every
    // frame with no work here, sized to the channel's authored maximum, and
    // ended THROUGH the handle in StopChannel — which every early exit
    // funnels through, so the beam cannot outlive the channel by even a
    // tick. Chest lift and figures O2 PLACEHOLDER.
    // (Server-only ability, cosmetic call — see BreakerEffectRenderer.h.)
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        BreakerFX::FEffectTiming BeamTiming;
        BeamTiming.DurationSeconds = FMath::Max(0.05f, ChannelSeconds);
        BeamTiming.FadeInSeconds = 0.08f;
        BeamTiming.FadeOutSeconds = 0.15f;
        BeamHandle = Effects->AddBeam(Character, Target, 5.0f, BreakerUI::Cyan, 2.6f, BeamTiming, 50.0f);
    }

    World->GetTimerManager().SetTimer(ChannelTimer, this, &ThisClass::TickChannel,
        FMath::Max(0.05f, TickIntervalSeconds), true, FMath::Max(0.05f, TickIntervalSeconds));
    // A hard end, independent of the tick timer. A channel whose only exit is
    // its own tick handler outlives its duration whenever a tick is skipped.
    TWeakObjectPtr<UBreakerAbility_Siphon> WeakThis(this);
    World->GetTimerManager().SetTimer(ChannelEndTimer, FTimerDelegate::CreateLambda([WeakThis]()
    {
        if (UBreakerAbility_Siphon* Ability = WeakThis.Get())
        {
            Ability->EndAbility(Ability->CurrentSpecHandle, Ability->CurrentActorInfo, Ability->CurrentActivationInfo, true, false);
        }
    }), FMath::Max(0.05f, ChannelSeconds), false);
}

void UBreakerAbility_Siphon::TickChannel()
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    AActor* Target = ChannelTarget.Get();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !Target)
    {
        // The target died or was destroyed. The channel ends; it does not hunt
        // for a new one.
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    if (FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation()) > BreakDistanceCm)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    ++TicksDelivered;
    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();

    FBreakerDamageRequest Damage;
    // O35: flat ability damage rides the equipped weapon's item-level scalar.
    // Read per tick, not snapshotted at channel start, because a channel is a
    // repeated LIVE hit — the same reason it carries no bIsDamageOverTime.
    Damage.BaseDamage = DamagePerTick * AbilityDamageScalarFor(Character);
    // O5: Elemental is the pipeline family until the resistance model lands;
    // Void rides on the type tag, so Siphon gains resistance interaction later
    // with no rewrite.
    Damage.DamageFamily = EBreakerDamageFamily::Elemental;
    Damage.DamageTypeTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Void"), false);
    Damage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Siphon.GetTag());
    Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    UBreakerDamageLibrary::FillSourcePools(SourceAttributes, EBreakerDamageDelivery::Ability, Damage);
    // A channel is not a damage-over-time STATUS: it is a repeated direct hit
    // from a live ability, so it does not snapshot and it does not carry
    // bIsDamageOverTime. Setting that flag would suppress Mana generation on a
    // cast the player is actively paying attention to, and would exempt it from
    // the passive defensive layer it should respect.
    Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(TicksDelivered));
    Damage.SourceLocation = Character->GetActorLocation();
    Damage.bHasSourceLocation = true;
    Damage.SetInstigator(Character);

    UBreakerCombatComponent* CasterCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
    if (CasterCombat) CasterCombat->ApplyOutgoingModifiers(Damage);

    const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);

    // Heal on DAMAGE LANDED, not on damage requested: a dodged, blocked or
    // shield-absorbed tick must not pay the caster full value. This is the
    // whole reason the heal is a portion of the RESULT.
    const float HealAmount = HealForTick(Result.HealthDamage + Result.ShieldDamage, LeechFraction);
    if (HealAmount > 0.0f && CasterCombat)
    {
        FBreakerHealRequest Heal;
        Heal.Amount = HealAmount;
        Heal.SourceTag = BreakerAbilityTags::Ability_Class_Caster_Siphon.GetTag();
        Heal.SetHealer(Character);
        CasterCombat->ApplyHealing(Heal);
    }
}

void UBreakerAbility_Siphon::HandleCasterDamaged(const FBreakerDamageResult& Result)
{
    const UBreakerAttributeSet* Attributes = GetBreakerAttributes();
    const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.0f;
    if (!ShouldBreakChannel(Result.HealthDamage, MaxHealth, BreakThresholdFraction)) return;
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled*/ true);
}

void UBreakerAbility_Siphon::StopChannel()
{
    if (const ABreakerCharacter* Character = GetBreakerCharacter())
    {
        if (UWorld* World = Character->GetWorld())
        {
            World->GetTimerManager().ClearTimer(ChannelTimer);
            World->GetTimerManager().ClearTimer(ChannelEndTimer);
            // The beam breaks the instant the channel does — a leash break,
            // a qualifying hit, target death and natural expiry all arrive
            // here, and the duration rewrite gives every one of them the
            // same 0.15 s snap-out. Stale handle after expiry is a no-op.
            if (BeamHandle != 0)
            {
                if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
                {
                    Effects->EndEffect(BeamHandle, 0.15f);
                }
                BeamHandle = 0;
            }
        }
        if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
        {
            State->CloseWindow(ChannelWindowKey());
        }
    }
    if (UBreakerCombatComponent* Bound = BoundCasterCombat.Get())
    {
        Bound->OnDamageReceived.RemoveDynamic(this, &ThisClass::HandleCasterDamaged);
    }
    BoundCasterCombat = nullptr;
    ChannelTarget = nullptr;
}

void UBreakerAbility_Siphon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // Unconditional teardown, including cancel and death — the same rule
    // Unmake's window follows. A channel left ticking after its ability ended
    // is an invisible permanent damage source with a live delegate binding.
    StopChannel();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
