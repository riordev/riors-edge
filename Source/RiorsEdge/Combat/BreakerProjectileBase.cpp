#include "Combat/BreakerProjectileBase.h"

#include "Combat/BreakerCombatComponent.h"
#include "Combat/BreakerStatusComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace BreakerProjectileBaseLocal
{
    // Prefixed for the unity build: identical anonymous-namespace names in two
    // translation units have collided in this project twice.
    static const TCHAR* BreakerProjectileSphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
    static const TCHAR* BreakerProjectileShapeMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

ABreakerProjectileBase::ABreakerProjectileBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    // Deliberately generous relative to the visual: a projectile the player can
    // see must also hit roughly where it looks like it will.
    Collision->InitSphereRadius(30.0f);
    Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(Collision);

    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(false);
    if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, BreakerProjectileBaseLocal::BreakerProjectileSphereMesh))
    {
        Visual->SetStaticMesh(Sphere);
    }

    // The stock basic-shape material has no emissive parameter, so the read at
    // distance comes from the light rather than the surface.
    Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
    Glow->SetupAttachment(Collision);
    Glow->SetMobility(EComponentMobility::Movable);
    Glow->SetCastShadows(false);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    // Straight line by default, no arc: a flight path has to be predictable
    // enough that sidestepping it is a decision rather than a guess.
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
}

FVector ABreakerProjectileBase::PositionAfter(const FVector& Origin, const FVector& Direction, float Speed, float Seconds)
{
    const FVector Unit = Direction.GetSafeNormal();
    if (Unit.IsNearlyZero()) return Origin;
    return Origin + Unit * (FMath::Max(0.0f, Speed) * FMath::Max(0.0f, Seconds));
}

float ABreakerProjectileBase::FlightTimeOver(float DistanceCm, float Speed)
{
    if (Speed <= 0.0f) return TNumericLimits<float>::Max();
    return FMath::Max(0.0f, DistanceCm) / Speed;
}

float ABreakerProjectileBase::MaximumTravelDistance(float Speed, float LifetimeSeconds)
{
    return FMath::Max(0.0f, Speed) * FMath::Max(0.0f, LifetimeSeconds);
}

void ABreakerProjectileBase::InitializeProjectile(const FBreakerDamageRequest& InDamage, const FVector& Direction, float Speed)
{
    Damage = InDamage;
    const FVector Unit = Direction.GetSafeNormal();
    if (!Unit.IsNearlyZero()) SetActorRotation(Unit.Rotation());
    if (Movement)
    {
        Movement->InitialSpeed = Speed;
        Movement->MaxSpeed = Speed;
        Movement->ProjectileGravityScale = GravityScale;
        Movement->Velocity = (Unit.IsNearlyZero() ? GetActorForwardVector() : Unit) * Speed;
    }
}

void ABreakerProjectileBase::AddImpactStatus(const FBreakerCarriedStatus& InStatus)
{
    // A status with no tag or no duration is dropped here rather than at the
    // impact, so a mis-authored payload fails at arming time where it is
    // debuggable instead of silently doing nothing on hit.
    if (!InStatus.Spec.StatusTag.IsValid() || InStatus.Spec.Duration <= 0.0f) return;
    ImpactStatuses.Add(InStatus);
}

void ABreakerProjectileBase::SetImpactStatuses(const TArray<FBreakerCarriedStatus>& InStatuses)
{
    ImpactStatuses.Reset();
    for (const FBreakerCarriedStatus& Status : InStatuses) AddImpactStatus(Status);
}

void ABreakerProjectileBase::BeginPlay()
{
    Super::BeginPlay();

    if (Collision) Collision->SetSphereRadius(FMath::Max(1.0f, CollisionRadiusCm));
    if (Visual)
    {
        Visual->SetVisibility(bShowFallbackVisual);
        Visual->SetRelativeScale3D(FVector(VisualScale));
        if (bShowFallbackVisual)
        {
            if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, BreakerProjectileBaseLocal::BreakerProjectileShapeMaterial))
            {
                if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(BaseMaterial, Visual))
                {
                    Dynamic->SetVectorParameterValue(TEXT("Color"), OrbColor);
                    Visual->SetMaterial(0, Dynamic);
                }
            }
        }
    }
    if (Glow)
    {
        Glow->SetLightColor(OrbColor);
        Glow->SetIntensity(bShowFallbackVisual ? GlowIntensity : 0.0f);
        Glow->SetAttenuationRadius(GlowRadius);
    }

    ConfigureIgnoredActors();
    if (Collision) Collision->OnComponentHit.AddDynamic(this, &ThisClass::HandleImpact);
    SetLifeSpan(MaximumLifetime);
}

void ABreakerProjectileBase::SetOrbColor(const FLinearColor& NewColor)
{
    OrbColor = NewColor;
    if (Visual)
    {
        if (UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Visual->GetMaterial(0)))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), OrbColor);
        }
    }
    if (Glow) Glow->SetLightColor(OrbColor);
}

void ABreakerProjectileBase::ConfigureIgnoredActors()
{
    // A shot must never be stopped by the thing that fired it.
    if (Collision && GetInstigator()) Collision->IgnoreActorWhenMoving(GetInstigator(), true);
}

void ABreakerProjectileBase::HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    // Damage is server-authoritative. A client-side impact would double-apply
    // on a listen server and apply nothing at all on a remote client.
    if (!HasAuthority()) return;
    Impact(OtherActor, Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint));
}

void ABreakerProjectileBase::Impact(AActor* HitActor, const FVector& Location)
{
    // One latch, in one place, so no override can resolve an impact twice.
    if (bImpacted) return;
    bImpacted = true;

    ResolveImpact(HitActor, Location);
    MulticastImpactCosmetics(Location);
    Destroy();
}

bool ABreakerProjectileBase::ShouldDamageActor(AActor* Candidate) const
{
    if (!Candidate || Candidate == GetInstigator()) return false;
    return Candidate->FindComponentByClass<UBreakerCombatComponent>() != nullptr;
}

void ABreakerProjectileBase::ResolveImpact(AActor* HitActor, const FVector& Location)
{
    if (!ShouldDamageActor(HitActor)) return;

    if (UBreakerCombatComponent* TargetCombat = HitActor->FindComponentByClass<UBreakerCombatComponent>())
    {
        FBreakerDamageRequest Applied = Damage;
        // Source position feeds the target's frontal defensive checks.
        Applied.SourceLocation = Location;
        Applied.bHasSourceLocation = true;
        // The projectile resolved at a real point in the world; that point is
        // also where the hit LANDED, so the HUD number draws there rather than
        // at the target's pivot.
        Applied.ImpactLocation = Location;
        Applied.bHasImpactLocation = true;
        // Fall back to the spawn instigator so a projectile armed without one
        // still credits its shooter — kill credit, Mana generation and every
        // on-hit affix hang off this.
        if (!Applied.Instigator.IsValid()) Applied.SetInstigator(GetInstigator());
        if (Applied.BaseDamage > 0.0f) TargetCombat->ReceiveDamage(Applied);
    }

    if (!ImpactStatuses.IsEmpty())
    {
        if (UBreakerStatusComponent* Status = HitActor->FindComponentByClass<UBreakerStatusComponent>())
        {
            // The applier is the shooter, not the projectile: every tick this
            // status produces has to credit the player, or a DoT applied by a
            // projectile generates no Mana and counts toward nobody's kill.
            AActor* Applier = Damage.Instigator.IsValid() ? Damage.Instigator.Get() : static_cast<AActor*>(GetInstigator());
            for (const FBreakerCarriedStatus& Carried : ImpactStatuses)
            {
                Status->ApplyStatus(Carried.Spec, Carried.DamageFamily, Applier);
            }
        }
    }
}

void ABreakerProjectileBase::MulticastImpactCosmetics_Implementation(const FVector& Location)
{
    PlayImpactCosmetics(Location);
    OnImpact.Broadcast(nullptr, Location);
}

void ABreakerProjectileBase::PlayImpactCosmetics(const FVector& Location)
{
    // Nothing by default. The delegate above is the hook; a subclass overrides
    // this when it needs to change itself rather than notify someone else.
}
