#include "Weapons/BreakerRocketProjectile.h"

#include "Combat/BreakerCombatComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABreakerRocketProjectile::ABreakerRocketProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(12.0f);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(Collision);

    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetRelativeScale3D(FVector(0.35f, 0.14f, 0.14f));
    if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        Visual->SetStaticMesh(Sphere);
    }

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
}

void ABreakerRocketProjectile::InitializeRocket(const FBreakerDamageRequest& InDamage, float Speed, float InExplosionRadius)
{
    Damage = InDamage;
    ExplosionRadius = InExplosionRadius;
    Movement->InitialSpeed = Speed;
    Movement->MaxSpeed = Speed;
    Movement->Velocity = GetActorForwardVector() * Speed;
}

void ABreakerRocketProjectile::BeginPlay()
{
    Super::BeginPlay();
    if (GetInstigator()) Collision->IgnoreActorWhenMoving(GetInstigator(), true);
    Collision->OnComponentHit.AddDynamic(this, &ThisClass::HandleImpact);
    SetLifeSpan(MaximumLifetime);
}

void ABreakerRocketProjectile::HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (HasAuthority()) Explode(Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint));
}

void ABreakerRocketProjectile::Explode(const FVector& Location)
{
    if (bExploded) return;
    bExploded = true;

    TArray<AActor*> Candidates;
    UGameplayStatics::GetAllActorsOfClass(this, AActor::StaticClass(), Candidates);
    for (AActor* Candidate : Candidates)
    {
        if (!Candidate || Candidate == GetInstigator()) continue;
        UBreakerCombatComponent* Combat = Candidate->FindComponentByClass<UBreakerCombatComponent>();
        if (!Combat) continue;
        const float Distance = FVector::Dist(Candidate->GetActorLocation(), Location);
        if (Distance > ExplosionRadius) continue;

        FBreakerDamageRequest AreaDamage = Damage;
        AreaDamage.BaseDamage *= FMath::Lerp(1.0f, EdgeDamageFraction, Distance / ExplosionRadius);
        AreaDamage.bWeakPointHit = false;
        AreaDamage.SourceLocation = Location;
        AreaDamage.bHasSourceLocation = true;
        AreaDamage.RandomSeed = HashCombine(Damage.RandomSeed, GetTypeHash(Candidate));
        Combat->ReceiveDamage(AreaDamage);
    }

    MulticastExplosionCosmetics(Location, ExplosionRadius);
    Destroy();
}

void ABreakerRocketProjectile::MulticastExplosionCosmetics_Implementation(const FVector& Location, float Radius)
{
    OnExploded.Broadcast(Location, Radius);
}
