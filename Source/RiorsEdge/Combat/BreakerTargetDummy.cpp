#include "Combat/BreakerTargetDummy.h"

#include "AbilitySystemComponent.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerHitReactionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ABreakerTargetDummy::ABreakerTargetDummy()
{
    PrimaryActorTick.bCanEverTick = true;
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
    HitReaction = CreateDefaultSubobject<UBreakerHitReactionComponent>(TEXT("HitReaction"));
}

void ABreakerTargetDummy::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    Combat->OnDeath.AddDynamic(this, &ThisClass::HandleDeath);
    Combat->OnDamageReceived.AddDynamic(this, &ThisClass::HandleDamageReceived);
    // The flash paints dynamic material instances, and a bare SetStaticMesh
    // carries none — without this the dummy's reaction layer would be wired
    // and silently paint nothing, the exact no-op-instrument shape this
    // project keeps finding. Same stock-material pattern as the zone
    // footprint; near-white keeps the dummy's current look.
    if (BodyVisual)
    {
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Base, BodyVisual))
            {
                Dynamic->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.82f, 0.84f, 0.88f));   // O2 PLACEHOLDER
                BodyVisual->SetMaterial(0, Dynamic);
            }
        }
    }
    if (HitReaction)
    {
        HitReaction->RegisterPart(BodyVisual);
        HitReaction->OnDeathPresentationFinished.AddUObject(this, &ABreakerTargetDummy::HandleDeathBeatFinished);
    }
    MotionOrigin = GetActorLocation();
    ConfigureProfile(Profile);
}

void ABreakerTargetDummy::HandleDamageReceived(const FBreakerDamageResult& Result)
{
    if (Result.HealthDamage <= 0.0f && Result.ShieldDamage <= 0.0f) return;
    bLastHitWasWeakPoint = Result.bWeakPoint;
    if (HitReaction) HitReaction->NotifyHit(Result.bWeakPoint);
}

UAbilitySystemComponent* ABreakerTargetDummy::GetAbilitySystemComponent() const { return AbilitySystem; }

void ABreakerTargetDummy::ConfigureProfile(EBreakerTargetProfile NewProfile)
{
    Profile = NewProfile;
    if (!Attributes) return;
    Attributes->SetMaxHealth(Profile == EBreakerTargetProfile::Armored ? 180.0f : 120.0f);
    Attributes->SetHealth(Attributes->GetMaxHealth());
    Attributes->SetMaxShield(Profile == EBreakerTargetProfile::Shielded ? 100.0f : 0.0f);
    Attributes->SetShield(Attributes->GetMaxShield());
    Attributes->SetArmor(Profile == EBreakerTargetProfile::Armored ? 100.0f : 0.0f);
    PrimaryActorTick.SetTickFunctionEnable(Profile == EBreakerTargetProfile::Moving);
}

FString ABreakerTargetDummy::GetProfileLabel() const
{
    switch (Profile)
    {
        case EBreakerTargetProfile::Shielded: return TEXT("SHIELD");
        case EBreakerTargetProfile::Armored: return TEXT("ARMOR");
        case EBreakerTargetProfile::Moving: return TEXT("MOVING");
        default: return TEXT("HEALTH");
    }
}

void ABreakerTargetDummy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Profile != EBreakerTargetProfile::Moving || !HasAuthority()) return;
    MotionPhase += DeltaSeconds * 1.4f;
    SetActorLocation(MotionOrigin + FVector(0.0f, FMath::Sin(MotionPhase) * 260.0f, 0.0f), false);
}

void ABreakerTargetDummy::HandleDeath()
{
    // Collision dies immediately — a corpse mid-crumple is not a target —
    // but the body stays visible for the death beat, then hides when the
    // crumple lands (HandleDeathBeatFinished). The old path vanished the
    // dummy the same frame it died, which read as deletion, not a kill.
    BodyCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WeakPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (HitReaction)
    {
        HitReaction->StartDeathPresentation(bLastHitWasWeakPoint);
    }
    else
    {
        HandleDeathBeatFinished();
    }
}

void ABreakerTargetDummy::HandleDeathBeatFinished()
{
    BodyVisual->SetVisibility(false, true);
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::RespawnDummy, RespawnDelay, false);
}

void ABreakerTargetDummy::RespawnDummy()
{
    if (!GetWorld()) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABreakerTargetDummy* Replacement = GetWorld()->SpawnActor<ABreakerTargetDummy>(GetClass(), GetActorTransform(), Params);
    if (Replacement) Replacement->ConfigureProfile(Profile);
    Destroy();
}
