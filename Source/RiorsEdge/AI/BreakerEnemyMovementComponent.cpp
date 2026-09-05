#include "AI/BreakerEnemyMovementComponent.h"
#include "AI/BreakerEnemyController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UBreakerEnemyMovementComponent::UBreakerEnemyMovementComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Tick moved bodies at a constant speed from the first frame and stopped
    // them dead on a zero direction; these keep that feel through a mover
    // that integrates. Both O2 PLACEHOLDER.
    Acceleration = 6000.0f;
    Deceleration = 8000.0f;
    TurningBoost = 8.0f;
    // MaxSpeed is written every Drive from MoveSpeed x SpeedScale; the default
    // only matters before the first behaviour frame.
    MaxSpeed = 330.0f;
}

EBreakerLocomotionMode UBreakerEnemyMovementComponent::Drive(const FVector& Direction, float SpeedScale,
    AActor* Target, float DistanceToTarget, float AttackRange, float MoveSpeed)
{
    MaxSpeed = BreakerLocomotionMath::MaxSpeed(MoveSpeed, SpeedScale);

    ABreakerEnemyController* Controller = PawnOwner
        ? Cast<ABreakerEnemyController>(PawnOwner->GetController()) : nullptr;

    const FVector ToTarget = (Target && PawnOwner)
        ? (Target->GetActorLocation() - PawnOwner->GetActorLocation()) : FVector::ZeroVector;
    const float Acceptance = BreakerLocomotionMath::AcceptanceRadius(AttackRange);
    // The trace is the one world fact the rule needs, and it is only asked
    // for when the answer could matter: a target, a controller to path with,
    // and a direction that is not a hold.
    const bool bCouldPath = Target && Controller && !Direction.IsNearlyZero();
    const bool bBlocked = bCouldPath && IsClosingLineBlocked(Target);

    EBreakerLocomotionMode Mode = BreakerLocomotionMath::ChooseMode(
        Direction, ToTarget, Target != nullptr, bBlocked, DistanceToTarget, Acceptance);

    if (Mode == EBreakerLocomotionMode::Path)
    {
        // A refused path (no navmesh yet, the first second of a level) is not
        // a hold: the body steers this frame and asks again next frame.
        if (!Controller->Chase(Target->GetActorLocation(), Acceptance))
        {
            Mode = EBreakerLocomotionMode::Steer;
        }
    }

    if (Mode != EBreakerLocomotionMode::Path && Controller)
    {
        Controller->StopChase();
    }
    if (Mode == EBreakerLocomotionMode::Steer)
    {
        AddInputVector(Direction.GetSafeNormal2D());
    }
    LastMode = Mode;
    return Mode;
}

bool UBreakerEnemyMovementComponent::IsClosingLineBlocked(const AActor* Target) const
{
    UWorld* World = GetWorld();
    if (!World || !PawnOwner || !Target || !UpdatedComponent) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerEnemyClosingLine), false, PawnOwner);
    Params.AddIgnoredActor(Target);
    FHitResult Hit;
    return World->LineTraceSingleByChannel(Hit, UpdatedComponent->GetComponentLocation(),
        Target->GetActorLocation(), ECC_WorldStatic, Params);
}

void UBreakerEnemyMovementComponent::ResetForRevive()
{
    StopMovementImmediately();
    ConsumeInputVector();
    WorldTouchCount = 0;
    LastMode = EBreakerLocomotionMode::Idle;
}

void UBreakerEnemyMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!PawnOwner || !UpdatedComponent || !PawnOwner->HasAuthority()) return;
    SnapToGround(DeltaTime);
}

void UBreakerEnemyMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
    Super::HandleImpact(Hit, TimeSlice, MoveDelta);
    // Upright geometry only: a normal pointing mostly up is the floor, and the
    // floor is the ground snap's business.
    if (Hit.IsValidBlockingHit() && FMath::Abs(Hit.ImpactNormal.Z) < 0.5f
        && Hit.Component.IsValid() && Hit.Component->GetCollisionObjectType() == ECC_WorldStatic)
    {
        ++WorldTouchCount;
    }
}

void UBreakerEnemyMovementComponent::SnapToGround(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World) return;
    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(UpdatedComponent);
    const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.0f;
    const FVector Location = UpdatedComponent->GetComponentLocation();
    const FVector TraceStart = Location + FVector(0, 0, 60.0f);
    const FVector TraceEnd = TraceStart - FVector(0, 0, 4000.0f);
    FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(BreakerEnemyGround), false, PawnOwner);
    FHitResult Ground;
    if (World->LineTraceSingleByChannel(Ground, TraceStart, TraceEnd, ECC_WorldStatic, GroundParams))
    {
        const float TargetZ = Ground.ImpactPoint.Z + HalfHeight;
        const float CurrentZ = Location.Z;
        const float NewZ = CurrentZ > TargetZ
            ? FMath::Max(TargetZ, CurrentZ - 1200.0f * DeltaTime)
            : FMath::Min(TargetZ, CurrentZ + 600.0f * DeltaTime);
        UpdatedComponent->SetWorldLocation(FVector(Location.X, Location.Y, NewZ), false);
    }
}
