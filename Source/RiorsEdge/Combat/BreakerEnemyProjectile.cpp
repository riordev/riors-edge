#include "Combat/BreakerEnemyProjectile.h"

#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerEnemy.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

ABreakerEnemyProjectile::ABreakerEnemyProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    // Deliberately generous relative to the visual: a projectile the player
    // can see must also hit roughly where it looks like it will.
    Collision->InitSphereRadius(30.0f);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(Collision);

    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // /Engine/BasicShapes/Sphere is 100uu across, so 0.85 gives a ~42cm orb —
    // roughly a beach ball. Big on purpose. O2 PLACEHOLDER.
    Visual->SetRelativeScale3D(FVector(0.85f));
    if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
    {
        Visual->SetStaticMesh(Sphere);
    }
    Visual->SetCastShadow(false);

    // The stock basic-shape material has no emissive parameter, so the read at
    // distance comes from the light rather than the surface.
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Collision);
    Glow->SetMobility(EComponentMobility::Movable);
    Glow->SetCastShadows(false);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    // Straight line, no arc: the flight path has to be predictable enough that
    // sidestepping it is a decision rather than a guess.
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
}

void ABreakerEnemyProjectile::InitializeProjectile(const FBreakerDamageRequest& InDamage, const FVector& Direction, float Speed)
{
    Damage = InDamage;
    const FVector Unit = Direction.GetSafeNormal();
    if (!Unit.IsNearlyZero()) SetActorRotation(Unit.Rotation());
    Movement->InitialSpeed = Speed;
    Movement->MaxSpeed = Speed;
    Movement->Velocity = (Unit.IsNearlyZero() ? GetActorForwardVector() : Unit) * Speed;
}

void ABreakerEnemyProjectile::BeginPlay()
{
    Super::BeginPlay();

    if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Visual))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), OrbColor);
            Visual->SetMaterial(0, Dynamic);
        }
    }
    Glow->SetLightColor(OrbColor);
    Glow->SetIntensity(GlowIntensity);
    Glow->SetAttenuationRadius(GlowRadius);

    if (AActor* Shooter = GetInstigator()) Collision->IgnoreActorWhenMoving(Shooter, true);
    // Enemies never block each other's shots. A pack whose front rank eats the
    // back rank's volley reads as a bug, and enemy friendly fire is not a
    // system this project has ruled on.
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ABreakerEnemy> It(World); It; ++It)
        {
            Collision->IgnoreActorWhenMoving(*It, true);
        }
    }

    Collision->OnComponentHit.AddDynamic(this, &ThisClass::HandleImpact);
    SetLifeSpan(MaximumLifetime);
}

void ABreakerEnemyProjectile::HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HasAuthority()) return;
    Impact(OtherActor, Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint));
}

void ABreakerEnemyProjectile::Impact(AActor* HitActor, const FVector& Location)
{
    if (bImpacted) return;
    bImpacted = true;

    // Single target, no splash. Splash would punish a player who dodged
    // correctly, which is the opposite of the point.
    if (HitActor && HitActor != GetInstigator() && !HitActor->IsA<ABreakerEnemy>())
    {
        if (UBreakerCombatComponent* TargetCombat = HitActor->FindComponentByClass<UBreakerCombatComponent>())
        {
            FBreakerDamageRequest Applied = Damage;
            // Source position feeds the player's frontal defensive checks.
            Applied.SourceLocation = Location;
            Applied.bHasSourceLocation = true;
            if (!Applied.Instigator.IsValid()) Applied.SetInstigator(GetInstigator());
            TargetCombat->ReceiveDamage(Applied);
        }
    }

    MulticastImpactCosmetics(Location);
    Destroy();
}

void ABreakerEnemyProjectile::MulticastImpactCosmetics_Implementation(const FVector& Location)
{
    OnImpact.Broadcast(Location);
}
