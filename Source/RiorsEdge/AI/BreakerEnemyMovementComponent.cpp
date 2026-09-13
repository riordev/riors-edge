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
    else ConsumeInputVector();
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
    ClearanceHoldCount = 0;
    MeasuredGroundVelocity = FVector::ZeroVector;
    AvoidanceHeading = FVector::ZeroVector;
    bHasClientLocation = false;
    bBlockedHold = false;
    LastMode = EBreakerLocomotionMode::Idle;
}

void UBreakerEnemyMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    if (!UpdatedComponent || !PawnOwner || DeltaTime <= SMALL_NUMBER) return;
    const FVector Before = UpdatedComponent->GetComponentLocation();
    if (PawnOwner->HasAuthority()) ConstrainNextStep(DeltaTime);
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (PawnOwner->HasAuthority())
    {
        SnapToGround(DeltaTime);
        const FVector Delta = UpdatedComponent->GetComponentLocation() - Before;
        MeasuredGroundVelocity = FVector(Delta.X, Delta.Y, 0) / DeltaTime;
    }
    else
    {
        // Replicated pawn transforms are applied outside this component tick.
        const FVector Delta = Before - PreviousClientLocation;
        constexpr float ClientGaitResponse = 12.0f; // O2 PLACEHOLDER: smooth gaps between replication updates
        const FVector Sample = bHasClientLocation ? FVector(Delta.X, Delta.Y, 0) / DeltaTime : FVector::ZeroVector;
        MeasuredGroundVelocity = FMath::VInterpTo(MeasuredGroundVelocity, Sample, DeltaTime, ClientGaitResponse);
        PreviousClientLocation = Before;
        bHasClientLocation = true;
    }
}

void UBreakerEnemyMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
    // The engine base stores an uncapped distance/time request as Velocity.
    // Keep the command bounded; animations use measured displacement instead.
    const FVector Planar(MoveVelocity.X, MoveVelocity.Y, 0);
    Super::RequestDirectMove(Planar.GetClampedToMaxSize(MaxSpeed), bForceMaxSpeed);
}

bool UBreakerEnemyMovementComponent::SweepStep(const FVector& Delta, FHitResult& Hit, bool bIncludePawns) const
{
    const auto* Capsule = Cast<UCapsuleComponent>(UpdatedComponent);
    if (!GetWorld() || !Capsule) return false;
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float Height = FMath::Max(Radius, Capsule->GetScaledCapsuleHalfHeight() - 1.0f);
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    if (bIncludePawns) Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerEnemyStep), false, PawnOwner);
    const FVector Start = Capsule->GetComponentLocation();
    return GetWorld()->SweepSingleByObjectType(Hit, Start, Start + Delta, FQuat::Identity,
        Objects, FCollisionShape::MakeCapsule(Radius, Height), Params);
}

void UBreakerEnemyMovementComponent::ConstrainNextStep(float DeltaTime)
{
    // A rejected route can already have cleared input. Recover a static
    // overlap even at rest so that the next route attempt has a valid start.
    FHitResult InitialOverlap;
    if (SweepStep(FVector::ZeroVector, InitialOverlap, false) && InitialOverlap.bStartPenetrating
        && FMath::Abs(InitialOverlap.ImpactNormal.Z) < .5f)
        ResolvePenetration(GetPenetrationAdjustment(InitialOverlap), InitialOverlap, UpdatedComponent->GetComponentQuat());
    const FVector Input = GetPendingInputVector().GetSafeNormal2D();
    const FVector Intent = Input.IsNearlyZero() ? FVector(Velocity.X, Velocity.Y, 0)
        : Input * MaxSpeed;
    if (Intent.IsNearlyZero()) return;
    AvoidanceHeading = FVector::ZeroVector;
    // Include existing momentum: a turn cannot erase an impending collision.
    const float Speed = FMath::Max(static_cast<float>(Intent.Size2D()), static_cast<float>(Velocity.Size2D()));
    constexpr float LookAheadSkinCm = 2.0f; // O2 PLACEHOLDER: margin along travel, not a wider nav agent
    const FVector Step = Intent.GetSafeNormal2D() * (Speed * DeltaTime + LookAheadSkinCm);
    FHitResult Hit;
    if (!SweepStep(Step, Hit, true)) return;
    // Spawn/ground corrections can start a body overlapping geometry. Let
    // the engine resolve that overlap before enforcing a no-contact step.
    if (Hit.bStartPenetrating && ResolvePenetration(GetPenetrationAdjustment(Hit), Hit, UpdatedComponent->GetComponentQuat()))
        if (!SweepStep(Step, Hit, true)) return;
    // Walkable floor edges are handled by the mover and ground snap. Treating
    // their upward normal as a wall strands the pawn on shallow pavement.
    if (Hit.ImpactNormal.Z >= .5f) return;
    const bool bPawn = Hit.Component.IsValid() && Hit.Component->GetCollisionObjectType() == ECC_Pawn;
    if (bPawn && Hit.GetActor()
        && FVector::DotProduct(Hit.GetActor()->GetActorForwardVector().GetSafeNormal2D(), Intent.GetSafeNormal2D()) < -.5f) // O2 PLACEHOLDER
    {
        // Both bodies use their own right: on opposing headings this selects
        // opposite world sides, breaking a mutual yield without randomness.
        const FVector Side = FVector::CrossProduct(FVector::UpVector, Intent.GetSafeNormal2D());
        FHitResult SideHit;
        if (!SweepStep(Side * (Speed * DeltaTime + LookAheadSkinCm), SideHit, true))
        {
            Velocity = Side * FMath::Min(Speed, MaxSpeed);
            AvoidanceHeading = Side;
            if (!Input.IsNearlyZero()) { ConsumeInputVector(); AddInputVector(Side); }
            return;
        }
    }
    const FVector Tangent = FVector::VectorPlaneProject(Intent, Hit.ImpactNormal).GetSafeNormal2D();
    FHitResult TangentHit;
    // Follow a clear wall tangent, but yield to bodies rather than jittering
    // around a moving capsule. The front body frees the lane for the next tick.
    if (!bPawn && !Hit.bStartPenetrating && !Tangent.IsNearlyZero()
        && FVector::DotProduct(Tangent, Intent.GetSafeNormal2D()) > .1f // O2 PLACEHOLDER
        && !SweepStep(Tangent * (Speed * DeltaTime + LookAheadSkinCm), TangentHit, true))
    {
        Velocity = Tangent * FMath::Min(Speed, MaxSpeed);
        AvoidanceHeading = Tangent;
        if (!Input.IsNearlyZero()) { ConsumeInputVector(); AddInputVector(Tangent); }
        return;
    }
    StopMovementImmediately();
    ConsumeInputVector();
    if (!bPawn)
        if (auto* Controller = Cast<ABreakerEnemyController>(PawnOwner->GetController())) Controller->StopChase();
    bBlockedHold = true;
    LastMode = EBreakerLocomotionMode::Idle;
    ++ClearanceHoldCount;
#if !UE_BUILD_SHIPPING
    if (ClearanceHoldCount == 1)
        UE_LOG(LogTemp, Display, TEXT("[BreakerClearance] pawn=%s obstacle=%s component=%s normal=%s penetrating=%d intent=%s tangent=%s tangentObstacle=%s"),
            *PawnOwner->GetName(), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
            *Hit.ImpactNormal.ToCompactString(), Hit.bStartPenetrating, *Intent.ToCompactString(), *Tangent.ToCompactString(),
            *GetNameSafe(TangentHit.GetActor()));
#endif
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
    if (!AvoidanceHeading.IsNearlyZero()) return AvoidanceHeading;
    const ABreakerEnemyController* Controller = PawnOwner
        ? Cast<ABreakerEnemyController>(PawnOwner->GetController()) : nullptr;
    const UPathFollowingComponent* Following = Controller ? Controller->GetPathFollowingComponent() : nullptr;
    if (!Following || !Following->HasValidPath()) return FVector::ZeroVector;
    return Following->GetCurrentDirection().GetSafeNormal2D();
}
