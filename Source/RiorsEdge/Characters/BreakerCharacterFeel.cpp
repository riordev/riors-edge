// ---------------------------------------------------------------------------
// The movement-feel layer of ABreakerCharacter, and nothing else (O283).
//
// WHY THIS FILE EXISTS. BreakerCharacter.cpp is 3,200 lines and every cue
// the camera and the hands pay for a change of state — the crouch ease, the
// brake plant, the slide plant — is one question with one owner: does the
// body's state change READ. A member function's definition may live in any
// translation unit and keeps full private access, so this split costs the
// class nothing: no widened access, no exported helpers, no friend. The
// enemy-bar TU (Combat/BreakerEnemyHealthBars.cpp) is the precedent.
//
// What lives here: the crouch hooks, UpdateMovementFeel, PayPlantImpulse,
// and the cast kick (O284): HandleAbilityCast, UpdateCastFeel and the FOV
// writer's cast term. What stays in the main TU: the FOV composer (one
// writer, D5) — the sprint push and the cast pulse are each one term added
// there — the dash clock, the landing dip's Landed and the slide-entry call
// site, each one line into this layer.
//
// The shapes are Characters/BreakerFeelPulseMath.h's. This file reads the
// body, eases, and writes the camera and the kick spring.
// ---------------------------------------------------------------------------

#include "Characters/BreakerCharacter.h"

#include "Abilities/BreakerAbilityComponent.h"
#include "Abilities/BreakerAbilityDefinition.h"
#include "Camera/CameraComponent.h"
#include "Characters/BreakerFeelPulseMath.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Movement/BreakerCharacterMovementComponent.h"
#include "UI/BreakerEffectMomentMath.h"
#include "UI/BreakerEffectRenderer.h"
#include "UI/BreakerHUDMath.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "Weapons/BreakerWeaponFeel.h"

namespace
{
    // Below this the crouch ease is at rest: the tail of an exponential never
    // reaches zero on its own, and a camera write per frame for a hundredth
    // of a millimetre is a write for nothing.
    constexpr float BreakerFeelCrouchRestCm = 0.05f;   // O2 PLACEHOLDER
    // How the cast FOV pulse's one authored length splits into attack and
    // recovery: it arrives in the first third and settles over the rest, the
    // same proportion as the camera kick's 0.06/0.12.
    constexpr float BreakerFeelCastFOVAttackFraction = 0.3f;   // O2 PLACEHOLDER
}

void ABreakerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    // The movement component just shrank the capsule and, while it keeps its
    // base (walking), moved the capsule's centre DOWN by the scaled adjust —
    // and the camera hangs from that centre, so it stepped the same distance
    // in one frame. Seed the offset that puts it back where it was; the ease
    // below brings it down over CrouchCameraEaseSeconds. Additive, not
    // assigned: a tap that stands before the ease finishes must not lose the
    // remainder. In the capsule's OWN space, so the unscaled adjust is the
    // one that cancels the scaled world step exactly.
    // A crouch in the air resizes about the centre and moves nothing the
    // camera can feel, so it seeds nothing.
    const UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move || !Move->bCrouchMaintainsBaseLocation) return;
    CameraCrouchOffsetCm += HalfHeightAdjust;
}

void ABreakerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    // The mirror: the centre stepped UP, so the camera holds low and rises.
    const UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move || !Move->bCrouchMaintainsBaseLocation) return;
    CameraCrouchOffsetCm -= HalfHeightAdjust;
}

void ABreakerCharacter::PayPlantImpulse(float HorizontalSpeed)
{
    // The same converter and the same spring as the landing dip: the speed
    // the body gave up becomes one impulse into the weapon's own kick, so a
    // stop recovers with the equipped archetype's character exactly as a
    // landing does. Purely presentation; a dedicated server has no
    // viewmodel and the call is inert there.
    if (!Weapon) return;
    const float KickUnits = FBreakerWeaponFeel::LandingKickUnits(ViewmodelMotion, HorizontalSpeed);
    if (KickUnits > 0.0f)
    {
        Weapon->AddViewmodelImpulse(KickUnits, -KickUnits * ViewmodelMotion.LandingPitchPerKickUnit);
    }
}

void ABreakerCharacter::UpdateMovementFeel(float DeltaSeconds)
{
    // ---- The crouch ease --------------------------------------------------
    if (!FMath::IsNearlyZero(CameraCrouchOffsetCm))
    {
        CameraCrouchOffsetCm = BreakerFeel::EaseToward(CameraCrouchOffsetCm, 0.0f, CrouchCameraEaseSeconds, DeltaSeconds);
        if (FMath::Abs(CameraCrouchOffsetCm) < BreakerFeelCrouchRestCm)
        {
            CameraCrouchOffsetCm = 0.0f;
        }
    }

    // ---- THE ONE WRITER of the camera's relative location -----------------
    // The crouch ease and the death beat's drop composed over the rest
    // location. Written only while an offset is live, or on the frame the
    // last one dies — so Done puts the camera back byte-identical to
    // CameraRestLocation, and a camera at rest is never touched.
    if (FirstPersonCamera)
    {
        const float Composed = CameraCrouchOffsetCm - DeathBeatCameraDropCm;
        const bool bLive = !FMath::IsNearlyZero(Composed);
        if (bLive || !FMath::IsNearlyZero(AppliedCameraOffsetCm))
        {
            FirstPersonCamera->SetRelativeLocation(CameraRestLocation + FVector(0.0, 0.0, Composed));
            AppliedCameraOffsetCm = bLive ? Composed : 0.0f;
        }
    }

    // ---- The brake plant ---------------------------------------------------
    // A stop is an edge: grounded last frame with a live input vector,
    // grounded this frame with none. The movement component's acceleration
    // IS the consumed input (scaled), so it is read rather than the raw
    // stick — a stick held into a wall still counts as input, a released one
    // never does. A slide and a traversal are postures, not strides, and a
    // dead body's hands do not plant.
    const UBreakerCharacterMovementComponent* Move = GetBreakerMovement();
    const bool bGrounded = Move && Move->IsMovingOnGround() && !Move->IsSliding() && !Move->IsTraversingLedge();
    const bool bInputLive = bGrounded && !Move->GetCurrentAcceleration().IsNearlyZero();
    if (bGrounded && bWasGroundedInput && !bInputLive && DeathBeatElapsed < 0.0f)
    {
        // The threshold is a fraction of the live walk cap, so a slowed or
        // aimed walk plants at ITS half, not the unmodified figure's.
        const float Threshold = BrakePlantMinSpeedFraction * Move->GetWalkSpeedCap();
        if (LastGroundedSpeed >= Threshold)
        {
            PayPlantImpulse(LastGroundedSpeed);
        }
    }
    bWasGroundedInput = bInputLive;
    LastGroundedSpeed = bGrounded ? static_cast<float>(Move->Velocity.Size2D()) : 0.0f;
}

// ---- The cast kick (O284) --------------------------------------------------

void ABreakerCharacter::HandleAbilityCast(EBreakerAbilitySlot Slot)
{
    // THE ONE HOOK. Instant activations and the wind-up's landing both arrive
    // here (O178), so every ability is felt on the frame it announces itself
    // — and never on the press of one still winding up, and never on a
    // cancel, because the component withholds the broadcast for those.

    // The burst of the verb's colour leaves the hand. Played for any pawn
    // whose world has a renderer: on a listen server a remote caster's cast
    // is on the host's screen too. The rig root IS the hand; the aim is the
    // pawn's base aim (the control rotation for a controlled pawn). The
    // renderer is client cosmetic and null where nothing can spawn.
    if (ABreakerEffectRenderer* Effects = ABreakerEffectRenderer::FindOrSpawn(GetWorld()))
    {
        const UBreakerAbilityDefinition* Definition = Abilities ? Abilities->GetDefinitionForSlot(Slot) : nullptr;
        // The same answer the HUD rail and the ability's own presentation
        // give, for the same reason: colour by verb, violet for the ultimate,
        // the resting border for an unsaid verb rather than a guess.
        const FLinearColor Paint = BreakerHUDMath::AbilityRailColor(
            Definition ? Definition->Verb : EBreakerAbilityVerb::None,
            Definition && Definition->IsUltimate());
        const FVector Aim = GetBaseAimRotation().Vector();
        const FVector Hand = PrototypeWeaponVisual
            ? PrototypeWeaponVisual->GetComponentLocation()
            : GetActorLocation() + FVector(0.0, 0.0, 20.0);
        Effects->PlayMoment(EBreakerEffectMoment::Cast, Hand, Aim, Paint);
    }

    // The camera and the hands are the viewer's own: a remote pawn's cast
    // must not kick the host's aim.
    if (!IsLocallyControlled()) return;

    // Restart, never accumulate: a second cast inside the first's recovery
    // reads as a second kick, not a bigger one.
    CastFeelElapsed = 0.0f;

    // The hands: the same converter and the same spring as the landing dip
    // and the brake plant, so the cast recovers with the equipped
    // archetype's character. Inert on a dedicated server (no viewmodel).
    if (Weapon && CastViewmodelKickUnits > 0.0f)
    {
        Weapon->AddViewmodelImpulse(CastViewmodelKickUnits, -CastViewmodelKickUnits * ViewmodelMotion.LandingPitchPerKickUnit);
    }
}

void ABreakerCharacter::UpdateCastFeel(float DeltaSeconds)
{
    if (CastFeelElapsed < 0.0f) return;
    CastFeelElapsed += DeltaSeconds;

    // The clock outlives the longer of the two envelopes, so the FOV writer
    // reads a live term for the whole pulse even when the camera kick has
    // already settled.
    const bool bDone = CastFeelElapsed >= FMath::Max(CastKickAttackSeconds + CastKickRecoverySeconds, CastFOVPulseSeconds);
    const float Pitch = bDone ? 0.0f
        : CastCameraPitchDegrees * BreakerFeel::PulseAlpha(CastFeelElapsed, CastKickAttackSeconds, CastKickRecoverySeconds);

    // A net-zero control-rotation delta, exactly the shake's and the death
    // beat's technique: the camera runs bUsePawnControlRotation and would
    // discard a relative pitch of its own, and subtracting last frame's
    // offset before adding this one leaves the aim where the player put it
    // once the pulse returns to zero. Written only while an offset is live
    // or on the frame the last one dies.
    if (Controller && (!FMath::IsNearlyZero(Pitch) || !FMath::IsNearlyZero(LastCastPitchOffset)))
    {
        FRotator Rotation = Controller->GetControlRotation();
        Rotation.Pitch += Pitch - LastCastPitchOffset;
        Controller->SetControlRotation(Rotation);
    }
    LastCastPitchOffset = Pitch;

    if (bDone)
    {
        CastFeelElapsed = -1.0f;
    }
}

float ABreakerCharacter::GetCastFOVPulseDegrees() const
{
    // 0 before and after the pulse: PulseAlpha is 0 for a negative elapsed
    // and past the recovery, so the FOV writer's live test needs no second
    // flag.
    return CastFOVPulseDegrees * BreakerFeel::PulseAlpha(CastFeelElapsed,
        CastFOVPulseSeconds * BreakerFeelCastFOVAttackFraction,
        CastFOVPulseSeconds * (1.0f - BreakerFeelCastFOVAttackFraction));
}
