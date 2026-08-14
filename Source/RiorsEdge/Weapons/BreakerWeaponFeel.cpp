#include "Weapons/BreakerWeaponFeel.h"

namespace BreakerRecoil
{
    // Salts the recoil stream away from the spread and bleed streams so the
    // jitter never correlates with where the pellet went.
    constexpr uint32 Salt = 0x5EC01100u;

    // Fixed substep so the viewmodel spring behaves the same at 30 and 240 fps.
    constexpr float SpringSubstep = 1.0f / 120.0f;
    constexpr int32 MaxSpringSubsteps = 8;

    float ClimbRamp(const FBreakerRecoilProfile& Profile, int32 BurstShotIndex)
    {
        if (Profile.ClimbRampShots <= 0.0f) return Profile.ClimbRampMultiplier;
        const float Alpha = FMath::Clamp(static_cast<float>(FMath::Max(0, BurstShotIndex)) / Profile.ClimbRampShots, 0.0f, 1.0f);
        return FMath::Lerp(1.0f, FMath::Max(1.0f, Profile.ClimbRampMultiplier), Alpha);
    }
}

FBreakerRecoilKick FBreakerWeaponFeel::ComputeShotKick(const FBreakerRecoilProfile& Profile, int32 BurstShotIndex, int32 RandomSeed, bool bAiming)
{
    FBreakerRecoilKick Kick;
    const int32 ShotIndex = FMath::Max(0, BurstShotIndex);
    const float Ramp = BreakerRecoil::ClimbRamp(Profile, ShotIndex);
    const float AimScale = bAiming ? FMath::Clamp(Profile.AimRecoilMultiplier, 0.0f, 1.0f) : 1.0f;

    // The learnable half of the pattern. A sine over the pattern period means
    // shot 0 goes straight up and the drift sweeps one way then the other, so
    // a player who counter-traces the curve holds the crosshair still.
    const int32 Period = FMath::Max(1, Profile.HorizontalPatternPeriod);
    const float Phase = (static_cast<float>(ShotIndex % Period) / static_cast<float>(Period)) * 2.0f * UE_PI;
    const float PatternYaw = Profile.HorizontalKickDegrees * FMath::Sin(Phase);

    // The small unlearnable half. Deterministic in the seed, exactly like the
    // existing spread cone, so it is reproducible on the server and testable.
    FRandomStream Stream(static_cast<int32>(HashCombine(static_cast<uint32>(RandomSeed) ^ BreakerRecoil::Salt, static_cast<uint32>(ShotIndex))));
    const float VerticalJitter = 1.0f + Stream.FRandRange(-Profile.VerticalRandomFraction, Profile.VerticalRandomFraction);
    const float HorizontalJitter = Stream.FRandRange(-Profile.HorizontalRandomDegrees, Profile.HorizontalRandomDegrees);

    Kick.PitchDegrees = Profile.VerticalKickDegrees * Ramp * VerticalJitter * AimScale;
    Kick.YawDegrees = (PatternYaw * Ramp + HorizontalJitter) * AimScale;
    return Kick;
}

FBreakerRecoilKick FBreakerWeaponFeel::AccumulateKick(const FBreakerRecoilProfile& Profile, const FBreakerRecoilKick& Kick, float& InOutPitch, float& InOutYaw)
{
    FBreakerRecoilKick Applied;

    const float DesiredPitch = FMath::Clamp(InOutPitch + Kick.PitchDegrees, -Profile.MaxVerticalDegrees, Profile.MaxVerticalDegrees);
    Applied.PitchDegrees = DesiredPitch - InOutPitch;
    InOutPitch = DesiredPitch;

    const float DesiredYaw = FMath::Clamp(InOutYaw + Kick.YawDegrees, -Profile.MaxHorizontalDegrees, Profile.MaxHorizontalDegrees);
    Applied.YawDegrees = DesiredYaw - InOutYaw;
    InOutYaw = DesiredYaw;

    // Only the recoverable fraction of THIS kick stays in the settle budget.
    // The rest has already moved the aim and stays there for the player to
    // correct. Subtracting per-kick rather than scaling the whole accumulator
    // keeps the loss linear in shots fired instead of compounding.
    const float Unrecoverable = 1.0f - FMath::Clamp(Profile.RecoveryFraction, 0.0f, 1.0f);
    InOutPitch -= Applied.PitchDegrees * Unrecoverable;
    InOutYaw -= Applied.YawDegrees * Unrecoverable;

    return Applied;
}

float FBreakerWeaponFeel::RecoverAxis(const FBreakerRecoilProfile& Profile, float Accumulated, float DeltaSeconds)
{
    if (Accumulated == 0.0f || DeltaSeconds <= 0.0f) return Accumulated;
    // Proportional first (fast off the peak, easing as it closes), then a
    // constant floor so the last fraction of a degree actually lands on zero.
    float Value = FMath::FInterpTo(Accumulated, 0.0f, DeltaSeconds, Profile.RecoveryInterpSpeed);
    Value = FMath::FInterpConstantTo(Value, 0.0f, DeltaSeconds, Profile.RecoveryConstantDegreesPerSecond);
    return Value;
}

float FBreakerWeaponFeel::ConsumeCompensation(float Accumulated, float PlayerDelta)
{
    if (Accumulated > 0.0f && PlayerDelta < 0.0f) return FMath::Max(0.0f, Accumulated + PlayerDelta);
    if (Accumulated < 0.0f && PlayerDelta > 0.0f) return FMath::Min(0.0f, Accumulated + PlayerDelta);
    return Accumulated;
}

float FBreakerWeaponFeel::BloomAfterShot(const FBreakerRecoilProfile& Profile, float CurrentBloom, bool bAiming)
{
    const float Growth = Profile.BloomPerShotDegrees * (bAiming ? FMath::Clamp(Profile.AimBloomMultiplier, 0.0f, 1.0f) : 1.0f);
    return FMath::Clamp(CurrentBloom + Growth, 0.0f, Profile.MaxBloomDegrees);
}

float FBreakerWeaponFeel::BloomAfterTime(const FBreakerRecoilProfile& Profile, float CurrentBloom, float DeltaSeconds)
{
    if (CurrentBloom <= 0.0f || DeltaSeconds <= 0.0f) return FMath::Max(0.0f, CurrentBloom);
    return FMath::Max(0.0f, CurrentBloom - Profile.BloomRecoveryDegreesPerSecond * DeltaSeconds);
}

float FBreakerWeaponFeel::EffectiveSpreadDegrees(const FBreakerRecoilProfile& Profile, float BaseSpreadDegrees, float CurrentBloom, int32 BurstShotIndex, float ExtraMovementSpread)
{
    const float Base = FMath::Max(0.0f, BaseSpreadDegrees);
    const float Movement = FMath::Max(0.0f, ExtraMovementSpread);
    if (BurstShotIndex <= 0)
    {
        // First shot of a burst ignores bloom entirely and scales the base
        // cone: at 0.0 the weapon puts the round exactly on the crosshair.
        // Movement is not forgiven — standing still is what buys the perfect
        // shot, and that is the same decision ADS is now asking for.
        return Base * FMath::Clamp(Profile.FirstShotSpreadMultiplier, 0.0f, 1.0f) + Movement;
    }
    return Base + FMath::Clamp(CurrentBloom, 0.0f, Profile.MaxBloomDegrees) + Movement;
}

float FBreakerWeaponFeel::MovementSpreadDegrees(const FBreakerRecoilProfile& Profile, float SpeedFraction, float AimAlpha)
{
    const float Speed = FMath::Clamp(SpeedFraction, 0.0f, 1.0f);
    if (Speed <= 0.0f || Profile.MoveSpreadDegrees <= 0.0f) return 0.0f;
    const float Alpha = FMath::Clamp(AimAlpha, 0.0f, 1.0f);
    const float AimScale = FMath::Lerp(1.0f, FMath::Max(0.0f, Profile.AimMoveSpreadMultiplier), Alpha);
    return Profile.MoveSpreadDegrees * Speed * AimScale;
}

float FBreakerWeaponFeel::AimMoveSpeedMultiplier(const FBreakerRecoilProfile& Profile, float AimAlpha)
{
    // Clamped to 1.0 at the top: this channel may only ever slow the player
    // down. An archetype that authored 1.3 here would make sighting a weapon
    // FASTER than hip firing it, which inverts the entire trade the ADS bill
    // exists to create.
    const float Aimed = FMath::Clamp(Profile.AimMoveSpeedMultiplier, 0.05f, 1.0f);
    return FMath::Lerp(1.0f, Aimed, FMath::Clamp(AimAlpha, 0.0f, 1.0f));
}

FBreakerRecoilProfile FBreakerWeaponFeel::ProfileAtAimAlpha(const FBreakerRecoilProfile& Profile, float AimAlpha)
{
    const float Alpha = FMath::Clamp(AimAlpha, 0.0f, 1.0f);
    if (Alpha >= 1.0f) return Profile;

    // 1.0 is the hip value of every one of these multipliers, so lerping from
    // 1.0 toward the authored number is exactly "this much of the sights".
    FBreakerRecoilProfile Blended = Profile;
    Blended.AimRecoilMultiplier = FMath::Lerp(1.0f, FMath::Clamp(Profile.AimRecoilMultiplier, 0.0f, 1.0f), Alpha);
    Blended.AimBloomMultiplier = FMath::Lerp(1.0f, FMath::Clamp(Profile.AimBloomMultiplier, 0.0f, 1.0f), Alpha);
    Blended.AimViewmodelMultiplier = FMath::Lerp(1.0f, FMath::Clamp(Profile.AimViewmodelMultiplier, 0.0f, 1.0f), Alpha);
    // AimMoveSpeedMultiplier is deliberately NOT blended here. It is a live
    // per-frame query rather than a per-shot one, so AimMoveSpeedMultiplier()
    // does its own ramp against the alpha; blending it here as well would
    // charge the movement penalty twice for a half-raised weapon.
    return Blended;
}

void FBreakerWeaponFeel::AddViewmodelKick(const FBreakerRecoilProfile& Profile, FBreakerViewmodelState& State, float HorizontalKickSign, bool bAiming)
{
    const float Scale = bAiming ? FMath::Clamp(Profile.AimViewmodelMultiplier, 0.0f, 1.0f) : 1.0f;
    const float LateralSign = HorizontalKickSign >= 0.0f ? 1.0f : -1.0f;

    State.BackOffset = FMath::Clamp(State.BackOffset + Profile.ViewmodelKickUnits * Scale, -Profile.MaxViewmodelKickUnits, Profile.MaxViewmodelKickUnits);
    State.LateralOffset = FMath::Clamp(State.LateralOffset + Profile.ViewmodelKickLateralUnits * Scale * LateralSign, -Profile.MaxViewmodelKickUnits, Profile.MaxViewmodelKickUnits);
    State.PitchOffset = FMath::Clamp(State.PitchOffset + Profile.ViewmodelKickPitchDegrees * Scale, -Profile.MaxViewmodelKickPitchDegrees, Profile.MaxViewmodelKickPitchDegrees);
}

void FBreakerWeaponFeel::IntegrateViewmodel(const FBreakerRecoilProfile& Profile, FBreakerViewmodelState& State, float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f || State.IsAtRest()) return;

    const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / BreakerRecoil::SpringSubstep), 1, BreakerRecoil::MaxSpringSubsteps);
    const float Step = DeltaSeconds / static_cast<float>(Steps);
    const float Stiffness = FMath::Max(1.0f, Profile.ViewmodelSpringStiffness);
    const float Damping = FMath::Max(0.0f, Profile.ViewmodelSpringDamping);

    auto IntegrateAxis = [Steps, Step, Stiffness, Damping](float& Value, float& Velocity)
    {
        for (int32 Index = 0; Index < Steps; ++Index)
        {
            const float Acceleration = -Stiffness * Value - Damping * Velocity;
            Velocity += Acceleration * Step;
            Value += Velocity * Step;
        }
        // Snap the tail to exact rest so the component can stop ticking.
        if (FMath::Abs(Value) < 0.002f && FMath::Abs(Velocity) < 0.02f)
        {
            Value = 0.0f;
            Velocity = 0.0f;
        }
    };

    IntegrateAxis(State.BackOffset, State.BackVelocity);
    IntegrateAxis(State.LateralOffset, State.LateralVelocity);
    IntegrateAxis(State.PitchOffset, State.PitchVelocity);
}
