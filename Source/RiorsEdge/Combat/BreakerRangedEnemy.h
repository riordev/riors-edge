#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerEnemy.h"
#include "Combat/BreakerRangedBehavior.h"
#include "BreakerRangedEnemy.generated.h"

class ABreakerEnemyProjectile;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

// LATTICE — the ranged archetype (Encounter-Design §2.2).
//
// The gym's only enemy until now was a melee chaser: every gear it had ended
// in contact, so nothing in the game asked the player to take cover, strafe,
// or care about distance. This one holds a band instead of closing, strafes
// while it fires, and throws a slow, large, replicated projectile the player
// can watch coming and step out of.
//
// It is a SUBCLASS of ABreakerEnemy on purpose, not a parallel actor: loot on
// death, the ammo return, the on-death chain, the engagement-gapped TTK
// sample, the HUD's health bars and state labels, and the wave-mode alive
// count are all keyed off ABreakerEnemy and are inherited untouched.
//
// EVERY NUMBER BELOW IS AN O2 PLACEHOLDER and none of it is playtested.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerRangedEnemy : public ABreakerEnemy
{
    GENERATED_BODY()

public:
    ABreakerRangedEnemy();

    UFUNCTION(BlueprintPure, Category="Enemy|Ranged") EBreakerRangedBand GetBand() const { return Band; }
    UFUNCTION(BlueprintPure, Category="Enemy|Ranged") bool IsWindingUp() const { return bWindingUp; }

    // --- Engagement band (O2 PLACEHOLDER) ----------------------------------
    // The archetype's whole identity. Inside [Min, Max] it strafes and shoots;
    // beyond Max it advances; inside Min it backs off. Encounter-Design §2.2
    // wants it to hold roughly 1800cm and withdraw rather than melee.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Band", meta=(ClampMin="0")) float MinEngagementDistance = 900.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Band", meta=(ClampMin="0")) float MaxEngagementDistance = 1900.0f;
    // Boundary deadband. Without it the enemy sitting on an edge flips band
    // every frame and reads as a twitching bug.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Band", meta=(ClampMin="0")) float BandHysteresis = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Band", meta=(ClampMin="0")) float AdvanceSpeedMultiplier = 1.25f;
    // Backing off is its fastest gear: being crowded is the failure state.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Band", meta=(ClampMin="0")) float RetreatSpeedMultiplier = 1.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Band", meta=(ClampMin="0")) float StrafeSpeedMultiplier = 0.90f;
    // Strafe direction flips on this cadence so it never orbits forever in one
    // direction and become trivially trackable. Desynced by PatrolPhase.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Band", meta=(ClampMin="0.1")) float StrafeReverseSeconds = 2.1f;

    // --- Fire cycle (O2 PLACEHOLDER) ---------------------------------------
    // The telegraph. A dodgeable projectile is only fair if the player knows
    // it is coming, and O1 (passive block/dodge) means the answer has to be
    // positional — so the tell must be long enough to reposition inside, not
    // a reaction-window frame. Encounter-Design §0 forbids shortening these.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire", meta=(ClampMin="0")) float WindupSeconds = 0.85f;
    // Move speed is cut during the wind-up so the shot reads as committed.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire", meta=(ClampMin="0", ClampMax="1")) float WindupMoveScale = 0.30f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire", meta=(ClampMin="0")) float ShotCooldown = 2.4f;
    // Player sprint is 950 cm/s. At 1100 the orb crosses the band in 0.8-1.7s,
    // which is the reaction budget the "(that i can see)" request is about.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire", meta=(ClampMin="1")) float ProjectileSpeed = 1100.0f;
    // PARTIAL LEAD, and the choice is deliberate. 0 (Encounter-Design §2.2's
    // literal spec) means a moving player is never hit from the front, so the
    // enemy reads as harmless. 1 means only a direction change beats it, which
    // punishes a player who cannot yet read the tell. 0.35 aims at where a
    // player holding a straight line would be: keeping a sprint lane gets
    // punished, and ANY change of direction — strafe, dash, slide, jump —
    // beats it outright. Set to 0 to recover the doc's pure Lattice.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire", meta=(ClampMin="0", ClampMax="1")) float LeadFraction = 0.35f;
    // One big orb by default: a single readable object beats a spread the
    // player cannot parse. Raise for the doc's three-projectile volley.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire", meta=(ClampMin="1")) int32 ProjectilesPerVolley = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire", meta=(ClampMin="0")) float VolleySpreadDegrees = 7.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Fire") TSubclassOf<ABreakerEnemyProjectile> ProjectileClass;

    // --- Telegraph presentation (replaceable) ------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Telegraph") FLinearColor TelegraphIdleColor = FLinearColor(0.13f, 0.05f, 0.20f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Telegraph") FLinearColor TelegraphHotColor = FLinearColor(0.72f, 0.24f, 0.98f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Telegraph", meta=(ClampMin="0")) float TelegraphLightIntensity = 2600.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Telegraph", meta=(ClampMin="0")) float TelegraphIdleScale = 0.16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy|Ranged|Telegraph", meta=(ClampMin="0")) float TelegraphHotScale = 0.38f;

protected:
    virtual void BeginPlay() override;
    virtual void TickEngagedBehaviour(class ABreakerCharacter* Player, float Distance, float DeltaSeconds,
        FVector& OutDirection, float& OutSpeedScale) override;
    virtual void SetBodyVisible(bool bVisible) override;
    // The base melee contact attack is not this archetype's damage path.
    virtual void PerformAttack(APawn* TargetPawn) override {}

    void FireVolley(const AActor* Target);
    void UpdateTelegraph(float Alpha);
    bool HasLineOfSightTo(const AActor* Target) const;
    FVector GetMuzzleLocation() const;

    // The "core node" of Encounter-Design §2.2: dim between volleys, hot and
    // wide during the wind-up. It is the tell, and it is also where the shot
    // comes from, so the player learns one object rather than two.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> EmitterVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPointLightComponent> EmitterLight;

private:
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> EmitterMaterial;
    EBreakerRangedBand Band = EBreakerRangedBand::Advance;
    bool bWindingUp = false;
    double WindupStartTime = -1000.0;
    float StrafeTimer = 0.0f;
    float StrafeSign = 1.0f;
};
