#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerEnemy.h"
#include "BreakerWardenEnemy.generated.h"

class ABreakerZoneActor;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// SEVERED WARDEN — the anchor archetype (Encounter-Design §2.3).
//
// THE GAP IT FILLS. The gym had two archetypes and they asked for two things:
// SKITTER punishes standing still, LATTICE punishes moving predictably. Both
// are answered by the same verb — keep moving — so a pack of them is one
// problem at two ranges. §2.4 names the third axis: "Wardens punish
// approaching from the front." That is a different question, and it is the
// only one of the three whose answer is a DIRECTION rather than a speed.
//
// WHAT IT DEMANDS. A player who trades with it frontally loses: 90 armour is
// roughly 47% mitigation at slice values, and its sweep out-damages anything
// the player gains by standing there. A player who gets behind it pays no
// armour at all AND hits a 1.75x weak point. §2.3: "the POSITIONING is the
// multiplier, via armour geometry, not a stat" — this is how movement becomes
// a damage stat without master sheet 3.3's retired momentum-to-damage
// conversion coming back.
//
// It is the first enemy that fights with two attacks rather than one, and both
// are telegraphed spatially: the sweep draws its shield arm back for 0.5s, and
// the slam grows a ground ring that IS the hitbox preview. Per O1 the player
// answers both with position, so both telegraphs are long and neither is a
// reaction-window frame.
//
// FAMILY NOTE. §2.3 marks the Warden Altered and POST-TURN ONLY: master sheet
// 8.2 forbids any Altered enemy before the Act II turn beat, and the beat is
// not built. Nothing here gates on it, because nothing in the project can
// express the beat yet — the gate belongs to whatever spawns this, and
// Encounter-Design OPEN QUESTION 5 is the ruling it waits on.
//
// EVERY NUMBER IS AN O2 PLACEHOLDER and none of it is playtested.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerWardenEnemy : public ABreakerEnemy
{
    GENERATED_BODY()

public:
    ABreakerWardenEnemy();

    UFUNCTION(BlueprintPure, Category="Enemy|Warden") bool IsSweeping() const { return bSweepWindup; }
    UFUNCTION(BlueprintPure, Category="Enemy|Warden") bool IsSlamming() const { return bSlamWindup; }

    // --- Facing armour (§2.3, §7) -----------------------------------------
    // Frontally armoured, rear unarmoured. Authored here and published onto the
    // combat component in BeginPlay, so the ONE armour number lives with the
    // archetype that means it rather than in two places.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Armor", meta=(ClampMin="0"))
    float FrontalArmor = 90.0f;   // O2 PLACEHOLDER (§2.3: ~47% mitigation at slice values)
    // 0 means a rear or flank hit lands on unarmoured flesh. §2.3 is explicit:
    // "rear and flank hits bypass it entirely".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Armor", meta=(ClampMin="0", ClampMax="1"))
    float RearArmorFraction = 0.0f;   // O2 PLACEHOLDER

    // --- Attack A: shield sweep -------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Sweep", meta=(ClampMin="0"))
    float SweepRangeCm = 320.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Sweep", meta=(ClampMin="0", ClampMax="180"))
    float SweepHalfAngleDegrees = 65.0f;   // O2 PLACEHOLDER
    // The draw-back. §2.3 authors 0.5s; O1 forbids shortening it further.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Sweep", meta=(ClampMin="0"))
    float SweepWindupSeconds = 0.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Sweep", meta=(ClampMin="0"))
    float SweepCooldownSeconds = 2.2f;   // O2 PLACEHOLDER
    // It plants while it swings. A Warden that walks through its own attack is
    // a Warden the player cannot step around.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Sweep", meta=(ClampMin="0", ClampMax="1"))
    float SweepWindupMoveScale = 0.0f;   // O2 PLACEHOLDER

    // --- Attack B: ground slam --------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="0"))
    float SlamRadiusCm = 650.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="0"))
    float SlamCooldownSeconds = 7.0f;   // O2 PLACEHOLDER
    // The ring grows to full radius over this window and the ring IS the hitbox
    // preview, so the mechanic is legible with zero art (§2.3). Do not shorten:
    // this is the single most important number on the archetype, because the
    // slam is the one attack a player cannot answer by facing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="0"))
    float SlamWindupSeconds = 0.9f;   // O2 PLACEHOLDER
    // Relative to the sweep, which is the archetype's baseline attack. §2.3
    // authors 34 slam against 26 sweep.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="0"))
    float SlamDamageRelativeToSweep = 1.31f;   // O2 PLACEHOLDER

    // Phase 3 of the boss reuses the slam and leaves a hazard behind it. Off
    // for a plain Warden: §1.2 puts lingering hazards behind the Cascading
    // MODIFIER, and giving every Warden one for free would make the modifier
    // invisible when it appeared.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam")
    bool bSlamLeavesHazard = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="0"))
    float SlamHazardSeconds = 8.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="0"))
    float SlamHazardTickInterval = 0.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="0"))
    float SlamHazardTickFractionOfSlam = 0.18f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Slam", meta=(ClampMin="1"))
    int32 MaximumLiveSlamHazards = 6;   // O2 PLACEHOLDER (§5.3 caps hazards at 8)

    // --- Telegraph presentation (replaceable) -----------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Telegraph")
    FLinearColor ShieldColor = FLinearColor(0.42f, 0.44f, 0.50f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Telegraph")
    FLinearColor SweepHotColor = FLinearColor(1.00f, 0.52f, 0.18f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Telegraph")
    FLinearColor SlamRingColor = FLinearColor(1.00f, 0.36f, 0.12f);
    // How far the shield arm draws back, in cm, at full wind-up.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Telegraph", meta=(ClampMin="0"))
    float ShieldDrawBackCm = 46.0f;

    // --- Facing turn rate (owner playtest: "the shield guys instantly turn to
    // you so its hard to hit their weakspot") ------------------------------
    // §2.3's whole axis for this archetype is "punishes frontal approach": the
    // counterplay is to circle to the exposed side/back. ABreakerEnemy::Tick
    // (Combat/BreakerEnemy.cpp) sets actor rotation with SetActorRotation(
    // Facing.Rotation()) every frame — an unconditional snap, once per tick, to
    // whatever DesiredFacing says — and TickEngagedBehaviour below used to feed
    // it the raw direction-to-player every frame with no rate limit at all. A
    // Warden that re-faces in a single tick has no blind arc a flanking player
    // can ever reach, so the archetype's one axis silently did not exist.
    //
    // This is out of BreakerEnemy.cpp's reach (out of territory), so the cap is
    // enforced by feeding SetActorRotation an ALREADY-RATE-LIMITED direction
    // instead: TickEngagedBehaviour rotates the Warden's current forward
    // toward the player by at most this many degrees per tick's DeltaSeconds,
    // through ComputeCappedFacing below, before writing DesiredFacing.
    //
    // SEED ARITHMETIC (O2 placeholder, derived not guessed). Player sprint is
    // 950 cm/s — the design-canon figure this codebase already cites at
    // Combat/BreakerRangedEnemy.h:83 and Tests/BreakerBossAndArchetypeTests.cpp:182
    // (the raw UBreakerCharacterMovementComponent::SprintSpeed literal is 1100,
    // authored headroom above that canon figure, not the number to plan around).
    // At the Warden's own sweep range (SweepRangeCm = 320cm — the distance at
    // which "engaged" actually starts mattering for this archetype) a player
    // strafing at full sprint sweeps an angle at v/r radians/sec:
    //   omega_player = 950 / 320 = 2.969 rad/s = 170.1 deg/s.
    // The cap must sit strictly below that or the counterplay is unbeatable by
    // definition. 100 deg/s is ~59% of the player's angular ceiling: a clear,
    // visible turn-rate deficit (the whole point — O2's "perceptible") while
    // leaving the player roughly 1.7x angular headroom, enough to win the
    // flank even with an imperfect strafe rather than a frame-perfect one.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Warden|Facing", meta=(ClampMin="0"))
    float MaxTurnRateDegreesPerSecond = 100.0f;   // O2 PLACEHOLDER (see arithmetic above)

    // Pure maths behind the cap: rotates CurrentForward toward DesiredDirection
    // by at most MaxDegreesPerSecond * DeltaSeconds, turning the short way
    // round. World-free and deliberately usable with no Warden instance at
    // all, matching the UBreakerRangedBehaviorLibrary / CanBeginWallRide /
    // ComputeGravityMultiplier idiom for anything that has to be testable
    // without a running game.
    static FVector ComputeCappedFacing(const FVector& CurrentForward, const FVector& DesiredDirection,
        float MaxDegreesPerSecond, float DeltaSeconds);

protected:
    virtual void BeginPlay() override;
    virtual void TickEngagedBehaviour(class ABreakerCharacter* Player, float Distance, float DeltaSeconds,
        FVector& OutDirection, float& OutSpeedScale) override;
    virtual void SetBodyVisible(bool bVisible) override;
    // The base contact attack is replaced by the sweep, which has a wind-up and
    // an arc. Left empty rather than deleted so nothing inherited calls it.
    virtual void PerformAttack(APawn* TargetPawn) override {}

    // Both attacks resolve through the ordinary damage contract with this actor
    // as Instigator, so armour, shields, the passive dodge/block layer, the
    // incoming-modifier chain and kill credit all apply.
    void ResolveSweep(AActor* Target);
    void ResolveSlam();
    void UpdateSweepTelegraph(float Alpha);
    void UpdateSlamTelegraph(float Alpha, bool bVisible);

    UFUNCTION(BlueprintPure, Category="Enemy|Warden") float GetSweepDamage() const;
    UFUNCTION(BlueprintPure, Category="Enemy|Warden") float GetSlamDamage() const;

    // The rectangular shield primitive §2.3 asks for, and the ground ring.
    // Both are engine shapes: the whole fight has to read in untextured
    // graybox, so the tells cannot depend on an art pass.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> ShieldVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> SlamRingVisual;

    double SweepWindupStart = -1000.0;
    double LastSweepTime = -1000.0;
    double SlamWindupStart = -1000.0;
    double LastSlamTime = -1000.0;
    bool bSweepWindup = false;
    bool bSlamWindup = false;

private:
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShieldMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SlamRingMaterial;
    FVector ShieldRestLocation = FVector::ZeroVector;
    TArray<TWeakObjectPtr<ABreakerZoneActor>> SlamHazards;
};
