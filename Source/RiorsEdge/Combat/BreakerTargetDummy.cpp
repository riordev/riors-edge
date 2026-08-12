#include "Combat/BreakerTargetDummy.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ABreakerTargetDummy::ABreakerTargetDummy()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
    SetRootComponent(BodyCollision);
    BodyCollision->InitCapsuleSize(42.0f, 90.0f);
    BodyCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);

    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(BodyCollision);
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetRelativeScale3D(FVector(0.75f, 0.75f, 1.5f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded()) BodyVisual->SetStaticMesh(CylinderMesh.Object);

    BodyHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitBox"));
    BodyHitBox->SetupAttachment(BodyCollision);
    BodyHitBox->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
    BodyHitBox->SetBoxExtent(FVector(38.0f, 38.0f, 55.0f));
    BodyHitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    BodyHitBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    WeakPoint = CreateDefaultSubobject<USphereComponent>(TEXT("WeakPoint"));
    WeakPoint->SetupAttachment(BodyCollision);
    WeakPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
    WeakPoint->SetSphereRadius(18.0f);
    WeakPoint->ComponentTags.Add(TEXT("WeakPoint"));
    WeakPoint->SetCollisionResponseToAllChannels(ECR_Ignore);
    WeakPoint->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);

    WeakPointVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeakPointVisual"));
    WeakPointVisual->SetupAttachment(WeakPoint);
    WeakPointVisual->SetRelativeScale3D(FVector(0.36f));
    WeakPointVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded()) WeakPointVisual->SetStaticMesh(SphereMesh.Object);

    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    Attributes = CreateDefaultSubobject<UBreakerAttributeSet>(TEXT("Attributes"));
    Combat = CreateDefaultSubobject<UBreakerCombatComponent>(TEXT("Combat"));
}

void ABreakerTargetDummy::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Combat->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);
}

UAbilitySystemComponent* ABreakerTargetDummy::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerTargetDummy::HandleDeath()
{
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetVisibility(false, true);
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::RespawnDummy, RespawnDelay, false);
}

void ABreakerTargetDummy::RespawnDummy()
{
    if (!GetWorld()) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    GetWorld()->SpawnActor<ABreakerTargetDummy>(GetClass(), GetActorTransform(), Params);
    Destroy();
}
