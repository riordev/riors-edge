#include "Movement/BreakerCharacterMovementComponent.h"

#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"

#include "GameFramework/Character.h"
#include "Engine/World.h"

UBreakerCharacterMovementComponent::UBreakerCharacterMovementComponent()
{
    MaxWalkSpeed = WalkSpeed;
    MaxWalkSpeedCrouched = WalkSpeed * 0.55f;
    MaxAcceleration = 4200.0f;
    // Weight pass: floaty is often really "slow to stop". Braking and friction
    // both go up so a released stick plants the character instead of skating
    // it out. Acceleration is deliberately unchanged — it is already twice the
    // engine default, and raising it further reads as twitchy, not heavy.
    BrakingDecelerationWalking = 2400.0f; // OLD: 1800.0f
    GroundFriction = 8.5f; // OLD: 7.5f
    // Air control is NOT reduced. The audit asks for restrained air control and
    // this is already restrained; cutting it further would remove the player's
    // authority in the air, which reads as MORE drift, not less.
    AirControl = 0.55f;
    AirControlBoostMultiplier = 1.4f;
    AirControlBoostVelocityThreshold = 300.0f;
    // Jump impulse is deliberately left alone. The gravity change below already
    // lowers the apex (185 cm -> ~156 cm) and shortens the arc; cutting the
    // impulse as well would compound into a >25% height loss that could stop
    // authored ledges and wall-ride approaches from being reachable, and that
    // cannot be verified without playing the level.
    JumpZVelocity = 700.0f;
    // Two playtests, opposite complaints, and they are about different halves
    // of the arc. 1.35 read as floaty; 1.60 and then 1.45 both read as too
    // heavy. The heaviness is the RISE — it is paid on every single jump — and
    // the floatiness was the DESCENT. So the rise returns to near its original
    // weight and FallGravityMultiplier keeps the fall heavy. Deliberately only
    // one value moved this pass, so the next report attributes cleanly.
    // If it is STILL heavy, drop FallGravityMultiplier next, then set
    // LandingMinimumSpeedScale to 1.0 to remove the landing cost outright.
    GravityScale = 1.38f; // WAS 1.45f, 1.60f AT THE WEIGHT PASS, 1.35f ORIGINALLY
    // Explicit rather than inherited: JumpHoldWindow makes the engine's jump
    // force window non-zero, and gravity must keep applying inside it. False
    // here would be a zero-gravity hold — maximum floatiness.
    bApplyGravityWhileJumping = true;
    FallingLateralFriction = 0.05f;
    MaxSimulationTimeStep = 1.0f / 60.0f;
    MaxSimulationIterations = 8;
    NavAgentProps.bCanCrouch = true;
}

void UBreakerCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    // The jump-release signal lives on ACharacter, not here: with
    // JumpMaxHoldTime at the engine default of 0 the character clears
    // bPressedJump one frame after the press, so a movement component can
    // never tell a tap from a hold. Writing the window here keeps the variable
    // jump entirely inside the movement layer instead of asking the character
    // class (owned elsewhere) to know about it. Setting JumpHoldWindow to 0 in
    // the editor restores stock engine behaviour and disables the cut.
    if (CharacterOwner)
    {
        CharacterOwner->JumpMaxHoldTime = JumpHoldWindow;
    }
}

float UBreakerCharacterMovementComponent::ComputeGravityMultiplier(float VerticalVelocity, float ApexBand, float ApexMultiplier, float FallMultiplier)
{
    const float Band = FMath::Max(ApexBand, UE_KINDA_SMALL_NUMBER);
    if (VerticalVelocity >= Band)
    {
        // Rising freely: the rise keeps the authored jump feel.
        return 1.0f;
    }
    if (VerticalVelocity <= -Band)
    {
        return FallMultiplier;
    }
    return VerticalVelocity >= 0.0f
        ? FMath::Lerp(ApexMultiplier, 1.0f, VerticalVelocity / Band)
        : FMath::Lerp(ApexMultiplier, FallMultiplier, -VerticalVelocity / Band);
}

float UBreakerCharacterMovementComponent::ClampFallSpeed(float VerticalVelocity, float MaxDownwardSpeed)
{
    return MaxDownwardSpeed > 0.0f ? FMath::Max(VerticalVelocity, -MaxDownwardSpeed) : VerticalVelocity;
}

float UBreakerCharacterMovementComponent::ApplyJumpCut(float VerticalVelocity, float CutMultiplier, float MinimumRiseSpeed)
{
    if (VerticalVelocity <= FMath::Max(MinimumRiseSpeed, 0.0f))
    {
        return VerticalVelocity;
    }
    return VerticalVelocity * FMath::Clamp(CutMultiplier, 0.0f, 1.0f);
}

float UBreakerCharacterMovementComponent::LandingSpeedScale(float ImpactSpeed, float HeavyFallSpeed, float MaxImpactSpeed, float MinimumScale)
{
    const float Clamped = FMath::Clamp(MinimumScale, 0.0f, 1.0f);
    if (ImpactSpeed <= HeavyFallSpeed || MaxImpactSpeed <= HeavyFallSpeed)
    {
        return 1.0f;
    }
    const float Alpha = FMath::Clamp((ImpactSpeed - HeavyFallSpeed) / (MaxImpactSpeed - HeavyFallSpeed), 0.0f, 1.0f);
    return FMath::Lerp(1.0f, Clamped, Alpha);
}

FVector UBreakerCharacterMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
    // Wall riding is exempt on purpose. It already runs its own reduced
    // GravityScale, it is capped at WallRideMaxDuration, and it is specified as
    // short and SPEED-NEUTRAL; letting the fall multiplier through would both
    // change its arc and let it end faster than its own duration cap intends.
    if (bWallRiding)
    {
        return Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
    }

    // The project uses standard downward gravity everywhere (wall ride, dash
    // and slide all reason in world Z), so the phase is read off Z directly.
    const float Multiplier = ComputeGravityMultiplier(InitialVelocity.Z, ApexBandSpeed, ApexGravityMultiplier, FallGravityMultiplier);
    FVector Result = Super::NewFallVelocity(InitialVelocity, Gravity * Multiplier, DeltaTime);
    Result.Z = ClampFallSpeed(Result.Z, MaxFallSpeed);
    return Result;
}

bool UBreakerCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
    // JumpHoldWindow makes the engine call DoJump on every held frame and
    // re-floor Velocity.Z at JumpZVelocity, which is a constant-speed rise —
    // exactly the floaty segment this pass removes. One impulse per press: the
    // hold window exists only so the release can be seen.
    if (CharacterOwner && CharacterOwner->bWasJumping)
    {
        return true;
    }

    const bool bJumped = Super::DoJump(bReplayingMoves, DeltaTime);
    if (bJumped)
    {
        bJumpCutArmed = true;
    }
    return bJumped;
}

void UBreakerCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
    // Super zeroes the fall, so the impact has to be read first.
    const float ImpactSpeed = FMath::Max(-Velocity.Z, 0.0f);
    // A queued slide owns its landing: scrubbing speed here would drop the
    // player under SlideEntrySpeed and silently eat the slide.
    const bool bSlideOwnsLanding = bSliding || (bSlideRequested && !bSlideRequestConsumed);
    const float Scale = bSlideOwnsLanding
        ? 1.0f
        : LandingSpeedScale(ImpactSpeed, LandingHeavyFallSpeed, LandingMaxFallSpeed, LandingMinimumSpeedScale);
    if (Scale < 1.0f)
    {
        Velocity.X *= Scale;
        Velocity.Y *= Scale;
        // BoostedSpeedCeiling is left alone: it is a ceiling, not a floor, so
        // the player does not snap back to it, and clearing it here would
        // quietly change how dash momentum survives a landing.
    }
    bJumpCutArmed = false;

    Super::ProcessLanded(Hit, remainingTime, Iterations);

    if (ImpactSpeed >= LandingHeavyFallSpeed)
    {
        OnLandingImpact.Broadcast(ImpactSpeed);
    }
}

UBreakerEquipmentComponent* UBreakerCharacterMovementComponent::GetEquipment() const
{
    if (!CachedEquipment.IsValid() && GetOwner())
    {
        CachedEquipment = GetOwner()->FindComponentByClass<UBreakerEquipmentComponent>();
    }
    return CachedEquipment.Get();
}

UBreakerProgressionComponent* UBreakerCharacterMovementComponent::GetProgression() const
{
    if (!CachedProgression.IsValid() && GetOwner())
    {
        CachedProgression = GetOwner()->FindComponentByClass<UBreakerProgressionComponent>();
    }
    return CachedProgression.Get();
}

float UBreakerCharacterMovementComponent::GearMoveSpeedMultiplier() const
{
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    const UBreakerProgressionComponent* Progression = GetProgression();
    return (Equipment ? Equipment->GetStats().MoveSpeedMultiplier : 1.0f)
        * (Progression ? Progression->GetMoveSpeedMultiplier() : 1.0f);
}

float UBreakerCharacterMovementComponent::GearSlideSpeedMultiplier() const
{
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    const UBreakerProgressionComponent* Progression = GetProgression();
    return (Equipment ? Equipment->GetStats().SlideSpeedMultiplier : 1.0f)
        * (Progression ? Progression->GetSlideSpeedMultiplier() : 1.0f);
}

float UBreakerCharacterMovementComponent::GearAirControlMultiplier() const
{
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    const UBreakerProgressionComponent* Progression = GetProgression();
    return (Equipment ? Equipment->GetStats().AirControlMultiplier : 1.0f)
        * (Progression ? Progression->GetAirControlMultiplier() : 1.0f);
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
    const float GroundedCap = (bWantsToSprint ? SprintSpeed : WalkSpeed) * GearMoveSpeedMultiplier() * GetSpeedMultiplier();
    return FMath::Max(GroundedCap, BoostedSpeedCeiling);
}

bool UBreakerCharacterMovementComponent::CanRedirect(float HorizontalSpeed, float MinimumSpeed)
{
    return HorizontalSpeed >= MinimumSpeed && MinimumSpeed > 0.0f;
}

FVector UBreakerCharacterMovementComponent::RedirectHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float MinimumSpeed)
{
    const FVector Heading = Direction.GetSafeNormal2D();
    if (Heading.IsNearlyZero())
    {
        return HorizontalVelocity;
    }
    const float InputSpeed = HorizontalVelocity.Size2D();
    // Floor keeps a redirect from dead-stopping the player; the input speed is
    // the ceiling, so this can never manufacture speed (Master 5.4).
    const float OutputSpeed = FMath::Max(InputSpeed, FMath::Max(MinimumSpeed, 0.0f));
    return FVector(Heading.X * OutputSpeed, Heading.Y * OutputSpeed, 0.0f);
}

bool UBreakerCharacterMovementComponent::TryRedirect(const FVector& Direction)
{
    const FVector Heading = Direction.GetSafeNormal2D();
    if (Heading.IsNearlyZero())
    {
        return false;
    }
    // The redirect floor is the walk speed as it stands right now, gear and
    // tree multipliers included, so the same call reads the same threshold the
    // rest of the movement layer uses.
    const float MinimumSpeed = WalkSpeed * GearMoveSpeedMultiplier() * GetSpeedMultiplier();
    const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
    if (!CanRedirect(Horizontal.Size(), MinimumSpeed))
    {
        return false;
    }

    const FVector Redirected = RedirectHorizontalVelocity(Horizontal, Heading, MinimumSpeed);
    Velocity.X = Redirected.X;
    Velocity.Y = Redirected.Y;
    // Vertical velocity is deliberately untouched: Skim is horizontal only, and
    // it must not double as an air-time extender.
    // No dash bookkeeping here on purpose: a redirect neither consumes nor
    // starts the dash cooldown, and it owns no cooldown of its own — the
    // ability pays for it.
    return true;
}

void UBreakerCharacterMovementComponent::PushSpeedMultiplier(FName Key, float Multiplier, float Duration)
{
    if (Key.IsNone() || Multiplier <= 0.0f)
    {
        return;
    }
    FSpeedMultiplierEntry Entry;
    Entry.Multiplier = Multiplier;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    SpeedMultipliers.Add(Key, Entry);
}

void UBreakerCharacterMovementComponent::PopSpeedMultiplier(FName Key)
{
    SpeedMultipliers.Remove(Key);
}

void UBreakerCharacterMovementComponent::PruneSpeedMultipliers() const
{
    const UWorld* World = GetWorld();
    if (!World || SpeedMultipliers.Num() == 0)
    {
        return;
    }
    const double Now = World->GetTimeSeconds();
    for (auto It = SpeedMultipliers.CreateIterator(); It; ++It)
    {
        if (It.Value().ExpiryTime >= 0.0 && It.Value().ExpiryTime <= Now)
        {
            It.RemoveCurrent();
        }
    }
}

float UBreakerCharacterMovementComponent::ComposeSpeedMultipliers(const TArray<float>& Multipliers)
{
    float Composed = 1.0f;
    for (const float Multiplier : Multipliers)
    {
        if (Multiplier > 0.0f)
        {
            Composed *= Multiplier;
        }
    }
    return Composed;
}

float UBreakerCharacterMovementComponent::GetSpeedMultiplier() const
{
    PruneSpeedMultipliers();
    TArray<float> Active;
    Active.Reserve(SpeedMultipliers.Num());
    for (const TPair<FName, FSpeedMultiplierEntry>& Pair : SpeedMultipliers)
    {
        Active.Add(Pair.Value.Multiplier);
    }
    return ComposeSpeedMultipliers(Active);
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
    // The dash owns its vertical floor from here on; releasing jump after an
    // air dash must not cut it.
    bJumpCutArmed = false;
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

    // Variable jump height. The character clears bPressedJump on release (and
    // again when the hold window expires, which is well past the apex, where a
    // cut is a no-op by construction), so one armed jump can be cut at most
    // once and only while it is still rising.
    if (bJumpCutArmed && CharacterOwner && !CharacterOwner->bPressedJump)
    {
        Velocity.Z = ApplyJumpCut(Velocity.Z, JumpCutMultiplier, JumpCutMinimumRiseSpeed);
        bJumpCutArmed = false;
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
    // The wall owns the vertical arc while it lasts.
    bJumpCutArmed = false;
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
