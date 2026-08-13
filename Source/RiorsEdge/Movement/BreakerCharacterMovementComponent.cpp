#include "Movement/BreakerCharacterMovementComponent.h"

#include "Items/BreakerEquipmentComponent.h"

#include "GameFramework/Character.h"
#include "Engine/World.h"

UBreakerCharacterMovementComponent::UBreakerCharacterMovementComponent()
{
    MaxWalkSpeed = WalkSpeed;
    MaxWalkSpeedCrouched = WalkSpeed * 0.55f;
    MaxAcceleration = 4200.0f;
    BrakingDecelerationWalking = 1800.0f;
    GroundFriction = 7.5f;
    AirControl = 0.55f;
    AirControlBoostMultiplier = 1.4f;
    AirControlBoostVelocityThreshold = 300.0f;
    JumpZVelocity = 700.0f;
    GravityScale = 1.35f;
    FallingLateralFriction = 0.05f;
    MaxSimulationTimeStep = 1.0f / 60.0f;
    MaxSimulationIterations = 8;
    NavAgentProps.bCanCrouch = true;
}

UBreakerEquipmentComponent* UBreakerCharacterMovementComponent::GetEquipment() const
{
    if (!CachedEquipment.IsValid() && GetOwner())
    {
        CachedEquipment = GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>();
    }
    return CachedEquipment.Get();
}

float UBreakerCharacterMovementComponent::GearMoveSpeedMultiplier() const
{
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    return Equipment ? Equipment->GetStats().MoveSpeedMultiplier : 1.0f;
}

float UBreakerCharacterMovementComponent::GearSlideSpeedMultiplier() const
{
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    return Equipment ? Equipment->GetStats().SlideSpeedMultiplier : 1.0f;
}

float UBreakerCharacterMovementComponent::GearAirControlMultiplier() const
{
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    return Equipment ? Equipment->GetStats().AirControlMultiplier : 1.0f;
}

float UBreakerCharacterMovementComponent::GearDashCooldownMultiplier() const
{
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    return Equipment ? Equipment->GetStats().DashCooldownMultiplier : 1.0f;
}

float UBreakerCharacterMovementComponent::GetMaxSpeed() const
{
    if (bSliding)
    {
        return FMath::Max(SprintSpeed * GearSlideSpeedMultiplier(), Velocity.Size2D());
    }
    const float GroundedCap = (bWantsToSprint ? SprintSpeed : WalkSpeed) * GearMoveSpeedMultiplier();
    return FMath::Max(GroundedCap, BoostedSpeedCeiling);
}

void UBreakerCharacterMovementComponent::SetSprinting(bool bEnabled)
{
    bWantsToSprint = bEnabled;
}

void UBreakerCharacterMovementComponent::SetSlideRequested(bool bEnabled)
{
    if (bEnabled && !bSlideRequested)
    {
        bSlideRequestConsumed = false;
    }
    bSlideRequested = bEnabled;
    if (!bEnabled)
    {
        bSlideRequestConsumed = false;
        EndSlide();
    }
}

bool UBreakerCharacterMovementComponent::TryDash(const FVector& RequestedDirection)
{
    const UWorld* World = GetWorld();
    if (!World || bSliding || World->GetTimeSeconds() - LastDashTime < DashCooldown * GearDashCooldownMultiplier())
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

    const float OutputSpeed = FMath::Min(FMath::Max(Velocity.Size2D(), DashSpeedFloor) + DashSpeedBonus, MomentumHardCap);
    Velocity.X = Direction.X * OutputSpeed;
    Velocity.Y = Direction.Y * OutputSpeed;
    Velocity.Z = FMath::Max(Velocity.Z, DashVerticalFloor);
    LastDashTime = World->GetTimeSeconds();
    BoostedSpeedCeiling = OutputSpeed;
    return true;
}

bool UBreakerCharacterMovementComponent::BeginSlide()
{
    if (bSliding || !IsMovingOnGround() || Velocity.Size2D() < SlideEntrySpeed || !CharacterOwner)
    {
        return false;
    }

    bSliding = true;
    bSlideRequestConsumed = true;
    SlideElapsed = 0.0f;
    const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (CurrentTime - LastSlideBoostTime >= SlideBoostCooldown)
    {
        SlideEntryBoostRemaining = SlideEntryBoost;
        LastSlideBoostTime = CurrentTime;
    }
    else
    {
        SlideEntryBoostRemaining = 0.0f;
    }
    SavedGroundFriction = GroundFriction;
    SavedBrakingDeceleration = BrakingDecelerationWalking;
    GroundFriction = SlideGroundFriction;
    BrakingDecelerationWalking = SlideBrakingDeceleration;
    CharacterOwner->Crouch();

    return true;
}

void UBreakerCharacterMovementComponent::EndSlide()
{
    if (!bSliding)
    {
        return;
    }

    bSliding = false;
    SlideElapsed = 0.0f;
    SlideEntryBoostRemaining = 0.0f;
    GroundFriction = SavedGroundFriction;
    BrakingDecelerationWalking = SavedBrakingDeceleration;
    if (CharacterOwner)
    {
        CharacterOwner->UnCrouch();
    }
}

void UBreakerCharacterMovementComponent::PrepareSlideJump()
{
    if (!bSliding) return;

    const FVector PreservedHorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
    const float PreservedSpeed = PreservedHorizontalVelocity.Size();
    EndSlide();
    SetSprinting(true);
    BoostedSpeedCeiling = FMath::Max(BoostedSpeedCeiling, PreservedSpeed);
    Velocity.X = PreservedHorizontalVelocity.X;
    Velocity.Y = PreservedHorizontalVelocity.Y;
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

    const float SpeedBeforeMovement = Velocity.Size2D();
    const FVector DirectionBeforeMovement = Velocity.GetSafeNormal2D();
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (BoostedSpeedCeiling > 0.0f)
    {
        const float SpeedAfterMovement = Velocity.Size2D();
        const FVector DirectionAfterMovement = Velocity.GetSafeNormal2D();
        const bool bMovementReleased = Acceleration.SizeSquared2D() <= UE_KINDA_SMALL_NUMBER;
        const bool bCollisionSlowed = SpeedBeforeMovement > SprintSpeed
            && SpeedAfterMovement < SpeedBeforeMovement - 100.0f;
        const bool bCollisionRedirected = !DirectionBeforeMovement.IsNearlyZero()
            && !DirectionAfterMovement.IsNearlyZero()
            && FVector::DotProduct(DirectionBeforeMovement, DirectionAfterMovement) < 0.65f;
        if (bMovementReleased || bCollisionSlowed || bCollisionRedirected)
        {
            BoostedSpeedCeiling = 0.0f;
        }
        else
        {
            BoostedSpeedCeiling = FMath::Min(FMath::Max(BoostedSpeedCeiling, SpeedAfterMovement), MomentumHardCap);
        }
    }

    ApplyAirSteering(DeltaTime);

    if (bSlideRequested && !bSlideRequestConsumed && !bSliding && IsMovingOnGround())
    {
        BeginSlide();
    }

    if (!bSliding)
    {
        return;
    }

    SlideElapsed += DeltaTime;
    if (!IsMovingOnGround() || Velocity.Size2D() < SlideExitSpeed || SlideElapsed >= SlideMaxDuration)
    {
        EndSlide();
        return;
    }

    const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal.GetSafeNormal();
    if (SlideEntryBoostRemaining > 0.0f && SlideEntryBoostDuration > UE_SMALL_NUMBER)
    {
        const float BoostRoom = FMath::Max(0.0f, (SprintSpeed + SlideEntryBoost) * GearSlideSpeedMultiplier() - Velocity.Size2D());
        const float AppliedBoost = FMath::Min3(SlideEntryBoostRemaining, SlideEntryBoost / SlideEntryBoostDuration * DeltaTime, BoostRoom);
        Velocity += Velocity.GetSafeNormal2D() * AppliedBoost;
        SlideEntryBoostRemaining -= AppliedBoost;
    }
    const FVector DownSlope = FVector::VectorPlaneProject(FVector::DownVector, FloorNormal).GetSafeNormal2D();
    const float SlopeAmount = FMath::Clamp(1.0f - FloorNormal.Z, 0.0f, 1.0f);
    Velocity += DownSlope * SlideSlopeAcceleration * SlopeAmount * DeltaTime;
}

void UBreakerCharacterMovementComponent::ApplyAirSteering(float DeltaTime)
{
    if (!IsFalling() || bWallRiding)
    {
        return;
    }

    const FVector WishDirection = Acceleration.GetSafeNormal2D();
    const float HorizontalSpeed = Velocity.Size2D();
    if (WishDirection.IsNearlyZero() || HorizontalSpeed <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FVector HorizontalDirection = Velocity.GetSafeNormal2D();
    const float Alignment = FVector::DotProduct(HorizontalDirection, WishDirection);
    if (Alignment <= AirSteerMinimumAlignment)
    {
        return;
    }

    const float SteerRate = AirSteerRate * GearAirControlMultiplier() * (0.35f + 0.65f * FMath::Max(Alignment, 0.0f));
    const float Alpha = FMath::Clamp(SteerRate * DeltaTime, 0.0f, 1.0f);
    const FVector SteeredDirection = FMath::Lerp(HorizontalDirection, WishDirection, Alpha).GetSafeNormal2D();
    Velocity.X = SteeredDirection.X * HorizontalSpeed;
    Velocity.Y = SteeredDirection.Y * HorizontalSpeed;
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
