#include "Abilities/BreakerAbility_Rot.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

UBreakerAbility_Rot::UBreakerAbility_Rot()
{
    FallbackAbilityId = TEXT("Caster.Rot");
    // Spec §5.3: it spawns an actor and damages a volume, so it never runs on
    // a client.
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    FGameplayTagContainer Tags;
    Tags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Rot.GetTag());
    SetAssetTags(Tags);
}

FVector UBreakerAbility_Rot::AimPoint(const FVector& ViewLocation, const FVector& ViewDirection, float MaximumRangeCm, bool bTraceHit, const FVector& HitLocation)
{
    if (bTraceHit) return HitLocation;
    // Nothing under the reticle: place the puddle at the end of the aim line
    // rather than refusing the cast or dropping it underfoot. Aiming at the sky
    // is a mis-aim, and the honest feedback for a mis-aim is a puddle in the
    // wrong place, not a swallowed input.
    const FVector Unit = ViewDirection.GetSafeNormal();
    return ViewLocation + (Unit.IsNearlyZero() ? FVector::ForwardVector : Unit) * FMath::Max(0.0f, MaximumRangeCm);
}

void UBreakerAbility_Rot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerRotAim), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaximumRangeCm;
    const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);
    const FVector Center = AimPoint(ViewLocation, ViewRotation.Vector(), MaximumRangeCm, bHit, Hit.ImpactPoint);

    const FGameplayTag ZoneTag = BreakerAbilityTags::Zone_Caster_Rot.GetTag();

    // VW4's anti-stack rule lives at the SPAWNER, once, exactly as the spec
    // requires — never per ability. A recast on top of a live Rot refreshes it;
    // two Rots do not stack their armour strip, and the zone actor's key makes
    // sure even genuinely separate puddles cannot double-strip.
    if (ABreakerZoneActor* Existing = ABreakerZoneActor::FindRefreshableZone(World, ZoneTag, Character, Center, RadiusCm, 0.5f))
    {
        Existing->RefreshDuration(DurationSeconds);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();

    FBreakerZoneSpec Spec;
    Spec.ZoneTag = ZoneTag;
    Spec.RadiusCm = RadiusCm;
    Spec.HalfHeightCm = HalfHeightCm;
    Spec.Duration = DurationSeconds;
    Spec.TickInterval = TickIntervalSeconds;
    Spec.FlatArmorReduction = FlatArmorReduction;
    // Not teal (O19): saturated teal is a property of rift objects and
    // suppression hardware, and a Void Whisperer puddle is neither. Sick green.
    Spec.ZoneColor = FLinearColor(0.34f, 0.78f, 0.20f);

    Spec.TickDamage.BaseDamage = ZoneDamagePerTick;
    Spec.TickDamage.DamageFamily = EBreakerDamageFamily::Physical;
    Spec.TickDamage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Rot.GetTag());
    Spec.TickDamage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
    Spec.TickDamage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
    Spec.TickDamage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    Spec.TickDamage.SetInstigator(Character);

    // Poison is the payload Class-Kits names. It is a snapshotting DoT like
    // every other, so it captures the caster's offensive stats HERE, at
    // application, and one critical roll decides every tick of an application —
    // the locked DoT ruling, not a per-zone choice.
    Spec.bAppliesStatus = true;
    Spec.StatusFamily = EBreakerDamageFamily::Physical;
    Spec.StatusSpec.StatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Poison"), false);
    Spec.StatusSpec.BaseDamagePerTick = PoisonDamagePerTick;
    Spec.StatusSpec.Duration = PoisonDuration;
    Spec.StatusSpec.TickInterval = PoisonTickInterval;
    Spec.StatusSpec.Snapshot.SourcePower = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
    Spec.StatusSpec.Snapshot.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : 0.05f;
    Spec.StatusSpec.Snapshot.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : 1.5f;
    Spec.StatusSpec.Snapshot.DamageOverTimeMultiplier = SourceAttributes ? SourceAttributes->GetDamageOverTimeMultiplier() : 1.0f;
    FRandomStream Stream(static_cast<int32>(HashCombine(GetTypeHash(Character), static_cast<uint32>(World->GetTimeSeconds() * 1000.0))));
    Spec.StatusSpec.Snapshot.bRolledCritical = Stream.FRand() < Spec.StatusSpec.Snapshot.CriticalChance;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    SpawnParams.Instigator = Character;
    if (ABreakerZoneActor* Zone = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams))
    {
        Zone->ConfigureZone(Spec, Character);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
