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
#include "UI/BreakerEffectMomentMath.h"
#include "UI/BreakerEffectRenderer.h"

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
    return FMath::Max(0.0f, DurationSeconds) * FMath::Max(0.0f, AbilityDurationMultiplierFor(OwnerActor, EBreakerAbilityDurationKind::Zone));
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

void UBreakerAbility_Rot::SolveAim(const ABreakerCharacter& Character, const UWorld& World, FAimSolve& Out) const
{
    FVector ViewLocation = Character.GetActorLocation();
    FRotator ViewRotation = Character.GetControlRotation();
    if (const AController* Controller = Character.GetController())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerRotAim), false, &Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaximumRangeCm;
    const bool bHit = World.LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_GameTraceChannel2, QueryParams);
    Out.bFollowCaster = ShouldFollowCaster(&Character, bHit, Hit.ImpactPoint, Hit.ImpactNormal);
    Out.Center = Out.bFollowCaster ? Hit.ImpactPoint
        : AimPoint(ViewLocation, ViewRotation.Vector(), MaximumRangeCm, bHit, Hit.ImpactPoint);
    FVector& Center = Out.Center;

    // AND THEN IT FALLS TO THE FLOOR. Owner: "rot looks so weird casting
    // sometimes its in the air". AimPoint's own comment says a trace that hits
    // NOTHING should place the zone at the end of the reticle line rather than
    // at the caster's feet, and that is right about the HORIZONTAL position and
    // wrong about the vertical one: aim across a yard at nothing and the line
    // ends in mid-air, so a ground zone spawns hanging there. The same is true
    // of a trace that hits a WALL — the zone lands on the wall's face.
    //
    // So the aim decides WHERE and the floor decides HOW HIGH. Traced from
    // above the aim point so a shallow aim that landed just under the ground
    // still finds the surface it belongs on.
    {
        TArray<FHitResult> Ground;
        FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(BreakerRotFloor), false, &Character);
        constexpr float LiftCm = 400.0f;     // O2 PLACEHOLDER
        constexpr float ReachCm = 4000.0f;   // O2 PLACEHOLDER
        // EVERY SURFACE UNDER THE AIM POINT, not just the first one. The rule
        // that picks among them is BreakerRotFloor::PickFloorZ, which is pure
        // and tested; this is only the part that has to touch a world.
        World.LineTraceMultiByObjectType(Ground, Center + FVector(0, 0, LiftCm),
            Center - FVector(0, 0, ReachCm), FCollisionObjectQueryParams(ECC_WorldStatic), GroundQuery);
        TArray<BreakerRotFloor::FProbeHit> Probe;
        Probe.Reserve(Ground.Num());
        for (const FHitResult& Surface : Ground)
        {
            BreakerRotFloor::FProbeHit Entry;
            Entry.PointZ = Surface.ImpactPoint.Z;
            Entry.NormalZ = Surface.ImpactNormal.Z;
            Entry.bStartPenetrating = Surface.bStartPenetrating;
            Probe.Add(Entry);
        }
        float FloorZ = 0.0f;
        if (BreakerRotFloor::PickFloorZ(Probe, FloorZ))
        {
            Center.Z = FloorZ;
        }
        // NO FLOOR FOUND — a void, or a world with no geometry at all — and
        // the aim point is then LEFT EXACTLY WHERE IT WAS. Moving it to the
        // caster's feet was the first attempt and the suite refused it: four
        // ability fixtures cast Rot in empty worlds and assert where the zone
        // lands, so a fallback that relocates it broke the thing they measure.
        // It is also the wrong rule generally — this correction should only
        // ever fire on EVIDENCE of a floor, never on the absence of one.
    }
}

bool UBreakerAbility_Rot::PrepareCast()
{
    // THE PUDDLE LANDS WHERE THE PRESS POINTED, not where the reticle drifted
    // to during the wind-up (O266: the wind-up delays the resolution, not the
    // press). Solved here, before the price, and held until the landing.
    // Never a refusal: a mis-aim is a puddle in the wrong place, and a key
    // that does nothing is the worse feedback. A snapshot left over from a
    // cast that never landed — a refused commit, an interrupt — is simply
    // overwritten by the next press's solve.
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UWorld* World = Character ? Character->GetWorld() : nullptr;
    bAimSnapshotValid = false;
    if (!Character || !World) return true;
    SolveAim(*Character, *World, CastAimSnapshot);
    bAimSnapshotValid = true;
    return true;
}

bool UBreakerAbility_Rot::PrepareQueuedCast()
{
    // O271: THE QUEUED PRESS IS AIMED AT THE QUEUED PRESS. The active snapshot
    // belongs to the cast still winding up, so the second press solves into
    // its own slot and waits. Same solve, same never-refuse rule.
    const ABreakerCharacter* Character = GetBreakerCharacter();
    const UWorld* World = Character ? Character->GetWorld() : nullptr;
    bQueuedAimValid = false;
    if (!Character || !World) return true;
    SolveAim(*Character, *World, QueuedAimSnapshot);
    bQueuedAimValid = true;
    return true;
}

void UBreakerAbility_Rot::PromoteQueuedCast()
{
    // The first landing has consumed the active snapshot by the time this
    // runs; the queued one takes its place and the fresh cast that follows
    // skips PrepareCast, so this aim — not the reticle at the landing — is
    // where the second puddle goes.
    CastAimSnapshot = QueuedAimSnapshot;
    bAimSnapshotValid = bQueuedAimValid;
    bQueuedAimValid = false;
}

void UBreakerAbility_Rot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // O266: the wind-up. Returns false when it has started a cast — the cost
    // is already paid, the window is open, and this function is called again
    // when the wind-up completes. Zero authored cast time is a no-op.
    if (!BeginCastIfNeeded(Handle, ActorInfo, ActivationInfo)) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // THE AIM COMES FROM THE CAST START WHEN THERE WAS ONE. PrepareCast solved
    // it at the press; this is the landing, and re-solving here would let the
    // puddle follow the crosshair through the wind-up, which is the drift the
    // owner reported. With no authored cast time there is no snapshot and the
    // same solve runs inline, which is what keeps a zero-cast-time build
    // identical.
    FAimSolve Aimed;
    if (bAimSnapshotValid)
    {
        bAimSnapshotValid = false;
        Aimed = CastAimSnapshot;
    }
    else
    {
        SolveAim(*Character, *World, Aimed);
    }
    const bool bFollowCaster = Aimed.bFollowCaster;
    const FVector Center = Aimed.Center;

    // AND THE CAST IS VISIBLE AT THE CASTER. The burst at the hand is the
    // character's (O284, HandleAbilityCast on OnAbilityActivated), one for
    // every ability; this composition draws the throw from the chest to the
    // disc, so the disc has a cause and a direction.
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World))
    {
        const FVector Chest = Character->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
        const FLinearColor Paint = GetPresentationColor();
        // A short fall of light from the caster's hand to the ground it lands
        // on.
        BreakerFX::FEffectTiming Throw;
        Throw.DurationSeconds = 0.26f;
        Throw.FadeInSeconds = 0.02f;
        Throw.FadeOutSeconds = 0.18f;
        for (int32 Step = 0; Step < 3; ++Step)
        {
            const float Near = 0.18f + 0.26f * Step;
            const float Far = Near + 0.24f;
            Effects->AddStroke(FMath::Lerp(Chest, Center, Near), FMath::Lerp(Chest, Center, Far),
                3.0f, Paint, 2.2f, Throw, 0.03f * Step);
        }
    }

    const FGameplayTag ZoneTag = BreakerAbilityTags::Zone_Caster_Rot.GetTag();

    // The geometry seam, read once for the whole cast so the spawned volume
    // and the Wellspring refresh share one reading.
    const float EffectiveRadiusCm = ComputeEffectiveRadiusCm(Character);
    const float EffectiveDuration = ComputeEffectiveDurationSeconds(Character);
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

    Spec.TickDamage.BaseDamage = AbilityBaseDamageFor(Character, ZoneDamagePerTick * LevelScalar);
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

    // THE ONE REFRESH THAT REMAINS is Wellspring's: a puddle riding the caster
    // is the caster's puddle, and a second one riding the same caster would be
    // the same puddle twice. A recast while one follows renews it in place.
    auto RefreshExisting = [&](ABreakerZoneActor* Existing)
    {
        Existing->RefreshPaidPayload(Spec);
        if (Character->GetProgression()->GetNodeRank(TEXT("Caster.VoidWhisperer.Lingering"), EBreakerPointCurrency::DoctrinePoints) >= 1   /* O272: single rank */)
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

    // A RECAST SPAWNS A NEW PUDDLE. NOTHING MERGES INTO A LIVE ONE (O271).
    // VW4's anti-stack rule used to run here — a recast within half a radius
    // of a live own Rot refreshed that one instead of spawning — and the
    // owner felt it as the second press doing nothing: the puddle he was
    // looking at got a little longer and no new puddle appeared. Two puddles
    // still cannot double-strip the same target; that guard is the zone
    // actor's own key, not a merge at the spawner.
    //
    // LINGERING R2 IS A RULE ABOUT THE NEW PUDDLE. "A zone cast over a live
    // one grows by 1 m": the ground you keep working stays worked. Half a
    // radius is the same overlap the old merge read, so the gesture that used
    // to do nothing now lands a bigger disc. A fresh zone is grown once by
    // construction; the old one is not touched. (Owner, on the desk's
    // question; the following puddle keeps its renew-and-grow.)
    if (!bFollowCaster
        && Character->GetProgression()->GetNodeRank(TEXT("Caster.VoidWhisperer.Lingering"), EBreakerPointCurrency::DoctrinePoints) >= 1   /* O272: single rank */
        && ABreakerZoneActor::FindRefreshableZone(World, Spec.ZoneTag, Character, Center, EffectiveRadiusCm, 0.5f))
    {
        Spec.RadiusCm += LingeringRefreshGrowthCm;
    }
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
