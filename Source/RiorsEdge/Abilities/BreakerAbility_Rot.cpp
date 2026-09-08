#include "Abilities/BreakerAbility_Rot.h"

#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Combat/BreakerZoneActor.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "UI/BreakerEffectMath.h"

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

float UBreakerAbility_Rot::ComputeEffectiveRadiusCm(const AActor* OwnerActor) const
{
    return FMath::Max(0.0f, RadiusCm) * FMath::Max(0.0f, AbilityAreaMultiplierFor(OwnerActor));
}

float UBreakerAbility_Rot::ComputeEffectiveDurationSeconds(const AActor* OwnerActor) const
{
    // The AbilityDuration lane. Lingering's "zones linger" line lands here —
    // and on the refresh path below, so a refreshed puddle lingers too.
    return FMath::Max(0.0f, DurationSeconds) * FMath::Max(0.0f, AbilityDurationMultiplierFor(OwnerActor));
}

bool UBreakerAbility_Rot::ShouldFollowCaster(const AActor* OwnerActor, bool bGroundHit, const FVector& HitPoint, const FVector& HitNormal) const
{
    const ACharacter* Character = Cast<ACharacter>(OwnerActor);
    const UBreakerProgressionComponent* Progression = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    return Character && Character->GetCharacterMovement()->IsMovingOnGround() && Progression
        && Progression->GetNodeRank(TEXT("Caster.VoidWhisperer.Wellspring"), EBreakerPointCurrency::DoctrinePoints) > 0
        && bGroundHit && HitNormal.Z >= WellspringMinimumGroundNormalZ
        && FVector::Dist2D(OwnerActor->GetActorLocation(), HitPoint) <= WellspringSelfPlacementRadiusCm;
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
    const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams);
    const bool bFollowCaster = ShouldFollowCaster(Character, bHit, Hit.ImpactPoint, Hit.ImpactNormal);
    const FVector Center = bFollowCaster ? Hit.ImpactPoint
        : AimPoint(ViewLocation, ViewRotation.Vector(), MaximumRangeCm, bHit, Hit.ImpactPoint);

    const FGameplayTag ZoneTag = BreakerAbilityTags::Zone_Caster_Rot.GetTag();

    // The geometry seam, read once for the whole cast so the anti-stack
    // search, the spawned volume and the refresh all share one reading.
    const float EffectiveRadiusCm = ComputeEffectiveRadiusCm(Character);
    const float EffectiveDuration = ComputeEffectiveDurationSeconds(Character);
    auto RefreshExisting = [&](ABreakerZoneActor* Existing)
    {
        Existing->RefreshDuration(EffectiveDuration);
        if (Character->GetProgression()->GetNodeRank(TEXT("Caster.VoidWhisperer.Lingering"), EBreakerPointCurrency::DoctrinePoints) >= 2)
            Existing->GrowRadiusOnce(LingeringRefreshGrowthCm);
    };
    if (bFollowCaster)
        for (const TWeakObjectPtr<ABreakerZoneActor>& Held : ABreakerZoneActor::GetLiveZones())
            if (ABreakerZoneActor* Existing = Held.Get())
                if (Existing->GetWorld() == World && Existing->GetZoneInstigator() == Character
                    && Existing->GetSpec().ZoneTag == ZoneTag && Existing->GetFollowActor() == Character
                    && Existing->GetRemainingDuration() > 0.0f)
                {
                    RefreshExisting(Existing);
                    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
                    return;
                }

    // VW4's anti-stack rule lives at the SPAWNER, once, exactly as the spec
    // requires — never per ability. A recast on top of a live Rot refreshes it;
    // two Rots do not stack their armour strip, and the zone actor's key makes
    // sure even genuinely separate puddles cannot double-strip.
    if (ABreakerZoneActor* Existing = ABreakerZoneActor::FindRefreshableZone(World, ZoneTag, Character, Center, EffectiveRadiusCm, 0.5f))
    {
        RefreshExisting(Existing);
        if (bFollowCaster) Existing->SetFollowActor(Character);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();
    const UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
    // O35: one item-level reading for the whole cast's Entropy hits.
    const float LevelScalar = AbilityDamageScalarFor(Character);

    FBreakerZoneSpec Spec;
    Spec.ZoneTag = ZoneTag;
    Spec.RadiusCm = EffectiveRadiusCm;
    Spec.HalfHeightCm = HalfHeightCm;
    Spec.Duration = EffectiveDuration;
    Spec.TickInterval = TickIntervalSeconds;
    Spec.FlatArmorReduction = FlatArmorReduction;
    if (Character->GetProgression()->GetNodeRank(TEXT("Caster.VoidWhisperer.Zonework"), EBreakerPointCurrency::DoctrinePoints) > 0)
        Spec.AfflictedArmorReduction = ZoneworkAdditionalArmorReduction;
    // The footprint shares Entropy's projectile, activation and meter palette.
    Spec.ZoneColor = BreakerFX::ColorForStatusTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Rot")), FLinearColor::White);

    Spec.TickDamage.BaseDamage = ZoneDamagePerTick * LevelScalar;
    Spec.TickDamage.DamageFamily = EBreakerDamageFamily::Elemental;
    Spec.TickDamage.Element = EBreakerElement::Entropy;
    Spec.TickDamage.ElementalFraction = 1.0f;
    Spec.TickDamage.SourceTags.AddTag(BreakerAbilityTags::Ability_Class_Caster_Rot.GetTag());
    Spec.TickDamage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Spec.TickDamage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    UBreakerDamageLibrary::FillSourcePools(SourceAttributes, EBreakerDamageDelivery::Ability, Spec.TickDamage);
    Spec.TickDamage.SetInstigator(Character);
    if (const auto* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>()) State->SnapshotSympatheticEntropy(Spec.TickDamage);

    // Rot now builds Entropy on accepted zone hits; physical Poison remains a separate status.

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    SpawnParams.Instigator = Character;
    if (ABreakerZoneActor* Zone = World->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams))
    {
        Zone->ConfigureZone(Spec, Character);
        if (bFollowCaster) Zone->SetFollowActor(Character);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
