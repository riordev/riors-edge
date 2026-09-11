#include "Combat/BreakerProjectileBase.h"
#include "UI/BreakerEffectMath.h"
#include "UI/BreakerEffectMomentMath.h"
#include "UI/BreakerEffectRenderer.h"
#include "Combat/BreakerStatusCycleComponent.h"

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
    Collision->SetCollisionProfileName(TEXT("Projectile"));
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
    // InitializeProjectile always arms a WORLD direction. The engine default
    // here is local space, and that is harmless for a plain SpawnActor (the
    // component initialises with zero velocity and the arm lands afterwards)
    // but a DEFERRED spawn arms first and initialises second: InitializeComponent
    // saw a non-zero velocity and re-rotated it by the round's own yaw, which
    // FireRound had already set to the aim. A round at yaw θ flew at 2θ, so
    // only a player on world +X was ever hit — the owner saw it as skirmishers
    // shooting at random.
    Movement->bInitialVelocityInLocalSpace = false;
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

void ABreakerProjectileBase::SetCycleAdvanceOnHit(UBreakerStatusCycleComponent* Cycle, int32 Positions)
{
    if (!HasAuthority()) return;
    ImpactCycle = Cycle;
    CyclePositionsOnHit = FMath::Max(0, Positions);
}

void ABreakerProjectileBase::Impact(AActor* HitActor, const FVector& Location)
{
    // One latch, in one place, so no override can resolve an impact twice.
    if (!HasAuthority() || bImpacted) return;
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

    bool bLandedHit = false;
    bool bImpactAvoided = false;
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
        if (Applied.BaseDamage > 0.0f)
        {
            const FBreakerDamageResult Result = TargetCombat->ReceiveDamage(Applied);
            bImpactAvoided = Result.bDodged || Result.bParried;
            bLandedHit = !Result.bDodged && (Result.HealthDamage > 0.0f || Result.ShieldDamage > 0.0f);
        }
    }

    // Carried ailments belong to this impact; a dodge/parry avoids its riders too.
    // No damage request (status-only projectile) retains the ordinary application path.
    if (!bImpactAvoided && !ImpactStatuses.IsEmpty())
    {
        if (UBreakerStatusComponent* Status = HitActor->FindComponentByClass<UBreakerStatusComponent>())
        {
            // The applier is the shooter, not the projectile: every tick this
            // status produces has to credit the player, or a DoT applied by a
            // projectile generates no Mana and counts toward nobody's kill.
            AActor* Applier = Damage.Instigator.IsValid() ? Damage.Instigator.Get() : static_cast<AActor*>(GetInstigator());
            FBreakerDamageRequest ApplyingHit = Damage;
            ApplyingHit.SetInstigator(Applier);
            for (const FBreakerCarriedStatus& Carried : ImpactStatuses)
            {
                Status->ApplyStatusFromHit(Carried.Spec, Carried.DamageFamily, ApplyingHit);
            }
        }
    }
    if (bLandedHit)
    {
        if (UBreakerStatusCycleComponent* Cycle = ImpactCycle.Get())
        {
            for (int32 Index = 0; Index < CyclePositionsOnHit; ++Index) Cycle->AdvanceCycle();
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
    // A ROUND THAT LANDS MAKES A MARK. Owner: "adding some minor visual effects
    // for each of them is super important". This body was empty, so Fracture's
    // orb — the ability's whole visible existence — simply stopped existing on
    // contact: no flash, no debris, nothing to tell a player the shot arrived
    // rather than expired.
    //
    // IN THE ORB'S OWN COLOUR, which already carries the status it is delivering
    // (SetOrbColor tints it per carried tag), so a Bleed round and a Rot round
    // do not land identically.
    UWorld* World = GetWorld();
    if (!World) return;
    ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(World);
    if (!Effects) return;

    const FVector Forward = GetVelocity().GetSafeNormal();
    Effects->PlayMoment(EBreakerEffectMoment::Impact, Location,
        Forward.IsNearlyZero() ? FVector::UpVector : -Forward, OrbColor);

    BreakerFX::FEffectTiming Burst;
    Burst.DurationSeconds = 0.20f;     // O2 PLACEHOLDER
    Burst.FadeInSeconds = 0.0f;        // an impact is a hard edge, not a swell
    Burst.FadeOutSeconds = 0.16f;
    Effects->AddGlow(Location, 26.0f, OrbColor, 3.4f, Burst);
    Effects->AddBlinkLight(Location, 280.0f, OrbColor, 1600.0f, Burst);

    // Four splinters away from the surface. Short, thin and staggered by a
    // frame each, so the mark has a direction instead of being a dot.
    BreakerFX::FEffectTiming Splinter = Burst;
    Splinter.DurationSeconds = 0.16f;
    const FVector Out = Forward.IsNearlyZero() ? FVector::UpVector : -Forward;
    const FVector Side = FVector::CrossProduct(Out, FVector::UpVector).GetSafeNormal();
    const FVector Up = FVector::CrossProduct(Side, Out).GetSafeNormal();
    const FVector Spread[4] = { Side, -Side, Up, -Up };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FVector Direction = (Out * 0.55f + Spread[Index] * 0.85f).GetSafeNormal();
        Effects->AddStroke(Location + Direction * 12.0f, Location + Direction * 62.0f,
            2.5f, OrbColor, 2.4f, Splinter, 0.015f * Index);
    }
}
