#include "Combat/BreakerEnemy.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Game/BreakerGameMode.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"
#include "Items/BreakerLootPickup.h"
#include "Playtest/BreakerPlaytestComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // Muted Vestige-ish grey-violet for the humanoid body parts. Local copy of
    // the dressing helper so the enemy never pulls in game mode internals.
    void ApplyEnemyBodyColor(UStaticMeshComponent* Mesh)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.32f, 0.42f));
            Mesh->SetMaterial(0, Dynamic);
        }
    }
}

ABreakerEnemy::ABreakerEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
    SetRootComponent(BodyCollision);
    BodyCollision->InitCapsuleSize(45.0f, 90.0f);
    BodyCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    // Humanoid silhouette from basic shapes: torso, head, two arms, two legs.
    // Purely cosmetic — every piece is NoCollision and the capsule, hit box
    // and weak point keep doing all the collision work. Elites inherit the
    // actor scale multiplier, so an elite simply reads as a bigger humanoid.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BodySphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    // BodyVisual is the torso now (was the single cylinder body).
    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(BodyCollision);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 22.0f));
    BodyVisual->SetRelativeScale3D(FVector(0.55f, 0.36f, 0.78f));
    if (CubeMesh.Succeeded()) BodyVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(BodyVisual);

    HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
    HeadVisual->SetupAttachment(BodyCollision);
    HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
    HeadVisual->SetRelativeScale3D(FVector(0.34f));
    if (BodySphereMesh.Succeeded()) HeadVisual->SetStaticMesh(BodySphereMesh.Object);
    ApplyEnemyBodyColor(HeadVisual);

    LeftArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArmVisual"));
    LeftArmVisual->SetupAttachment(BodyCollision);
    LeftArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftArmVisual->SetRelativeLocation(FVector(0.0f, -34.0f, 24.0f));
    LeftArmVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, 12.0f));
    LeftArmVisual->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.62f));
    if (CubeMesh.Succeeded()) LeftArmVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(LeftArmVisual);

    RightArmVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArmVisual"));
    RightArmVisual->SetupAttachment(BodyCollision);
    RightArmVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightArmVisual->SetRelativeLocation(FVector(0.0f, 34.0f, 24.0f));
    RightArmVisual->SetRelativeRotation(FRotator(0.0f, 0.0f, -12.0f));
    RightArmVisual->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.62f));
    if (CubeMesh.Succeeded()) RightArmVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(RightArmVisual);

    LeftLegVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLegVisual"));
    LeftLegVisual->SetupAttachment(BodyCollision);
    LeftLegVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftLegVisual->SetRelativeLocation(FVector(0.0f, -14.0f, -50.0f));
    LeftLegVisual->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.80f));
    if (CubeMesh.Succeeded()) LeftLegVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(LeftLegVisual);

    RightLegVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLegVisual"));
    RightLegVisual->SetupAttachment(BodyCollision);
    RightLegVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RightLegVisual->SetRelativeLocation(FVector(0.0f, 14.0f, -50.0f));
    RightLegVisual->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.80f));
    if (CubeMesh.Succeeded()) RightLegVisual->SetStaticMesh(CubeMesh.Object);
    ApplyEnemyBodyColor(RightLegVisual);

    BodyHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitBox"));
    BodyHitBox->SetupAttachment(BodyCollision);
    BodyHitBox->SetBoxExtent(FVector(42.0f, 42.0f, 58.0f));
    BodyHitBox->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
    BodyHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    BodyHitBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    WeakPoint = CreateDefaultSubobject<USphereComponent>(TEXT("WeakPoint"));
    WeakPoint->SetupAttachment(BodyCollision);
    WeakPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
    WeakPoint->SetSphereRadius(20.0f);
    WeakPoint->ComponentTags.Add(TEXT("WeakPoint"));
    WeakPoint->SetCollisionResponseToAllChannels(ECR_Ignore);
    WeakPoint->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    WeakPointVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeakPointVisual"));
    WeakPointVisual->SetupAttachment(WeakPoint);
    WeakPointVisual->SetRelativeScale3D(FVector(0.4f));
    WeakPointVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded()) WeakPointVisual->SetStaticMesh(SphereMesh.Object);

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    Attributes = CreateDefaultSubobject<UBreakerAttributeSet>(TEXT("Attributes"));
    Combat = CreateDefaultSubobject<UBreakerCombatComponent>(TEXT("Combat"));
    Status = CreateDefaultSubobject<UBreakerStatusComponent>(TEXT("Status"));
}

void ABreakerEnemy::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Combat->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);
    Combat->OnDamageReceived.AddDynamic(this, &ThisClass::HandleDamageReceived);
    if (LeashOrigin.IsNearlyZero()) LeashOrigin = GetActorLocation();
    Attributes->SetMaxHealth(220.0f);
    Combat->RestoreVitals();
}

UAbilitySystemComponent* ABreakerEnemy::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerEnemy::ConfigureEncounter(const FVector& NewLeashOrigin, float NewPatrolPhase)
{
    LeashOrigin = NewLeashOrigin;
    PatrolPhase = NewPatrolPhase;
}

void ABreakerEnemy::ConfigureElite()
{
    // Canonical elite (Veteran+) chassis per Encounter-Design §1.1:
    // 1.25x scale, 2.0x health, 1.5x damage.
    bIsElite = true;
    SetActorScale3D(GetActorScale3D() * 1.25f);
    AttackDamage *= 1.5f;
    MoveSpeed *= 0.85f;
    EnemyLevel = FMath::Min(EnemyLevel + 5, 50);
    if (Attributes)
    {
        Attributes->SetMaxHealth(440.0f);
        if (Combat) Combat->RestoreVitals();
    }
    StateLabel = TEXT("ELITE PATROL");
}

FString ABreakerEnemy::GetEnemyStateLabel() const { return StateLabel; }

void ABreakerEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || bDead || !GetWorld()) return;

    ABreakerCharacter* NearestPlayer = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<ABreakerCharacter> It(GetWorld()); It; ++It)
    {
        const float DistanceSq = FVector::DistSquared2D(GetActorLocation(), It->GetActorLocation());
        if (DistanceSq < NearestDistanceSq)
        {
            NearestDistanceSq = DistanceSq;
            NearestPlayer = *It;
        }
    }

    // Players standing in the safe zone are off-limits, and enemies stop at
    // its edge rather than following them in.
    const ABreakerGameMode* GameMode = GetWorld()->GetAuthGameMode<ABreakerGameMode>();
    if (NearestPlayer && GameMode && GameMode->IsInSafeZone(NearestPlayer->GetActorLocation()))
    {
        NearestPlayer = nullptr;
    }

    const float Distance = FMath::Sqrt(NearestDistanceSq);
    const double Now = GetWorld()->GetTimeSeconds();
    FVector DesiredDirection = FVector::ZeroVector;
    // Speed multiplier for this frame. 1.0 = the old constant walk.
    float SpeedScale = 1.0f;
    if (NearestPlayer && Distance <= DetectionRange)
    {
        const FVector ToPlayer = (NearestPlayer->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        DesiredDirection = ToPlayer;
        StateLabel = Distance <= AttackRange ? TEXT("ATTACK") : TEXT("CHASE");
        if (Distance <= AttackRange) PerformAttack(NearestPlayer);

        // (a) Closing sprint: far away, they commit to closing the gap
        // instead of ambling. Inside SprintRange they drop to normal so the
        // player still gets readable spacing at knife range.
        if (Distance > SprintRange)
        {
            SpeedScale = SprintSpeedMultiplier;
            StateLabel = TEXT("CLOSING");
        }

        // (b) Strafe weave: a lateral sinusoid folded into the chase vector.
        // Elites are exempt — the identity is that an elite advances
        // implacably and does not juke (Encounter-Design §1.1 chassis).
        if (!bIsElite && Distance > AttackRange)
        {
            WeaveTime += DeltaSeconds;
            const FVector Lateral = FVector::CrossProduct(FVector::UpVector, ToPlayer).GetSafeNormal2D();
            const float Weave = FMath::Sin((WeaveTime + PatrolPhase) * WeaveFrequency) * WeaveStrength;
            DesiredDirection = (ToPlayer + Lateral * Weave).GetSafeNormal2D();
        }

        // (c) Committed lunge: once inside LungeRange, a short burst straight
        // at the player on a cooldown. Telegraphed via StateLabel so the
        // playtest HUD shows the tell.
        const bool bLungeActive = (Now - LungeStartTime) < LungeDuration;
        if (bLungeActive)
        {
            SpeedScale = LungeSpeedMultiplier;
            DesiredDirection = ToPlayer;   // no weave mid-commit
            StateLabel = TEXT("LUNGE");
        }
        else if (Distance <= LungeRange && Distance > AttackRange
            && (Now - LungeStartTime) >= (LungeDuration + LungeCooldown))
        {
            LungeStartTime = Now;
            SpeedScale = LungeSpeedMultiplier;
            DesiredDirection = ToPlayer;
            StateLabel = TEXT("LUNGE");
        }
    }
    else
    {
        PatrolPhase += DeltaSeconds * 0.7f;
        const FVector PatrolTarget = LeashOrigin + FVector(0.0f, FMath::Sin(PatrolPhase) * 350.0f, 0.0f);
        DesiredDirection = (PatrolTarget - GetActorLocation()).GetSafeNormal2D();
        StateLabel = TEXT("PATROL");
    }

    if (!DesiredDirection.IsNearlyZero())
    {
        const FVector Step = DesiredDirection * MoveSpeed * SpeedScale * DeltaSeconds;
        const FVector NextLocation = GetActorLocation() + Step;
        if (GameMode && GameMode->IsInSafeZone(NextLocation))
        {
            StateLabel = TEXT("HELD");
            return;
        }
        FHitResult MoveHit;
        AddActorWorldOffset(Step, true, &MoveHit);
        SetActorRotation(DesiredDirection.Rotation());
    }

    // Ground snap: enemies move by offset with no gravity, so without this
    // they hover over the apron slabs or float where spawn height was off.
    // Trace down, plant the capsule base on whatever is below.
    {
        const float HalfHeight = BodyCollision ? BodyCollision->GetScaledCapsuleHalfHeight() : 88.0f;
        const FVector TraceStart = GetActorLocation() + FVector(0, 0, 60.0f);
        const FVector TraceEnd = TraceStart - FVector(0, 0, 4000.0f);
        FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(BreakerEnemyGround), false, this);
        FHitResult Ground;
        if (GetWorld()->LineTraceSingleByChannel(Ground, TraceStart, TraceEnd, ECC_WorldStatic, GroundParams))
        {
            const float TargetZ = Ground.ImpactPoint.Z + HalfHeight;
            const float CurrentZ = GetActorLocation().Z;
            // Snap down instantly, step up smoothly, so slabs read as steps
            // rather than teleports.
            const float NewZ = CurrentZ > TargetZ
                ? FMath::Max(TargetZ, CurrentZ - 1200.0f * DeltaSeconds)
                : FMath::Min(TargetZ, CurrentZ + 600.0f * DeltaSeconds);
            SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, NewZ), false);
        }
    }
}

void ABreakerEnemy::PerformAttack(APawn* TargetPawn)
{
    if (!TargetPawn || !GetWorld() || GetWorld()->GetTimeSeconds() - LastAttackTime < AttackCooldown) return;
    UBreakerCombatComponent* TargetCombat = TargetPawn->FindComponentByClass<UBreakerCombatComponent>();
    if (!TargetCombat) return;
    FBreakerDamageRequest Damage;
    Damage.BaseDamage = AttackDamage;
    Damage.DamageFamily = EBreakerDamageFamily::Physical;
    Damage.bCanCritical = false;
    Damage.SetInstigator(this);
    TargetCombat->ReceiveDamage(Damage);
    LastAttackTime = GetWorld()->GetTimeSeconds();
}

void ABreakerEnemy::SetBodyVisible(bool bVisible)
{
    for (UStaticMeshComponent* Part : { BodyVisual.Get(), HeadVisual.Get(), LeftArmVisual.Get(),
        RightArmVisual.Get(), LeftLegVisual.Get(), RightLegVisual.Get(), WeakPointVisual.Get() })
    {
        if (Part) Part->SetVisibility(bVisible, true);
    }
}

void ABreakerEnemy::HandleDeath()
{
    bDead = true;
    StateLabel = TEXT("DEAD");
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetBodyVisible(false);
    if (HasAuthority() && bDropsLoot) GrantLoot();
    if (HasAuthority()) GrantAmmo();

    // On-death chain detonation: hurts other enemies only, so packed
    // spawns cascade without turning the player's own kills against them.
    if (HasAuthority() && bExplodesOnDeath && Attributes)
    {
        const float ExplosionDamage = Attributes->GetMaxHealth() * DeathExplosionHealthFraction;
        for (TActorIterator<ABreakerEnemy> It(GetWorld()); It; ++It)
        {
            if (*It == this || It->bDead) continue;
            if (FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) > FMath::Square(DeathExplosionRadius)) continue;
            FBreakerDamageRequest ChainDamage;
            ChainDamage.BaseDamage = ExplosionDamage;
            ChainDamage.DamageFamily = EBreakerDamageFamily::Physical;
            ChainDamage.bCanCritical = false;
            ChainDamage.SourceLocation = GetActorLocation();
            ChainDamage.bHasSourceLocation = true;
            ChainDamage.SetInstigator(this);
            It->Combat->ReceiveDamage(ChainDamage);
        }
    }

    // Feed the time-to-kill instrument (Decisions.md O2).
    if (FirstDamageTime >= 0.0 && GetWorld())
    {
        APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
        if (UBreakerPlaytestComponent* Playtest = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerPlaytestComponent>() : nullptr)
        {
            // Engagement-gapped TTK: idle stretches between damage events are
            // capped, so target-switching doesn't inflate the sample the way
            // wall-clock first-damage-to-death did (session 3 finding).
            Playtest->AddTimeToKillSample(FMath::Max(EngagedSeconds, 0.05f), bIsElite);
        }
        FirstDamageTime = -1.0;
        LastDamageEventTime = -1.0;
        EngagedSeconds = 0.0f;
    }

    if (bRespawns) GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RespawnEnemy);
    else SetLifeSpan(2.0f);
}

void ABreakerEnemy::HandleDamageReceived(const FBreakerDamageResult& Result)
{
    if (!GetWorld() || (Result.HealthDamage <= 0.0f && Result.ShieldDamage <= 0.0f)) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (FirstDamageTime < 0.0) FirstDamageTime = Now;
    if (LastDamageEventTime >= 0.0)
    {
        // Gaps longer than 1.5s are disengagement, not fighting.
        EngagedSeconds += static_cast<float>(FMath::Min(Now - LastDamageEventTime, 1.5));
    }
    LastDamageEventTime = Now;
}

void ABreakerEnemy::GrantLoot()
{
    APawn* PlayerPawn = GetWorld() ? GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr : nullptr;
    UBreakerEquipmentComponent* Equipment = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerEquipmentComponent>() : nullptr;
    if (!Equipment) return;

    ++KillCount;
    const int32 Seed = HashCombine(GetTypeHash(GetActorLocation()), KillCount);
    EBreakerItemRarity Rarity = UBreakerLootLibrary::RollRarity(Seed, Equipment->GetStats().DropChancePercent);
    if (bIsElite && Rarity < EBreakerItemRarity::Exceptional) Rarity = EBreakerItemRarity::Exceptional;
    const EBreakerEquipSlot Slot = static_cast<EBreakerEquipSlot>(FRandomStream(Seed).RandRange(0, static_cast<int32>(EBreakerEquipSlot::Count) - 1));
    const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("GymDrop"), Slot, Rarity, EnemyLevel, Seed);

    // Drops land on the ground now instead of teleporting into the backpack:
    // the player walks over, reads the popup, and presses F. Scatter keeps a
    // stack of kills on one spot from overlapping into a single column.
    FRandomStream ScatterStream(Seed ^ 0x5EED);
    const FVector Scatter(ScatterStream.FRandRange(-80.0f, 80.0f), ScatterStream.FRandRange(-80.0f, 80.0f), 0.0f);
    const FVector DropLocation = GetActorLocation() + Scatter + FVector(0.0f, 0.0f, 40.0f);
    if (ABreakerLootPickup* Pickup = GetWorld()->SpawnActor<ABreakerLootPickup>(ABreakerLootPickup::StaticClass(), DropLocation, FRotator::ZeroRotator))
    {
        Pickup->SetItem(Item);
    }
}

void ABreakerEnemy::GrantAmmo()
{
    // Owner feedback: "ran out of ammo after 3 waves — no way to regain
    // ammo". Kills now feed the gun. O2 placeholders: a normal kill returns
    // 15% of a magazine-weapon's starting reserve, an elite half of it —
    // roughly, sustained accurate play is ammo-neutral and sloppy play still
    // runs dry. Uses the first player pawn, same as GrantLoot.
    const float NormalKillFraction = 0.15f;
    const float EliteKillFraction = 0.50f;

    APawn* PlayerPawn = GetWorld() && GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    if (UBreakerWeaponComponent* Weapon = PlayerPawn ? PlayerPawn->FindComponentByClass<UBreakerWeaponComponent>() : nullptr)
    {
        Weapon->AddReserveAmmoFraction(bIsElite ? EliteKillFraction : NormalKillFraction);
    }
}

void ABreakerEnemy::RespawnEnemy()
{
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, [this]()
    {
        SetActorLocation(LeashOrigin);
        BodyCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BodyHitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WeakPoint->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SetBodyVisible(true);
        bDead = false;
        FirstDamageTime = -1.0;
        LastDamageEventTime = -1.0;
        EngagedSeconds = 0.0f;
        Combat->RestoreVitals();
    }, RespawnDelay, false);
}
