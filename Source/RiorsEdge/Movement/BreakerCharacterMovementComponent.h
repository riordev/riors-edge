#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BreakerCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWallRideStateChanged, bool, bNowWallRiding);
// Broadcast when the character arrives on the ground faster than
// LandingHeavyFallSpeed. Presentation only (camera dip, dust, audio) — the
// weight itself is applied in C++ before this fires.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerLandingImpact, float, ImpactSpeed);

UCLASS(ClassGroup=Movement, BlueprintType)
class RIORSEDGE_API UBreakerCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UBreakerCharacterMovementComponent();

    virtual float GetMaxSpeed() const override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // Weight hooks. All three are engine extension points, so the heavier fall
    // curve is integrated inside the movement sub-steps rather than patched on
    // afterwards, and it therefore survives variable frame rates and the
    // client-prediction replay path unchanged.
    virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
    virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
    virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

    UFUNCTION(BlueprintCallable, Category="Movement") void SetSprinting(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Movement") void SetSlideRequested(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryDash(const FVector& RequestedDirection);
    // Ability-Implementation-Spec §4.3: rotates existing horizontal velocity onto
    // Direction with NO magnitude gain. Skim's verb. It owns no cooldown of its
    // own — the ability pays cost and cooldown — and it never consumes the dash
    // charge. Returns false when there is not enough horizontal speed to
    // redirect (below walk speed), so a standing player cannot use it as a
    // free reposition.
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryRedirect(const FVector& Direction);
    // Tag/key-keyed temporary speed multiplier, composed multiplicatively with
    // the gear and tree multipliers (Ability-Implementation-Spec §6, the shape
    // Gunsmith's Disruptor and Swift's Overdrive both need). Lazily expired:
    // no timers, no tick cost when nothing is pushed.
    UFUNCTION(BlueprintCallable, Category="Movement") void PushSpeedMultiplier(FName Key, float Multiplier, float Duration);
    UFUNCTION(BlueprintCallable, Category="Movement") void PopSpeedMultiplier(FName Key);
    UFUNCTION(BlueprintPure, Category="Movement") float GetSpeedMultiplier() const;
    UFUNCTION(BlueprintCallable, Category="Movement") bool BeginSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") void PrepareSlideJump();
    UFUNCTION(BlueprintCallable, Category="Movement") void EndSlide();
    UFUNCTION(BlueprintCallable, Category="Movement") bool TryWallJump();
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSprinting() const { return bWantsToSprint; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSliding() const { return bSliding; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsSlideRequested() const { return bSlideRequested; }
    UFUNCTION(BlueprintPure, Category="Movement") bool IsWallRiding() const { return bWallRiding; }
    UFUNCTION(BlueprintPure, Category="Movement") FVector GetWallRideNormal() const { return WallRideNormal; }
    UFUNCTION(BlueprintPure, Category="Movement") float GetHorizontalSpeed() const { return Velocity.Size2D(); }
    // World time of the last consumed dash charge; class loops watch it to
    // credit a dash exactly once.
    UFUNCTION(BlueprintPure, Category="Movement") float GetLastDashTime() const { return static_cast<float>(LastDashTime); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float WalkSpeed = 700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grounded Movement", meta=(ClampMin="0")) float SprintSpeed = 1100.0f;

    // --- Weight (owner report: "movement should be less floaty") ---------
    // Everything in this category is new; the OLD behaviour is "no curve at
    // all", i.e. a symmetric arc under a flat GravityScale. Set
    // FallGravityMultiplier and ApexGravityMultiplier to 1.0, MaxFallSpeed to
    // 0, JumpCutMultiplier to 1.0 and LandingMinimumSpeedScale to 1.0 and the
    // component behaves exactly as it did before this pass.

    // Gravity multiplier applied once the character is falling faster than
    // ApexBandSpeed downward. A jump that falls faster than it rises is the
    // single strongest "this body has mass" cue; a symmetric arc is the
    // classic floaty read. OLD: 1.0 (no fall multiplier existed).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0.1")) float FallGravityMultiplier = 1.80f;
    // Gravity multiplier exactly at the apex, where vertical velocity is near
    // zero and hang time is felt directly. Blended into 1.0 on the way up and
    // into FallGravityMultiplier on the way down, so the curve is continuous.
    // OLD: 1.0 (no apex treatment existed).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0.1")) float ApexGravityMultiplier = 1.50f;
    // Half-width of the apex band, in cm/s of vertical speed. OLD: n/a.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="1")) float ApexBandSpeed = 220.0f;
    // Terminal velocity, so the heavier fall curve cannot turn a long drop into
    // a bullet. Binds well before the physics volume's 4000. 0 disables.
    // OLD: 0 (only the volume's 4000 cm/s applied).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float MaxFallSpeed = 2400.0f;
    // Variable jump height: releasing jump while still rising scales the
    // remaining rise by this. Authority over the arc reads as control rather
    // than drift. 1.0 restores the old fixed-height jump. OLD: 1.0.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0", ClampMax="1")) float JumpCutMultiplier = 0.55f;
    // Below this rise speed a cut is not worth the discontinuity. OLD: n/a.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float JumpCutMinimumRiseSpeed = 50.0f;
    // Written onto ACharacter::JumpMaxHoldTime at BeginPlay. The engine clears
    // bPressedJump one frame after the press when this is 0, which leaves the
    // movement layer with no way to see a release; this window keeps the flag
    // alive long enough to detect one. It must comfortably exceed the rise
    // time (~0.45 s) or a held jump would read as a release and self-cut. The
    // engine's own hold-to-rise is suppressed in DoJump. OLD: 0 (engine
    // default; the character never authored it).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float JumpHoldWindow = 0.60f;
    // Landing impact. Below LandingHeavyFallSpeed a landing costs nothing (a
    // full-height jump lands at about 920 cm/s, so ordinary jumping is never
    // taxed); from there to LandingMaxFallSpeed the horizontal speed kept on
    // arrival ramps down to LandingMinimumSpeedScale. A queued slide owns its
    // own landing and is exempt. OLD: no landing behaviour whatsoever.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0")) float LandingHeavyFallSpeed = 950.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="1")) float LandingMaxFallSpeed = 2400.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weight", meta=(ClampMin="0", ClampMax="1")) float LandingMinimumSpeedScale = 0.78f;

    // Forgiving Source-style air steering: turns existing momentum toward
    // input without increasing its magnitude or allowing a free reversal.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Air Movement", meta=(ClampMin="0")) float AirSteerRate = 4.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Air Movement", meta=(ClampMin="-1", ClampMax="1")) float AirSteerMinimumAlignment = -0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedFloor = 1500.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashSpeedBonus = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashVerticalFloor = 80.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float DashCooldown = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dash", meta=(ClampMin="0")) float MomentumHardCap = 4200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntrySpeed = 550.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntryBoost = 120.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideEntryBoostDuration = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideBoostCooldown = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideExitSpeed = 450.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideGroundFriction = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideBrakingDeceleration = 350.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideSlopeAcceleration = 900.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float SlideMaxDuration = 1.0f;

    // Short wall ride: preserves traversal flow without generating speed or
    // replacing combat.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideMaxDuration = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideMinimumSpeed = 700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideGravityScale = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideTraceDistance = 85.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpAwaySpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpUpSpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideCooldown = 0.3f;

    UPROPERTY(BlueprintAssignable, Category="Movement|Wall Ride") FWallRideStateChanged OnWallRideStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Movement|Weight") FBreakerLandingImpact OnLandingImpact;

    // Pure rules, exposed for world-free tests.
    // A redirect is legal only when there is real horizontal speed to turn.
    static bool CanRedirect(float HorizontalSpeed, float MinimumSpeed);
    // Rotates HorizontalVelocity onto Direction. The output magnitude is the
    // input magnitude, floored at MinimumSpeed so a redirect never dead-stops,
    // and never above the input magnitude (Master 5.4: no self-acceleration).
    static FVector RedirectHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float MinimumSpeed);
    // Multiplicative composition of the active temporary multipliers.
    static float ComposeSpeedMultipliers(const TArray<float>& Multipliers);

    // --- Weight rules, pure maths so they can be tested without a world ---
    // Gravity multiplier as a continuous function of vertical velocity:
    // 1.0 while rising outside the apex band, ApexMultiplier at zero vertical
    // velocity, FallMultiplier once falling outside the band, linear between.
    // Continuity matters: a step change in gravity at the apex is visible as a
    // hitch in the camera.
    static float ComputeGravityMultiplier(float VerticalVelocity, float ApexBand, float ApexMultiplier, float FallMultiplier);
    // Terminal velocity, applied downward only; rising velocity is never
    // touched (a dash or wall jump must keep its impulse).
    static float ClampFallSpeed(float VerticalVelocity, float MaxDownwardSpeed);
    // Variable jump height. Only a rise is cut, so this can never accelerate a
    // fall and can never turn a descent into a boost.
    static float ApplyJumpCut(float VerticalVelocity, float CutMultiplier, float MinimumRiseSpeed);
    // Fraction of horizontal speed kept on a landing of the given downward
    // impact speed. 1.0 up to HeavyFallSpeed, then a linear ramp to
    // MinimumScale at MaxFallSpeed, clamped.
    static float LandingSpeedScale(float ImpactSpeed, float HeavyFallSpeed, float MaxImpactSpeed, float MinimumScale);

private:
    struct FSpeedMultiplierEntry
    {
        float Multiplier = 1.0f;
        // Negative = no expiry; popped explicitly.
        double ExpiryTime = -1.0;
    };
    // Mutable: GetMaxSpeed() is const and is the natural place to drop expired
    // entries, which is what "lazy expiry" means here.
    mutable TMap<FName, FSpeedMultiplierEntry> SpeedMultipliers;
    void PruneSpeedMultipliers() const;

    // Gear-rolled movement multipliers, read from the owner's equipment
    // component: move/slide speed, air control, dash cooldown.
    class UBreakerEquipmentComponent* GetEquipment() const;
    float GearMoveSpeedMultiplier() const;
    float GearSlideSpeedMultiplier() const;
    float GearAirControlMultiplier() const;
    float GearDashCooldownMultiplier() const;
    mutable TWeakObjectPtr<class UBreakerEquipmentComponent> CachedEquipment;
    // Tree-node movement multipliers compose multiplicatively with gear.
    class UBreakerProgressionComponent* GetProgression() const;
    mutable TWeakObjectPtr<class UBreakerProgressionComponent> CachedProgression;

    bool bWantsToSprint = false;
    bool bSlideRequested = false;
    bool bSlideRequestConsumed = false;
    bool bSliding = false;
    double LastDashTime = -1000.0;
    double LastSlideBoostTime = -1000.0;
    float BoostedSpeedCeiling = 0.0f;
    float SavedGroundFriction = 0.0f;
    float SavedBrakingDeceleration = 0.0f;
    float SlideElapsed = 0.0f;
    float SlideEntryBoostRemaining = 0.0f;
    // True between a real jump impulse and the moment the cut is spent (or the
    // jump ends). Only DoJump arms it, which is what keeps the wall jump, the
    // slide jump and the dash's vertical floor — none of which run through the
    // jump state machine — from being cut by a stray key release.
    bool bJumpCutArmed = false;
    bool bWallRiding = false;
    FVector WallRideNormal = FVector::ZeroVector;
    float WallRideElapsed = 0.0f;
    float SavedGravityScale = 1.0f;
    double LastWallRideEndTime = -1000.0;

    bool FindRunnableWall(FHitResult& OutHit) const;
    void ApplyAirSteering(float DeltaTime);
    void BeginWallRide(const FHitResult& WallHit);
    void EndWallRide();
};
