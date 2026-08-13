#include "Weapons/BreakerRocketProjectile.h"

#include "Combat/BreakerCombatComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/BreakerUIStyle.h"

namespace
{
    // Same asset-free pattern the gym's dressing and the loot pickups use: the
    // stock basic-shape material exposes one "Color" vector parameter, so a
    // dynamic instance per primitive is the whole palette. A Blueprint art
    // pass replaces the mesh and the material and nothing else changes.
    void ApplyShapeColor(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (!BaseMaterial) return;
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Dynamic);
        }
    }

    UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Parent, const TCHAR* Name,
        const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
    {
        UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Parent);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetRelativeLocationAndRotation(Location, Rotation);
        Part->SetRelativeScale3D(Scale);
        if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
        {
            Part->SetStaticMesh(Mesh);
        }
        return Part;
    }

    const TCHAR* ShapeCube = TEXT("/Engine/BasicShapes/Cube.Cube");
    const TCHAR* ShapeCone = TEXT("/Engine/BasicShapes/Cone.Cone");
    const TCHAR* ShapeSphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
}

ABreakerRocketProjectile::ABreakerRocketProjectile()
{
    // Ticks only to roll the warhead and pulse the exhaust. One rocket in the
    // air at a time on a four-round magazine; this is not the SMG.
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(12.0f);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(Collision);

    // Casing. X is forward: the projectile movement component keeps the actor
    // rotation on the velocity, so every part is authored nose-down-X.
    Visual = MakePart(this, Collision, TEXT("Visual"), ShapeCube,
        FVector(-6.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.34f, 0.12f, 0.12f));
    ApplyShapeColor(Visual, BreakerUI::OrangeDeep);

    // Nose. The engine cone points +Z, so it is pitched onto +X.
    Nose = MakePart(this, Collision, TEXT("Nose"), ShapeCone,
        FVector(22.0f, 0.0f, 0.0f), FRotator(-90.0f, 0.0f, 0.0f), FVector(0.11f, 0.11f, 0.16f));
    ApplyShapeColor(Nose, BreakerUI::Orange);

    FinVertical = MakePart(this, Collision, TEXT("FinVertical"), ShapeCube,
        FVector(-19.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.10f, 0.02f, 0.20f));
    ApplyShapeColor(FinVertical, BreakerUI::OrangeDeep);

    FinHorizontal = MakePart(this, Collision, TEXT("FinHorizontal"), ShapeCube,
        FVector(-19.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.10f, 0.20f, 0.02f));
    ApplyShapeColor(FinHorizontal, BreakerUI::OrangeDeep);

    // Exhaust bloom trailing the casing. Bright orange so the round is legible
    // against the gym's bright sand from behind as well as from the side.
    Exhaust = MakePart(this, Collision, TEXT("Exhaust"), ShapeSphere,
        FVector(-26.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.22f, 0.14f, 0.14f));
    ApplyShapeColor(Exhaust, BreakerUI::Orange);

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Collision);
    Glow->SetRelativeLocation(FVector(-26.0f, 0.0f, 0.0f));
    Glow->SetLightColor(BreakerUI::Orange);
    Glow->SetIntensity(3200.0f);
    Glow->SetAttenuationRadius(420.0f);
    Glow->SetCastShadows(false);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
}

void ABreakerRocketProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bExploded || !Visual) return;
    // Roll about the travel axis. bRotationFollowsVelocity owns the actor's
    // pitch/yaw, so the spin lives on the mesh instead.
    const float Roll = SpinDegreesPerSecond * DeltaSeconds;
    Visual->AddLocalRotation(FRotator(0.0f, 0.0f, Roll));
    if (FinVertical) FinVertical->AddLocalRotation(FRotator(0.0f, 0.0f, Roll));
    if (FinHorizontal) FinHorizontal->AddLocalRotation(FRotator(0.0f, 0.0f, Roll));
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
        // The firing request normally already carries the shooter; fall back to
        // the spawn instigator so a rocket fired without one still credits.
        if (!AreaDamage.Instigator.IsValid()) AreaDamage.SetInstigator(GetInstigator());
        Combat->ReceiveDamage(AreaDamage);
    }

    MulticastExplosionCosmetics(Location, ExplosionRadius);
    // The rocket is its own explosion: it stops, goes inert, blooms to the
    // blast radius for a fraction of a second and then dies. No second actor,
    // no pool, and the client sees the same thing off the multicast below.
    SetLifeSpan(FMath::Max(ExplosionFlashSeconds, 0.01f));
}

void ABreakerRocketProjectile::MulticastExplosionCosmetics_Implementation(const FVector& Location, float Radius)
{
    PlayExplosionCosmetics(Location, Radius);
    OnExploded.Broadcast(Location, Radius);
}

void ABreakerRocketProjectile::PlayExplosionCosmetics(const FVector& Location, float Radius)
{
    bExploded = true;
    SetActorLocation(Location);
    if (Movement)
    {
        Movement->StopMovementImmediately();
        Movement->SetComponentTickEnabled(false);
    }
    if (Collision) Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Casing parts off; the exhaust bloom becomes the fireball, scaled to the
    // radius the damage actually used so the visual does not lie about reach.
    for (UStaticMeshComponent* Part : { Visual.Get(), Nose.Get(), FinVertical.Get(), FinHorizontal.Get() })
    {
        if (Part) Part->SetVisibility(false);
    }
    if (Exhaust)
    {
        Exhaust->SetRelativeLocation(FVector::ZeroVector);
        // BasicShapes/Sphere is 100 cm across, so a unit of scale is 50 cm of
        // radius. Two thirds of the blast: a fireball drawn at the full lethal
        // radius reads as far bigger than it is.
        const float Scale = FMath::Max(Radius, 50.0f) * 2.0f / 3.0f / 50.0f;
        Exhaust->SetRelativeScale3D(FVector(Scale));
        Exhaust->SetVisibility(true);
    }
    if (Glow)
    {
        Glow->SetRelativeLocation(FVector::ZeroVector);
        Glow->SetIntensity(42000.0f);
        Glow->SetAttenuationRadius(FMath::Max(Radius, 50.0f) * 3.0f);
    }
}
