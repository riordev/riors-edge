#include "Combat/BreakerModifierComponent.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

namespace BreakerModifierRuntime
{
    // Distinctively prefixed for the unity build: a bare `AuraKey` would
    // collide with whatever the next translation unit in the blob declares.
    static const FName BreakerAuraModifierKey(TEXT("Modifier.WardingAura"));
    static const FName BreakerHazardZoneTagName(TEXT("Modifier.Cascading.Hazard"));
}

UBreakerEnemyModifierComponent::UBreakerEnemyModifierComponent()
{
    PrimaryComponentTick.bCanEverTick = false;   // the owner drives AdvanceModifiers
    SetIsReplicatedByDefault(true);
}

void UBreakerEnemyModifierComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreakerEnemyModifierComponent, Modifiers);
}

void UBreakerEnemyModifierComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UBreakerCombatComponent* Combat = OwnerCombat())
    {
        Combat->OnDamageTaken.AddDynamic(this, &ThisClass::HandleOwnerDamaged);
    }
    // A set granted before BeginPlay (the spawner's normal order) still has to
    // land its persistent half once the components exist.
    if (!Modifiers.IsEmpty()) ApplyPersistentModifiers();
}

void UBreakerEnemyModifierComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseExternalEffects();
    Super::EndPlay(Reason);
}

UBreakerCombatComponent* UBreakerEnemyModifierComponent::OwnerCombat() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
}

float UBreakerEnemyModifierComponent::OwnerMaxHealth() const
{
    const ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    return Enemy ? Enemy->GetMonsterMaxHealth() : 0.0f;
}

float UBreakerEnemyModifierComponent::OwnerAttackDamage() const
{
    const ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    return Enemy ? Enemy->GetAttackDamage() : 0.0f;
}

bool UBreakerEnemyModifierComponent::SetModifiers(const TArray<EBreakerEnemyModifier>& NewModifiers)
{
    FString Reason;
    if (!UBreakerEnemyModifierLibrary::IsLegalModifierSet(NewModifiers, Reason))
    {
        // Loud but not fatal: an illegal set is an authoring bug in the caller,
        // and a spawner that thinks it made a Champion needs to be told it did
        // not rather than quietly getting a Veteran.
        UE_LOG(LogTemp, Warning, TEXT("Rejected an illegal enemy modifier set: %s"), *Reason);
        return false;
    }

    ReleaseExternalEffects();
    bExternalEffectsReleased = false;
    Modifiers = NewModifiers;
    bWakefulSpent = false;
    ApplyPersistentModifiers();
    OnModifiersChanged.Broadcast();
    return true;
}

TArray<EBreakerEnemyModifier> UBreakerEnemyModifierComponent::RollAndApplyModifiers(int32 Seed, EBreakerEnemyFamily Family)
{
    const TArray<EBreakerEnemyModifier> Rolled = UBreakerEnemyModifierLibrary::RollModifiers(Seed, Params, Family);
    SetModifiers(Rolled);
    return Rolled;
}

FString UBreakerEnemyModifierComponent::GetBanner() const
{
    return UBreakerEnemyModifierLibrary::GetModifierBanner(Modifiers);
}

void UBreakerEnemyModifierComponent::OnRep_Modifiers()
{
    // Clients rebuild the tell from the replicated list. They never run
    // membership, damage or spawning.
    RefreshHalo();
    OnModifiersChanged.Broadcast();
}

void UBreakerEnemyModifierComponent::ApplyPersistentModifiers()
{
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());

    // WARDED. A real shield on the attribute set, so it routes through the
    // existing shield step in ReceiveDamage — which is also what makes the
    // "DoT ignores the ward" rule free: every status tick already sets
    // bBypassShield, so a bleed build walks straight past this modifier while
    // a burst build has to out-race the recharge.
    if (Enemy && HasModifier(EBreakerEnemyModifier::Warded))
    {
        const float Ward = UBreakerEnemyModifierLibrary::GetWardShieldAmount(OwnerMaxHealth(), Params);
        Enemy->SetModifierShield(Ward);
    }

    // FLEETFOOT. Speed and circling, both applied through the enemy's own
    // mutator rather than by reaching into its protected tuning.
    if (Enemy)
    {
        const bool bFleet = HasModifier(EBreakerEnemyModifier::Fleetfoot);
        Enemy->ApplyModifierMovementProfile(
            bFleet ? FMath::Max(1.0f, Params.FleetfootSpeedMultiplier) : 1.0f,
            bFleet ? FMath::Clamp(Params.FleetfootWeaveStrength, 0.0f, 1.0f) : -1.0f);
    }

    // ANCHORED. There is no stagger or knockback system in the project yet
    // (Encounter-Design OPEN QUESTION 9), so the immunity half of this modifier
    // has nothing to be immune to and is recorded rather than faked. The half
    // that IS live is the slow on hit, applied in NotifyAttackLanded — and that
    // half is the whole design anyway: §1.2 says the slow is the punish.

    RefreshHalo();
}

void UBreakerEnemyModifierComponent::RefreshHalo()
{
    if (!bShowHalo || !GetOwner() || !GetOwner()->GetRootComponent()) return;

    const bool bWanted = !Modifiers.IsEmpty();
    if (!Halo && bWanted)
    {
        // Runtime-created rather than a constructor subobject: an unmodified
        // trash mob is the overwhelmingly common case and must not pay for two
        // components it will never show.
        Halo = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("BreakerModifierHalo"));
        if (Halo)
        {
            Halo->SetupAttachment(GetOwner()->GetRootComponent());
            Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Halo->SetCastShadow(false);
            // A GROUND DISC, not a sphere. The halo was an engine sphere in the
            // opaque BasicShapeMaterial — a solid ball larger than the body, so
            // every modifier-bearer was a floating orb with its whole silhouette
            // (primitives then, the mech cast now) hidden inside. The colour and
            // grows-with-count language survives on the project's proven
            // at-range idiom instead — the ground ring (Provoke, deployables) —
            // and the body stays visible above it.
            if (UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
            {
                Halo->SetStaticMesh(Disc);
            }
            if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(
                nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
            {
                HaloMaterial = UMaterialInstanceDynamic::Create(Base, Halo);
                if (HaloMaterial) Halo->SetMaterial(0, HaloMaterial);
            }
            Halo->RegisterComponent();
        }

        HaloLight = NewObject<UPointLightComponent>(GetOwner(), TEXT("BreakerModifierHaloLight"));
        if (HaloLight)
        {
            HaloLight->SetupAttachment(GetOwner()->GetRootComponent());
            HaloLight->SetCastShadows(false);
            HaloLight->SetAttenuationRadius(HaloLightRadius);
            HaloLight->RegisterComponent();
        }
    }
    if (!Halo) return;

    Halo->SetVisibility(bWanted, true);
    if (HaloLight) HaloLight->SetVisibility(bWanted, true);
    if (!bWanted) return;

    // Colour comes from the FIRST modifier and the SIZE from the count, so an
    // enemy carrying three reads as bigger and more dangerous at range before
    // any of the three names is legible. The count widens the DISC; a 4 cm
    // slab at the feet, dropped to the capsule's bottom so it sits on the
    // ground whatever the owner's capsule height is.
    const FLinearColor Color = UBreakerEnemyModifierLibrary::GetModifierColor(Modifiers[0]);
    const float Scale = HaloBaseScale + HaloScalePerModifier * static_cast<float>(Modifiers.Num() - 1);
    Halo->SetRelativeScale3D(FVector(Scale, Scale, 0.04f));
    float FeetDrop = 0.0f;
    if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()))
    {
        FeetDrop = Capsule->GetUnscaledCapsuleHalfHeight() - 3.0f;
    }
    Halo->SetRelativeLocation(FVector(0.0f, 0.0f, -FeetDrop));
    if (HaloMaterial) HaloMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    if (HaloLight)
    {
        HaloLight->SetLightColor(Color);
        HaloLight->SetIntensity(HaloLightIntensity);
    }
}

void UBreakerEnemyModifierComponent::AdvanceModifiers(float DeltaSeconds)
{
    if (Modifiers.IsEmpty() || DeltaSeconds <= 0.0f) return;

    // The Volatile fuse keeps running after the owner is dead — that is the
    // whole modifier — so it is advanced before the live-only clocks.
    if (FuseRemaining > 0.0f)
    {
        FuseRemaining -= DeltaSeconds;
        if (HaloMaterial && FuseTotal > 0.0f)
        {
            // Strobe: a square wave rather than a sine, because a hard on/off
            // reads as "about to go off" and a smooth pulse reads as ambience.
            const float Elapsed = FuseTotal - FMath::Max(FuseRemaining, 0.0f);
            const bool bBright = FMath::Fmod(Elapsed * FMath::Max(FuseStrobeHz, 0.1f), 1.0f) < 0.5f;
            const FLinearColor Fuse = UBreakerEnemyModifierLibrary::GetModifierColor(EBreakerEnemyModifier::Volatile);
            HaloMaterial->SetVectorParameterValue(TEXT("Color"), bBright ? Fuse : Fuse * 0.15f);
        }
        if (FuseRemaining <= 0.0f)
        {
            FuseRemaining = -1.0f;
            DetonateVolatile();
        }
        return;
    }

    if (HasModifier(EBreakerEnemyModifier::Warded))     TickWard(DeltaSeconds);
    if (HasModifier(EBreakerEnemyModifier::WardingAura)) TickAura(DeltaSeconds);
    if (HasModifier(EBreakerEnemyModifier::Phasing))    TickPhasing(DeltaSeconds);

    // Cascading hazards clean their own weak entries so the live count that
    // caps them cannot be inflated by zones the world already destroyed.
    LiveHazards.RemoveAll([](const TWeakObjectPtr<ABreakerZoneActor>& Zone) { return !Zone.IsValid(); });
}

void UBreakerEnemyModifierComponent::TickWard(float DeltaSeconds)
{
    UBreakerCombatComponent* Combat = OwnerCombat();
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    if (!Combat || !Enemy || Combat->IsDead()) return;
    if (Combat->GetSecondsSinceDamage() < FMath::Max(0.0f, Params.WardRechargeDelaySeconds)) return;

    const float Full = UBreakerEnemyModifierLibrary::GetWardShieldAmount(OwnerMaxHealth(), Params);
    if (Full <= 0.0f) return;
    const float Step = Full * FMath::Max(0.0f, Params.WardRechargeRatePerSecond) * DeltaSeconds;
    Enemy->AddModifierShield(Step);
}

void UBreakerEnemyModifierComponent::TickAura(float DeltaSeconds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) return;

    // Cheap cadence rather than every frame: the aura is a radius check over
    // every live enemy, and a pack of 24 running that 60 times a second is
    // 34,000 distance tests for a buff that changes on a walking timescale.
    AuraPulseTimer -= DeltaSeconds;
    if (AuraPulseTimer > 0.0f) return;
    AuraPulseTimer = 0.25f;

    const float RadiusSq = FMath::Square(FMath::Max(0.0f, Params.AuraRadiusCm));
    const float Multiplier = 1.0f - FMath::Clamp(Params.AuraDamageReduction, 0.0f, 1.0f);
    const FVector Center = GetOwner()->GetActorLocation();

    TArray<TWeakObjectPtr<AActor>> StillInside;
    for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
    {
        ABreakerEnemy* Ally = *It;
        if (!Ally || Ally == GetOwner() || Ally->IsDeadEnemy()) continue;
        if (FVector::DistSquared(Ally->GetActorLocation(), Center) > RadiusSq) continue;
        if (UBreakerCombatComponent* AllyCombat = Ally->FindComponentByClass<UBreakerCombatComponent>())
        {
            // §1.2: "does not stack with another Warding Aura". Keyed pushes
            // REPLACE, and every aura in the game shares this one key, so two
            // overlapping auras are one aura by construction rather than by a
            // rule somebody has to remember.
            AllyCombat->PushIncomingDamageModifier(BreakerModifierRuntime::BreakerAuraModifierKey, Multiplier);
            StillInside.Add(Ally);
        }
    }

    // Anyone who left the radius loses the entry immediately. A buff that
    // outlives its source is a buff the player cannot reason about.
    for (const TWeakObjectPtr<AActor>& Previous : AuraTargets)
    {
        AActor* Actor = Previous.Get();
        if (!Actor || StillInside.ContainsByPredicate(
            [Actor](const TWeakObjectPtr<AActor>& Kept) { return Kept.Get() == Actor; })) continue;
        if (UBreakerCombatComponent* Combat = Actor->FindComponentByClass<UBreakerCombatComponent>())
        {
            Combat->RemoveIncomingDamageModifier(BreakerModifierRuntime::BreakerAuraModifierKey);
        }
    }
    AuraTargets = MoveTemp(StillInside);
}

void UBreakerEnemyModifierComponent::TickPhasing(float DeltaSeconds)
{
    AActor* Target = TrackedTarget.Get();
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    if (!Enemy || Enemy->IsDeadEnemy()) return;

    // The blink itself: briefly untargetable, then hittable again.
    if (BlinkRemaining > 0.0f)
    {
        BlinkRemaining -= DeltaSeconds;
        if (BlinkRemaining <= 0.0f)
        {
            BlinkRemaining = 0.0f;
            Enemy->SetModifierUntargetable(false);
        }
        return;
    }

    if (!Target) return;

    // The tell has to come FIRST. A blink with no wind-up is an unannounced
    // teleport, which §1.2's readability test forbids outright and which O1's
    // passive defence has no answer to.
    if (bPhaseTelegraphing)
    {
        PhaseTelegraphRemaining -= DeltaSeconds;
        if (PhaseTelegraphRemaining > 0.0f) return;

        bPhaseTelegraphing = false;
        const FVector Destination = UBreakerEnemyModifierLibrary::GetPhaseDestination(
            Enemy->GetActorLocation(), Target->GetActorLocation(), Params);
        if (!Destination.Equals(Enemy->GetActorLocation(), 1.0f))
        {
            Enemy->SetActorLocation(Destination, false);
        }
        BlinkRemaining = FMath::Max(0.0f, Params.PhaseBlinkSeconds);
        Enemy->SetModifierUntargetable(BlinkRemaining > 0.0f);
        PhaseTimer = 0.0f;
        return;
    }

    PhaseTimer += DeltaSeconds;
    if (PhaseTimer >= FMath::Max(0.1f, Params.PhaseIntervalSeconds))
    {
        bPhaseTelegraphing = true;
        PhaseTelegraphRemaining = FMath::Max(0.0f, Params.PhaseTelegraphSeconds);
    }
}

void UBreakerEnemyModifierComponent::NotifyAttackLanded(const FVector& ImpactLocation)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // ANCHORED's live half. The slow is applied to whatever was hit through the
    // movement layer's existing keyed push/pop; it is not a damage number and
    // deliberately does not scale with anything.
    if (HasModifier(EBreakerEnemyModifier::Anchored))
    {
        if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner()))
        {
            Enemy->ApplyModifierSlowToTarget(
                FMath::Clamp(1.0f - Params.AnchoredSlowFraction, 0.0f, 1.0f),
                FMath::Max(0.0f, Params.AnchoredSlowSeconds));
        }
    }

    if (HasModifier(EBreakerEnemyModifier::Cascading))
    {
        SpawnCascadeHazard(ImpactLocation);
    }
}

void UBreakerEnemyModifierComponent::SpawnCascadeHazard(const FVector& Location)
{
    if (!GetWorld()) return;
    LiveHazards.RemoveAll([](const TWeakObjectPtr<ABreakerZoneActor>& Zone) { return !Zone.IsValid(); });
    // §5.3 caps simultaneous ground hazards. Past the cap the oldest is
    // retired rather than the newest suppressed: a player who has just been
    // told "move" must find that the ground they moved to is the safe one.
    if (LiveHazards.Num() >= FMath::Max(1, Params.MaximumLiveHazardsPerEnemy))
    {
        if (ABreakerZoneActor* Oldest = LiveHazards[0].Get()) Oldest->Destroy();
        LiveHazards.RemoveAt(0);
    }

    FBreakerZoneSpec Spec;
    Spec.RadiusCm = FMath::Max(0.0f, Params.HazardRadiusCm);
    Spec.HalfHeightCm = 250.0f;
    Spec.Duration = FMath::Max(0.0f, Params.HazardDurationSeconds);
    Spec.TickInterval = FMath::Max(0.05f, Params.HazardTickInterval);
    Spec.ZoneColor = UBreakerEnemyModifierLibrary::GetModifierColor(EBreakerEnemyModifier::Cascading);
    Spec.TickDamage.BaseDamage = UBreakerEnemyModifierLibrary::GetHazardTickDamage(OwnerAttackDamage(), Params);
    Spec.TickDamage.DamageFamily = EBreakerDamageFamily::Physical;
    Spec.TickDamage.bCanCritical = false;

    // The zone is the shared primitive, not a bespoke hazard: it already routes
    // every tick through FBreakerDamageRequest/ReceiveDamage with a proper
    // Instigator, so the player's armour, shields and passive dodge/block layer
    // all apply exactly as they do to a bullet, and it already refuses to
    // damage other enemies.
    if (ABreakerZoneActor* Zone = GetWorld()->SpawnActor<ABreakerZoneActor>(
        ABreakerZoneActor::StaticClass(), Location, FRotator::ZeroRotator))
    {
        Zone->ConfigureZone(Spec, GetOwner());
        LiveHazards.Add(Zone);
    }
}

void UBreakerEnemyModifierComponent::HandleOwnerDamaged(const FBreakerHitContext& Hit)
{
    if (!HasModifier(EBreakerEnemyModifier::Reflective) || bReflecting) return;
    AActor* Attacker = Hit.Instigator;
    if (!Attacker || Attacker == GetOwner()) return;
    // A reflect is dealt as TrueDamage and this filter refuses TrueDamage, so
    // a reflect can never reflect a reflect. That is the loop guard; bReflecting
    // is only the belt to its braces.
    if (Hit.DamageFamily == EBreakerDamageFamily::TrueDamage) return;
    // §1.2 puts no reflect on damage over time, and it would be a strange rule:
    // a bleed is not the player holding a trigger.
    if (Hit.bFromDoT) return;

    UBreakerCombatComponent* AttackerCombat = Attacker->FindComponentByClass<UBreakerCombatComponent>();
    if (!AttackerCombat) return;

    const float Dealt = Hit.Result.HealthDamage + Hit.Result.ShieldDamage;
    const float Reflected = UBreakerEnemyModifierLibrary::GetReflectDamage(Dealt, OwnerMaxHealth(), Params);
    if (Reflected <= 0.0f) return;

    FBreakerDamageRequest Request;
    Request.BaseDamage = Reflected;
    // "Unmitigated-by-armour" per §1.2. TrueDamage is the family that already
    // means exactly that in this pipeline, so no new bypass flag is invented.
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;
    Request.bCanCritical = false;
    Request.SourceLocation = GetOwner()->GetActorLocation();
    Request.bHasSourceLocation = true;
    Request.SetInstigator(GetOwner());

    bReflecting = true;
    AttackerCombat->ReceiveDamage(Request);
    bReflecting = false;
}

bool UBreakerEnemyModifierComponent::TryConsumeWakefulRevive(bool bKilledByWeakPoint)
{
    if (!HasModifier(EBreakerEnemyModifier::Wakeful) || bWakefulSpent) return false;
    // §1.2: a weak-point killing blow denies the revive. That is the entire
    // mechanic — it converts "spray the body" into "finish the head", which is
    // a skill the game already teaches everywhere else.
    if (bKilledByWeakPoint)
    {
        bWakefulSpent = true;
        return false;
    }
    bWakefulSpent = true;
    return true;
}

float UBreakerEnemyModifierComponent::GetWakefulReviveDelay() const
{
    return FMath::Max(0.0f, Params.WakefulReviveDelaySeconds);
}

float UBreakerEnemyModifierComponent::GetWakefulReviveHealthFraction() const
{
    return FMath::Clamp(Params.WakefulReviveHealthFraction, 0.01f, 1.0f);
}

void UBreakerEnemyModifierComponent::NotifyOwnerDied()
{
    ReleaseExternalEffects();

    if (HasModifier(EBreakerEnemyModifier::Volatile))
    {
        FuseTotal = FMath::Max(0.0f, Params.VolatileFuseSeconds);
        FuseRemaining = FuseTotal;
        // The corpse's halo has to stay visible through the fuse or the
        // detonation is unannounced. The enemy hides its BODY on death; the
        // halo is this component's and it deliberately survives.
        if (Halo) Halo->SetVisibility(true, true);
        if (HaloLight) HaloLight->SetVisibility(true, true);
        // A zero fuse is a legal authoring choice and must detonate now rather
        // than wait a frame for a clock that will never tick again.
        if (FuseTotal <= 0.0f)
        {
            FuseRemaining = -1.0f;
            DetonateVolatile();
        }
    }

    if (HasModifier(EBreakerEnemyModifier::Splitting)) SpawnSplits();
}

void UBreakerEnemyModifierComponent::DetonateVolatile()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority()) return;
    if (Halo) Halo->SetVisibility(false, true);
    if (HaloLight) HaloLight->SetVisibility(false, true);

    const FVector Center = GetOwner()->GetActorLocation();
    const float Full = UBreakerEnemyModifierLibrary::GetVolatileDetonationDamage(OwnerAttackDamage(), Params);
    if (Full <= 0.0f) return;
    const float OuterSq = FMath::Square(FMath::Max(Params.VolatileInnerRadiusCm, Params.VolatileOuterRadiusCm));

    // Hits PAWNS with a combat component and skips enemies, which is the same
    // rule the enemy death chain and the enemy projectile already use: enemy
    // friendly fire is not a system this project has ruled on, and a Volatile
    // that wipes its own pack would make the modifier a gift.
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* Candidate = *It;
        if (!Candidate || Candidate == GetOwner() || Candidate->IsA<ABreakerEnemy>()) continue;
        const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), Center);
        if (DistanceSq > OuterSq) continue;
        UBreakerCombatComponent* Combat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!Combat) continue;

        FBreakerDamageRequest Request;
        Request.BaseDamage = Full * UBreakerEnemyModifierLibrary::GetVolatileFalloff(FMath::Sqrt(DistanceSq), Params);
        Request.DamageFamily = EBreakerDamageFamily::Physical;
        Request.bCanCritical = false;
        Request.SourceLocation = Center;
        Request.bHasSourceLocation = true;
        Request.SetInstigator(GetOwner());
        if (Request.BaseDamage > 0.0f) Combat->ReceiveDamage(Request);
    }
}

void UBreakerEnemyModifierComponent::SpawnSplits()
{
    ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(GetOwner());
    if (!Enemy || !GetWorld() || !Enemy->HasAuthority()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Center = Enemy->GetActorLocation();
    const int32 Count = FMath::Max(0, Params.SplitCount);

    for (int32 Index = 0; Index < Count; ++Index)
    {
        // Deterministic fan rather than a random scatter, so a Splitting death
        // always produces the same readable shape and the player learns where
        // the copies appear.
        const float Angle = 360.0f / FMath::Max(1, Count) * Index;
        const FVector Offset = FRotator(0.0f, Angle, 0.0f).RotateVector(
            FVector(FMath::Max(0.0f, Params.SplitScatterCm), 0.0f, 0.0f));
        ABreakerEnemy* Copy = GetWorld()->SpawnActor<ABreakerEnemy>(
            Enemy->GetClass(), Center + Offset, Enemy->GetActorRotation(), SpawnParams);
        // §1.2: copies carry NO modifiers and drop NO loot. No modifiers is why
        // this cannot recurse: a split of a Splitting enemy does not split.
        if (Copy) Copy->ConfigureAsSplitCopy(Enemy->GetAreaLevel(),
            FMath::Clamp(Params.SplitHealthFraction, 0.01f, 1.0f));
    }
}

void UBreakerEnemyModifierComponent::ReleaseExternalEffects()
{
    if (bExternalEffectsReleased) return;
    bExternalEffectsReleased = true;
    for (const TWeakObjectPtr<AActor>& Buffed : AuraTargets)
    {
        if (AActor* Actor = Buffed.Get())
        {
            if (UBreakerCombatComponent* Combat = Actor->FindComponentByClass<UBreakerCombatComponent>())
            {
                Combat->RemoveIncomingDamageModifier(BreakerModifierRuntime::BreakerAuraModifierKey);
            }
        }
    }
    AuraTargets.Reset();
}
