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
#include "Progression/BreakerProgressionComponent.h"
#include "Progression/BreakerProgressionLibrary.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponDefinition.h"

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

    // FT5 Requisition's pending replacement discounts: one live credit per
    // owner+type, world-time expiring. The same static-registry shape as the
    // density override above.
    struct FBreakerReplacementCredit
    {
        EBreakerDeployableType Type = EBreakerDeployableType::Turret;
        float Discount = 0.0f;
        double ExpiryWorldTime = -1.0;
    };
    static TMap<TWeakObjectPtr<const AActor>, TArray<FBreakerReplacementCredit>> BreakerReplacementCredits;
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
    {
        LifetimeRemaining = 60.0f;   // §2.2: 60s or all charges triggered
        Health = MineClusterHealth;
        // TK7 Ordnance: 4 charges instead of 3. TK2 Quick Set: the arm delay
        // halves (R2: goes to zero, trading a smaller trigger radius for 1s —
        // the radius half lives in the trigger check).
        const int32 EffectiveCount = OrdnanceMineCount(OwnerHasNodeTag(BreakerNodeTags::Node_TK_Ordnance.GetTag()), MineCount);
        const float EffectiveArmDelay = QuickSetArmDelay(OwnerNodeRank(TEXT("Gunsmith.Tinkerer.QuickSet")), MineArmDelay);
        for (int32 Index = 0; Index < EffectiveCount; ++Index)
        {
            FMineCharge Mine;
            const float Angle = (2.0f * PI / FMath::Max(1, EffectiveCount)) * Index;
            Mine.Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * MineSpreadCm;
            Mine.ArmRemaining = EffectiveArmDelay;
            Mines.Add(Mine);
        }
        break;
    }
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

    // The authored clock, before any node extends it: FT3's 2x ceiling is
    // measured against THIS, never against an already-extended remainder.
    BaseLifetime = LifetimeRemaining;

    // TK6 Overlap: an overlapping owned Disruptor extends BOTH fields to the
    // longer of the two remaining lifetimes. The armour strip's never-stack
    // half is already structural (one zone tag, one keyed strip) — the node
    // adds only the benefit half.
    if (Type == EBreakerDeployableType::Disruptor && OwnerHasNodeTag(BreakerNodeTags::Node_TK_Overlap.GetTag()))
    {
        for (const TWeakObjectPtr<ABreakerDeployable>& Entry : GetLiveDeployables())
        {
            ABreakerDeployable* Other = Entry.Get();
            if (!Other || Other == this || Other->GetOwningCharacter() != InOwnerCharacter) continue;
            if (Other->GetDeployableType() != EBreakerDeployableType::Disruptor) continue;
            const float OverlapReach = (DisruptorRadiusCm + Other->DisruptorRadiusCm);
            if (FVector::DistSquared(GetActorLocation(), Other->GetActorLocation()) > OverlapReach * OverlapReach) continue;
            const float Longer = FMath::Max(LifetimeRemaining, Other->LifetimeRemaining);
            LifetimeRemaining = Longer;
            Other->LifetimeRemaining = Longer;
            if (Other->DisruptorZone) Other->DisruptorZone->RefreshDuration(Longer);
        }
    }

    // Node event wiring, bound once per deployable and tag-checked at event
    // time so a respec mid-life changes the rule without a rebind. Reload is
    // every type's seam (FT3); the combat pair is per-type.
    if (const AActor* OwnerActor = OwningCharacter.Get())
    {
        if (UBreakerWeaponComponent* OwnerWeapon = OwnerActor->FindComponentByClass<UBreakerWeaponComponent>())
        {
            OwnerWeapon->OnReloadCompleted.AddDynamic(this, &ABreakerDeployable::HandleOwnerReloadCompleted);
            BoundOwnerWeapon = OwnerWeapon;
        }
        if (Type == EBreakerDeployableType::Turret || Type == EBreakerDeployableType::Disruptor)
        {
            if (UBreakerCombatComponent* OwnerCombat = OwnerActor->FindComponentByClass<UBreakerCombatComponent>())
            {
                if (Type == EBreakerDeployableType::Turret)
                {
                    OwnerCombat->OnHitDealt.AddDynamic(this, &ABreakerDeployable::HandleOwnerHitDealt);
                }
                else
                {
                    OwnerCombat->OnKillDealt.AddDynamic(this, &ABreakerDeployable::HandleOwnerKillDealt);
                }
                BoundOwnerCombat = OwnerCombat;
            }
        }
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

    // Age never pauses: TK9's "armed and untriggered for 10s" is about how
    // long the trap has waited, which a Foundry lifetime pause does not reset.
    AgeSeconds += DeltaSeconds;

    if (!bLifetimePaused)
    {
        LifetimeRemaining -= DeltaSeconds;
        if (LifetimeRemaining <= 0.0f)
        {
            DestroyDeployableWithCause(EBreakerDeployableDestructionCause::Expired);
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
    const double Now = World->GetTimeSeconds();

    // FT2 R2 / FT10: the tracked target dying opens the reacquire rules. R2
    // Overwatch skips the reacquire wait; Automation additionally makes the
    // immediate shot FREE (it does not advance the cadence clock).
    const bool bHasOverwatchR2 = OwnerNodeRank(TEXT("Gunsmith.FieldTech.Overwatch")) >= 2;
    const bool bHasAutomation = OwnerHasNodeTag(BreakerNodeTags::Node_FT_Automation.GetTag());
    if (CurrentTurretTarget.IsValid())
    {
        const UBreakerCombatComponent* TrackedCombat = CurrentTurretTarget->FindComponentByClass<UBreakerCombatComponent>();
        if (TrackedCombat && TrackedCombat->IsDead())
        {
            if (bHasOverwatchR2 || bHasAutomation) bTurretFreeShotPending = true;
            CurrentTurretTarget.Reset();
        }
    }
    else if (CurrentTurretTarget.IsStale())
    {
        if (bHasOverwatchR2 || bHasAutomation) bTurretFreeShotPending = true;
        CurrentTurretTarget.Reset();
    }

    if (!bTurretFreeShotPending && Now - LastTurretShotTime < TurretFireInterval) return;

    const float BaseDamage = OwnerWeaponBaseDamage() * TurretDamageCoefficient;
    if (BaseDamage <= 0.0f) return;

    // Base rule (§G3): nearest valid target with line of sight. Deliberately
    // artless — a turret is consistent, never optimal. The node layer sharpens
    // it: FT2 Overwatch prefers the target the owner last damaged; FT7
    // Emplacement acquires through the owner's crosshair priority and holds a
    // target through brief LOS breaks.
    const bool bHasOverwatch = OwnerHasNodeTag(BreakerNodeTags::Node_FT_Overwatch.GetTag());
    const bool bHasEmplacement = OwnerHasNodeTag(BreakerNodeTags::Node_FT_Emplacement.GetTag());
    const FVector MuzzleLocation = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);

    auto HasLineOfSight = [&](const ABreakerEnemy* Candidate) -> bool
    {
        FHitResult Blocked;
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerTurretLOS), false, this);
        QueryParams.AddIgnoredActor(Candidate);
        if (OwningCharacter.IsValid()) QueryParams.AddIgnoredActor(OwningCharacter.Get());
        return !World->LineTraceSingleByChannel(Blocked, MuzzleLocation, Candidate->GetActorLocation(), ECC_Visibility, QueryParams);
    };
    auto IsValidTarget = [&](const ABreakerEnemy* Candidate) -> bool
    {
        if (!Candidate) return false;
        const UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!CandidateCombat || CandidateCombat->IsDead()) return false;
        return FVector::DistSquared(MuzzleLocation, Candidate->GetActorLocation()) < TurretRangeCm * TurretRangeCm;
    };

    // FT7's grace: a held target surviving a brief LOS break stays held (and
    // is not fired at until LOS returns). Without the node the held target is
    // simply re-derived every shot, which is bit-identical to the old scan.
    ABreakerEnemy* Target = nullptr;
    if (bHasEmplacement && CurrentTurretTarget.IsValid() && IsValidTarget(CurrentTurretTarget.Get()))
    {
        if (HasLineOfSight(CurrentTurretTarget.Get()))
        {
            Target = CurrentTurretTarget.Get();
            TurretLOSLostTime = -1000.0;
        }
        else
        {
            if (TurretLOSLostTime < 0.0) TurretLOSLostTime = Now;
            if (Now - TurretLOSLostTime <= TurretLOSGraceSeconds)
            {
                return;   // held through the break; no shot while blind
            }
            CurrentTurretTarget.Reset();
            TurretLOSLostTime = -1000.0;
        }
    }

    // FT2: the owner's last-damaged enemy outranks everything, in range and
    // line of sight ("if it is in range and line of sight" — the doc's gate).
    if (!Target && bHasOverwatch && LastOwnerDamagedEnemy.IsValid()
        && IsValidTarget(LastOwnerDamagedEnemy.Get()) && HasLineOfSight(LastOwnerDamagedEnemy.Get()))
    {
        Target = LastOwnerDamagedEnemy.Get();
    }

    if (!Target)
    {
        // FT7: crosshair priority — the candidate nearest the owner's aim ray.
        // Base: nearest by distance.
        FVector OwnerViewDir = FVector::ZeroVector;
        FVector OwnerViewOrigin = MuzzleLocation;
        if (bHasEmplacement)
        {
            if (const APawn* OwnerPawn = Cast<APawn>(OwningCharacter.Get()))
            {
                OwnerViewOrigin = OwnerPawn->GetActorLocation();
                OwnerViewDir = OwnerPawn->GetControlRotation().Vector();
            }
            else if (const AActor* OwnerActor = OwningCharacter.Get())
            {
                OwnerViewOrigin = OwnerActor->GetActorLocation();
                OwnerViewDir = OwnerActor->GetActorRotation().Vector();
            }
        }
        float BestScore = TNumericLimits<float>::Max();
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            ABreakerEnemy* Candidate = *It;
            if (!IsValidTarget(Candidate)) continue;
            float Score;
            if (bHasEmplacement && !OwnerViewDir.IsNearlyZero())
            {
                const FVector ToCandidate = (Candidate->GetActorLocation() - OwnerViewOrigin).GetSafeNormal();
                Score = 1.0f - FVector::DotProduct(OwnerViewDir, ToCandidate);   // smallest angle wins
            }
            else
            {
                Score = FVector::DistSquared(MuzzleLocation, Candidate->GetActorLocation());
            }
            if (Score >= BestScore) continue;
            if (!HasLineOfSight(Candidate)) continue;
            Target = Candidate;
            BestScore = Score;
        }
    }
    if (!Target)
    {
        bTurretFreeShotPending = false;
        return;
    }
    CurrentTurretTarget = Target;

    // FT10: the free burst does not advance the cadence clock — the turret's
    // next ordinary shot still lands on time. FT2 R2 merely skipped the wait.
    const bool bFreeShot = bTurretFreeShotPending;
    bTurretFreeShotPending = false;
    if (!bFreeShot || !bHasAutomation)
    {
        LastTurretShotTime = Now;
    }
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
    MarkActed();

    // FT6 Foreman: a full-reserve interactor consumes charges at half rate —
    // every other dispense is free. Full reserve is the 2x-starting cap
    // AddReserveAmmoFraction itself enforces, so the two rules agree on what
    // "full" means.
    const int32 ForemanRank = OwnerNodeRank(TEXT("Gunsmith.FieldTech.Foreman"));
    bool bConsumeCharge = true;
    if (ForemanRank > 0)
    {
        const UBreakerWeaponDefinition* Definition = Weapon->GetActiveDefinition();
        const bool bReserveFull = Definition && Weapon->GetReserveAmmo() >= Definition->StartingReserveAmmo * 2;
        if (bReserveFull)
        {
            bForemanSkipCharge = !bForemanSkipCharge;
            bConsumeCharge = bForemanSkipCharge;
        }
    }
    if (bConsumeCharge) --ChargesRemaining;

    // §G4: a portion of base reserve per charge. AddReserveAmmoFraction already
    // caps at 2x starting reserve and tops the stowed slot too. Generates NO
    // Scrap on use (§1.1).
    Weapon->AddReserveAmmoFraction(CrateReserveFraction);

    // FT6's heal half, through the ONE healing path so overheal rules and
    // listeners all see it (rank 2 doubles the portion, per the row).
    if (ForemanRank > 0)
    {
        if (UBreakerCombatComponent* OwnerCombat = OwnerActor->FindComponentByClass<UBreakerCombatComponent>())
        {
            OwnerCombat->ApplyHealingAmount(ForemanHealPerCharge * (ForemanRank >= 2 ? 2.0f : 1.0f), OwnerActor, FGameplayTag());
        }
    }

    if (ChargesRemaining <= 0)
    {
        // §2.2: "45s or on charges exhausted, whichever first" — through the
        // one destruction path, so the refund still happens.
        DestroyDeployableWithCause(EBreakerDeployableDestructionCause::Exhausted);
    }
}

void ABreakerDeployable::TickMineCluster(float DeltaSeconds)
{
    UWorld* World = GetWorld();
    if (!World) return;

    const int32 QuickSetRank = OwnerNodeRank(TEXT("Gunsmith.Tinkerer.QuickSet"));
    const bool bTripwire = OwnerHasNodeTag(BreakerNodeTags::Node_TK_Tripwire.GetTag());

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
        Mine.SecondsSinceArmed += DeltaSeconds;

        // TK2 R2's trade: a no-delay charge triggers on a smaller radius for
        // its first armed second. TK3 Tripwire swaps the condition entirely:
        // line of sight within the tripwire range instead of proximity. (TK3
        // R2's per-placement choice still waits — there is no placement-time
        // input seam to hang a cycling choice on.)
        const float TriggerRadius = QuickSetTriggerRadius(QuickSetRank, Mine.SecondsSinceArmed, MineTriggerRadiusCm);
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            const ABreakerEnemy* Candidate = *It;
            if (!Candidate) continue;
            const UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
            if (!CandidateCombat || CandidateCombat->IsDead()) continue;

            bool bTriggered;
            if (bTripwire)
            {
                if (FVector::DistSquared(Mine.Location, Candidate->GetActorLocation()) > TripwireTriggerRangeCm * TripwireTriggerRangeCm)
                {
                    continue;
                }
                FHitResult Blocked;
                FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BreakerMineTripwire), false, this);
                QueryParams.AddIgnoredActor(Candidate);
                if (OwningCharacter.IsValid()) QueryParams.AddIgnoredActor(OwningCharacter.Get());
                bTriggered = !World->LineTraceSingleByChannel(Blocked,
                    Mine.Location + FVector(0.0f, 0.0f, 20.0f), Candidate->GetActorLocation(), ECC_Visibility, QueryParams);
            }
            else
            {
                bTriggered = FVector::DistSquared(Mine.Location, Candidate->GetActorLocation()) <= TriggerRadius * TriggerRadius;
            }
            if (bTriggered)
            {
                // TK9 Patience: a 10s-patient charge triggers as if it had one
                // extra charge — the next live armed charge goes with it.
                const bool bPatient = OwnerHasNodeTag(BreakerNodeTags::Node_TK_Patience.GetTag())
                    && PatienceQualifies(Mine.SecondsSinceArmed);
                DetonateMine(Index);
                if (bPatient)
                {
                    for (int32 Extra = 0; Extra < Mines.Num(); ++Extra)
                    {
                        if (Mines[Extra].bLive && Mines[Extra].ArmRemaining <= 0.0f)
                        {
                            DetonateMine(Extra);
                            break;
                        }
                    }
                }
                break;
            }
        }
    }

    if (!bAnyLive)
    {
        // TK4 Rearm: an emptied cluster rearms one charge every 6s (R2: 4s)
        // for the rest of its lifetime, up to its original count — so the
        // exhausted-cluster destruction only fires for a build without it.
        const float Interval = RearmInterval(OwnerNodeRank(TEXT("Gunsmith.Tinkerer.Rearm")));
        if (Interval > 0.0f)
        {
            RearmAccumulator += DeltaSeconds;
            if (RearmAccumulator >= Interval)
            {
                RearmAccumulator = 0.0f;
                for (FMineCharge& Mine : Mines)
                {
                    if (Mine.bLive) continue;
                    Mine.bLive = true;
                    Mine.ArmRemaining = QuickSetArmDelay(OwnerNodeRank(TEXT("Gunsmith.Tinkerer.QuickSet")), MineArmDelay);
                    Mine.SecondsSinceArmed = 0.0f;
                    break;
                }
            }
            return;
        }
        // §2.2: "60s or on all charges triggered".
        DestroyDeployableWithCause(EBreakerDeployableDestructionCause::Exhausted);
    }
}

void ABreakerDeployable::DetonateMine(int32 MineIndex)
{
    UWorld* World = GetWorld();
    if (!World || !Mines.IsValidIndex(MineIndex)) return;
    FMineCharge& Mine = Mines[MineIndex];
    if (!Mine.bLive) return;
    Mine.bLive = false;
    MarkActed();

    // TK7's anti-explosion clause: same-cluster charges detonating within 1s
    // are ONE damage instance for proc purposes — the follow-on blasts land
    // their damage at proc coefficient zero.
    const float ProcCoefficient = OrdnanceProcCoefficient(
        OwnerHasNodeTag(BreakerNodeTags::Node_TK_Ordnance.GetTag()), World->GetTimeSeconds(), LastMineDetonationTime);
    LastMineDetonationTime = World->GetTimeSeconds();

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
        Damage.ProcCoefficient = ProcCoefficient;
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

        // TK9 Patience, the Disruptor half: a field armed and unentered for
        // 10s applies its FLAT armour cut at double value on the FIRST enemy
        // to enter — a second keyed strip beside the zone's own, popped on
        // exit. Flat and keyed, so it can never invert mitigation (§G6).
        if (!bAnyEnemyEnteredField)
        {
            if (OwnerHasNodeTag(BreakerNodeTags::Node_TK_Patience.GetTag()) && PatienceQualifies(AgeSeconds))
            {
                if (UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>())
                {
                    EnemyCombat->PushArmorReduction(FName(*FString::Printf(TEXT("Deployable.Patience.%d"), PlacementSerial)), DisruptorArmorReduction);
                    PatienceStruckEnemies.AddUnique(Enemy);
                }
            }
            bAnyEnemyEnteredField = true;
        }
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
        if (PatienceStruckEnemies.Remove(Enemy) > 0)
        {
            if (UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>())
            {
                EnemyCombat->PopArmorReduction(FName(*FString::Printf(TEXT("Deployable.Patience.%d"), PlacementSerial)));
            }
        }
    }
}

void ABreakerDeployable::HandleCombatDeath()
{
    // Deployables acquire no damage from their owner and enemies are the only
    // thing shooting at them, so a combat death IS enemy destruction — the
    // cause FT5 Requisition and FT11 Deadman key on.
    DestroyDeployableWithCause(EBreakerDeployableDestructionCause::EnemyDamage);
}

void ABreakerDeployable::DestroyDeployable()
{
    DestroyDeployableWithCause(EBreakerDeployableDestructionCause::Expired);
}

void ABreakerDeployable::DestroyDeployableWithCause(EBreakerDeployableDestructionCause Cause)
{
    if (bDestroyed) return;
    bDestroyed = true;

    const bool bEnemyDestroyed = Cause == EBreakerDeployableDestructionCause::EnemyDamage;

    // FT11 Deadman: an enemy-destroyed deployable detonates BEFORE refunding.
    // Cannot chain by construction: the blast damages enemies only, so no
    // second deployable can die to it.
    if (bEnemyDestroyed && OwnerHasNodeTag(BreakerNodeTags::Node_FT_Deadman.GetTag()))
    {
        DetonateRadialBlast(GetActorLocation(), DeadmanDamageCoefficient, DeadmanBlastRadiusCm, 1.0f);
    }

    // ONE destruction path, one refund arithmetic (§2.2): expiry, damage death
    // and the density cull pay identically; the Salvage fraction lives on the
    // Scrap component. Two causes bend the DELIVERY, never the arithmetic:
    // Command (TK11) refunds nothing — the doc's own words — and Requisition
    // (FT5) pays the enemy-destruction refund IMMEDIATELY (outside the metered
    // budget) and arms the same-type replacement discount for 8s. The Anchor
    // Point carries no Scrap cost and so refunds nothing (§T3).
    if (ScrapCost > 0.0f && Cause != EBreakerDeployableDestructionCause::Command)
    {
        if (AActor* OwnerActor = OwningCharacter.Get())
        {
            if (UBreakerScrapComponent* Scrap = OwnerActor->FindComponentByClass<UBreakerScrapComponent>())
            {
                const int32 RequisitionRank = OwnerNodeRank(TEXT("Gunsmith.FieldTech.Requisition"));
                if (bEnemyDestroyed && RequisitionRank > 0)
                {
                    Scrap->GrantScrap(UBreakerScrapComponent::DestructionRefund(ScrapCost, Scrap->GetEffectiveDestructionRefundFraction()));
                    const UWorld* World = GetWorld();
                    RegisterReplacementCredit(OwnerActor, Type, RequisitionDiscountFor(RequisitionRank),
                        (World ? World->GetTimeSeconds() : 0.0) + 8.0);   // §FT5: "within 8s"
                }
                else
                {
                    Scrap->NotifyDeployableDestroyed(ScrapCost);
                }
            }
        }
    }

    OnDeployableDestroyed.Broadcast(this);
    Destroy();
}

void ABreakerDeployable::DetonateRadialBlast(const FVector& Center, float DamageCoefficient, float RadiusCm, float ProcCoefficient)
{
    UWorld* World = GetWorld();
    const float BaseDamage = OwnerWeaponBaseDamage() * DamageCoefficient;
    if (!World || BaseDamage <= 0.0f || RadiusCm <= 0.0f) return;

    AActor* OwnerActor = OwningCharacter.Get();
    const ABreakerCharacter* OwnerBreaker = Cast<ABreakerCharacter>(OwnerActor);
    const UBreakerAttributeSet* OwnerAttributes = OwnerBreaker ? OwnerBreaker->GetAttributes() : nullptr;

    for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
    {
        ABreakerEnemy* Candidate = *It;
        if (!Candidate) continue;
        UBreakerCombatComponent* CandidateCombat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!CandidateCombat || CandidateCombat->IsDead()) continue;
        const float Distance = FVector::Dist(Center, Candidate->GetActorLocation());
        if (Distance > RadiusCm) continue;
        const float Falloff = FMath::Lerp(1.0f, MineEdgeDamageFraction, FMath::Clamp(Distance / RadiusCm, 0.0f, 1.0f));

        FBreakerDamageRequest Damage;
        Damage.BaseDamage = BaseDamage * Falloff;
        Damage.DamageFamily = EBreakerDamageFamily::Physical;
        Damage.ProcCoefficient = ProcCoefficient;
        Damage.CriticalChance = OwnerAttributes ? OwnerAttributes->GetCriticalChance() : UBreakerAttributeSet::DefaultCriticalChance;
        Damage.CriticalMultiplier = OwnerAttributes ? OwnerAttributes->GetCriticalMultiplier() : UBreakerAttributeSet::DefaultCriticalMultiplier;
        Damage.SourceDamageMultiplier = OwnerAttributes ? OwnerAttributes->GetDamageMultiplier() : 1.0f;
        Damage.SourceLocation = Center;
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
    // A dead field must not leave its Patience strip on anyone either.
    for (const TWeakObjectPtr<AActor>& Struck : PatienceStruckEnemies)
    {
        if (AActor* Enemy = Struck.Get())
        {
            if (UBreakerCombatComponent* EnemyCombat = Enemy->FindComponentByClass<UBreakerCombatComponent>())
            {
                EnemyCombat->PopArmorReduction(FName(*FString::Printf(TEXT("Deployable.Patience.%d"), PlacementSerial)));
            }
        }
    }
    PatienceStruckEnemies.Reset();
    // Unbind exactly what Initialize bound: a destroyed deployable listening
    // to the owner's reload is the stale-binding bug the Sidearm Rig teardown
    // comment warns about, one layer down.
    if (UBreakerWeaponComponent* OwnerWeapon = BoundOwnerWeapon.Get())
    {
        OwnerWeapon->OnReloadCompleted.RemoveDynamic(this, &ABreakerDeployable::HandleOwnerReloadCompleted);
    }
    BoundOwnerWeapon.Reset();
    if (UBreakerCombatComponent* OwnerCombat = BoundOwnerCombat.Get())
    {
        OwnerCombat->OnHitDealt.RemoveDynamic(this, &ABreakerDeployable::HandleOwnerHitDealt);
        OwnerCombat->OnKillDealt.RemoveDynamic(this, &ABreakerDeployable::HandleOwnerKillDealt);
    }
    BoundOwnerCombat.Reset();
    if (DisruptorZone && !DisruptorZone->IsActorBeingDestroyed())
    {
        DisruptorZone->Destroy();
    }
    BreakerDeployableLocal::BreakerLiveDeployables.Remove(this);
    Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------
// Node event handlers
// ---------------------------------------------------------------------------

void ABreakerDeployable::HandleOwnerReloadCompleted(bool bAnyRoundFired)
{
    // FT3 Second Shift: a reload completed near this deployable extends its
    // clock — once per deployable per reload by construction (one event per
    // reload), never past double the base lifetime. Any reload counts; the
    // bAnyRoundFired clause belongs to the SCRAP source, not to this one.
    const int32 Rank = OwnerNodeRank(TEXT("Gunsmith.FieldTech.SecondShift"));
    if (Rank <= 0 || bDestroyed) return;
    const AActor* OwnerActor = OwningCharacter.Get();
    if (!OwnerActor) return;
    if (FVector::DistSquared(GetActorLocation(), OwnerActor->GetActorLocation()) > SecondShiftRadiusCm * SecondShiftRadiusCm) return;

    const float NewRemaining = SecondShiftLifetime(Rank, BaseLifetime, LifetimeRemaining);
    if (NewRemaining <= LifetimeRemaining) return;
    LifetimeRemaining = NewRemaining;
    if (DisruptorZone) DisruptorZone->RefreshDuration(LifetimeRemaining);
}

void ABreakerDeployable::HandleOwnerHitDealt(const FBreakerHitContext& Hit)
{
    // FT2 Overwatch's tracking half. Recorded free of the node tag so buying
    // Overwatch mid-life starts working immediately; the PRIORITY rule is
    // gated at read time in TickTurret.
    if (ABreakerEnemy* Enemy = Cast<ABreakerEnemy>(Hit.Target.Get()))
    {
        LastOwnerDamagedEnemy = Enemy;
    }
}

void ABreakerDeployable::HandleOwnerKillDealt(const FBreakerHitContext& Hit)
{
    // TK5 Attrition Field: a kill landing inside this Disruptor's field pays
    // the field back. Position-checked against the field, not the emitter.
    if (Type != EBreakerDeployableType::Disruptor || bDestroyed) return;
    const AActor* Victim = Hit.Target.Get();
    if (!Victim) return;
    if (FVector::DistSquared(GetActorLocation(), Victim->GetActorLocation()) > DisruptorRadiusCm * DisruptorRadiusCm) return;
    if (UBreakerScrapComponent* Scrap = OwnerScrap())
    {
        Scrap->NotifyDisruptorFieldKill();
    }
}

// ---------------------------------------------------------------------------
// Node reads and pure rules
// ---------------------------------------------------------------------------

const UBreakerProgressionComponent* ABreakerDeployable::OwnerProgression() const
{
    const AActor* OwnerActor = OwningCharacter.Get();
    return OwnerActor ? OwnerActor->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
}

UBreakerScrapComponent* ABreakerDeployable::OwnerScrap() const
{
    const AActor* OwnerActor = OwningCharacter.Get();
    return OwnerActor ? OwnerActor->FindComponentByClass<UBreakerScrapComponent>() : nullptr;
}

int32 ABreakerDeployable::OwnerNodeRank(FName NodeId) const
{
    const UBreakerProgressionComponent* Progression = OwnerProgression();
    return Progression ? Progression->GetNodeRank(NodeId, EBreakerPointCurrency::ClassPoints) : 0;
}

bool ABreakerDeployable::OwnerHasNodeTag(const FGameplayTag& Tag) const
{
    const UBreakerProgressionComponent* Progression = OwnerProgression();
    return Progression && Progression->HasNodeTag(Tag);
}

bool ABreakerDeployable::CountsAgainstDensityCap(EBreakerDeployableType InType, bool bOwnerHasLogistics)
{
    // The Anchor Point sits outside the Scrap economy always (§T3); FT8
    // Logistics lifts the Ammo Crate out of the cap ("utility stops competing
    // with firepower").
    if (InType == EBreakerDeployableType::AnchorPoint) return false;
    if (InType == EBreakerDeployableType::AmmoCrate && bOwnerHasLogistics) return false;
    return true;
}

int32 ABreakerDeployable::BaseTotalCapFor(bool bHasRedundancy)
{
    // FT9: 4 -> 5 in total; per-type NEVER moves (§2.1: "nothing may raise it").
    return bHasRedundancy ? BaseTotalDensityCap + 1 : BaseTotalDensityCap;
}

float ABreakerDeployable::RequisitionDiscountFor(int32 Rank)
{
    if (Rank >= 2) return 18.0f;   // §FT5 R2, transcribed
    if (Rank == 1) return 10.0f;   // §FT5, transcribed
    return 0.0f;
}

float ABreakerDeployable::SecondShiftLifetime(int32 Rank, float InBaseLifetime, float CurrentRemaining)
{
    if (Rank <= 0) return CurrentRemaining;
    const float Extension = Rank >= 2 ? 14.0f : 8.0f;   // §FT3, transcribed
    // The 2x-base ceiling is the anti-farm rule; a remainder already above it
    // (impossible without this node) is left alone rather than clipped down.
    return FMath::Max(CurrentRemaining, FMath::Min(CurrentRemaining + Extension, InBaseLifetime * 2.0f));
}

float ABreakerDeployable::QuickSetArmDelay(int32 Rank, float BaseDelay)
{
    if (Rank >= 2) return 0.0f;              // §TK2 R2: removed
    if (Rank == 1) return BaseDelay * 0.5f;  // §TK2: halved
    return FMath::Max(0.0f, BaseDelay);
}

float ABreakerDeployable::QuickSetTriggerRadius(int32 Rank, float SecondsSinceArmed, float BaseRadiusCm)
{
    // §TK2 R2's explicit trade: no delay, but 1 m less trigger radius until a
    // second has passed. Rank 1 keeps the full radius (its delay is merely
    // halved, not removed).
    if (Rank >= 2 && SecondsSinceArmed < 1.0f)
    {
        return FMath::Max(0.0f, BaseRadiusCm - 100.0f);
    }
    return FMath::Max(0.0f, BaseRadiusCm);
}

float ABreakerDeployable::RearmInterval(int32 Rank)
{
    if (Rank >= 2) return 4.0f;   // §TK4 R2, transcribed
    if (Rank == 1) return 6.0f;   // §TK4, transcribed
    return 0.0f;
}

int32 ABreakerDeployable::OrdnanceMineCount(bool bHasOrdnance, int32 BaseCount)
{
    return bHasOrdnance ? 4 : FMath::Max(1, BaseCount);   // §TK7: "4 charges instead of 3"
}

float ABreakerDeployable::OrdnanceProcCoefficient(bool bHasOrdnance, double Now, double LastDetonationTime)
{
    // Without the node the mine's request keeps its default coefficient (1.0),
    // bit-identical to before this rule existed.
    if (!bHasOrdnance) return 1.0f;
    return (Now - LastDetonationTime) < 1.0 ? 0.0f : 1.0f;   // §TK7: the 1s merge window
}

bool ABreakerDeployable::PatienceQualifies(float ArmedUntriggeredSeconds)
{
    return ArmedUntriggeredSeconds >= 10.0f;   // §TK9, transcribed
}

void ABreakerDeployable::RegisterReplacementCredit(AActor* OwnerCharacter, EBreakerDeployableType InType, float Discount, double ExpiryWorldTime)
{
    if (!OwnerCharacter || Discount <= 0.0f) return;
    using namespace BreakerDeployableLocal;
    TArray<FBreakerReplacementCredit>& Credits = BreakerReplacementCredits.FindOrAdd(OwnerCharacter);
    // One live credit per type: a second destruction refreshes rather than
    // stacking discounts — the node compensates for being punished, and two
    // punishments do not make one placement doubly cheap.
    Credits.RemoveAll([InType](const FBreakerReplacementCredit& Credit) { return Credit.Type == InType; });
    FBreakerReplacementCredit Credit;
    Credit.Type = InType;
    Credit.Discount = Discount;
    Credit.ExpiryWorldTime = ExpiryWorldTime;
    Credits.Add(Credit);
}

float ABreakerDeployable::PendingReplacementDiscount(const AActor* OwnerCharacter, EBreakerDeployableType InType, double Now)
{
    if (!OwnerCharacter) return 0.0f;
    using namespace BreakerDeployableLocal;
    const TArray<FBreakerReplacementCredit>* Credits = BreakerReplacementCredits.Find(OwnerCharacter);
    if (!Credits) return 0.0f;
    for (const FBreakerReplacementCredit& Credit : *Credits)
    {
        if (Credit.Type == InType && Now <= Credit.ExpiryWorldTime) return Credit.Discount;
    }
    return 0.0f;
}

void ABreakerDeployable::ConsumeReplacementCredit(AActor* OwnerCharacter, EBreakerDeployableType InType)
{
    if (!OwnerCharacter) return;
    using namespace BreakerDeployableLocal;
    if (TArray<FBreakerReplacementCredit>* Credits = BreakerReplacementCredits.Find(OwnerCharacter))
    {
        Credits->RemoveAll([InType](const FBreakerReplacementCredit& Credit) { return Credit.Type == InType; });
        if (Credits->Num() == 0) BreakerReplacementCredits.Remove(OwnerCharacter);
    }
}

int32 ABreakerDeployable::CommandDetonateOwnedMines(AActor* OwnerCharacter)
{
    if (!OwnerCharacter) return 0;
    int32 Detonated = 0;
    // Copy the list: detonation destroys clusters, which mutates the registry.
    TArray<ABreakerDeployable*> OwnedClusters;
    for (const TWeakObjectPtr<ABreakerDeployable>& Entry : GetLiveDeployables())
    {
        ABreakerDeployable* Deployable = Entry.Get();
        if (Deployable && Deployable->GetOwningCharacter() == OwnerCharacter
            && Deployable->GetDeployableType() == EBreakerDeployableType::MineCluster
            && !Deployable->bDestroyed)
        {
            OwnedClusters.Add(Deployable);
        }
    }
    for (ABreakerDeployable* Cluster : OwnedClusters)
    {
        for (int32 Index = 0; Index < Cluster->Mines.Num(); ++Index)
        {
            if (Cluster->Mines[Index].bLive && Cluster->Mines[Index].ArmRemaining <= 0.0f)
            {
                Cluster->DetonateMine(Index);
                ++Detonated;
            }
        }
        // §TK11: "refunds nothing" — a cluster emptied by the command leaves
        // through the Command cause. A charge still arming survives; only its
        // ARMED siblings answered the command.
        bool bAnyLive = false;
        for (const FMineCharge& Mine : Cluster->Mines)
        {
            if (Mine.bLive) { bAnyLive = true; break; }
        }
        if (!bAnyLive)
        {
            Cluster->DestroyDeployableWithCause(EBreakerDeployableDestructionCause::Command);
        }
    }
    return Detonated;
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
    // FT8 Logistics: the Ammo Crate leaves the count. Read once, here, so the
    // count and the cull agree about what a crate is.
    const UBreakerProgressionComponent* Progression = OwnerCharacter ? OwnerCharacter->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    const bool bLogistics = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_FT_Logistics.GetTag());
    for (const TWeakObjectPtr<ABreakerDeployable>& Entry : GetLiveDeployables())
    {
        const ABreakerDeployable* Deployable = Entry.Get();
        if (!Deployable || Deployable->GetOwningCharacter() != OwnerCharacter) continue;
        // Anchor Point always outside the Scrap density economy (see the enum).
        if (Deployable->GetDeployableType() == EBreakerDeployableType::AnchorPoint) continue;
        // FT8 lifts the crate from the TOTAL count only: the per-type 2 still
        // holds for crates ("per-type stays 2" is the one invariant nothing in
        // the treatment touches), so Logistics frees the slots, not the spam.
        if (CountsAgainstDensityCap(Deployable->GetDeployableType(), bLogistics)) ++OutTotal;
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
    // FT9 Redundancy: the resting cap is 5 for its owner. A Field Assembly
    // override still wins while live (8 > 5); the node moves the floor.
    const UBreakerProgressionComponent* Progression = OwnerCharacter ? OwnerCharacter->FindComponentByClass<UBreakerProgressionComponent>() : nullptr;
    const int32 BaseCap = BaseTotalCapFor(Progression && Progression->HasNodeTag(BreakerNodeTags::Node_FT_Redundancy.GetTag()));
    if (const FBreakerDensityCapOverride* Override = BreakerDensityCapOverrides.Find(OwnerCharacter))
    {
        const UWorld* World = OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr;
        const double Now = World ? World->GetTimeSeconds() : 0.0;
        if (Override->ExpiryWorldTime < 0.0 || Now < Override->ExpiryWorldTime)
        {
            return FMath::Max(Override->TotalCap, BaseCap);
        }
        BreakerDensityCapOverrides.Remove(OwnerCharacter);
    }
    return BaseCap;
}

void ABreakerDeployable::EnforceDensityCapForPlacement(AActor* OwnerCharacter, EBreakerDeployableType TypeAboutToPlace)
{
    if (!OwnerCharacter || TypeAboutToPlace == EBreakerDeployableType::AnchorPoint) return;

    // Destroy-oldest until the new placement fits (§2.1). Oldest is PLACEMENT
    // ORDER, never remaining lifetime; the cull refunds through the one
    // destruction path, and it never fails and never prompts — the class must
    // not be blocked by its own furniture. The per-type cap of 2 is what stops
    // turret-stacking from being the only build; NOTHING raises it.
    const UBreakerProgressionComponent* Progression = OwnerCharacter->FindComponentByClass<UBreakerProgressionComponent>();
    const bool bLogistics = Progression && Progression->HasNodeTag(BreakerNodeTags::Node_FT_Logistics.GetTag());
    auto CullOldest = [OwnerCharacter, bLogistics](bool bSameTypeOnly, EBreakerDeployableType Type)
    {
        ABreakerDeployable* Oldest = nullptr;
        for (const TWeakObjectPtr<ABreakerDeployable>& Entry : GetLiveDeployables())
        {
            ABreakerDeployable* Deployable = Entry.Get();
            if (!Deployable || Deployable->GetOwningCharacter() != OwnerCharacter) continue;
            if (Deployable->GetDeployableType() == EBreakerDeployableType::AnchorPoint) continue;
            if (bSameTypeOnly && Deployable->GetDeployableType() != Type) continue;
            // The TOTAL cull may only destroy what the total COUNTS: under
            // Logistics (FT8) an over-cap cull must never spend itself on a
            // crate that was not crowding the field in the first place.
            if (!bSameTypeOnly && !CountsAgainstDensityCap(Deployable->GetDeployableType(), bLogistics)) continue;
            if (!Oldest || Deployable->PlacementSerial < Oldest->PlacementSerial) Oldest = Deployable;
        }
        if (Oldest) Oldest->DestroyDeployableWithCause(EBreakerDeployableDestructionCause::DensityCull);
    };

    int32 Total = 0;
    int32 OfType = 0;
    CountOwnedDeployables(OwnerCharacter, Total, TypeAboutToPlace, OfType);
    while (OfType >= PerTypeDensityCap)
    {
        CullOldest(true, TypeAboutToPlace);
        CountOwnedDeployables(OwnerCharacter, Total, TypeAboutToPlace, OfType);
    }
    // A placement that does not count against the total (a Logistics crate)
    // must not cull for the total either.
    if (!CountsAgainstDensityCap(TypeAboutToPlace, bLogistics)) return;
    while (Total >= TotalCapFor(OwnerCharacter))
    {
        CullOldest(false, TypeAboutToPlace);
        CountOwnedDeployables(OwnerCharacter, Total, TypeAboutToPlace, OfType);
    }
}
