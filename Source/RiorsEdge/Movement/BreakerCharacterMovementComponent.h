#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWallRideStateChanged, bool, bNowWallRiding);
// Broadcast when the character arrives on the ground faster than
// LandingHeavyFallSpeed. Presentation only (camera dip, dust, audio) — the
// weight itself is applied in C++ before this fires.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerLandingImpact, float, ImpactSpeed);
// Broadcast the instant a dash charge is consumed, carrying the horizontal
// direction the dash committed to and the horizontal speed it produced.
// Presentation only, exactly like OnLandingImpact: the velocity change itself
// has already happened in C++ when this fires, so a listener can only dress it
// (camera punch, speed lines, controller rumble) and can never change the
// movement rule. Same shape so a HUD or Blueprint can bind either without the
// movement layer knowing anything about cameras or widgets.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBreakerDashStarted, FVector, DashDirection, float, DashSpeed);

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

    // --- Composed movement stats (the locked one-additive-bucket rule) -----
    // These four used to multiply the gear multiplier by the tree multiplier,
    // which made +20% gear and +20% tree read x1.44 against a rule that says
    // x1.40. They now read the composed ATTRIBUTE, which is where both layers
    // bid into one additive Increased bucket. The attribute set is the single
    // source of truth; the additive fallback below only runs for a movement
    // component with no attribute set at all (a bare test object).
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedMoveSpeedMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedSlideSpeedMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedAirControlMultiplier() const;
    // The movement half of the hip-fire / ADS trade, and the one consumer the
    // weapons layer named at `UBreakerWeaponComponent::GetAimMoveSpeedMultiplier`.
    // Applied to the GROUNDED cap only: sliding and the boosted ceiling keep no
    // opinion, because whether an aimed slide is slowed is a movement-feel
    // ruling nobody has made. Returns exactly 1.0 with no weapon component, so
    // a bare test object and a hip-firing player are bit-identical to before.
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetAimSpeedMultiplier() const;
    // Cooldown SCALE, so 0.80 is a 20% shorter dash cooldown. The attribute
    // stores the reduction (x1.20) because that is the shape an additive
    // bucket can hold; this is its reciprocal, which is what the dash wants.
    UFUNCTION(BlueprintPure, Category="Movement|Stats") float GetComposedDashCooldownMultiplier() const;

    // --- Jump budget (ruling O25) ------------------------------------------
    // Two jumps for everyone, a third for Swift. Recomputed from the PERMANENT
    // class on OnProgressionChanged and polled as a backstop, so a dev class
    // swap can never strand a non-Swift character with three jumps.
    UFUNCTION(BlueprintPure, Category="Movement|Jump") int32 GetGrantedJumpCount() const { return GrantedJumpCount; }
    UFUNCTION(BlueprintCallable, Category="Movement|Jump") void RefreshJumpGrant();

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

    // --- Jump budget (ruling O25) ------------------------------------------
    // "TWO JUMPS are base kit for everyone and Swift innately unlocks a third
    // later — innate to the class, not a tree purchase." Before this the count
    // was a single constant on ABreakerCharacter and the third jump did not
    // exist in any form. The MECHANISM is what this change delivers; the
    // numbers below are placeholders.

    // Base kit, every class, every level. Written onto
    // ACharacter::JumpMaxCount, so this is the one authority on the budget —
    // a Blueprint override of JumpMaxCount is deliberately overwritten, because
    // O25 is a rule and not a per-Blueprint preference.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump", meta=(ClampMin="1")) int32 BaseJumpCount = 2;
    // Master switch. False restores exactly the pre-O25 behaviour (two jumps
    // for everyone including Swift) with no other change.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump") bool bSwiftThirdJumpEnabled = true;
    // O2 PLACEHOLDER — THE THRESHOLD IS AWAITING AN OWNER RULING. CONTEXT.md
    // lists "when it unlocks and whether it is free" as open. 20 and free (no
    // resource cost, no cooldown, no point spend) is a stand-in chosen so the
    // grant is visibly a LATER unlock rather than part of the starting kit; it
    // carries no design authority. What this change delivers is the mechanism:
    // a class-gated, level-gated jump budget that reacts to a class change.
    // NOTE for a playtest: nothing raises CharacterLevel yet, so at the shipped
    // 20 the third jump is unreachable in the gym. Set it to 1 to feel it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump", meta=(ClampMin="1", ClampMax="50")) int32 SwiftThirdJumpUnlockLevel = 20;
    // O2 PLACEHOLDER. The third jump must not read as "the second jump again".
    // It blends horizontal velocity toward the current input direction while
    // PRESERVING its magnitude, so it is a course correction, not a speed
    // source — Swift's identity is redirection (Skim is the same verb) and
    // Master 5.4 forbids self-acceleration. 0 makes the third jump identical
    // to the second; 1 would snap it fully onto input, which reads as a dash.
    // Deliberately restrained: O26 says movement gets no further dedicated
    // passes, so this executes O25 and adds nothing else.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jump", meta=(ClampMin="0", ClampMax="1")) float SwiftThirdJumpRedirectAlpha = 0.55f;

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
    // BUG FIX (owner: "wall riding doesnt work"). This gate is read AFTER the
    // engine has already deflected the approach velocity along the wall, so it
    // must be sized for the along-wall speed that SURVIVES contact, not for the
    // approach speed. At the old 700 it sat exactly ON WalkSpeed — the hard
    // airborne horizontal ceiling when the sprint toggle is off — so a
    // non-sprinting player could never pass it at all, and a sprinting player
    // lost it as soon as the approach angle exceeded ~50 degrees
    // (1100 * cos 50 = 707). 450 restores the invariant every other gate in
    // this component already follows: an entry threshold sits strictly BELOW
    // the speed it gates (slide enters at 550, under the 700 walk speed).
    // Covered by RiorsEdge.Movement.WallRideEntry.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideMinimumSpeed = 450.0f; // OLD: 700.0f (== WalkSpeed, unreachable)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideGravityScale = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideTraceDistance = 85.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpAwaySpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpUpSpeed = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideCooldown = 0.3f;
    // The wall jump keeps its own exit floor. It used to borrow
    // WallRideMinimumSpeed, so lowering the entry gate above would silently
    // have made every wall jump weaker — two different jobs, two values.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride", meta=(ClampMin="0")) float WallRideJumpMinimumSpeed = 700.0f;
    // FEEL, not a bug fix. Two jumps are base kit (O25) and are spent by the
    // time most players reach a wall, so a wall jump used to launch you with
    // nothing left and no way to correct — which is most of what "awkward"
    // describes. A wall jump now hands back one air jump (never more than the
    // O25 baseline: the count is clamped, not cleared), so wall-to-wall
    // traversal is a chain instead of a dead end. False restores the old
    // behaviour exactly.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall Ride") bool bWallJumpRefreshesAirJump = true;

    UPROPERTY(BlueprintAssignable, Category="Movement|Wall Ride") FWallRideStateChanged OnWallRideStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Movement|Weight") FBreakerLandingImpact OnLandingImpact;
    UPROPERTY(BlueprintAssignable, Category="Movement|Dash") FBreakerDashStarted OnDashStarted;

    // Pure rules, exposed for world-free tests.
    // A redirect is legal only when there is real horizontal speed to turn.
    static bool CanRedirect(float HorizontalSpeed, float MinimumSpeed);
    // Rotates HorizontalVelocity onto Direction. The output magnitude is the
    // input magnitude, floored at MinimumSpeed so a redirect never dead-stops,
    // and never above the input magnitude (Master 5.4: no self-acceleration).
    static FVector RedirectHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float MinimumSpeed);
    // Multiplicative composition of the active temporary multipliers.
    static float ComposeSpeedMultipliers(const TArray<float>& Multipliers);

    // The locked aggregation rule, expressed over two 1.0-based layer
    // multipliers that are each already a single additive bucket: the shared
    // bucket is the SUM of the two fractional parts, so +20% gear and +20% tree
    // is x1.40. Multiplying them (x1.44) is the bug this replaces — the same
    // bug class the damage pass fixed when it deleted GearWeaponDamageMultiplier.
    // Only used when there is no attribute set to read from; the composed
    // attribute is the real source of truth and produces the same number.
    static float ComposeAdditiveMultiplier(float LayerA, float LayerB);

    // Jump budget as a pure rule (ruling O25). Two for everyone, three for
    // Swift at or past the unlock level. Swapping AWAY from Swift returns two,
    // which is the case that must never be missed: a dev class swap that left
    // three jumps behind would be a permanent illegal grant.
    static int32 ResolveJumpCount(EBreakerClassId PermanentClass, int32 CharacterLevel, int32 BaseCount, bool bThirdJumpEnabled, int32 UnlockLevel);
    // Rotates HorizontalVelocity partway toward Direction, magnitude preserved
    // exactly. Alpha 0 is a no-op, 1 is a full redirect. A zero or vertical
    // direction leaves the velocity alone.
    static FVector BlendHorizontalVelocity(const FVector& HorizontalVelocity, const FVector& Direction, float Alpha);

    // Every non-spatial half of the wall-ride entry decision, lifted out of
    // TickComponent so it can be tested without a world. Only the wall trace
    // itself stays in the component. This verb broke silently once; the rule
    // now has a name and a regression test (RiorsEdge.Movement.WallRideEntry).
    // HorizontalSpeed is the speed the entry frame actually observes, which —
    // once the capsule has touched the wall — is the ALONG-WALL component the
    // engine's falling deflection leaves behind, not the approach speed.
    static bool CanBeginWallRide(
        bool bAlreadyWallRiding,
        bool bFalling,
        bool bSlidingNow,
        float HorizontalSpeed,
        float MinimumSpeed,
        bool bHasMovementInput,
        float SecondsSinceLastWallRide,
        float Cooldown);

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

    // The composed attribute set is the source of truth for the four movement
    // stats above. Equipment and progression are still resolved, but only as
    // the fallback for a component with no attribute set (a bare test object).
    class UBreakerAttributeSet* GetAttributes() const;
    class UBreakerEquipmentComponent* GetEquipment() const;
    class UBreakerProgressionComponent* GetProgression() const;
    mutable TWeakObjectPtr<class UBreakerAttributeSet> CachedAttributes;
    mutable TWeakObjectPtr<class UBreakerEquipmentComponent> CachedEquipment;
    mutable TWeakObjectPtr<class UBreakerProgressionComponent> CachedProgression;
    // WalkSpeed is authored HERE, not on the attribute set, so the composed
    // MoveSpeed attribute would otherwise be a number close to the walk speed
    // but never equal to it. Published once, as soon as the ability system has
    // actually registered the set (which is after this component's BeginPlay).
    bool bPublishedMoveSpeedBase = false;
    void PublishMoveSpeedBase();

    // O25 jump budget. ObservedClass/ObservedLevel are the poll backstop, in
    // the precedent of UBreakerManaComponent::AdvanceLoop: DevForceClass does
    // broadcast today, but a grant that can outlive its class is exactly the
    // kind of thing that must not depend on one caller remembering to.
    UFUNCTION() void HandleProgressionChanged();
    int32 GrantedJumpCount = 2;
    EBreakerClassId ObservedClass = EBreakerClassId::None;
    int32 ObservedLevel = 0;
    bool bBoundProgression = false;
    // Applies the third jump's course correction, if this jump is one.
    void ApplyBonusJumpRedirect();

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
