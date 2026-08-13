#pragma once

#include "CoreMinimal.h"
#include "BreakerWeaponFeel.generated.h"

/**
 * Weapon feel: recoil, bloom, and viewmodel kick.
 *
 * Everything here is pure maths with no world, no actor, and no timers, so the
 * whole model is unit-testable. The component owns the state; this owns the
 * rules.
 *
 * Two invariants hold for every function below:
 *
 *  1. Recoil moves the AIM. It never moves the bullet relative to the aim.
 *     The trace already follows the controller's view rotation, so kicking
 *     that rotation moves crosshair and bullet together. A weapon that shoots
 *     somewhere other than its crosshair is a bug, not feel.
 *  2. Given the same seed and the same shot index, the pattern is identical.
 *     The random component is a small perturbation on a learnable curve, in
 *     the same spirit as the existing deterministic spread cone.
 *
 * All default values are O2 PLACEHOLDER.
 */
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerRecoilProfile
{
    GENERATED_BODY()

    // ---- Per-shot kick -----------------------------------------------------

    // Upward kick of a single shot, before ramp and ADS scaling. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0"))
    float VerticalKickDegrees = 0.42f;

    // Peak sideways kick. The sign follows a repeating deterministic curve so
    // the pattern is learnable rather than sprayed. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0"))
    float HorizontalKickDegrees = 0.16f;

    // Shots per full left-right cycle of the horizontal pattern. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="1"))
    int32 HorizontalPatternPeriod = 7;

    // Random jitter as a fraction of the vertical kick. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0", ClampMax="1"))
    float VerticalRandomFraction = 0.12f;

    // Absolute random jitter added to the horizontal kick. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0"))
    float HorizontalRandomDegrees = 0.05f;

    // Shots taken to reach the full climb multiplier while held. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0"))
    float ClimbRampShots = 6.0f;

    // Kick multiplier once the ramp is complete. 1.0 disables the ramp.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="1"))
    float ClimbRampMultiplier = 1.7f;

    // Ceiling on how far accumulated recoil may lift the aim. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0"))
    float MaxVerticalDegrees = 7.0f;

    // Ceiling on accumulated horizontal drift, either side. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0"))
    float MaxHorizontalDegrees = 3.0f;

    // Recoil scale while aiming down sights. The main reason to ADS.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil", meta=(ClampMin="0", ClampMax="1"))
    float AimRecoilMultiplier = 0.7f;

    // ---- What ADS costs ----------------------------------------------------
    //
    // Hip fire has to be a real option, not the worse option at every range.
    // ADS still wins everything it used to win; it now PAYS for it twice.
    //
    // 1. Time. The aimed spread, aimed recoil, aimed bloom and aimed viewmodel
    //    all ramp in over AimInSeconds instead of snapping on. Hip fire is the
    //    fast-to-first-shot option; dropping ADS is instant, so the fast option
    //    is always one release away.
    // 2. Mobility. Movement adds cone, and it adds MORE cone while aimed
    //    (AimMoveSpreadMultiplier > 1). Planted and aimed is the accurate
    //    build; moving and aimed is worse than moving and hip firing. That is
    //    the decision: plant to shoot, or move and shoot close.

    // Seconds from pressing aim to the sights being fully worth it.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin="0"))
    float AimInSeconds = 0.20f;

    // Extra cone half-angle at MoveSpreadReferenceSpeed, hip fired.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin="0"))
    float MoveSpreadDegrees = 0.35f;

    // How much harder movement punishes an aimed shot. Above 1.0 by design:
    // this is the mobility half of the ADS bill, and it is what gives hip fire
    // a range band it genuinely owns. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Aim", meta=(ClampMin="0"))
    float AimMoveSpreadMultiplier = 2.2f;

    // ---- Recovery ----------------------------------------------------------

    // Dead time after the last shot before the aim starts settling back.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Recovery", meta=(ClampMin="0"))
    float RecoveryDelaySeconds = 0.08f;

    // Proportional settle: fast at first, slowing as it closes. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Recovery", meta=(ClampMin="0"))
    float RecoveryInterpSpeed = 9.0f;

    // Constant floor on the settle so recovery actually reaches zero instead
    // of asymptoting forever. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Recovery", meta=(ClampMin="0"))
    float RecoveryConstantDegreesPerSecond = 14.0f;

    // Fraction of each kick that is recoverable. Below 1.0 a sliver of every
    // shot is permanent and the player must correct it. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Recovery", meta=(ClampMin="0", ClampMax="1"))
    float RecoveryFraction = 1.0f;

    // ---- First-shot accuracy and bloom -------------------------------------

    // Spread scale on the first shot of a burst. 0 is dead accurate and is the
    // whole point of trigger discipline. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bloom", meta=(ClampMin="0", ClampMax="1"))
    float FirstShotSpreadMultiplier = 0.0f;

    // Trigger-idle time that resets burst index, bloom, and first-shot
    // accuracy. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bloom", meta=(ClampMin="0"))
    float BurstResetSeconds = 0.35f;

    // Extra cone half-angle added per shot fired. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bloom", meta=(ClampMin="0"))
    float BloomPerShotDegrees = 0.09f;

    // Ceiling on accumulated bloom. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bloom", meta=(ClampMin="0"))
    float MaxBloomDegrees = 1.4f;

    // Bloom bled off per second once the trigger is released. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bloom", meta=(ClampMin="0"))
    float BloomRecoveryDegreesPerSecond = 2.2f;

    // Bloom growth scale while aiming down sights. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Bloom", meta=(ClampMin="0", ClampMax="1"))
    float AimBloomMultiplier = 0.55f;

    // ---- Viewmodel ---------------------------------------------------------

    // Instant backward displacement of the weapon mesh per shot, in
    // centimetres. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="0"))
    float ViewmodelKickUnits = 3.2f;

    // Sideways component of that displacement; follows the horizontal recoil
    // sign so mesh and aim agree. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="0"))
    float ViewmodelKickLateralUnits = 0.9f;

    // Muzzle-up rotation of the weapon mesh per shot. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="0"))
    float ViewmodelKickPitchDegrees = 2.4f;

    // Ceilings so sustained fire cannot walk the mesh off the screen.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="0"))
    float MaxViewmodelKickUnits = 9.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="0"))
    float MaxViewmodelKickPitchDegrees = 7.0f;

    // Spring that pulls the mesh back to rest. Damping below
    // 2*sqrt(Stiffness) overshoots slightly, which reads as snap.
    // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="1"))
    float ViewmodelSpringStiffness = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="0"))
    float ViewmodelSpringDamping = 26.0f;

    // Viewmodel kick scale while aiming down sights: a sighted weapon must
    // stay readable through the sight. O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recoil|Viewmodel", meta=(ClampMin="0", ClampMax="1"))
    float AimViewmodelMultiplier = 0.45f;
};

/** A single shot's kick, in degrees of control rotation. */
struct FBreakerRecoilKick
{
    float PitchDegrees = 0.0f;
    float YawDegrees = 0.0f;
};

/** Springy offset of the placeholder weapon mesh, in camera-relative space. */
struct FBreakerViewmodelState
{
    float BackOffset = 0.0f;      // centimetres along -X (toward the player)
    float BackVelocity = 0.0f;
    float LateralOffset = 0.0f;   // centimetres along Y
    float LateralVelocity = 0.0f;
    float PitchOffset = 0.0f;     // degrees, muzzle up
    float PitchVelocity = 0.0f;

    bool IsAtRest() const
    {
        return BackOffset == 0.0f && LateralOffset == 0.0f && PitchOffset == 0.0f
            && BackVelocity == 0.0f && LateralVelocity == 0.0f && PitchVelocity == 0.0f;
    }
};

class RIORSEDGE_API FBreakerWeaponFeel
{
public:
    /**
     * The kick a single shot wants to apply, before clamping.
     * BurstShotIndex is 0 for the first shot of a burst and increments while
     * the trigger stays effectively held.
     */
    static FBreakerRecoilKick ComputeShotKick(const FBreakerRecoilProfile& Profile, int32 BurstShotIndex, int32 RandomSeed, bool bAiming);

    /**
     * Folds a kick into the accumulators, clamped to the profile ceilings, and
     * returns the delta actually applied. Callers must apply exactly the
     * returned delta to the control rotation: applying anything else is what
     * makes a weapon shoot away from its crosshair.
     */
    static FBreakerRecoilKick AccumulateKick(const FBreakerRecoilProfile& Profile, const FBreakerRecoilKick& Kick, float& InOutPitch, float& InOutYaw);

    /** One settle step toward zero. Never overshoots past zero. */
    static float RecoverAxis(const FBreakerRecoilProfile& Profile, float Accumulated, float DeltaSeconds);

    /**
     * Player-compensation credit: manual aim movement that opposes the
     * accumulated recoil consumes the recovery budget instead of being
     * undone by it. Without this, pulling down mid-burst gets punished by the
     * recovery pushing the view further down afterwards.
     */
    static float ConsumeCompensation(float Accumulated, float PlayerDelta);

    /** Bloom after a shot is fired. */
    static float BloomAfterShot(const FBreakerRecoilProfile& Profile, float CurrentBloom, bool bAiming);

    /** Bloom after DeltaSeconds of not firing. */
    static float BloomAfterTime(const FBreakerRecoilProfile& Profile, float CurrentBloom, float DeltaSeconds);

    /**
     * Cone half-angle for this shot: the definition's hip/aim spread, scaled
     * by first-shot accuracy on shot 0 and widened by bloom thereafter.
     *
     * ExtraMovementSpread is added on TOP of both branches, including the
     * first shot. That is deliberate: first-shot accuracy is the reward for
     * trigger discipline, not for running, and a moving player must not get a
     * free perfect shot just because they let the trigger rest.
     */
    static float EffectiveSpreadDegrees(const FBreakerRecoilProfile& Profile, float BaseSpreadDegrees, float CurrentBloom, int32 BurstShotIndex, float ExtraMovementSpread = 0.0f);

    /**
     * Extra cone from being in motion. SpeedFraction is the owner's ground
     * speed over the reference speed, clamped to [0,1]; AimAlpha is how far
     * into ADS the weapon is. Aimed movement costs AimMoveSpreadMultiplier
     * times as much, which is the entire reason hip fire exists.
     */
    static float MovementSpreadDegrees(const FBreakerRecoilProfile& Profile, float SpeedFraction, float AimAlpha);

    /**
     * The profile as it stands partway into ADS. Every aim benefit is
     * interpolated from its hip value (1.0, i.e. no benefit) toward its
     * authored aimed value, so snapping to sights and firing immediately gets
     * a fraction of the sights. Alpha 0 returns hip behaviour exactly; alpha 1
     * returns the authored profile unchanged.
     */
    static FBreakerRecoilProfile ProfileAtAimAlpha(const FBreakerRecoilProfile& Profile, float AimAlpha);

    /** Displaces the viewmodel instantly; the spring returns it. */
    static void AddViewmodelKick(const FBreakerRecoilProfile& Profile, FBreakerViewmodelState& State, float HorizontalKickSign, bool bAiming);

    /** Integrates the viewmodel spring back toward rest. */
    static void IntegrateViewmodel(const FBreakerRecoilProfile& Profile, FBreakerViewmodelState& State, float DeltaSeconds);
};
