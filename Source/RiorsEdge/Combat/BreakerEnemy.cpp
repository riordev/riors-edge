#include "Combat/BreakerEnemy.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Characters/BreakerCharacter.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ABreakerEnemy::ABreakerEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
    SetRootComponent(BodyCollision);
    BodyCollision->InitCapsuleSize(45.0f, 90.0f);
    BodyCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(BodyCollision);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.5f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded()) BodyVisual->SetStaticMesh(CylinderMesh.Object);

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
}

void ABreakerEnemy::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Combat->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);
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

    const float Distance = FMath::Sqrt(NearestDistanceSq);
    FVector DesiredDirection = FVector::ZeroVector;
    if (NearestPlayer && Distance <= DetectionRange)
    {
        DesiredDirection = (NearestPlayer->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        StateLabel = Distance <= AttackRange ? TEXT("ATTACK") : TEXT("CHASE");
        if (Distance <= AttackRange) PerformAttack(NearestPlayer);
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
        FHitResult MoveHit;
        AddActorWorldOffset(DesiredDirection * MoveSpeed * DeltaSeconds, true, &MoveHit);
        SetActorRotation(DesiredDirection.Rotation());
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
    TargetCombat->ReceiveDamage(Damage);
    LastAttackTime = GetWorld()->GetTimeSeconds();
}

void ABreakerEnemy::HandleDeath()
{
    bDead = true;
    StateLabel = TEXT("DEAD");
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetVisibility(false, true);
    GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RespawnEnemy);
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
        BodyVisual->SetVisibility(true, true);
        bDead = false;
        Combat->RestoreVitals();
    }, RespawnDelay, false);
}
