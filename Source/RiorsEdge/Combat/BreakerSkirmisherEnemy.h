#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerCoverBehavior.h"
#include "BreakerSkirmisherEnemy.generated.h"

class ABreakerEnemyProjectile;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

// SEVERED SKIRMISHER — the EARLY-severance Altered (Assets/story-source.md §1.5).
//
// THE GAP IT FILLS, and it is the biggest one in the roster. Every enemy in the
// project answers to the same verb. Skitter closes, Lattice holds a band,
// Warden advances and holds ground — all three are "shoot the thing in front of
// you while moving", and all three are on screen the whole time. Nothing in the
// game has ever broken line of sight, and nothing has ever reacted to being
// shot.
//
// Assets/story-source.md §1.5 says an early-stage Altered "still wears insignia, still
// uses cover, still flinches", and that severance stage should be readable from
// the BEHAVIOUR and not only the model. This archetype is that sentence: it is
// a trained soldier, and it fights like one.
//
// WHAT IT DEMANDS OF THE PLAYER — a fourth answer, and the only one that is not
// about aim:
//   * You cannot out-shoot it, because most of the time it is behind
//     something. Standing at range trading is strictly losing.
//   * Its exposure window is short and self-chosen, so waiting for it is
//     slow and it will win the attrition.
//   * The answer is to PUSH — close the distance, take an angle its cover does
//     not face, and catch it during a relocation. That is a movement decision
//     under O1's passive defence, and it is the only enemy that rewards
//     aggression as such.
//   * The flinch is the reward for connecting: a hit while it is exposed
//     interrupts the burst and sends it back to cover, so chip damage buys
//     tempo even when it does not kill.
//
// WHY IT IS NOT A VESTIGE, mechanically and not just fictionally: cover
// discipline, burst fire and flinching are all DECISIONS, and §1.5 says a
// Vestige has no tactics that resemble a military. UsesCoverDiscipline() and
// FlinchesWhenHit() are asked of the family/stage pair rather than hardcoded,
// so authoring this actor as a Vestige — or as a late-stage Altered — turns
// both behaviours off and leaves a plain open-field shooter. That is the stage
// spectrum working, not a bug.
//
// EVERY NUMBER IS AN O2 PLACEHOLDER and none of it is playtested. In
// particular, nobody has confirmed that the gym has any geometry a skirmisher
// can actually hide behind — in an empty field it correctly finds no cover and
// degrades to an open-ground shooter, which is honest but is NOT the fight.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerSkirmisherEnemy : public ABreakerEnemy
{
    GENERATED_BODY()

public:
    ABreakerSkirmisherEnemy();

    // Its kills belong in the ranged TTK bucket, not the melee one: it is
    // fought across a distance and has no contact attack.
    virtual bool IsRangedForTelemetry() const override { return true; }

    UFUNCTION(BlueprintPure, Category="Enemy|Skirmisher") EBreakerCoverState GetCoverState() const { return CoverState; }
    UFUNCTION(BlueprintPure, Category="Enemy|Skirmisher") bool HasCover() const { return bHasCoverPoint; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Cover") FBreakerCoverParams Cover;

    // How long it stays behind cover before peeking. This is the player's
    // window to close, and it is the archetype's main tuning dial: shorter
    // makes it aggressive, longer makes it a turtle.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Cover", meta=(ClampMin="0"))
    float PeekDelaySeconds = 1.1f;   // O2 PLACEHOLDER
    // Maximum time it stays up once it has stood. It goes back down on its own
    // even if untouched, so a player who cannot land a hit still sees the loop.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Cover", meta=(ClampMin="0"))
    float MaximumExposureSeconds = 2.6f;   // O2 PLACEHOLDER
    // Give-up time on a relocation. Without it, a cover point behind
    // unreachable geometry strands the enemy walking into a wall forever.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Cover", meta=(ClampMin="0"))
    float RelocateTimeoutSeconds = 3.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Cover", meta=(ClampMin="0"))
    float CoverArrivalToleranceCm = 140.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Cover", meta=(ClampMin="0"))
    float RelocateSpeedMultiplier = 1.45f;   // O2 PLACEHOLDER

    // THE FLINCH. Short: it is a tempo reward for the player, not a stun-lock.
    // A long flinch on a burst weapon would let the player perma-suppress it,
    // which removes the fight rather than winning it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Flinch", meta=(ClampMin="0"))
    float FlinchSeconds = 0.45f;   // O2 PLACEHOLDER
    // Minimum gap between flinches, so a high-RPM weapon cannot chain them.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Flinch", meta=(ClampMin="0"))
    float FlinchCooldownSeconds = 1.2f;   // O2 PLACEHOLDER

    // --- The burst ---------------------------------------------------------
    // Recognisable weapon handling: a short controlled burst on a cadence, not
    // a lobbed orb. That is the readable difference from LATTICE, which is the
    // Vestige answer to the same range.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire", meta=(ClampMin="1"))
    int32 RoundsPerBurst = 3;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire", meta=(ClampMin="0.01"))
    float RoundIntervalSeconds = 0.14f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire", meta=(ClampMin="0"))
    float BurstCooldownSeconds = 0.9f;   // O2 PLACEHOLDER
    // The aim tell before the first round of a burst. Shorter than LATTICE's
    // 0.85s because standing up out of cover IS most of the telegraph — the
    // player has already been given a whole silhouette appearing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire", meta=(ClampMin="0"))
    float AimSeconds = 0.45f;   // O2 PLACEHOLDER
    // Fast and flat, unlike the Lattice orb. A soldier's round.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire", meta=(ClampMin="1"))
    float ProjectileSpeed = 2600.0f;   // O2 PLACEHOLDER
    // Damage per ROUND, as a fraction of the chassis attack, so a full burst is
    // roughly one chassis attack and the archetype is not secretly three times
    // as dangerous as its area level says.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire", meta=(ClampMin="0"))
    float DamagePerRoundFraction = 0.34f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire", meta=(ClampMin="0"))
    float SpreadDegrees = 2.2f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Fire")
    TSubclassOf<ABreakerEnemyProjectile> ProjectileClass;

    // --- Presentation ------------------------------------------------------
    // The insignia §1.5 asks for: a bright plate that survives untextured
    // graybox and separates it from a Vestige at a glance.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Presentation")
    FLinearColor InsigniaColor = FLinearColor(0.92f, 0.74f, 0.22f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Presentation")
    FLinearColor MuzzleIdleColor = FLinearColor(0.10f, 0.10f, 0.12f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Presentation")
    FLinearColor MuzzleHotColor = FLinearColor(1.00f, 0.86f, 0.42f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Presentation", meta=(ClampMin="0"))
    float MuzzleLightIntensity = 2400.0f;
    // How far it drops while behind cover. A crouch is the only thing that
    // makes "in cover" legible when the cover itself is a grey box.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Skirmisher|Presentation", meta=(ClampMin="0"))
    float CrouchDropCm = 55.0f;

protected:
    virtual void BeginPlay() override;
    virtual void TickEngagedBehaviour(class ABreakerCharacter* Player, float Distance, float DeltaSeconds,
        FVector& OutDirection, float& OutSpeedScale) override;
    virtual void SetBodyVisible(bool bVisible) override;
    // No contact attack: it is a shooter.
    virtual void PerformAttack(APawn* TargetPawn) override {}

    UFUNCTION() void HandleFlinchSource(const FBreakerHitContext& Hit);

    // Finds the best cover point whose line from the threat is actually
    // blocked. Returns false when there is no cover in range, which is a
    // legitimate state — see the class note about an empty gym.
    bool FindCoverPoint(const FVector& ThreatLocation, FVector& OutPoint);
    bool IsBlockedFrom(const FVector& ThreatLocation, const FVector& Point) const;
    void FireRound(const AActor* Target);
    void UpdateMuzzle(float Alpha);
    void SetCrouched(bool bCrouched);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> InsigniaVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> MuzzleVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPointLightComponent> MuzzleLight;

private:
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MuzzleMaterial;

    EBreakerCoverState CoverState = EBreakerCoverState::Relocating;
    FVector CoverPoint = FVector::ZeroVector;
    bool bHasCoverPoint = false;
    bool bCrouched = false;
    float StateElapsed = 0.0f;
    float AimElapsed = 0.0f;
    float RoundTimer = 0.0f;
    int32 RoundsLeftInBurst = 0;
    double LastFlinchTime = -1000.0;
    float BodyRestZ = 0.0f;
    // Set by the damage handler and consumed by the behaviour tick, so the
    // flinch is applied inside the enemy's own update rather than re-entrantly
    // from inside a damage broadcast.
    bool bFlinchRequested = false;
};
