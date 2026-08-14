#include "Movement/BreakerCharacterMovementComponent.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
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
    // THAT NEXT STEP HAS NOW BEEN TAKEN: a fourth report ("gravity needs to be
    // tuned down just a little bit") moved FallGravityMultiplier 1.80 -> 1.55
    // and left this value alone. Do not chase it with GravityScale as well —
    // 1.38 is already a hair above the 1.35 the project started at, and moving
    // two dials for one report makes the fifth report unattributable.
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

    // O25. Binds the progression delegate and resolves the starting budget;
    // the tick poll re-runs it if the progression component was not up yet.
    RefreshJumpGrant();
}

UBreakerAttributeSet* UBreakerCharacterMovementComponent::GetAttributes() const
{
    if (!CachedAttributes.IsValid())
    {
        if (const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner()))
        {
            if (UAbilitySystemComponent* ASC = AbilityOwner->GetAbilitySystemComponent())
            {
                CachedAttributes = const_cast<UBreakerAttributeSet*>(ASC->GetSet<UBreakerAttributeSet>());
            }
        }
    }
    return CachedAttributes.Get();
}

void UBreakerCharacterMovementComponent::PublishMoveSpeedBase()
{
    if (bPublishedMoveSpeedBase) return;
    // Deliberately NOT done in BeginPlay: the attribute set is registered with
    // the ability system in ABreakerCharacter::BeginPlay, which can run after
    // this component's, so a one-shot there would silently find nothing. Polled
    // from the tick until it lands, then never again.
    UBreakerAttributeSet* Attributes = GetAttributes();
    if (!Attributes) return;
    Attributes->SetAggregatedAttributeBase(EBreakerAggregatedAttribute::MoveSpeed, WalkSpeed);
    bPublishedMoveSpeedBase = true;
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

    // ACharacter::CheckJumpInput increments JumpCurrentCount AFTER DoJump
    // returns, so the jump about to be taken is index JumpCurrentCount + 1.
    // Two spent already means this one is the third — Swift's, by construction,
    // because nobody else is ever granted a third.
    const bool bIsBonusJump = CharacterOwner && CharacterOwner->JumpCurrentCount >= FMath::Max(BaseJumpCount, 1);

    const bool bJumped = Super::DoJump(bReplayingMoves, DeltaTime);
    if (bJumped)
    {
        bJumpCutArmed = true;
        if (bIsBonusJump)
        {
            ApplyBonusJumpRedirect();
        }
    }
    return bJumped;
}

void UBreakerCharacterMovementComponent::ApplyBonusJumpRedirect()
{
    // The third jump must not read as a repeat of the second. It buys a course
    // correction, not altitude and not speed: horizontal velocity rotates
    // partway onto the current input direction with its magnitude preserved
    // exactly. With no input it is a plain jump that keeps every bit of the
    // momentum it arrived with, which is the "preserved momentum" half.
    if (SwiftThirdJumpRedirectAlpha <= 0.0f)
    {
        return;
    }
    const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
    const FVector Blended = BlendHorizontalVelocity(Horizontal, Acceleration.GetSafeNormal2D(), SwiftThirdJumpRedirectAlpha);
    Velocity.X = Blended.X;
    Velocity.Y = Blended.Y;
    // Vertical velocity is Super's business. Touching it here would turn a
    // course correction into a height buff, which O26 puts firmly out of scope.
}

FVector UBreakerCharacterMovementComponent::BlendHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float Alpha)
{
    const FVector Heading = Direction.GetSafeNormal2D();
    const FVector Current = FVector(HorizontalVelocity.X, HorizontalVelocity.Y, 0.0f);
    const float Speed = Current.Size();
    if (Heading.IsNearlyZero() || Speed <= UE_KINDA_SMALL_NUMBER)
    {
        return Current;
    }
    const float Blend = FMath::Clamp(Alpha, 0.0f, 1.0f);
    const FVector Steered = FMath::Lerp(Current.GetSafeNormal(), Heading, Blend).GetSafeNormal2D();
    // A perfect 180 with alpha 0.5 lerps to the zero vector and normalizes to
    // nothing; keeping the original heading is the honest degenerate answer,
    // and it is still speed-preserving.
    if (Steered.IsNearlyZero())
    {
        return Current;
    }
    // Magnitude is re-applied from the INPUT, never from the blend, so this can
    // never manufacture speed (Master 5.4).
    return Steered * Speed;
}

int32 UBreakerCharacterMovementComponent::ResolveJumpCount(EBreakerClassId PermanentClass, int32 CharacterLevel, int32 BaseCount, bool bThirdJumpEnabled, int32 UnlockLevel)
{
    const int32 Base = FMath::Max(1, BaseCount);
    if (!bThirdJumpEnabled || PermanentClass != EBreakerClassId::Swift)
    {
        return Base;
    }
    return CharacterLevel >= UnlockLevel ? Base + 1 : Base;
}

void UBreakerCharacterMovementComponent::HandleProgressionChanged()
{
    RefreshJumpGrant();
}

void UBreakerCharacterMovementComponent::RefreshJumpGrant()
{
    UBreakerProgressionComponent* Progression = GetProgression();
    // Binding here rather than only in BeginPlay: component BeginPlay order is
    // not guaranteed, so the progression component may not exist yet when this
    // one starts. The tick poll calls back in, and the bind lands then.
    if (Progression && !bBoundProgression)
    {
        Progression->OnProgressionChanged.AddDynamic(this, &UBreakerCharacterMovementComponent::HandleProgressionChanged);
        bBoundProgression = true;
    }
    ObservedClass = Progression ? Progression->GetProgressionState().PermanentClass : EBreakerClassId::None;
    ObservedLevel = Progression ? Progression->GetProgressionState().CharacterLevel : 0;
    const int32 PreviousGrant = GrantedJumpCount;
    GrantedJumpCount = ResolveJumpCount(ObservedClass, ObservedLevel, BaseJumpCount, bSwiftThirdJumpEnabled, SwiftThirdJumpUnlockLevel);
    // Say the budget out loud whenever it changes. The third jump was
    // unreachable for its whole life and left NO trace anywhere — no warning,
    // no log line, no failing test — so the only instrument was a player
    // noticing they could not do it. A class swap now prints what it granted.
    if (GrantedJumpCount != PreviousGrant)
    {
        UE_LOG(LogTemp, Display, TEXT("[BreakerMovement] jump budget %d -> %d (class %d, level %d, O25 base %d, Swift unlock %d)"),
            PreviousGrant, GrantedJumpCount, static_cast<int32>(ObservedClass), ObservedLevel,
            BaseJumpCount, SwiftThirdJumpUnlockLevel);
    }

    // The failure this exists for: the third jump shipped gated at level 20
    // while nothing in the project writes CharacterLevel, so a Swift player
    // could never reach it and the only report was "i never could do a 3rd
    // jump". A gate above the level the game can actually reach is a DISABLED
    // feature, and it must say so out loud rather than be discovered by
    // playing. One shot per component; harmless in the reachable case.
    if (!bWarnedUnreachableThirdJump && bSwiftThirdJumpEnabled &&
        ObservedClass == EBreakerClassId::Swift && ObservedLevel < SwiftThirdJumpUnlockLevel)
    {
        bWarnedUnreachableThirdJump = true;
        UE_LOG(LogTemp, Warning,
            TEXT("[BreakerMovement] Swift's third jump (O25) is gated at character level %d and this character is level %d. ")
            TEXT("Nothing raises CharacterLevel yet — there is no XP loop — so this grant is UNREACHABLE, not merely locked. ")
            TEXT("Set SwiftThirdJumpUnlockLevel to 1 until an XP loop exists."),
            SwiftThirdJumpUnlockLevel, ObservedLevel);
    }

    if (!CharacterOwner)
    {
        return;
    }
    CharacterOwner->JumpMaxCount = GrantedJumpCount;
    // A swap AWAY from Swift mid-air must not leave a jump already banked
    // against a budget the character no longer has. Clamping the spent count
    // is the whole reason this is a refresh rather than a one-time grant.
    CharacterOwner->JumpCurrentCount = FMath::Min(CharacterOwner->JumpCurrentCount, GrantedJumpCount);
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

float UBreakerCharacterMovementComponent::ComposeAdditiveMultiplier(float LayerA, float LayerB)
{
    // Each layer hands over a 1.0-based multiplier that is ITSELF already one
    // additive bucket, so merging the two buckets is adding the fractional
    // parts: (1 + a) and (1 + b) become (1 + a + b). Floored at zero so a
    // pathological pair of debuffs reverses nobody's direction of travel.
    return FMath::Max(0.0f, LayerA + LayerB - 1.0f);
}

float UBreakerCharacterMovementComponent::GetComposedMoveSpeedMultiplier() const
{
    if (const UBreakerAttributeSet* Attributes = GetAttributes())
    {
        // The attribute holds a SPEED in cm/s (its base is this component's
        // WalkSpeed, published in PublishMoveSpeedBase). What the rest of the
        // movement layer wants is the ratio, because sprint, the redirect floor
        // and the boosted ceiling all scale off their own authored speeds.
        const float Base = Attributes->GetAttributeBase(EBreakerAggregatedAttribute::MoveSpeed);
        if (Base > UE_KINDA_SMALL_NUMBER)
        {
            return FMath::Max(0.0f, Attributes->GetMoveSpeed() / Base);
        }
    }
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    const UBreakerProgressionComponent* Progression = GetProgression();
    return ComposeAdditiveMultiplier(
        Equipment ? Equipment->GetStats().MoveSpeedMultiplier : 1.0f,
        Progression ? Progression->GetMoveSpeedMultiplier() : 1.0f);
}

float UBreakerCharacterMovementComponent::GetComposedSlideSpeedMultiplier() const
{
    if (const UBreakerAttributeSet* Attributes = GetAttributes())
    {
        return FMath::Max(0.0f, Attributes->GetSlideSpeedMultiplier());
    }
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    const UBreakerProgressionComponent* Progression = GetProgression();
    return ComposeAdditiveMultiplier(
        Equipment ? Equipment->GetStats().SlideSpeedMultiplier : 1.0f,
        Progression ? Progression->GetSlideSpeedMultiplier() : 1.0f);
}

float UBreakerCharacterMovementComponent::GetComposedAirControlMultiplier() const
{
    if (const UBreakerAttributeSet* Attributes = GetAttributes())
    {
        return FMath::Max(0.0f, Attributes->GetAirControlMultiplier());
    }
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    const UBreakerProgressionComponent* Progression = GetProgression();
    return ComposeAdditiveMultiplier(
        Equipment ? Equipment->GetStats().AirControlMultiplier : 1.0f,
        Progression ? Progression->GetAirControlMultiplier() : 1.0f);
}

float UBreakerCharacterMovementComponent::GetComposedDashCooldownMultiplier() const
{
    if (const UBreakerAttributeSet* Attributes = GetAttributes())
    {
        // The attribute is the REDUCTION and shares the additive bucket in that
        // shape; the dash wants the scale, which is its reciprocal. The
        // attribute set floors the reduction, so this cannot divide by zero.
        return 1.0f / FMath::Max(0.05f, Attributes->GetDashCooldownReduction());
    }
    const UBreakerEquipmentComponent* Equipment = GetEquipment();
    // No tree layer to merge: no node stat target authors dash cooldown yet, so
    // gear's own single bucket IS the composed bucket.
    return Equipment ? Equipment->GetStats().DashCooldownMultiplier : 1.0f;
}

float UBreakerCharacterMovementComponent::GetMaxSpeed() const
{
    if (bSliding)
    {
        return FMath::Max(SprintSpeed * GetComposedSlideSpeedMultiplier(), Velocity.Size2D());
    }
    const float GroundedCap = (bWantsToSprint ? SprintSpeed : WalkSpeed)
        * GetComposedMoveSpeedMultiplier() * GetSpeedMultiplier() * GetAimSpeedMultiplier();
    return FMath::Max(GroundedCap, BoostedSpeedCeiling);
}

float UBreakerCharacterMovementComponent::GetAimSpeedMultiplier() const
{
    // The movement half of the hip-fire / ADS trade. The weapons layer authors
    // the penalty per archetype and composes it against live aim progress, so
    // everything about HOW MUCH and HOW FAST belongs over there; this is the
    // single consumer that side asked for, and it deliberately holds no state
    // and no opinion of its own.
    //
    // Read through the owning pawn rather than cached, because the player
    // swaps weapons mid-movement and a cached component would apply the
    // outgoing archetype's penalty until something invalidated it.
    const AActor* Owner = GetOwner();
    if (!Owner) return 1.0f;
    const UBreakerWeaponComponent* Weapon = Owner->FindComponentByClass<UBreakerWeaponComponent>();
    if (!Weapon) return 1.0f;

    // Clamped at 1.0 on this side as well as the weapon's. Two independent
    // clamps on the same invariant is not redundancy worth deleting: an
    // archetype authored above 1.0 by mistake would otherwise turn aiming into
    // a speed BUFF, which inverts the whole trade rather than merely
    // mistuning it.
    return FMath::Clamp(Weapon->GetAimMoveSpeedMultiplier(), 0.0f, 1.0f);
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
    const float MinimumSpeed = WalkSpeed * GetComposedMoveSpeedMultiplier() * GetSpeedMultiplier();
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

bool UBreakerCharacterMovementComponent::IsSuspensionActive(double ExpiryTime, double Now)
{
    return ExpiryTime < 0.0 || ExpiryTime > Now;
}

void UBreakerCharacterMovementComponent::PushDashCooldownSuspension(FName Key, float Duration)
{
    if (Key.IsNone())
    {
        return;
    }
    FSuspensionEntry Entry;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    DashCooldownSuspensions.Add(Key, Entry);
}

void UBreakerCharacterMovementComponent::PopDashCooldownSuspension(FName Key)
{
    DashCooldownSuspensions.Remove(Key);
}

void UBreakerCharacterMovementComponent::PruneDashCooldownSuspensions() const
{
    const UWorld* World = GetWorld();
    if (!World || DashCooldownSuspensions.Num() == 0)
    {
        return;
    }
    const double Now = World->GetTimeSeconds();
    for (auto It = DashCooldownSuspensions.CreateIterator(); It; ++It)
    {
        if (!IsSuspensionActive(It.Value().ExpiryTime, Now))
        {
            It.RemoveCurrent();
        }
    }
}

bool UBreakerCharacterMovementComponent::IsDashCooldownSuspended() const
{
    PruneDashCooldownSuspensions();
    return DashCooldownSuspensions.Num() > 0;
}

void UBreakerCharacterMovementComponent::PushWallRideTimerSuspension(FName Key, float Duration)
{
    if (Key.IsNone())
    {
        return;
    }
    FSuspensionEntry Entry;
    const UWorld* World = GetWorld();
    Entry.ExpiryTime = (Duration > 0.0f && World) ? World->GetTimeSeconds() + Duration : -1.0;
    WallRideTimerSuspensions.Add(Key, Entry);
}

void UBreakerCharacterMovementComponent::PopWallRideTimerSuspension(FName Key)
{
    WallRideTimerSuspensions.Remove(Key);
}

void UBreakerCharacterMovementComponent::PruneWallRideTimerSuspensions() const
{
    const UWorld* World = GetWorld();
    if (!World || WallRideTimerSuspensions.Num() == 0)
    {
        return;
    }
    const double Now = World->GetTimeSeconds();
    for (auto It = WallRideTimerSuspensions.CreateIterator(); It; ++It)
    {
        if (!IsSuspensionActive(It.Value().ExpiryTime, Now))
        {
            It.RemoveCurrent();
        }
    }
}

bool UBreakerCharacterMovementComponent::IsWallRideTimerSuspended() const
{
    PruneWallRideTimerSuspensions();
    return WallRideTimerSuspensions.Num() > 0;
}

bool UBreakerCharacterMovementComponent::CanBeginWallRide(
    bool bAlreadyWallRiding,
    bool bFalling,
    bool bSlidingNow,
    float HorizontalSpeed,
    float MinimumSpeed,
    bool bHasMovementInput,
    float SecondsSinceLastWallRide,
    float Cooldown)
{
    if (bAlreadyWallRiding || !bFalling || bSlidingNow || !bHasMovementInput)
    {
        return false;
    }
    if (SecondsSinceLastWallRide < Cooldown)
    {
        return false;
    }
    // Strictly-greater on the threshold would make an exactly-at-cap approach a
    // coin flip against float rounding, which is half of how this verb died.
    return HorizontalSpeed >= MinimumSpeed;
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
    // bSliding is an unconditional gate: Terminal Velocity is an AVAILABILITY
    // rewrite of the cooldown term only (Class-Kits.md:192), so a suspended
    // cooldown must never let a dash through a slide it could not pass
    // otherwise. GetComposedDashCooldownMultiplier() keeps composing normally
    // either way — the equipment scale it returns is a separate lane of the
    // same stat and is untouched by the suspension.
    if (!World || bSliding)
    {
        return false;
    }
    const bool bCooldownReady = IsDashCooldownSuspended()
        || (World->GetTimeSeconds() - LastDashTime >= DashCooldown * GetComposedDashCooldownMultiplier());
    if (!bCooldownReady)
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
    // Owner report: "cant really feel it or see it since your speed just jumps
    // up". The rule is right and the READ is missing, so the fix is entirely on
    // the presentation side. Broadcast last, after the velocity is committed,
    // so a listener can never observe a half-applied dash.
    OnDashStarted.Broadcast(Direction, OutputSpeed);
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
    // The attribute set is registered with the ability system after this
    // component's BeginPlay, so the base it cannot know until then is published
    // here. Costs one branch per frame after the first success.
    PublishMoveSpeedBase();

    // O25 poll backstop, in the precedent of UBreakerManaComponent::AdvanceLoop.
    // DevForceClass DOES broadcast OnProgressionChanged today — that was
    // checked, not assumed — but an illegal third jump that outlives its class
    // is exactly the failure that must not depend on every future writer of the
    // progression state remembering to broadcast. Two integer comparisons.
    if (const UBreakerProgressionComponent* Progression = GetProgression())
    {
        const FBreakerProgressionState& LiveState = Progression->GetProgressionState();
        if (LiveState.PermanentClass != ObservedClass || LiveState.CharacterLevel != ObservedLevel)
        {
            RefreshJumpGrant();
        }
    }

    FHitResult RunnableWall;
    const float SecondsSinceLastWallRide = GetWorld()
        ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastWallRideEndTime)
        : 0.0f;
    if (GetWorld()
        && CanBeginWallRide(bWallRiding, IsFalling(), bSliding, Velocity.Size2D(), WallRideMinimumSpeed,
            Acceleration.SizeSquared2D() > UE_KINDA_SMALL_NUMBER, SecondsSinceLastWallRide, WallRideCooldown)
        && FindRunnableWall(RunnableWall))
    {
        BeginWallRide(RunnableWall);
    }

    if (bWallRiding)
    {
        WallRideElapsed += DeltaTime;
        // Terminal Velocity removes only the DURATION timer (Class-Kits.md:192).
        // Everything else that can end a ride — leaving the ground, losing the
        // wall — is untouched, so the elapsed clock keeps running (for
        // whatever reads it later) but its expiry check alone is suspended.
        if (IsMovingOnGround() || (!IsWallRideTimerSuspended() && WallRideElapsed >= WallRideMaxDuration) || !FindRunnableWall(RunnableWall))
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
        const float BoostRoom = FMath::Max(0.0f, (SprintSpeed + SlideEntryBoost) * GetComposedSlideSpeedMultiplier() - Velocity.Size2D());
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

    const float SteerRate = AirSteerRate * GetComposedAirControlMultiplier() * (0.35f + 0.65f * FMath::Max(Alignment, 0.0f));
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
    // Its own floor, not the entry gate's: the two used to share a value, so
    // lowering the entry gate would have quietly weakened every wall jump.
    const float PreservedSpeed = FMath::Max(AlongWall.Size2D(), WallRideJumpMinimumSpeed);
    Velocity = AlongWall.GetSafeNormal2D() * PreservedSpeed
        + WallRideNormal * WallRideJumpAwaySpeed
        + FVector::UpVector * WallRideJumpUpSpeed;
    // Wall jump and the O25 second jump share one key and used to compete for
    // the same budget: by the time a player reached a wall both jumps were
    // usually spent, so the wall jump threw them off the wall with no way to
    // correct. Clamping (never clearing) the count hands back exactly one air
    // jump, so a wall jump chains instead of stranding, and the baseline is
    // still two.
    if (bWallJumpRefreshesAirJump && CharacterOwner)
    {
        CharacterOwner->JumpCurrentCount = FMath::Min(CharacterOwner->JumpCurrentCount, FMath::Max(CharacterOwner->JumpMaxCount - 1, 0));
    }
    EndWallRide();
    return true;
}
