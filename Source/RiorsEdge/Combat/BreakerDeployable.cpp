#include "Combat/BreakerDeployable.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Classes/BreakerScrapComponent.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerZoneActor.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NativeGameplayTags.h"
#include "Weapons/BreakerWeaponComponent.h"

namespace BreakerDeployableLocal
{
    // Prefixed for the unity build: identical anonymous-namespace names in two
    // translation units have collided in this project before.
    static const TCHAR* BreakerDeployableCubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
    static const TCHAR* BreakerDeployableSphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    static const TCHAR* BreakerDeployableShapeMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

    // The Disruptor's zone identity: two Disruptors are one field refreshed,
    // never a double armour strip (the Rot anti-stack rule, §G6).
    UE_DEFINE_GAMEPLAY_TAG_STATIC(Zone_Gunsmith_Disruptor, "Zone.Gunsmith.Disruptor");

    // Server-side registry of live deployables. Weak, pruned on read.
    static TArray<TWeakObjectPtr<ABreakerDeployable>> BreakerLiveDeployables;

    struct FBreakerDensityCapOverride
    {
        int32 TotalCap = ABreakerDeployable::BaseTotalDensityCap;
        double ExpiryWorldTime = -1.0;
    };
    static TMap<TWeakObjectPtr<const AActor>, FBreakerDensityCapOverride> BreakerDensityCapOverrides;
}

int32 ABreakerDeployable::NextPlacementSerial = 0;

ABreakerDeployable::ABreakerDeployable()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(Root);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetCastShadow(false);

    TopVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopVisual"));
    TopVisual->SetupAttachment(BodyVisual);
    TopVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TopVisual->SetCastShadow(false);

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Root);
    Glow->SetCastShadows(false);
    Glow->SetIntensity(1200.0f);
    Glow->SetAttenuationRadius(300.0f);

    // Real damage intake through the one damage pipeline. Enemy projectiles
    // resolve against any actor with a combat component, so stray fire kills a
    // deployable exactly as §2.4 wants without any bespoke health arithmetic.
    Combat = CreateDefaultSubobject<UBreakerCombatComponent>(TEXT("Combat"));
    Attributes = CreateDefaultSubobject<UBreakerAttributeSet>(TEXT("Attributes"));
}

void ABreakerDeployable::BeginPlay()
{
    Super::BeginPlay();
    if (Combat && Attributes)
    {
        Combat->BindAttributes(Attributes);
        Combat->OnDeath.AddDynamic(this, &ABreakerDeployable::HandleCombatDeath);
    }
}

void ABreakerDeployable::InitializeDeployable(EBreakerDeployableType InType, AActor* InOwnerCharacter, float InScrapCost)
{
    Type = InType;
    OwningCharacter = InOwnerCharacter;
    ScrapCost = FMath::Max(0.0f, InScrapCost);
    PlacementSerial = ++NextPlacementSerial;
    BreakerDeployableLocal::BreakerLiveDeployables.Add(this);

    // Per-type lifetime and durability, Class-Kits-Gunsmith §2.2's table (all
    // O2 PLACEHOLDER). Traps live longest and cost least; active damage lives
    // shortest and costs most — the gradient IS the branch identity.
    float Health = 100.0f;
    switch (Type)
    {
    case EBreakerDeployableType::Turret:
        LifetimeRemaining = 30.0f;   // §2.2
        Health = TurretHealth;
        break;
    case EBreakerDeployableType::AmmoCrate:
        LifetimeRemaining = 45.0f;   // §2.2: 45s or charges exhausted, whichever first
        Health = CrateHealth;
        ChargesRemaining = CrateCharges;
        break;
    case EBreakerDeployableType::MineCluster:
        LifetimeRemaining = 60.0f;   // §2.2: 60s or all charges triggered
        Health = MineClusterHealth;
        for (int32 Index = 0; Index < MineCount; ++Index)
        {
            FMineCharge Mine;
            const float Angle = (2.0f * PI / FMath::Max(1, MineCount)) * Index;
            Mine.Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * MineSpreadCm;
            Mine.ArmRemaining = MineArmDelay;
            Mines.Add(Mine);
        }
        break;
    case EBreakerDeployableType::Disruptor:
        LifetimeRemaining = 20.0f;   // §2.2
        Health = DisruptorHealth;
        break;
    case EBreakerDeployableType::AnchorPoint:
    {
        LifetimeRemaining = 12.0f;   // Class-Kits-Tank §2 T3
        // §T3: its own health pool at 20% of the Tank's maximum health.
        float OwnerMaxHealth = 500.0f;   // O2 PLACEHOLDER fallback with no owner attributes
        if (const ABreakerCharacter* OwnerBreaker = Cast<ABreakerCharacter>(InOwnerCharacter))
        {
            if (const UBreakerAttributeSet* OwnerAttributes = OwnerBreaker->GetAttributes())
            {
                OwnerMaxHealth = OwnerAttributes->GetMaxHealth();
            }
        }
        Health = FMath::Max(1.0f, OwnerMaxHealth * AnchorHealthFraction);
        break;
    }
    default:
        break;
    }

    if (Attributes)
    {
        Attributes->ApplyMaxHealth(Health);
        Attributes->ApplyHealth(Health);
    }

    BuildPlaceholderVisual();

    // The Disruptor's field is an ordinary zone: slow + flat armour strip, no
    // damage, the emitter actor is the destructible half (§G6).
    if (Type == EBreakerDeployableType::Disruptor && GetWorld() && HasAuthority())
    {
        FBreakerZoneSpec Spec;
        Spec.ZoneTag = BreakerDeployableLocal::Zone_Gunsmith_Disruptor.GetTag();
        Spec.RadiusCm = DisruptorRadiusCm;
        Spec.Duration = LifetimeRemaining;
        Spec.TickInterval = 1.0f;
        Spec.FlatArmorReduction = DisruptorArmorReduction;   // FLAT, never percentage (§G6 / Master 7.10.5)
        Spec.ZoneColor = FLinearColor(0.95f, 0.55f, 0.10f);  // Gunsmith orange; teal is reserved (O19)

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParams.Owner = this;
        DisruptorZone = GetWorld()->SpawnActor<ABreakerZoneActor>(ABreakerZoneActor::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
        if (DisruptorZone)
        {
            DisruptorZone->ConfigureZone(Spec, OwningCharacter.Get());
            DisruptorZone->OnOccupantEntered.AddDynamic(this, &ABreakerDeployable::HandleZoneOccupantEntered);
            DisruptorZone->OnOccupantExited.AddDynamic(this, &ABreakerDeployable::HandleZoneOccupantExited);
        }
    }
}

void ABreakerDeployable::BuildPlaceholderVisual()
{
    // The enemies' basic-shape idiom: engine primitives plus a dynamic material
    // instance, so a clean clone renders with no content. O2 PLACEHOLDER
    // proportions throughout.
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, BreakerDeployableLocal::BreakerDeployableCubeMesh);
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, BreakerDeployableLocal::BreakerDeployableSphereMesh);
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, BreakerDeployableLocal::BreakerDeployableShapeMaterial);
    if (!Cube || !Sphere || !BodyVisual || !TopVisual) return;

    FLinearColor Color = FLinearColor(0.95f, 0.55f, 0.10f);   // Gunsmith orange (teal reserved, O19)
    switch (Type)
    {
    case EBreakerDeployableType::Turret:
        BodyVisual->SetStaticMesh(Cube);
        BodyVisual->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.5f));
        BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
        TopVisual->SetStaticMesh(Cube);
        TopVisual->SetRelativeScale3D(FVector(1.4f, 0.25f, 0.25f));
        TopVisual->SetRelativeLocation(FVector(30.0f, 0.0f, 60.0f));
        break;
    case EBreakerDeployableType::AmmoCrate:
        BodyVisual->SetStaticMesh(Cube);
        BodyVisual->SetRelativeScale3D(FVector(0.7f, 0.5f, 0.45f));
        BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
        Color = FLinearColor(0.85f, 0.75f, 0.25f);
        break;
    case EBreakerDeployableType::MineCluster:
        // The placement marker only; each charge draws as a small sphere below.
        BodyVisual->SetStaticMesh(Sphere);
        BodyVisual->SetRelativeScale3D(FVector(0.25f));
        BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 12.0f));
        Color = FLinearColor(0.9f, 0.25f, 0.12f);
        break;
    case EBreakerDeployableType::Disruptor:
        BodyVisual->SetStaticMesh(Cube);
        BodyVisual->SetRelativeScale3D(FVector(0.3f, 0.3f, 1.0f));
        BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
        TopVisual->SetStaticMesh(Sphere);
        TopVisual->SetRelativeScale3D(FVector(0.35f));
        TopVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
        break;
    case EBreakerDeployableType::AnchorPoint:
    {
        BodyVisual->SetStaticMesh(Cube);
        // 2.5 m wide, 2 m tall, thin (§T3). The panel is the collider too.
        BodyVisual->SetRelativeScale3D(FVector(0.15f, AnchorWidthCm / 100.0f, AnchorHeightCm / 100.0f));
        BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, AnchorHeightCm * 0.5f));
        // ONE-WAY COVER, nearest honest version. Blocks enemy projectiles
        // (they fly on BlockAllDynamic) and enemy line-of-sight, and IGNORES
        // the player's weapon trace channel — so the Tank shoots through it
        // freely. The difference from §T3's true one-way rule is that shots
        // pass through from BOTH sides; a per-direction filter needs a hit
        // callback the trace path does not expose. Pawns are not blocked
        // (§2.4's no-movement-blocking rule, and an enemy wedged on a panel
        // is worse than one walking around it).
        BodyVisual->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BodyVisual->SetCollisionObjectType(ECC_WorldDynamic);
        BodyVisual->SetCollisionResponseToAllChannels(ECR_Block);
        BodyVisual->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);   // the player weapon trace
        BodyVisual->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Color = FLinearColor(0.25f, 0.55f, 0.95f);
        break;
    }
    default:
        break;
    }

    if (Material)
    {
        if (UMaterialInstanceDynamic* BodyMaterial = UMaterialInstanceDynamic::Create(Material, this))
        {
            BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
            BodyVisual->SetMaterial(0, BodyMaterial);
            if (TopVisual->GetStaticMesh()) TopVisual->SetMaterial(0, BodyMaterial);
        }
    }
    Glow->SetLightColor(Color);
}

void ABreakerDeployable::SetHiddenUntilAction(bool bHiddenUntilActed)
{
    bHiddenUntilAction = bHiddenUntilActed;
    if (bHiddenUntilActed && !bHasActed)
    {
        SetActorHiddenInGame(true);
    }
}

void ABreakerDeployable::MarkActed()
{
    if (bHasActed) return;
    bHasActed = true;
    if (bHiddenUntilAction)
    {
        SetActorHiddenInGame(false);
    }
}

float ABreakerDeployable::OwnerWeaponBaseDamage() const
{
    // §1.3: all deployable damage is the PLAYER'S damage. The scaled weapon
    // base is the same number every weapon round uses (O35), so a deployable
    // rides gear depth exactly as the gun does and is worthless without one.
    const AActor* OwnerActor = OwningCharacter.Get();
    const UBreakerWeaponComponent* Weapon = OwnerActor ? OwnerActor->FindComponentByClass<UBreakerWeaponComponent>() : nullptr;
    return Weapon ? Weapon->GetScaledBaseDamage() : 0.0f;
}

void ABreakerDeployable::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || bDestroyed) return;

    if (!bLifetimePaused)
    {
        LifetimeRemaining -= DeltaSeconds;
        if (LifetimeRemaining <= 0.0f)
        {
            DestroyDeployable();
            return;
        }
    }

    switch (Type)
    {
    case EBreakerDeployableType::Turret:     TickTurret(DeltaSeconds); break;
    case EBreakerDeployableType::AmmoCrate:  TickAmmoCrate(DeltaSeconds); break;
    case EBreakerDeployableType::MineCluster: TickMineCluster(DeltaSeconds); break;
    default: break;   // Disruptor's field is the zone's job; the Anchor Point is geometry.
    }
}

void ABreakerDeployable::TickTurret(float DeltaSeconds)
{
    UWorld* World = GetWorld();
    if (!World) return;
    if (World->GetTimeSeconds() - LastTurretShotTime < TurretFireInterval) return;

    const float BaseDamage = OwnerWeaponBaseDamage() * TurretDamageCoefficient;
    if (BaseDamage <= 0.0f) return;

    // Nearest valid target with line of sight (§G3). Deliberately artless: no
    // leading, no weak-point preference — a turret is consistent, never
    // optimal, and that gap is why the player still holds a gun.
    ABreakerEnemy* Target = nullptr;
    float BestDistanceSq = TurretRangeCm * TurretRangeCm;
    const FVector MuzzleLocation = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        ABreakerEnemy* Candidate = *It;
        if (!Candidate) continue;
        const UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!CandidateCombat || CandidateCombat->IsDead()) continue;
        const float DistanceSq = FVector::DistSquared(MuzzleLocation, Candidate->GetActorLocation());
        if (DistanceSq >= BestDistanceSq) continue;
        FHitResult Blocked;
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerTurretLOS), false, this);
        QueryParams.AddIgnoredActor(Candidate);
        if (OwningCharacter.IsValid()) QueryParams.AddIgnoredActor(OwningCharacter.Get());
        if (World->LineTraceSingleByChannel(Blocked, MuzzleLocation, Candidate->GetActorLocation(), ECC_Visibility, QueryParams)) continue;
        Target = Candidate;
        BestDistanceSq = DistanceSq;
    }
    if (!Target) return;

    LastTurretShotTime = World->GetTimeSeconds();
    MarkActed();

    AActor* OwnerActor = OwningCharacter.Get();
    const ABreakerCharacter* OwnerBreaker = Cast<ABreakerCharacter>(OwnerActor);
    const UBreakerAttributeSet* OwnerAttributes = OwnerBreaker ? OwnerBreaker->GetAttributes() : nullptr;

    FBreakerDamageRequest Damage;
    Damage.BaseDamage = BaseDamage;
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.ProcCoefficient = TurretProcCoefficient;   // §G3: 0.5
    Damage.CriticalChance = OwnerAttributes ? OwnerAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
    Damage.CriticalMultiplier = OwnerAttributes ? OwnerAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
    Damage.SourceDamageMultiplier = OwnerAttributes ? OwnerAttributes->GetDamageMultiplier() : 1.0f;
    Damage.SourceLocation = MuzzleLocation;
    Damage.bHasSourceLocation = true;
    Damage.ImpactLocation = Target->GetActorLocation();
    Damage.bHasImpactLocation = true;
    // Instigator attribution (§G3 / SI-8): the PLAYER is the source, so kill
    // credit, XP, quests and OnKillDealt-driven Scrap all flow normally.
    // Deliberately NOT run through the owner's outgoing-modifier chain: those
    // are personal ability windows (Overdrive's shape), and a turret firing
    // during one would double-dip a buff authored for the player's own hands.
    Damage.SetInstigator(OwnerActor);

    if (UBreakerCombatComponent* TargetCombat = Target->FindComponentByClass<UBreakerCombatComponent>())
    {
        const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Damage);
        // §1.1: deployable damage credits Scrap per damage ACTUALLY APPLIED —
        // overkill pays nothing, which HealthDamage's clamp already guarantees.
        if (OwnerActor)
        {
            if (UBreakerScrapComponent* Scrap = OwnerActor->FindComponentByClass<UBreakerScrapComponent>())
            {
                Scrap->NotifyDeployableDamageDealt(Result.HealthDamage + Result.ShieldDamage);
            }
        }
    }
}

void ABreakerDeployable::TickAmmoCrate(float DeltaSeconds)
{
    // NEAREST HONEST INTERACT (§G4): the crate dispenses to the owner standing
    // beside it, one charge per interval, instead of the F-key interact chain
    // this pass does not own. Fully self-usable at full value; a party would
    // drain the shared pool faster, which is an efficiency difference and
    // never a solo penalty — the charge pool is the shared thing.
    UWorld* World = GetWorld();
    AActor* OwnerActor = OwningCharacter.Get();
    if (!World || !OwnerActor || ChargesRemaining <= 0) return;
    if (World->GetTimeSeconds() - LastCrateUseTime < CrateUseInterval) return;
    if (FVector::DistSquared(GetActorLocation(), OwnerActor->GetActorLocation()) > CrateUseRadiusCm * CrateUseRadiusCm) return;

    UBreakerWeaponComponent* Weapon = OwnerActor->FindComponentByClass<UBreakerWeaponComponent>();
    if (!Weapon) return;

    LastCrateUseTime = World->GetTimeSeconds();
    --ChargesRemaining;
    MarkActed();
    // §G4: a portion of base reserve per charge. AddReserveAmmoFraction already
    // caps at 2x starting reserve and tops the stowed slot too. Generates NO
    // Scrap on use (§1.1).
    Weapon->AddReserveAmmoFraction(CrateReserveFraction);

    if (ChargesRemaining <= 0)
    {
        // §2.2: "45s or on charges exhausted, whichever first" — through the
        // one destruction path, so the refund still happens.
        DestroyDeployable();
    }
}

void ABreakerDeployable::TickMineCluster(float DeltaSeconds)
{
    UWorld* World = GetWorld();
    if (!World) return;

    bool bAnyLive = false;
    for (int32 Index = 0; Index < Mines.Num(); ++Index)
    {
        FMineCharge& Mine = Mines[Index];
        if (!Mine.bLive) continue;
        bAnyLive = true;
        if (Mine.ArmRemaining > 0.0f)
        {
            Mine.ArmRemaining -= DeltaSeconds;
            continue;
        }
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            const ABreakerEnemy* Candidate = *It;
            if (!Candidate) continue;
            const UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
            if (!CandidateCombat || CandidateCombat->IsDead()) continue;
            if (FVector::DistSquared(Mine.Location, Candidate->GetActorLocation()) <= MineTriggerRadiusCm * MineTriggerRadiusCm)
            {
                DetonateMine(Index);
                break;
            }
        }
    }

    if (!bAnyLive)
    {
        // §2.2: "60s or on all charges triggered".
        DestroyDeployable();
    }
}

void ABreakerDeployable::DetonateMine(int32 MineIndex)
{
    UWorld* World = GetWorld();
    if (!World || !Mines.IsValidIndex(MineIndex)) return;
    FMineCharge& Mine = Mines[MineIndex];
    Mine.bLive = false;
    MarkActed();

    const float BaseDamage = OwnerWeaponBaseDamage() * MineDamageCoefficient;
    if (BaseDamage <= 0.0f) return;

    AActor* OwnerActor = OwningCharacter.Get();
    const ABreakerCharacter* OwnerBreaker = Cast<ABreakerCharacter>(OwnerActor);
    const UBreakerAttributeSet* OwnerAttributes = OwnerBreaker ? OwnerBreaker->GetAttributes() : nullptr;

    // Radial damage with linear falloff to the edge fraction — the rocket's
    // falloff shape reused (§G5). Enemies only: a Gunsmith's own mine must not
    // feed the Tank's self-damage rules or clip the owner.
    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        ABreakerEnemy* Candidate = *It;
        if (!Candidate) continue;
        UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!CandidateCombat || CandidateCombat->IsDead()) continue;
        const float Distance = FVector::Dist(Mine.Location, Candidate->GetActorLocation());
        if (Distance > MineBlastRadiusCm) continue;
        const float Falloff = FMath::Lerp(1.0f, MineEdgeDamageFraction, FMath::Clamp(Distance / MineBlastRadiusCm, 0.0f, 1.0f));

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage * Falloff;
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        Damage.CriticalChance = OwnerAttributes ? OwnerAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = OwnerAttributes ? OwnerAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        Damage.SourceDamageMultiplier = OwnerAttributes ? OwnerAttributes->GetDamageMultiplier() : 1.0f;
        Damage.SourceLocation = Mine.Location;
        Damage.bHasSourceLocation = true;
        Damage.SetInstigator(OwnerActor);

        const FBreakerDamageResult Result = CandidateCombat->ReceiveDamage(Damage);
        if (OwnerActor)
        {
            if (UBreakerScrapComponent* Scrap = OwnerActor->FindComponentByClass<UBreakerScrapComponent>())
            {
                Scrap->NotifyDeployableDamageDealt(Result.HealthDamage + Result.ShieldDamage);
            }
        }
    }
}

void ABreakerDeployable::HandleZoneOccupantEntered(AActor* Occupant)
{
    // §G6: enemies inside the field are slowed. Applied through the enemy's own
    // public movement-profile mutator against its AUTHORED base speed, so
    // re-entry does not compound; the armour strip is the zone's own keyed rule.
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Occupant))
    {
        MarkActed();
        Enemy->ApplyModifierMovementProfile(DisruptorSlowMultiplier, -1.0f);
        SlowedEnemies.AddUnique(Enemy);
    }
}

void ABreakerDeployable::HandleZoneOccupantExited(AActor* Occupant)
{
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Occupant))
    {
        // KNOWN LIMITATION (recorded on the tunable): restores to the authored
        // 1.0x, which stomps a Fleetfoot modifier's own profile. No keyed slow
        // primitive exists on enemies to compose with.
        Enemy->ApplyModifierMovementProfile(1.0f, -1.0f);
        SlowedEnemies.Remove(Enemy);
    }
}

void ABreakerDeployable::HandleCombatDeath()
{
    DestroyDeployable();
}

void ABreakerDeployable::DestroyDeployable()
{
    if (bDestroyed) return;
    bDestroyed = true;

    // ONE destruction path, one refund (§2.2): expiry, damage death and the
    // density cull are indistinguishable to the economy. REFUND, NEVER PROFIT —
    // the Scrap component owns the 50% fraction. The Anchor Point carries no
    // Scrap cost and so refunds nothing ("this is not a Scrap economy", §T3).
    if (ScrapCost > 0.0f)
    {
        if (AActor* OwnerActor = OwningCharacter.Get())
        {
            if (UBreakerScrapComponent* Scrap = OwnerActor->FindComponentByClass<UBreakerScrapComponent>())
            {
                Scrap->NotifyDeployableDestroyed(ScrapCost);
            }
        }
    }

    OnDeployableDestroyed.Broadcast(this);
    Destroy();
}

void ABreakerDeployable::EndPlay(const EEndPlayReason::Type Reason)
{
    // Restore anything this Disruptor was slowing; a field that dies must not
    // leave enemies at half speed forever.
    for (const TWeakObjectPtr<AActor>& Slowed : SlowedEnemies)
    {
        if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Slowed.Get()))
        {
            Enemy->ApplyModifierMovementProfile(1.0f, -1.0f);
        }
    }
    SlowedEnemies.Reset();
    if (DisruptorZone && !DisruptorZone->IsActorBeingDestroyed())
    {
        DisruptorZone->Destroy();
    }
    BreakerDeployableLocal::BreakerLiveDeployables.Remove(this);
    Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------
// Placement + density statics
// ---------------------------------------------------------------------------

bool ABreakerDeployable::ResolvePlacement(UWorld* World, AActor* OwnerCharacter, const FVector& ViewLocation,
    const FVector& ViewDirection, float RangeCm, FVector& OutLocation)
{
    if (!World) return false;

    // §2.3: seed 8 m along the aim ray, snapping to the nearest valid floor.
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerDeployablePlace), false, OwnerCharacter);
    FHitResult AimHit;
    const FVector AimEnd = ViewLocation + ViewDirection.GetSafeNormal() * FMath::Max(0.0f, RangeCm);
    const FVector Anchor = World->LineTraceSingleByChannel(AimHit, ViewLocation, AimEnd, ECC_Visibility, QueryParams)
        ? AimHit.ImpactPoint : AimEnd;

    // Floor snap: 1.5 m of tolerance under the impact point (§2.3). Floor
    // objects only — no wall or ceiling placement in this design.
    FHitResult FloorHit;
    const FVector SnapStart = Anchor + FVector(0.0f, 0.0f, 50.0f);
    const FVector SnapEnd = Anchor - FVector(0.0f, 0.0f, 150.0f);
    if (!World->LineTraceSingleByChannel(FloorHit, SnapStart, SnapEnd, ECC_Visibility, QueryParams))
    {
        // §2.3: fails LOUDLY rather than silently relocating, and a failed
        // placement costs nothing — callers check before committing.
        UE_LOG(LogTemp, Warning, TEXT("BreakerDeployable: no valid floor within 1.5 m of the aim point; placement refused."));
        return false;
    }
    OutLocation = FloorHit.ImpactPoint + FVector(0.0f, 0.0f, 2.0f);
    return true;
}

const TArray<TWeakObjectPtr<ABreakerDeployable>>& ABreakerDeployable::GetLiveDeployables()
{
    BreakerDeployableLocal::BreakerLiveDeployables.RemoveAll(
        [](const TWeakObjectPtr<ABreakerDeployable>& Entry) { return !Entry.IsValid(); });
    return BreakerDeployableLocal::BreakerLiveDeployables;
}

int32 ABreakerDeployable::CountOwnedDeployables(const AActor* OwnerCharacter, int32& OutTotal, EBreakerDeployableType Type, int32& OutOfType)
{
    OutTotal = 0;
    OutOfType = 0;
    for (const TWeakObjectPtr<ABreakerDeployable>& Entry : GetLiveDeployables())
    {
        const ABreakerDeployable* Deployable = Entry.Get();
        if (!Deployable || Deployable->GetOwningCharacter() != OwnerCharacter) continue;
        // The Anchor Point is outside the Scrap density economy (see the enum).
        if (Deployable->GetDeployableType() == EBreakerDeployableType::AnchorPoint) continue;
        ++OutTotal;
        if (Deployable->GetDeployableType() == Type) ++OutOfType;
    }
    return OutTotal;
}

void ABreakerDeployable::PushDensityCapOverride(AActor* OwnerCharacter, int32 NewTotalCap, double ExpiryWorldTime)
{
    if (!OwnerCharacter) return;
    BreakerDeployableLocal::FBreakerDensityCapOverride Override;
    Override.TotalCap = FMath::Max(NewTotalCap, BaseTotalDensityCap);
    Override.ExpiryWorldTime = ExpiryWorldTime;
    BreakerDeployableLocal::BreakerDensityCapOverrides.Add(OwnerCharacter, Override);
}

int32 ABreakerDeployable::TotalCapFor(const AActor* OwnerCharacter)
{
    using namespace BreakerDeployableLocal;
    if (const FBreakerDensityCapOverride* Override = BreakerDensityCapOverrides.Find(OwnerCharacter))
    {
        const UWorld* World = OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr;
        const double Now = World ? World->GetTimeSeconds() : 0.0;
        if (Override->ExpiryWorldTime < 0.0 || Now < Override->ExpiryWorldTime)
        {
            return Override->TotalCap;
        }
        BreakerDensityCapOverrides.Remove(OwnerCharacter);
    }
    return BaseTotalDensityCap;
}

void ABreakerDeployable::EnforceDensityCapForPlacement(AActor* OwnerCharacter, EBreakerDeployableType TypeAboutToPlace)
{
    if (!OwnerCharacter || TypeAboutToPlace == EBreakerDeployableType::AnchorPoint) return;

    // Destroy-oldest until the new placement fits (§2.1). Oldest is PLACEMENT
    // ORDER, never remaining lifetime; the cull refunds through the one
    // destruction path, and it never fails and never prompts — the class must
    // not be blocked by its own furniture. The per-type cap of 2 is what stops
    // turret-stacking from being the only build; NOTHING raises it.
    auto CullOldest = [OwnerCharacter](bool bSameTypeOnly, EBreakerDeployableType Type)
    {
        ABreakerDeployable* Oldest = nullptr;
        for (const TWeakObjectPtr<ABreakerDeployable>& Entry : GetLiveDeployables())
        {
            ABreakerDeployable* Deployable = Entry.Get();
            if (!Deployable || Deployable->GetOwningCharacter() != OwnerCharacter) continue;
            if (Deployable->GetDeployableType() == EBreakerDeployableType::AnchorPoint) continue;
            if (bSameTypeOnly && Deployable->GetDeployableType() != Type) continue;
            if (!Oldest || Deployable->PlacementSerial < Oldest->PlacementSerial) Oldest = Deployable;
        }
        if (Oldest) Oldest->DestroyDeployable();
    };

    int32 Total = 0;
    int32 OfType = 0;
    CountOwnedDeployables(OwnerCharacter, Total, TypeAboutToPlace, OfType);
    while (OfType >= PerTypeDensityCap)
    {
        CullOldest(true, TypeAboutToPlace);
        CountOwnedDeployables(OwnerCharacter, Total, TypeAboutToPlace, OfType);
    }
    while (Total >= TotalCapFor(OwnerCharacter))
    {
        CullOldest(false, TypeAboutToPlace);
        CountOwnedDeployables(OwnerCharacter, Total, TypeAboutToPlace, OfType);
    }
}
