#include "Abilities/BreakerTankAbilities.h"

#include "AbilitySystemComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Abilities/BreakerAbilityStateComponent.h"
#include "Abilities/BreakerAbilityTags.h"
#include "Abilities/BreakerMeleeSweep.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerGritComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerDeployable.h"
#include "Combat/BreakerEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Items/BreakerEquipmentComponent.h"
#include "TimerManager.h"
#include "Weapons/BreakerWeaponComponent.h"

namespace BreakerTankAbilityLocal
{
    // Prefixed for the unity build, per house rule.

    // The shared "weapon-scaled with an unarmed floor" base the melee/blast
    // verbs use — the Cleave precedent (O35: reads the SCALED weapon base, so
    // ability damage rides gear depth; item level 1 is the authored number).
    // FULL BLAST, not per pellet (audit F1, the Cleave fix verbatim): the
    // per-pellet base made a shotgun Tank — the class/weapon pairing the
    // theme is built on — swing 10 while a sniper Tank swung 72. Rend,
    // Breach Charge and Ground Zero all mean the whole trigger pull by
    // "weapon damage", so all three read the full-blast accessor through
    // this one seam.
    float BreakerTankAbilityBaseDamage(const ABreakerCharacter* Character, float WeaponCoefficient, float UnarmedDamage)
    {
        float WeaponDamage = 0.0f;
        if (const UBreakerWeaponComponent* Weapon = Character ? Character->GetWeapon() : nullptr)
        {
            if (Weapon->GetActiveDefinition())
            {
                WeaponDamage = Weapon->GetScaledFullBlastDamage();
            }
        }
        const float ScaledUnarmed = UnarmedDamage * UBreakerGameplayAbility::AbilityDamageScalarFor(Character);
        const float Base = WeaponDamage > 0.0f ? WeaponDamage * WeaponCoefficient : ScaledUnarmed;
        return FMath::Max(0.0f, Base);
    }

    // One radial damage application against enemies only, linear falloff to
    // the edge fraction — the rocket's falloff shape. Returns total damage
    // actually applied to health+shield.
    float BreakerTankRadialDamage(UWorld* World, ABreakerCharacter* Character, const FVector& Center,
        float RadiusCm, float BaseDamage, float EdgeFraction, bool bApplyFalloff)
    {
        if (!World || BaseDamage <= 0.0f) return 0.0f;
        const UBreakerAttributeSet* SourceAttributes = Character ? Character->GetAttributes() : nullptr;
        UBreakerCombatComponent* OwnerCombat = Character ? Character->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        float TotalApplied = 0.0f;
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Enemy = *It;
            if (!Enemy) continue;
            UBreakerCombatComponent* TargetCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
            if (!TargetCombat || TargetCombat->IsDead()) continue;
            const float Distance = FVector::Dist(Center, Enemy->GetActorLocation());
            if (Distance > RadiusCm) continue;
            const float Falloff = bApplyFalloff
                ? FMath::Lerp(1.0f, EdgeFraction, FMath::Clamp(Distance / RadiusCm, 0.0f, 1.0f))
                : 1.0f;

            FBreakerDamageRequest Damage;
            Damage.BaseDamage = BaseDamage * Falloff;
            Damage.DamageFamily = EBreakerDamageFamily::Physical;
            Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
            Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
            Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
            Damage.SourceLocation = Center;
            Damage.bHasSourceLocation = true;
            Damage.SetInstigator(Character);
            if (OwnerCombat) OwnerCombat->ApplyOutgoingModifiers(Damage);
            const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);
            TotalApplied += Result.HealthDamage + Result.ShieldDamage;
        }
        return TotalApplied;
    }
}

// ---------------------------------------------------------------------------
// T1 — REND
// ---------------------------------------------------------------------------

UBreakerAbility_Rend::UBreakerAbility_Rend()
{
    FallbackAbilityId = TEXT("Tank.Rend");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_Rend::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
    FVector Forward = ViewRotation.Vector();
    Forward.Z = 0.0;
    if (!Forward.Normalize()) Forward = Character->GetActorForwardVector();

    FBreakerMeleeSweepParams Params;
    Params.Origin = Character->GetActorLocation();
    Params.Forward = Forward;
    Params.RangeCm = RangeCm;
    Params.ArcDegrees = ArcDegrees;

    const float BaseDamage = BreakerTankAbilityLocal::BreakerTankAbilityBaseDamage(Character, WeaponDamageCoefficient, UnarmedDamage);
    const UBreakerAttributeSet* SourceAttributes = GetBreakerAttributes();
    UBreakerCombatComponent* OwnerCombat = Character->FindComponentByClass<UBreakerCombatComponent>();
    UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>();

    float TotalPostMitigation = 0.0f;
    int32 TargetIndex = 0;
    for (AActor* Target : UBreakerMeleeSweep::SweepTargets(World, Character, Params))
    {
        UBreakerCombatComponent* TargetCombat = Target ? Target->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
        if (!TargetCombat) continue;

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage;
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        // Damage.Melee gates the melee affix/More layer, exactly as Cleave.
        Damage.SourceTags.AddTag(BreakerAbilityTags::Damage_Melee.GetTag());
        Damage.CriticalChance = SourceAttributes ? SourceAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = SourceAttributes ? SourceAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        Damage.SourceDamageMultiplier = SourceAttributes ? SourceAttributes->GetDamageMultiplier() : 1.0f;
        Damage.RandomSeed = HashCombine(GetTypeHash(Character), static_cast<uint32>(TargetIndex) + static_cast<uint32>(World->GetTimeSeconds() * 1000.0));
        Damage.SourceLocation = Params.Origin;
        Damage.bHasSourceLocation = true;
        Damage.SetInstigator(Character);
        if (OwnerCombat) OwnerCombat->ApplyOutgoingModifiers(Damage);
        const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);
        TotalPostMitigation += Result.HealthDamage + Result.ShieldDamage;

        // §1.1's aggression source: a melee kill pays Grit, and Rend is the
        // Tank's melee verb — the one honest caller NotifyMeleeKill has.
        if (Result.bKilled && Grit)
        {
            Grit->NotifyMeleeKill();
        }
        ++TargetIndex;
    }

    // §T1: heals 35% of POST-MITIGATION damage dealt; overheal becomes shield.
    // Two recorded differences from the doc: the shield cap here is a fraction
    // of MaxShield (the healing path's cap) rather than 25% of maximum health,
    // and the granted shield does not decay (no shield-decay primitive exists).
    if (TotalPostMitigation > 0.0f && OwnerCombat)
    {
        FBreakerHealRequest Heal;
        Heal.Amount = TotalPostMitigation * HealFraction;
        Heal.bOverhealToShield = true;
        Heal.OverhealToShieldFraction = OverhealShieldFraction;
        Heal.SourceTag = FGameplayTag();   // no heal-source vocabulary exists yet; empty is the honest tag
        Heal.SetHealer(Character);
        OwnerCombat->ApplyHealing(Heal);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// T2 — BLOODLINE
// ---------------------------------------------------------------------------

UBreakerAbility_Bloodline::UBreakerAbility_Bloodline()
{
    FallbackAbilityId = TEXT("Tank.Bloodline");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Bloodline::WindowKey() { return TEXT("Window.Tank.Bloodline"); }

void UBreakerAbility_Bloodline::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
    BoundCombat = Combat;
    Combat->OnHitDealt.AddDynamic(this, &UBreakerAbility_Bloodline::HandleHitDealt);
    bBloodlineActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseBloodline(); }), Duration, false);
}

void UBreakerAbility_Bloodline::HandleHitDealt(const FBreakerHitContext& Hit)
{
    if (!bBloodlineActive) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character) return;
    const UBreakerEquipmentComponent* Equipment = Character->GetEquipment();
    // MULTIPLIES WHAT YOU HAVE, GRANTS NOTHING IF YOU HAVE NONE (§T2): the
    // gear stat is the whole payout, so a Tank with no leech-side gear feels
    // Bloodline do exactly nothing — the intended gear-payoff identity. DoT
    // ticks pay too (the hit context flags them), which is the §T2 extension.
    const float LeechStat = Equipment ? Equipment->GetStats().LifeOnKill : 0.0f;
    if (LeechStat <= 0.0f) return;
    if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
    {
        Combat->ApplyHealingAmount(LeechStat, Character, FGameplayTag());
    }
}

void UBreakerAbility_Bloodline::CloseBloodline()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Bloodline::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bBloodlineActive)
    {
        bBloodlineActive = false;
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->OnHitDealt.RemoveDynamic(this, &UBreakerAbility_Bloodline::HandleHitDealt);
        }
        BoundCombat.Reset();
        if (ABreakerCharacter* Character = GetBreakerCharacter())
        {
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------------------
// T3 — ANCHOR POINT
// ---------------------------------------------------------------------------

UBreakerAbility_AnchorPoint::UBreakerAbility_AnchorPoint()
{
    FallbackAbilityId = TEXT("Tank.AnchorPoint");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_AnchorPoint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!Character || !World)
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
    FVector Forward = ViewRotation.Vector();
    Forward.Z = 0.0;
    if (!Forward.Normalize()) Forward = Character->GetActorForwardVector();

    // Validate BEFORE commit — a refused placement costs no Grit, matching the
    // deployable rule (§2.3 by symmetry).
    FVector PlaceLocation;
    if (!ABreakerDeployable::ResolvePlacement(World, Character, Character->GetActorLocation() + FVector(0, 0, 50.0f), Forward, PlacementRangeCm, PlaceLocation))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.Owner = Character;
    // Faces back at the Tank, so the panel stands between the Tank and where
    // they were looking.
    const FRotator PanelFacing = FRotator(0.0f, Forward.Rotation().Yaw, 0.0f);
    if (ABreakerDeployable* Panel = World->SpawnActor<ABreakerDeployable>(ABreakerDeployable::StaticClass(), PlaceLocation, PanelFacing, SpawnParams))
    {
        // Cost 0 into the deployable: Grit was already spent through the GAS
        // cost effect, and Anchor Point refunds nothing ("this is not a Scrap
        // economy", §T3).
        Panel->InitializeDeployable(EBreakerDeployableType::AnchorPoint, Character, 0.0f);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// T4 — PROVOKE
// ---------------------------------------------------------------------------

UBreakerAbility_Provoke::UBreakerAbility_Provoke()
{
    FallbackAbilityId = TEXT("Tank.Provoke");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Provoke::OutgoingModifierKey() { return TEXT("Provoke"); }

void UBreakerAbility_Provoke::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Count the provoked. The FORCED-TARGETING half is honestly absent (no
    // threat concept on enemy AI — Class-Kits-Unbuilt §5.3); solo it is
    // vacuous anyway. The SOLO CONVERSION below is §T4's implemented half.
    int32 Provoked = 0;
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        const ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;
        const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        if (!EnemyCombat || EnemyCombat->IsDead()) continue;
        if (FVector::DistSquared(Character->GetActorLocation(), Enemy->GetActorLocation()) <= RadiusCm * RadiusCm)
        {
            ++Provoked;
        }
    }

    // §T4: each enemy provoked grants stacking FLAT damage (+4% of weapon base
    // per enemy, 6 stacks max, 6s), delivered as a flat contribution so it
    // cannot double-dip with the Increased bucket.
    if (Provoked > 0)
    {
        float WeaponBase = 0.0f;
        if (const UBreakerWeaponComponent* Weapon = Character->GetWeapon())
        {
            WeaponBase = Weapon->GetScaledBaseDamage();
        }
        const int32 Stacks = FMath::Min(Provoked, MaximumStacks);
        const float FlatBonus = WeaponBase * FlatDamagePerEnemyFraction * Stacks;
        if (UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->PushOutgoingModifier(OutgoingModifierKey(), FlatBonus, 1.0f, BonusDurationSeconds);
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// T5 — BREACH CHARGE
// ---------------------------------------------------------------------------

UBreakerAbility_BreachCharge::UBreakerAbility_BreachCharge()
{
    FallbackAbilityId = TEXT("Tank.BreachCharge");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_BreachCharge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

    // The throw resolves as an aim trace rather than a projectile actor: no
    // grenade arc exists to reuse and inventing one is not this pass. Where
    // the ray lands, the charge sits; 1.2s later it detonates (§T5).
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerBreachAim), false, Character);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * ThrowRangeCm;
    const FVector BlastLocation = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams)
        ? Hit.ImpactPoint : TraceEnd;

    TWeakObjectPtr<UBreakerAbility_BreachCharge> WeakThis(this);
    World->GetTimerManager().SetTimer(FuseTimer, FTimerDelegate::CreateLambda([WeakThis, BlastLocation]()
    {
        if (UBreakerAbility_BreachCharge* Ability = WeakThis.Get())
        {
            Ability->Detonate(BlastLocation);
        }
    }), FMath::Max(0.05f, FuseSeconds), false);
    // The ability itself ends now; the fuse timer owns the detonation. The
    // cooldown (8s) already prevents a second charge racing the first fuse.
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UBreakerAbility_BreachCharge::Detonate(FVector BlastLocation)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    if (!World) return;

    const float BaseDamage = BreakerTankAbilityLocal::BreakerTankAbilityBaseDamage(Character, WeaponDamageCoefficient, UnarmedDamage);
    BreakerTankAbilityLocal::BreakerTankRadialDamage(World, Character, BlastLocation, BlastRadiusCm, BaseDamage, EdgeDamageFraction, /*bApplyFalloff=*/true);

    // The Tank's end of the tool: knockback with FULL directional control (the
    // impulse is the blast normal, undamped) and self-damage that is reduced
    // and NEVER zero (O13). The self-hit goes through the Tank's own combat
    // component as a real self-instigated request, so post-mitigation Grit
    // generation sees it at the self-damage rate through the ordinary wiring.
    const float SelfDistance = FVector::Dist(BlastLocation, Character->GetActorLocation());
    if (SelfDistance <= BlastRadiusCm)
    {
        FVector Away = Character->GetActorLocation() - BlastLocation;
        if (!Away.Normalize()) Away = FVector::UpVector;
        // Guarantee real lift so a floor-adjacent blast still moves the Tank.
        Away.Z = FMath::Max(Away.Z, 0.35f);
        Away.Normalize();
        const float Proximity = 1.0f - FMath::Clamp(SelfDistance / BlastRadiusCm, 0.0f, 1.0f);
        Character->LaunchCharacter(Away * KnockbackImpulse * FMath::Max(0.4f, Proximity), true, true);

        const float Falloff = FMath::Lerp(1.0f, EdgeDamageFraction, FMath::Clamp(SelfDistance / BlastRadiusCm, 0.0f, 1.0f));
        FBreakerDamageRequest SelfDamage;
        SelfDamage.BaseDamage = FMath::Max(1.0f, BaseDamage * Falloff * SelfDamageFraction);
        SelfDamage.DamageFamily = EBreakerDamageFamily::Physical;
        SelfDamage.bCanCritical = false;
        SelfDamage.SourceLocation = BlastLocation;
        SelfDamage.bHasSourceLocation = true;
        SelfDamage.SetInstigator(Character);
        if (UBreakerCombatComponent* OwnCombat = Character->FindComponentByClass<UBreakerCombatComponent>())
        {
            OwnCombat->ReceiveDamage(SelfDamage);
        }
    }
}

// ---------------------------------------------------------------------------
// T6 — GROUND ZERO
// ---------------------------------------------------------------------------

UBreakerAbility_GroundZero::UBreakerAbility_GroundZero()
{
    FallbackAbilityId = TEXT("Tank.GroundZero");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBreakerAbility_GroundZero::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    ABreakerCharacter* Character = GetBreakerCharacter();
    UWorld* World = Character ? Character->GetWorld() : nullptr;
    const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    // §T6: usable ONLY while airborne — checked before commit, so a grounded
    // press refuses without spending 45 Grit.
    if (!World || !Movement || !Movement->IsFalling() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // NEAREST HONEST fall scaling (see the header): current downward speed
    // stands in for fall distance. A normal jump's fall reaches the minimum
    // fraction, so the verb is fully usable from ordinary geometry (§T6).
    const float DownSpeed = FMath::Max(0.0f, -Movement->Velocity.Z);
    const float Power = FMath::Clamp(DownSpeed / FullPowerFallSpeed, MinimumPowerFraction, 1.0f);

    // The slam itself: drive the Tank hard into the ground.
    Character->LaunchCharacter(FVector(0.0f, 0.0f, -SlamDownSpeed), false, true);

    // Damage resolves at the floor under the Tank, now — waiting for a landing
    // event would need a landing hook this ability does not own.
    FHitResult FloorHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerGroundZero), false, Character);
    const FVector Start = Character->GetActorLocation();
    const FVector Center = World->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0, 0, 2000.0f), ECC_Visibility, QueryParams)
        ? FloorHit.ImpactPoint : Start;

    const float BaseDamage = BreakerTankAbilityLocal::BreakerTankAbilityBaseDamage(Character, WeaponDamageCoefficient, UnarmedDamage) * Power;
    BreakerTankAbilityLocal::BreakerTankRadialDamage(World, Character, Center, BlastRadiusCm, BaseDamage, 0.5f, /*bApplyFalloff=*/true);

    // NEAREST HONEST STAGGER (see the header): a full stop through the public
    // movement-profile mutator, restored on a timer. Not a real stagger.
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        ABreakerEnemy* Enemy = *It;
        if (!Enemy) continue;
        const UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>();
        if (!EnemyCombat || EnemyCombat->IsDead()) continue;
        if (FVector::DistSquared(Center, Enemy->GetActorLocation()) > BlastRadiusCm * BlastRadiusCm) continue;
        Enemy->ApplyModifierMovementProfile(0.0f, -1.0f);
        TWeakObjectPtr<ABreakerEnemy> WeakEnemy(Enemy);
        FTimerHandle RestoreTimer;
        World->GetTimerManager().SetTimer(RestoreTimer, FTimerDelegate::CreateLambda([WeakEnemy]()
        {
            if (ABreakerEnemy* Restored = WeakEnemy.Get())
            {
                Restored->ApplyModifierMovementProfile(1.0f, -1.0f);
            }
        }), StaggerSeconds, false);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// ---------------------------------------------------------------------------
// ULTIMATE — HOLD
// ---------------------------------------------------------------------------

UBreakerAbility_Hold::UBreakerAbility_Hold()
{
    FallbackAbilityId = TEXT("Tank.Hold");
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

FName UBreakerAbility_Hold::WindowKey() { return TEXT("Window.Tank.Hold"); }

void UBreakerAbility_Hold::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
    const float Duration = Variant.WindowDuration > 0.0f ? Variant.WindowDuration : 10.0f;

    bVein = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Vein"), false);
    bDetonation = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Detonation"), false);
    const bool bWall = Variant.KeystoneTag == FGameplayTag::RequestGameplayTag(TEXT("Keystone.Tank.Wall"), false);
    AbsorbedDamage = 0.0f;

    if (UBreakerAbilityStateComponent* State = UBreakerAbilityStateComponent::FindOrAdd(Character))
    {
        State->StartWindow(WindowKey(), Duration);
    }
    // §2.1: generation triples against a raised cap. One push does both,
    // because the Grit loop multiplies its cap by the composed override.
    if (UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>())
    {
        Grit->PushLoopOverride(WindowKey(), /*bSuspendDecay=*/false, GenerationMultiplier, Duration);
    }

    UBreakerCombatComponent* Combat = Character->FindComponentByClass<UBreakerCombatComponent>();
    if (Combat)
    {
        if (bVein)
        {
            // VEIN removes the cap and converts incoming damage to healing at
            // a reduced rate. EXPLICITLY NOT IMMUNITY: the hit lands first and
            // the heal is partial, so enough incoming damage still kills.
        }
        else
        {
            // The multiplicative stand-in for the per-hit cap (class comment).
            // Wall solo: doubled effectiveness on the Tank; the ally extension
            // is unreachable until a party exists.
            Combat->PushIncomingDamageModifier(WindowKey(), bWall ? WallSoloIncomingMultiplier : BaseIncomingMultiplier);
        }
        if (bVein || bDetonation)
        {
            Combat->OnDamageTaken.AddDynamic(this, &UBreakerAbility_Hold::HandleDamageTaken);
        }
        BoundCombat = Combat;
    }

    bHoldActive = true;
    World->GetTimerManager().SetTimer(WindowTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { CloseHold(); }), Duration, false);
}

void UBreakerAbility_Hold::HandleDamageTaken(const FBreakerHitContext& Hit)
{
    if (!bHoldActive) return;
    ABreakerCharacter* Character = GetBreakerCharacter();
    if (!Character || Hit.Target != Character) return;
    const float Taken = Hit.Result.HealthDamage + Hit.Result.ShieldDamage;
    if (Taken <= 0.0f) return;

    if (bVein)
    {
        // §2.1 Vein: heal at 60% of the post-mitigation amount. Instant, not
        // over 1.5s (recorded on the tunable) — and a damped attrition window
        // either way, never immunity.
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->ApplyHealingAmount(Taken * VeinHealFraction, Character, FGameplayTag());
        }
    }
    if (bDetonation)
    {
        AbsorbedDamage += Taken;
    }
}

void UBreakerAbility_Hold::CloseHold()
{
    if (CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UBreakerAbility_Hold::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (bHoldActive)
    {
        bHoldActive = false;
        ABreakerCharacter* Character = GetBreakerCharacter();

        // §2.1 Detonation: the absorbed damage releases as a radial blast —
        // 70%, 8 m, NO falloff, self-exempt (the one self-damage exemption in
        // the class, and it is on the ultimate). Released here, at the window
        // end, because the second input binding it wants does not exist.
        if (bDetonation && !bWasCancelled && AbsorbedDamage > 0.0f && Character)
        {
            BreakerTankAbilityLocal::BreakerTankRadialDamage(Character->GetWorld(), Character,
                Character->GetActorLocation(), DetonationRadiusCm,
                AbsorbedDamage * DetonationReleaseFraction, 1.0f, /*bApplyFalloff=*/false);
        }
        AbsorbedDamage = 0.0f;

        if (Character)
        {
            if (UBreakerGritComponent* Grit = Character->FindComponentByClass<UBreakerGritComponent>())
            {
                Grit->PopLoopOverride(WindowKey());
            }
            if (UBreakerAbilityStateComponent* State = Character->FindComponentByClass<UBreakerAbilityStateComponent>())
            {
                State->CloseWindow(WindowKey());
            }
        }
        if (UBreakerCombatComponent* Combat = BoundCombat.Get())
        {
            Combat->RemoveIncomingDamageModifier(WindowKey());
            Combat->OnDamageTaken.RemoveDynamic(this, &UBreakerAbility_Hold::HandleDamageTaken);
        }
        BoundCombat.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(WindowTimer);
        }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
