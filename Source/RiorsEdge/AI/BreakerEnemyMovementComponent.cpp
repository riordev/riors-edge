#include "AI/BreakerEnemyMovementComponent.h"
#include "AI/BreakerEnemyController.h"
#include "Navigation/PathFollowingComponent.h"
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
    AActor* Target, float DistanceToTarget, float AttackRange, float MoveSpeed,
    bool bHasGoal, const FVector& Goal)
{
    MaxSpeed = BreakerLocomotionMath::MaxSpeed(MoveSpeed, SpeedScale);

    ABreakerEnemyController* Controller = PawnOwner
        ? Cast<ABreakerEnemyController>(PawnOwner->GetController()) : nullptr;

    EBreakerLocomotionMode Mode = EBreakerLocomotionMode::Idle;
    FVector PathTo = FVector::ZeroVector;
    float Acceptance = 0.0f;

    if (bHasGoal && PawnOwner)
    {
        // The goal override: the closing line, the distance and the path all
        // run to the goal. Acceptance is the capsule radius the pawn already
        // carries, the same arrival threshold PATROL uses.
        const FVector ToGoal = Goal - PawnOwner->GetActorLocation();
        const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(UpdatedComponent);
        Acceptance = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
        const bool bCouldPath = Controller && !Direction.IsNearlyZero();
        const bool bBlocked = bCouldPath && IsClosingLineBlocked(Goal, Target);
        Mode = BreakerLocomotionMath::ChooseGoalMode(Direction, bBlocked, ToGoal.Size2D(), Acceptance);
        PathTo = Goal;
    }
    else
    {
        const FVector ToTarget = (Target && PawnOwner)
            ? (Target->GetActorLocation() - PawnOwner->GetActorLocation()) : FVector::ZeroVector;
        Acceptance = BreakerLocomotionMath::AcceptanceRadius(AttackRange);
        // The trace is the one world fact the rule needs, and it is only asked
        // for when the answer could matter: a target, a controller to path with,
        // and a direction that is not a hold.
        const bool bCouldPath = Target && Controller && !Direction.IsNearlyZero();
        const bool bBlocked = bCouldPath && IsClosingLineBlocked(Target ? Target->GetActorLocation() : FVector::ZeroVector, Target);

        Mode = BreakerLocomotionMath::ChooseMode(
            Direction, ToTarget, Target != nullptr, bBlocked, DistanceToTarget, Acceptance);
        if (Target) PathTo = Target->GetActorLocation();
    }

    if (Mode == EBreakerLocomotionMode::Path)
    {
        // A failed route is not permission to walk through the obstruction.
        // The controller rate-limits retries while navigation builds/changes.
        if (!Controller || !Controller->Chase(PathTo, Acceptance))
        {
            Mode = EBreakerLocomotionMode::Idle;
        }
    }

    bBlockedHold = Mode == EBreakerLocomotionMode::Idle && !Direction.IsNearlyZero();
    if (bBlockedHold)
    {
        StopMovementImmediately();
        ConsumeInputVector();
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

bool UBreakerEnemyMovementComponent::IsClosingLineBlocked(const FVector& To, const AActor* Ignore) const
{
    UWorld* World = GetWorld();
    if (!World || !PawnOwner || !UpdatedComponent) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerEnemyClosingLine), false, PawnOwner);
    if (Ignore) Params.AddIgnoredActor(Ignore);
    FHitResult Hit;
    const FVector Start = UpdatedComponent->GetComponentLocation();
    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(UpdatedComponent);
    if (!Capsule) return World->LineTraceSingleByObjectType(Hit, Start, To,
        FCollisionObjectQueryParams(ECC_WorldStatic), Params);
    // Slightly inset the shell to avoid counting the floor we stand on.
    // Trace horizontally: target eye height must not lift our feet over a ledge.
    constexpr float ClearanceInsetCm = 1.0f; // O2 PLACEHOLDER
    const float Radius = FMath::Max(1.0f, Capsule->GetScaledCapsuleRadius() - ClearanceInsetCm);
    const float HalfHeight = FMath::Max(Radius, Capsule->GetScaledCapsuleHalfHeight() - ClearanceInsetCm);
    return World->SweepSingleByObjectType(Hit, Start, FVector(To.X, To.Y, Start.Z), FQuat::Identity,
        FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
}

void UBreakerEnemyMovementComponent::ResetForRevive()
{
    StopMovementImmediately();
    ConsumeInputVector();
    WorldTouchCount = 0;
    bBlockedHold = false;
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
    // Pawns block the WorldStatic trace channel too. They are not ground:
    // snapping onto another enemy's capsule makes crowds climb and jitter.
    if (World->LineTraceSingleByObjectType(Ground, TraceStart, TraceEnd,
        FCollisionObjectQueryParams(ECC_WorldStatic), GroundParams))
    {
        const float TargetZ = Ground.ImpactPoint.Z + HalfHeight;
        const float CurrentZ = Location.Z;
        const float NewZ = CurrentZ > TargetZ
            ? FMath::Max(TargetZ, CurrentZ - 1200.0f * DeltaTime)
            : FMath::Min(TargetZ, CurrentZ + 600.0f * DeltaTime);
        UpdatedComponent->SetWorldLocation(FVector(Location.X, Location.Y, NewZ), false);
    }
}

FVector UBreakerEnemyMovementComponent::GetPathHeading() const
{
    const ABreakerEnemyController* Controller = PawnOwner
        ? Cast<ABreakerEnemyController>(PawnOwner->GetController()) : nullptr;
    const UPathFollowingComponent* Following = Controller ? Controller->GetPathFollowingComponent() : nullptr;
    if (!Following || !Following->HasValidPath()) return FVector::ZeroVector;
    return Following->GetCurrentDirection().GetSafeNormal2D();
}
