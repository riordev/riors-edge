#include "Weapons/BreakerRocketProjectile.h"

#include "Combat/BreakerCombatComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/BreakerGlowMaterial.h"
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

    // The flame's brightness multiplier in flight, and at detonation. Both are
    // colour magnitudes on an additive surface, not alphas. O2 PLACEHOLDER.
    constexpr float ExhaustIntensity = 2.4f;
    constexpr float FireballIntensity = 6.0f;
}

ABreakerRocketProjectile::ABreakerRocketProjectile()
{
    // Ticks only to flicker the flame. One rocket in the air at a time on a
    // four-round magazine; this is not the SMG.
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(12.0f);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(Collision);

    // Casing. X is forward: the projectile movement component keeps the actor
    // rotation on the velocity, so every part is authored nose-down-X.
    // Dark, not orange — the flame behind it is the bright thing, and a body
    // the same colour as its own thrust has no silhouette.
    Visual = MakePart(this, Collision, TEXT("Visual"), ShapeCube,
        FVector(-6.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.34f, 0.12f, 0.12f));
    ApplyShapeColor(Visual, BreakerUI::Panel20);

    // Nose. The engine cone points +Z, so it is pitched onto +X.
    Nose = MakePart(this, Collision, TEXT("Nose"), ShapeCone,
        FVector(22.0f, 0.0f, 0.0f), FRotator(-90.0f, 0.0f, 0.0f), FVector(0.11f, 0.11f, 0.16f));
    ApplyShapeColor(Nose, BreakerUI::BorderEmphasis);

    // The flame. A stretched box rather than a ball, because thrust has a
    // direction and a sphere has none; unlit additive, so it is light rather
    // than a painted object, and so it survives the gym's bright sand.
    // Rebuilt in BeginPlay if the glow material is unavailable.
    Exhaust = MakePart(this, Collision, TEXT("Exhaust"), ShapeCube,
        FVector(-40.0f, 0.0f, 0.0f), FRotator::ZeroRotator, FVector(0.46f, 0.10f, 0.10f));

    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Collision);
    Glow->SetRelativeLocation(FVector(-34.0f, 0.0f, 0.0f));
    Glow->SetLightColor(BreakerUI::Orange);
    // Was 3200 over 420 cm. A rocket that lights the whole corridor as
    // brightly as a muzzle flash reads as a floating lamp; this is a warm
    // pool that moves with it.
    Glow->SetIntensity(2000.0f);
    Glow->SetAttenuationRadius(340.0f);
    Glow->SetCastShadows(false);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
}

void ABreakerRocketProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bExploded || !Exhaust) return;

    // The flame breathes; the rocket does not spin. Two detuned sine terms so
    // the cadence does not read as a loop, driven off world time so every
    // rocket in the air is out of phase with every other one.
    const float Time = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
    const float Phase = Time * ExhaustFlickerHz;
    const float Flicker = 1.0f + ExhaustFlickerAmount *
        (FMath::Sin(Phase) * 0.6f + FMath::Sin(Phase * 1.7f) * 0.4f);

    const float Length = FMath::Max(ExhaustLengthCm, 1.0f) * Flicker;
    // BasicShapes/Cube is 100 cm on a side, so a unit of scale is a metre.
    Exhaust->SetRelativeScale3D(FVector(Length / 100.0f, 0.10f, 0.10f));
    // The flame grows BACKWARD from the tail of the casing rather than around
    // its own centre, so the nozzle stays put while the plume lengthens.
    Exhaust->SetRelativeLocation(FVector(-17.0f - Length * 0.5f, 0.0f, 0.0f));
    BreakerUI::SetGlowColor(ExhaustMaterial, BreakerUI::Orange, ExhaustIntensity * Flicker);
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
    // Dynamic material instances cannot be made in a constructor. If the
    // engine's emissive material is missing the flame falls back to the same
    // lit shape material every other placeholder uses — dimmer, but present.
    ExhaustMaterial = BreakerUI::MakeGlowMaterial(Exhaust);
    if (ExhaustMaterial) BreakerUI::SetGlowColor(ExhaustMaterial, BreakerUI::Orange, ExhaustIntensity);
    else ApplyShapeColor(Exhaust, BreakerUI::Orange);

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

    // Casing parts off; the flame becomes the fireball, scaled to the radius
    // the damage actually used so the visual does not lie about reach.
    for (UStaticMeshComponent* Part : { Visual.Get(), Nose.Get() })
    {
        if (Part) Part->SetVisibility(false);
    }
    if (Exhaust)
    {
        Exhaust->SetRelativeLocation(FVector::ZeroVector);
        // BasicShapes/Cube is 100 cm on a side. Two thirds of the blast: a
        // fireball drawn at the full lethal radius reads as far bigger than it
        // is. Uniform now — a directional plume becomes a ball on detonation.
        const float Scale = FMath::Max(Radius, 50.0f) * 2.0f / 3.0f * 2.0f / 100.0f;
        Exhaust->SetRelativeScale3D(FVector(Scale));
        Exhaust->SetVisibility(true);
        BreakerUI::SetGlowColor(ExhaustMaterial, BreakerUI::Orange, FireballIntensity);
    }
    if (Glow)
    {
        Glow->SetRelativeLocation(FVector::ZeroVector);
        Glow->SetIntensity(42000.0f);
        Glow->SetAttenuationRadius(FMath::Max(Radius, 50.0f) * 3.0f);
    }
}
