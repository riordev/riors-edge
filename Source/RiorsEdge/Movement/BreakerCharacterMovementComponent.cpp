#include "Movement/BreakerCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Engine/World.h"

UBreakerCharacterMovementComponent::UBreakerCharacterMovementComponent()
{
    MaxWalkSpeed = WalkSpeed;
    MaxWalkSpeedCrouched = WalkSpeed * 0.55f;
    MaxAcceleration = 4200.0f;
    BrakingDecelerationWalking = 1800.0f;
    GroundFriction = 7.5f;
    AirControl = 0.32f;
    AirControlBoostMultiplier = 1.4f;
    AirControlBoostVelocityThreshold = 300.0f;
    JumpZVelocity = 700.0f;
    GravityScale = 1.25f;
    FallingLateralFriction = 0.15f;
    NavAgentProps.bCanCrouch = true;
}

float UBreakerCharacterMovementComponent::GetMaxSpeed() const
{
    if (bSliding)
    {
        return FMath::Max(SprintSpeed, Velocity.Size2D());
    }
    return bWantsToSprint ? SprintSpeed : WalkSpeed;
}

void UBreakerCharacterMovementComponent::SetSprinting(bool bEnabled)
{
    bWantsToSprint = bEnabled;
}

bool UBreakerCharacterMovementComponent::TryDash(const FVector& RequestedDirection)
{
    const UWorld* World = GetWorld();
    if (!World || World->GetTimeSeconds() - LastDashTime < DashCooldown)
    {
        return false;
    }

    FVector Direction = RequestedDirection.GetSafeNormal2D();
    if (Direction.IsNearlyZero() && CharacterOwner)
    {
        Direction = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
    }
    if (Direction.IsNearlyZero())
    {
        return false;
    }

    const float OutputSpeed = FMath::Max(Velocity.Size2D(), DashSpeedFloor) + DashSpeedBonus;
    Velocity.X = Direction.X * OutputSpeed;
    Velocity.Y = Direction.Y * OutputSpeed;
    Velocity.Z = FMath::Max(Velocity.Z, DashVerticalFloor);
    LastDashTime = World->GetTimeSeconds();
    return true;
}

bool UBreakerCharacterMovementComponent::BeginSlide()
{
    if (bSliding || !IsMovingOnGround() || Velocity.Size2D() < SlideEntrySpeed || !CharacterOwner)
    {
        return false;
    }

    bSliding = true;
    SavedGroundFriction = GroundFriction;
    SavedBrakingDeceleration = BrakingDecelerationWalking;
    GroundFriction = SlideGroundFriction;
    BrakingDecelerationWalking = SlideBrakingDeceleration;
    CharacterOwner->Crouch();

    const FVector Direction = Velocity.GetSafeNormal2D();
    Velocity += Direction * SlideEntryBoost;
    return true;
}

void UBreakerCharacterMovementComponent::EndSlide()
{
    if (!bSliding)
    {
        return;
    }

    bSliding = false;
    GroundFriction = SavedGroundFriction;
    BrakingDecelerationWalking = SavedBrakingDeceleration;
    if (CharacterOwner)
    {
        CharacterOwner->UnCrouch();
    }
}

void UBreakerCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    FHitResult RunnableWall;
    if (!bWallRiding && IsFalling() && !bSliding && Velocity.Size2D() >= WallRideMinimumSpeed
        && Acceleration.SizeSquared2D() > UE_KINDA_SMALL_NUMBER
        && GetWorld() && GetWorld()->GetTimeSeconds() - LastWallRideEndTime >= WallRideCooldown
        && FindRunnableWall(RunnableWall))
    {
        BeginWallRide(RunnableWall);
    }

    if (bWallRiding)
    {
        WallRideElapsed += DeltaTime;
        if (IsMovingOnGround() || WallRideElapsed >= WallRideMaxDuration || !FindRunnableWall(RunnableWall))
        {
            EndWallRide();
        }
        else
        {
            WallRideNormal = RunnableWall.ImpactNormal.GetSafeNormal();
            const float HorizontalSpeed = Velocity.Size2D();
            FVector AlongWall = FVector::VectorPlaneProject(Velocity, WallRideNormal);
            AlongWall.Z = Velocity.Z;
            const FVector HorizontalAlongWall = AlongWall.GetSafeNormal2D() * HorizontalSpeed;
            Velocity.X = HorizontalAlongWall.X;
            Velocity.Y = HorizontalAlongWall.Y;
            // A small contact bias prevents collision resolution from
            // bouncing the player away; it is not a speed boost.
            Velocity -= WallRideNormal * 35.0f;
        }
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bSliding)
    {
        return;
    }

    if (!IsMovingOnGround() || Velocity.Size2D() < SlideExitSpeed)
    {
        EndSlide();
        return;
    }

    const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal.GetSafeNormal();
    const FVector DownSlope = FVector::VectorPlaneProject(FVector::DownVector, FloorNormal).GetSafeNormal2D();
    const float SlopeAmount = FMath::Clamp(1.0f - FloorNormal.Z, 0.0f, 1.0f);
    Velocity += DownSlope * SlideSlopeAcceleration * SlopeAmount * DeltaTime;
}

bool UBreakerCharacterMovementComponent::FindRunnableWall(FHitResult& OutHit) const
{
    if (!CharacterOwner || !GetWorld())
    {
        return false;
    }

    const FVector Start = CharacterOwner->GetActorLocation();
    const FVector Right = CharacterOwner->GetActorRightVector();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerWallRide), false, CharacterOwner);
    FHitResult LeftHit;
    FHitResult RightHit;
    const bool bLeft = GetWorld()->LineTraceSingleByChannel(LeftHit, Start, Start - Right * WallRideTraceDistance, ECC_Visibility, Params);
    const bool bRight = GetWorld()->LineTraceSingleByChannel(RightHit, Start, Start + Right * WallRideTraceDistance, ECC_Visibility, Params);

    auto IsRunnable = [](const FHitResult& Hit)
    {
        return Hit.bBlockingHit && FMath::Abs(Hit.ImpactNormal.Z) <= 0.25f;
    };

    const bool bValidLeft = bLeft && IsRunnable(LeftHit);
    const bool bValidRight = bRight && IsRunnable(RightHit);
    if (!bValidLeft && !bValidRight)
    {
        return false;
    }

    OutHit = bValidLeft && bValidRight
        ? (LeftHit.Distance <= RightHit.Distance ? LeftHit : RightHit)
        : (bValidLeft ? LeftHit : RightHit);
    return true;
}

void UBreakerCharacterMovementComponent::BeginWallRide(const FHitResult& WallHit)
{
    bWallRiding = true;
    WallRideElapsed = 0.0f;
    WallRideNormal = WallHit.ImpactNormal.GetSafeNormal();
    SavedGravityScale = GravityScale;
    GravityScale = WallRideGravityScale;
    Velocity.Z = FMath::Max(Velocity.Z, -250.0f);
    OnWallRideStateChanged.Broadcast(true);
}

void UBreakerCharacterMovementComponent::EndWallRide()
{
    if (!bWallRiding)
    {
        return;
    }

    bWallRiding = false;
    GravityScale = SavedGravityScale;
    WallRideElapsed = 0.0f;
    LastWallRideEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    OnWallRideStateChanged.Broadcast(false);
}

bool UBreakerCharacterMovementComponent::TryWallJump()
{
    if (!bWallRiding)
    {
        return false;
    }

    FVector AlongWall = FVector::VectorPlaneProject(Velocity, WallRideNormal);
    AlongWall.Z = 0.0f;
    const float PreservedSpeed = FMath::Max(AlongWall.Size2D(), WallRideMinimumSpeed);
    Velocity = AlongWall.GetSafeNormal2D() * PreservedSpeed
        + WallRideNormal * WallRideJumpAwaySpeed
        + FVector::UpVector * WallRideJumpUpSpeed;
    EndWallRide();
    return true;
}
