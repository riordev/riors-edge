#include "Movement/BreakerCharacterMovementComponent.h"

#include "Attributes/BreakerAttributeSet.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Progression/BreakerProgressionComponent.h"
#include "Weapons/BreakerWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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
    // AUTHORED, not inherited (Part One-V): the silent step is the vault's
    // floor, and a number this load-bearing arriving as an engine default is
    // the collision-profile defect's third appearance. The kerb walks (45 and
    // under, no input, no awareness); the crate vaults (50 and up, a chosen
    // verb that costs 0.12s). LedgeMinimumHeightCm sits strictly above this,
    // and the LedgeVerbs test pins the relationship.
    MaxStepHeight = 45.0f;   // O2 PLACEHOLDER
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

    // Dev speed-trace instrument. The capture harness cannot press movement
    // keys, so a -BreakerMoveTrace session drives itself through a scripted
    // sprint / dash / hard-turn / slide-jump run and prints a rate-limited
    // speed line; the session log is the deliverable, not the screen.
    bMoveTraceArmed = FParse::Param(FCommandLine::Get(), TEXT("BreakerMoveTrace"));
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
    // The project uses standard downward gravity everywhere (dash and slide
    // both reason in world Z), so the phase is read off Z directly.
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

float UBreakerCharacterMovementComponent::GetWalkSpeedCap() const
{
    return WalkSpeed * GetComposedMoveSpeedMultiplier() * GetSpeedMultiplier() * GetAimSpeedMultiplier();
}

float UBreakerCharacterMovementComponent::GetSprintSpeedCap() const
{
    return SprintSpeed * GetComposedMoveSpeedMultiplier() * GetSpeedMultiplier() * GetAimSpeedMultiplier();
}

float UBreakerCharacterMovementComponent::GetGroundedSpeedCap() const
{
    return bWantsToSprint ? GetSprintSpeedCap() : GetWalkSpeedCap();
}

float UBreakerCharacterMovementComponent::GetMaxSpeed() const
{
    if (bSliding)
    {
        return FMath::Max(SprintSpeed * GetComposedSlideSpeedMultiplier(), Velocity.Size2D());
    }
    return FMath::Max(GetGroundedSpeedCap(), BoostedSpeedCeiling);
}

float UBreakerCharacterMovementComponent::BoostedCeilingBleedRate(float Ceiling, float RestingCap, float WindowSeconds)
{
    const float Gap = Ceiling - RestingCap;
    if (Gap <= 0.0f)
    {
        return 0.0f;
    }
    if (WindowSeconds <= 0.0f)
    {
        // A zero window is the old behaviour — the cut happens in one step —
        // encoded as a rate no frame can pay only partially.
        return TNumericLimits<float>::Max();
    }
    return Gap / WindowSeconds;
}

float UBreakerCharacterMovementComponent::StepBoostedCeilingBleed(float Ceiling, float RestingCap, float RatePerSecond, float DeltaTime)
{
    const float Bled = Ceiling - RatePerSecond * FMath::Max(DeltaTime, 0.0f);
    // At or below the resting cap the ceiling stops meaning anything; 0 is
    // the existing "no boost" sentinel and GetMaxSpeed's max() takes over.
    return Bled > RestingCap ? Bled : 0.0f;
}

FVector UBreakerCharacterMovementComponent::SlideJumpConservedVelocity(const FVector& HorizontalVelocity, float ConservationFraction)
{
    return FVector(HorizontalVelocity.X, HorizontalVelocity.Y, 0.0f)
        * FMath::Clamp(ConservationFraction, 0.0f, 1.0f);
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

    // The DashDistance node lane (ORDERS ruling 1 — Swift's granted
    // Longstride, LEDGER's single-bidder lane): multiplies the whole composed
    // impulse BEFORE the hard cap, so "dash carries further" is what the
    // player reads and the cap still has the last word at high Momentum. Read
    // per press, not per tick; a pawn with no progression component (an
    // enemy, a test rig) reads x1.0 and is bit-identical to before.
    const UBreakerProgressionComponent* DashProgression = GetProgression();
    const float DashDistanceMultiplier = DashProgression
        ? FMath::Max(0.0f, DashProgression->GetNodeStats().DashDistanceMultiplier) : 1.0f;
    const float OutputSpeed = FMath::Min(
        (FMath::Max(Velocity.Size2D(), DashSpeedFloor) + DashSpeedBonus) * DashDistanceMultiplier, MomentumHardCap);
    Velocity.X = Direction.X * OutputSpeed;
    Velocity.Y = Direction.Y * OutputSpeed;
    Velocity.Z = FMath::Max(Velocity.Z, DashVerticalFloor);
    LastDashTime = World->GetTimeSeconds();
    BoostedSpeedCeiling = OutputSpeed;
    // A fresh grant is a fresh boost: any bleed left over from an earlier
    // break must not carry its rate onto this one.
    BoostedCeilingBleedRatePerSecond = 0.0f;
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

    // D1(b), owner-ruled: the slide-jump conserves SlideJumpSpeedConservation
    // of the slide's horizontal speed, not all of it. Full conservation made
    // slide-jump strictly better than the vault at the vault's own crate —
    // the recon's finding — and the toll is what buys the verb choice back.
    const FVector Conserved = SlideJumpConservedVelocity(
        FVector(Velocity.X, Velocity.Y, 0.0f), SlideJumpSpeedConservation);
    EndSlide();
    SetSprinting(true);
    BoostedSpeedCeiling = FMath::Max(BoostedSpeedCeiling, static_cast<float>(Conserved.Size()));
    BoostedCeilingBleedRatePerSecond = 0.0f;
    Velocity.X = Conserved.X;
    Velocity.Y = Conserved.Y;
}

void UBreakerCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // The attribute set is registered with the ability system after this
    // component's BeginPlay, so the base it cannot know until then is published
    // here. Costs one branch per frame after the first success.
    PublishMoveSpeedBase();

    // Before Super, so the scripted input this frame feeds is consumed by
    // this frame's movement. Inert without -BreakerMoveTrace.
    if (bMoveTraceArmed)
    {
        TickMoveTrace();
    }

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
        // D1(a), owner-ruled: the boost BLEEDS instead of vanishing. The
        // bleed LATCHES at the break — a nonzero rate IS the latch — because
        // most breaks are one-frame events (a wall clip fires the slow
        // condition exactly once) and an unlatched bleed re-armed the
        // surviving ceiling on the next calm frame, which turned "your boost
        // ends gracefully" into "a wall costs you almost nothing"; measured
        // in the first AFTER trace, 2040 -> 2021 -> held. Once latched, the
        // ceiling glides linearly to the resting cap over AboveCapDecaySeconds
        // and the engine's own over-max braking walks the velocity down it
        // (braking never goes below MaxSpeed, and the ceiling IS MaxSpeed).
        // Only a fresh grant — TryDash, PrepareSlideJump — re-arms the boost.
        const bool bBoostBroken = bMovementReleased || bCollisionSlowed || bCollisionRedirected;
        const float RestingCap = GetGroundedSpeedCap();
        if (bBoostBroken && BoostedCeilingBleedRatePerSecond <= 0.0f)
        {
            if (BoostedSpeedCeiling <= RestingCap)
            {
                // Nothing above the cap to bleed: the old sentinel, no felt
                // difference, and the block stops running for free.
                BoostedSpeedCeiling = 0.0f;
            }
            else
            {
                BoostedCeilingBleedRatePerSecond = BoostedCeilingBleedRate(BoostedSpeedCeiling, RestingCap, AboveCapDecaySeconds);
            }
        }
        if (BoostedCeilingBleedRatePerSecond > 0.0f)
        {
            BoostedSpeedCeiling = StepBoostedCeilingBleed(BoostedSpeedCeiling, RestingCap, BoostedCeilingBleedRatePerSecond, DeltaTime);
            if (BoostedSpeedCeiling <= 0.0f)
            {
                BoostedCeilingBleedRatePerSecond = 0.0f;
            }
        }
        else if (BoostedSpeedCeiling > 0.0f)
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

void UBreakerCharacterMovementComponent::TickMoveTrace()
{
    UWorld* World = GetWorld();
    if (!World || !CharacterOwner || !CharacterOwner->IsPlayerControlled())
    {
        return;
    }
    const double Now = World->GetTimeSeconds();
    if (MoveTraceStartTime < 0.0)
    {
        MoveTraceStartTime = Now;
        // The first attempt sprinted the spawn's facing into a wall 26 m out
        // and spent the whole script pinned in a corner, so the instrument
        // picks its own ground: an 8-way horizontal probe, run once, longest
        // clear ray wins. The turn goes to whichever perpendicular is opener.
        const FVector Chest = CharacterOwner->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
        const FVector SpawnForward = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
        FCollisionQueryParams ProbeParams(SCENE_QUERY_STAT(BreakerMoveTraceProbe), false, CharacterOwner);
        float BestDistance = -1.0f;
        float BestWallScore = -1.0f;
        FVector WallNormal = FVector::ZeroVector;
        float WallDistance = 0.0f;
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const FVector Candidate = SpawnForward.RotateAngleAxis(45.0f * Index, FVector::UpVector);
            FHitResult ProbeHit;
            const bool bHitWall = World->LineTraceSingleByChannel(ProbeHit, Chest, Chest + Candidate * 9000.0f, ECC_Visibility, ProbeParams);
            const float Distance = bHitWall ? static_cast<float>(ProbeHit.Distance) : 9000.0f;
            if (Distance > BestDistance)
            {
                BestDistance = Distance;
                MoveTraceForward = Candidate;
            }
            // The glancing-collision leg wants a wall with a run-up: nearest
            // to 2500 cm wins. A wall hit is what makes the (a) reading
            // DETERMINISTIC — a smooth input turn fires the break conditions
            // only on frame-time jitter, a collision fires them every run.
            if (bHitWall && Distance > 1200.0f)
            {
                const float Score = 1.0f / (1.0f + FMath::Abs(Distance - 2500.0f));
                if (Score > BestWallScore)
                {
                    BestWallScore = Score;
                    WallNormal = ProbeHit.ImpactNormal.GetSafeNormal2D();
                    WallDistance = Distance;
                }
            }
        }
        // Aim 55 degrees off the wall's inward normal: enough incidence to
        // fire the collision-slow break, enough tangent that real speed
        // survives to show what happens to it.
        MoveTraceGlanceDir = !WallNormal.IsNearlyZero()
            ? (-WallNormal).RotateAngleAxis(55.0f, FVector::UpVector).GetSafeNormal2D()
            : MoveTraceForward;
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerMoveTrace] ARMED walk=%.0f sprint=%.0f hardcap=%.0f bleedwindow=%.2fs slidejumpconserve=%.2f openlane=%.0fcm wall=%.0fcm glance=(%.2f,%.2f)"),
            WalkSpeed, SprintSpeed, MomentumHardCap, AboveCapDecaySeconds, SlideJumpSpeedConservation,
            BestDistance, WallDistance, MoveTraceGlanceDir.X, MoveTraceGlanceDir.Y);
    }
    const double T = Now - MoveTraceStartTime;

    // The script, two legs. GLANCE LEG (the (a) reading): sprint at the
    // probed wall 55 degrees off its normal, dash at 2.0, hold input through
    // the impact — the glancing hit fires the collision-slow break with real
    // tangential speed left over, which is exactly the speed the old code
    // confiscated in one frame and the bleed now pays out over the window.
    // OPEN LEG (the (b) reading): at 6.0 swing onto the open lane, dash at
    // 6.8, slide at 7.5, slide-jump at 7.9 — in/out prints the conservation.
    const bool bHold = T >= 1.0 && T < 10.0;
    if (bHold)
    {
        CharacterOwner->AddMovementInput(T < 6.0 ? MoveTraceGlanceDir : MoveTraceForward, 1.0f);
        SetSprinting(true);
    }
    if (MoveTraceStep == 0 && T >= 2.0)
    {
        MoveTraceStep = 1;
        UE_LOG(LogTemp, Display, TEXT("[BreakerMoveTrace] t=%.2f EVENT dash-glance -> %d"), T, TryDash(MoveTraceGlanceDir) ? 1 : 0);
    }
    else if (MoveTraceStep == 1 && T >= 6.0)
    {
        MoveTraceStep = 2;
        UE_LOG(LogTemp, Display, TEXT("[BreakerMoveTrace] t=%.2f EVENT open-leg (input swings to the open lane)"), T);
    }
    else if (MoveTraceStep == 2 && T >= 6.8)
    {
        MoveTraceStep = 3;
        UE_LOG(LogTemp, Display, TEXT("[BreakerMoveTrace] t=%.2f EVENT dash-open -> %d"), T, TryDash(MoveTraceForward) ? 1 : 0);
    }
    else if (MoveTraceStep == 3 && T >= 7.5)
    {
        MoveTraceStep = 4;
        SetSlideRequested(true);
        UE_LOG(LogTemp, Display, TEXT("[BreakerMoveTrace] t=%.2f EVENT slide -> %d"), T, BeginSlide() ? 1 : 0);
    }
    else if (MoveTraceStep == 4 && T >= 7.9)
    {
        MoveTraceStep = 5;
        const float SpeedIntoJump = Velocity.Size2D();
        // The pawn's own slide-jump sequence (HandleJumpInput), driven here
        // because no key can be pressed: conserve, then launch.
        PrepareSlideJump();
        SetSlideRequested(false);
        CharacterOwner->LaunchCharacter(FVector(0.0f, 0.0f, JumpZVelocity), false, true);
        UE_LOG(LogTemp, Display, TEXT("[BreakerMoveTrace] t=%.2f EVENT slide-jump in=%.1f out=%.1f"), T, SpeedIntoJump, Velocity.Size2D());
    }

    // The rate-limited speed line, 10 Hz.
    if (Now - LastMoveTraceLogTime >= 0.1)
    {
        LastMoveTraceLogTime = Now;
        UE_LOG(LogTemp, Display,
            TEXT("[BreakerMoveTrace] t=%.2f speed=%.1f vz=%.1f ceiling=%.1f cap=%.1f mode=%d sliding=%d"),
            T, Velocity.Size2D(), Velocity.Z, BoostedSpeedCeiling, GetGroundedSpeedCap(),
            static_cast<int32>(MovementMode.GetValue()), bSliding ? 1 : 0);
    }
}

void UBreakerCharacterMovementComponent::ApplyAirSteering(float DeltaTime)
{
    if (!IsFalling())
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


void UBreakerCharacterMovementComponent::NotifyLedgeTraversalCompleted()
{
    LastLedgeTraversalEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

// ---- The traversal's own movement mode (the custom prediction pass) -------

float UBreakerCharacterMovementComponent::LedgeTraversalAlpha(float ElapsedSeconds, float DurationSeconds)
{
    // The 0.05 floor is the old pawn execution's own guard, kept exactly.
    return FMath::Clamp(ElapsedSeconds / FMath::Max(DurationSeconds, 0.05f), 0.0f, 1.0f);
}

FVector UBreakerCharacterMovementComponent::LedgeTraversalLocation(const FVector& Start, const FVector& Target, float Alpha)
{
    const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
    const float Smoothed = A * A * (3.0f - 2.0f * A);

    // TWO-PHASE PATH (found by photographing the glide, not by reading it):
    // the old straight start-to-target lerp passes through the ledge's
    // front-top corner, so a swept capsule ABORTS every standing mantle
    // against a solid face — the only traversals that ever completed were
    // resolved mid-fall from near or above the top, which is exactly the
    // flakiness the recon measured ("feet sweep the band while falling").
    // The path now RISES first, alongside the face, then CROSSES over the
    // top; a descending resolve (from above the ledge) mirrors it — cross
    // high first, settle after. Same clock, same smoothstep, same
    // durations: only the path's shape changed, and only so the verb can
    // actually finish what it resolved.
    const float RiseDist = FMath::Max(Target.Z - Start.Z, 0.0f);
    const FVector Flat(Target.X - Start.X, Target.Y - Start.Y, 0.0f);
    const float FlatDist = Flat.Size();
    const float DropDist = FMath::Max(Start.Z - Target.Z, 0.0f);
    const float PathLength = RiseDist + FlatDist + DropDist;
    if (PathLength <= UE_KINDA_SMALL_NUMBER)
    {
        return FMath::Lerp(Start, Target, Smoothed);
    }
    const float RiseFraction = RiseDist / PathLength;
    const float CrossFraction = FlatDist / PathLength;
    if (Smoothed <= RiseFraction)
    {
        const float RiseAlpha = FMath::Clamp(Smoothed / FMath::Max(RiseFraction, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
        return FVector(Start.X, Start.Y, FMath::Lerp(Start.Z, Target.Z, RiseAlpha));
    }
    if (Smoothed <= RiseFraction + CrossFraction)
    {
        const float CrossAlpha = FMath::Clamp((Smoothed - RiseFraction) / FMath::Max(CrossFraction, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
        const float CrossZ = RiseDist > 0.0f ? Target.Z : Start.Z;
        return FVector(Start.X + Flat.X * CrossAlpha, Start.Y + Flat.Y * CrossAlpha, CrossZ);
    }
    const float DropAlpha = FMath::Clamp(
        (Smoothed - RiseFraction - CrossFraction) / FMath::Max(1.0f - RiseFraction - CrossFraction, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
    return FVector(Target.X, Target.Y, FMath::Lerp(Start.Z, Target.Z, DropAlpha));
}

float UBreakerCharacterMovementComponent::LedgeTraversalStrideSpeed(float DistanceCm, float DurationSeconds, float Alpha)
{
    if (DurationSeconds <= 0.0f || DistanceCm <= 0.0f)
    {
        return 0.0f;
    }
    const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
    // d/dt of smoothstep(t/T) scaled by distance: 6a(1-a) x distance / T.
    // Zero at both ends, 1.5x the average speed at the middle of the glide.
    return DistanceCm * 6.0f * A * (1.0f - A) / DurationSeconds;
}

float UBreakerCharacterMovementComponent::GetLedgeTraversalStrideSpeed() const
{
    if (!IsTraversingLedge())
    {
        return 0.0f;
    }
    // The path is L-shaped (rise, cross, settle), so the honest distance is
    // the path's, not the chord's.
    const float PathLength = FMath::Abs(TraversalTarget.Z - TraversalStart.Z)
        + static_cast<float>(FVector::Dist2D(TraversalTarget, TraversalStart));
    return LedgeTraversalStrideSpeed(
        PathLength,
        TraversalDuration,
        LedgeTraversalAlpha(TraversalElapsed, TraversalDuration));
}

bool UBreakerCharacterMovementComponent::TryBeginLedgeTraversal()
{
    if (IsTraversingLedge())
    {
        return false;
    }
    FBreakerLedgeTraversal Traversal;
    if (!ResolveLedgeTraversal(Traversal))
    {
        return false;
    }
    // Request, not begin: the begin runs inside the movement update so the
    // same one-shot replays through the saved-move stream. The resolved copy
    // is kept so the initiating machine begins from exactly what it resolved.
    PendingTraversal = Traversal;
    bHasPendingTraversal = true;
    bWantsLedgeTraversal = true;
    return true;
}

void UBreakerCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    if (bWantsLedgeTraversal && !IsTraversingLedge())
    {
        // The server (and a replaying client) has the FLAG but no resolved
        // copy — it re-resolves from its own authoritative pose, which is the
        // whole point of consuming the request inside the movement update.
        FBreakerLedgeTraversal Traversal = PendingTraversal;
        const bool bResolved = bHasPendingTraversal || ResolveLedgeTraversal(Traversal);
        if (bResolved)
        {
            BeginLedgeTraversal(Traversal);
        }
    }
    bWantsLedgeTraversal = false;
    bHasPendingTraversal = false;
}

void UBreakerCharacterMovementComponent::BeginLedgeTraversal(const FBreakerLedgeTraversal& Traversal)
{
    if (Traversal.Verb == EBreakerLedgeVerb::None || !UpdatedComponent)
    {
        return;
    }
    TraversalVerb = Traversal.Verb;
    TraversalStart = UpdatedComponent->GetComponentLocation();
    TraversalTarget = Traversal.TargetLocation;
    TraversalElapsed = 0.0f;
    const float BaseDuration = Traversal.Verb == EBreakerLedgeVerb::Vault
        ? VaultDurationSeconds : MantleDurationSeconds;
    TraversalDuration = FMath::Max(BaseDuration * FMath::Max(DevTraversalDurationScale, 0.01f), 0.05f);
    // Horizontal momentum is carried across the glide and paid back on a
    // completed exit; the glide itself runs on zero velocity so nothing
    // downstream reads phantom speed (the stride read above is the honest
    // speed source for the viewmodel).
    TraversalExitVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
    Velocity = FVector::ZeroVector;
    SetMovementMode(MOVE_Custom, CustomModeLedgeTraversal);
}

void UBreakerCharacterMovementComponent::FinishLedgeTraversal(bool bCompleted)
{
    const EBreakerLedgeVerb CompletedVerb = TraversalVerb;
    TraversalVerb = EBreakerLedgeVerb::None;
    SetMovementMode(MOVE_Falling);
    // The old execution's exact exit rule: a completed glide keeps the
    // carried horizontal velocity, a blocked abort exits dead.
    Velocity = bCompleted ? TraversalExitVelocity : FVector::ZeroVector;
    if (bCompleted)
    {
        // Only a COMPLETED traversal records (Part One-T) or broadcasts — a
        // blocked abort granted nothing a recency window or an exit dip
        // should pay for.
        NotifyLedgeTraversalCompleted();
        OnLedgeTraversalCompleted.Broadcast(CompletedVerb);
    }
}

void UBreakerCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
    if (CustomMovementMode == CustomModeLedgeTraversal)
    {
        PhysLedgeTraversal(deltaTime, Iterations);
        return;
    }
    Super::PhysCustom(deltaTime, Iterations);
}

void UBreakerCharacterMovementComponent::PhysLedgeTraversal(float DeltaTime, int32 Iterations)
{
    if (DeltaTime < UE_KINDA_SMALL_NUMBER)
    {
        return;
    }
    if (!CharacterOwner || !UpdatedComponent)
    {
        FinishLedgeTraversal(false);
        return;
    }
    TraversalElapsed += DeltaTime;
    const float Alpha = LedgeTraversalAlpha(TraversalElapsed, TraversalDuration);
    const FVector Desired = LedgeTraversalLocation(TraversalStart, TraversalTarget, Alpha);
    FHitResult Hit;
    SafeMoveUpdatedComponent(Desired - UpdatedComponent->GetComponentLocation(),
        UpdatedComponent->GetComponentQuat(), true, Hit);
    if (Hit.bBlockingHit)
    {
        FinishLedgeTraversal(false);
    }
    else if (Alpha >= 1.0f)
    {
        FinishLedgeTraversal(true);
    }
}

FNetworkPredictionData_Client* UBreakerCharacterMovementComponent::GetPredictionData_Client() const
{
    if (!ClientPredictionData)
    {
        UBreakerCharacterMovementComponent* MutableThis = const_cast<UBreakerCharacterMovementComponent*>(this);
        MutableThis->ClientPredictionData = new FBreakerNetworkPredictionData_Client_Character(*this);
    }
    return ClientPredictionData;
}

void UBreakerCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    // Server and replay only — the autonomous client consumed its own copy
    // directly, and this path is what makes the two consume the SAME move.
    bWantsLedgeTraversal = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void FBreakerSavedMove_Character::Clear()
{
    Super::Clear();
    bSavedWantsLedgeTraversal = false;
    SavedTraversalStart = FVector::ZeroVector;
    SavedTraversalTarget = FVector::ZeroVector;
    SavedTraversalExitVelocity = FVector::ZeroVector;
    SavedTraversalElapsed = 0.0f;
    SavedTraversalDuration = 0.2f;
    SavedTraversalVerb = EBreakerLedgeVerb::None;
}

uint8 FBreakerSavedMove_Character::GetCompressedFlags() const
{
    uint8 Flags = Super::GetCompressedFlags();
    if (bSavedWantsLedgeTraversal)
    {
        Flags |= FLAG_Custom_0;
    }
    return Flags;
}

bool FBreakerSavedMove_Character::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
    const FBreakerSavedMove_Character* Other = static_cast<const FBreakerSavedMove_Character*>(NewMove.Get());
    // A one-shot request must reach the server as its own move, and a move
    // that starts mid-glide carries traversal state a combined move would
    // silently drop.
    if (bSavedWantsLedgeTraversal != Other->bSavedWantsLedgeTraversal)
    {
        return false;
    }
    if (SavedTraversalVerb != EBreakerLedgeVerb::None || Other->SavedTraversalVerb != EBreakerLedgeVerb::None)
    {
        return false;
    }
    return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FBreakerSavedMove_Character::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
    if (const UBreakerCharacterMovementComponent* Movement = C ? Cast<UBreakerCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
    {
        bSavedWantsLedgeTraversal = Movement->bWantsLedgeTraversal;
        SavedTraversalStart = Movement->TraversalStart;
        SavedTraversalTarget = Movement->TraversalTarget;
        SavedTraversalExitVelocity = Movement->TraversalExitVelocity;
        SavedTraversalElapsed = Movement->TraversalElapsed;
        SavedTraversalDuration = Movement->TraversalDuration;
        SavedTraversalVerb = Movement->TraversalVerb;
    }
}

void FBreakerSavedMove_Character::PrepMoveFor(ACharacter* C)
{
    Super::PrepMoveFor(C);
    if (UBreakerCharacterMovementComponent* Movement = C ? Cast<UBreakerCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
    {
        Movement->TraversalStart = SavedTraversalStart;
        Movement->TraversalTarget = SavedTraversalTarget;
        Movement->TraversalExitVelocity = SavedTraversalExitVelocity;
        Movement->TraversalElapsed = SavedTraversalElapsed;
        Movement->TraversalDuration = SavedTraversalDuration;
        Movement->TraversalVerb = SavedTraversalVerb;
    }
}

bool UBreakerCharacterMovementComponent::IsMantleableWallNormal(float ImpactNormalZ)
{
    // Near-vertical: the same 0.25-adjacent band FindRunnableWall uses, a
    // shade looser because a mantle approaches head-on rather than alongside.
    return FMath::Abs(ImpactNormalZ) <= 0.35f;
}

bool UBreakerCharacterMovementComponent::IsStandableTopNormal(float ImpactNormalZ)
{
    // Near-flat: anything steeper is a slope the engine's own walkable-floor
    // rule should own, not a ledge to pop onto.
    return ImpactNormalZ >= 0.65f;
}

EBreakerLedgeVerb UBreakerCharacterMovementComponent::ResolveLedgeVerb(float LedgeHeightCm, float MinimumCm, float VaultMaximumCm, float MantleMaximumCm)
{
    // Band comparisons biased by the published epsilon (D4): a measured
    // height within half a centimetre of an exact edge resolves
    // DETERMINISTICALLY to the side that keeps the ledge actionable, instead
    // of letting float noise in the trace pick the verb.
    if (LedgeHeightCm < MinimumCm - LedgeBandEpsilonCm || LedgeHeightCm > MantleMaximumCm + LedgeBandEpsilonCm)
    {
        return EBreakerLedgeVerb::None;
    }
    return LedgeHeightCm <= VaultMaximumCm + LedgeBandEpsilonCm ? EBreakerLedgeVerb::Vault : EBreakerLedgeVerb::Mantle;
}

bool UBreakerCharacterMovementComponent::ResolveLedgeTraversal(FBreakerLedgeTraversal& OutTraversal) const
{
    // The pawn's old TryMantle trace body, verbatim in shape: a forward wall
    // probe, a downward top probe, the height band, and a capsule clearance
    // test at the landing point — now with every rule a named, tested
    // predicate and every number authored once, here.
    const ACharacter* Owner = CharacterOwner.Get();
    UWorld* World = GetWorld();
    const UCapsuleComponent* Capsule = Owner ? Owner->GetCapsuleComponent() : nullptr;
    if (!World || !Capsule)
    {
        return false;
    }

    const FVector Up = FVector::UpVector;
    const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
    const FVector ActorLocation = Owner->GetActorLocation();
    const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
    const FVector FeetLocation = ActorLocation - Up * CapsuleHalfHeight;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakerLedgeTraversal), false, Owner);

    FHitResult WallHit;
    const FVector WallTraceStart = ActorLocation + Up * 15.0f;
    if (!World->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceStart + Forward * MantleReachCm, ECC_Visibility, Params)
        || !IsMantleableWallNormal(WallHit.ImpactNormal.Z))
    {
        return false;
    }

    FHitResult TopHit;
    const FVector TopProbe = WallHit.ImpactPoint + Forward * (CapsuleRadius + 12.0f) + Up * MantleMaximumHeightCm;
    if (!World->LineTraceSingleByChannel(TopHit, TopProbe, TopProbe - Up * (MantleMaximumHeightCm + 25.0f), ECC_Visibility, Params)
        || !IsStandableTopNormal(TopHit.ImpactNormal.Z))
    {
        return false;
    }

    const float LedgeHeight = TopHit.ImpactPoint.Z - FeetLocation.Z;
    const EBreakerLedgeVerb Verb = ResolveLedgeVerb(LedgeHeight, LedgeMinimumHeightCm, VaultMaximumHeightCm, MantleMaximumHeightCm);
    if (Verb == EBreakerLedgeVerb::None)
    {
        return false;
    }

    const FVector Target = TopHit.ImpactPoint + Up * (CapsuleHalfHeight + 3.0f) + Forward * 18.0f;
    if (World->OverlapBlockingTestByChannel(Target, FQuat::Identity, Capsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), Params))
    {
        return false;
    }

    OutTraversal.Verb = Verb;
    OutTraversal.TargetLocation = Target;
    return true;
}
