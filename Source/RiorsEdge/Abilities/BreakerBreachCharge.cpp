#include "Abilities/BreakerBreachCharge.h"
#include "Combat/BreakerCombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABreakerBreachCharge::ABreakerBreachCharge()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    UStaticMeshComponent* Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Charge"));
    SetRootComponent(Body);
    Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    Body->SetRelativeScale3D(FVector(0.25f));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCastShadow(false);
    UPointLightComponent* Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("FuseGlow"));
    Glow->SetupAttachment(Body);
    Glow->SetLightColor(FLinearColor(1.0f, 0.22f, 0.025f));
    Glow->SetIntensity(150.0f);
    Glow->SetAttenuationRadius(100.0f);
    Glow->SetCastShadows(false);
}

void ABreakerBreachCharge::Arm(AActor* Caster, float FuseSeconds, AActor* StickyTarget)
{
    if (!HasAuthority() || !Caster) { Destroy(); return; }
    SetOwner(Caster);
    FollowTarget = StickyTarget;
    if (StickyTarget) LocalImpact = StickyTarget->GetActorTransform().InverseTransformPosition(GetActorLocation());
    if (UBreakerCombatComponent* Combat = Caster->FindComponentByClass<UBreakerCombatComponent>())
    {
        if (Combat->IsDead()) { Destroy(); return; }
        Combat->OnDeath.AddDynamic(this, &ThisClass::CancelForOwnerDeath);
    }
    GetWorldTimerManager().SetTimer(FuseTimer, this, &ThisClass::DetonateNow, FMath::Max(0.05f, FuseSeconds), false);
}

void ABreakerBreachCharge::UpdateStickyLocation()
{
    AActor* Target = FollowTarget.Get();
    if (!Target) return;
    const UBreakerCombatComponent* Combat = Target->FindComponentByClass<UBreakerCombatComponent>();
    if (!Combat || Combat->IsDead()) { FollowTarget.Reset(); return; }
    SetActorLocation(Target->GetActorTransform().TransformPosition(LocalImpact));
}

void ABreakerBreachCharge::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority())
    {
        if (!IsValid(GetOwner())) { Destroy(); return; }
        UpdateStickyLocation();
    }
}

void ABreakerBreachCharge::DetonateNow()
{
    if (!HasAuthority() || bResolved) return;
    bResolved = true;
    UpdateStickyLocation();
    const UBreakerCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr;
    const FVector Position = GetActorLocation();
    FBreakerChargeDetonated Callback = OnDetonated;
    const bool bCanDetonate = IsValid(GetOwner()) && Combat && !Combat->IsDead();
    Destroy(); // Remove fuse and death binding before blast self-damage can kill.
    if (bCanDetonate) Callback.ExecuteIfBound(Position);
}

void ABreakerBreachCharge::CancelForOwnerDeath() { Destroy(); }
void ABreakerBreachCharge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(FuseTimer);
    if (UBreakerCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UBreakerCombatComponent>() : nullptr)
        Combat->OnDeath.RemoveDynamic(this, &ThisClass::CancelForOwnerDeath);
    OnDetonated.Unbind();
    Super::EndPlay(EndPlayReason);
}

