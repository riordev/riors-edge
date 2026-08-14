#pragma once

#include "CoreMinimal.h"
#include "Combat/BreakerWardenEnemy.h"
#include "Combat/BreakerBossPhases.h"
#include "BreakerBossEnemy.generated.h"

class ABreakerRangedEnemy;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerBossPhaseChanged, EBreakerBossPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerBossDefeated);

// THE FIELD MARSHAL — the slice boss (Encounter-Design §3).
//
// THE DESIGN THESIS, and the reason this class subclasses the Warden rather
// than ABreakerEnemy: §3.1 says "the boss is not a big Warden. The boss is a
// Warden that COMMANDS the other three archetypes." Inheriting the Warden is
// that sentence in the type system — the sweep, the slam, the facing armour
// and the always-face-the-player advance are all the same fight the player has
// already learned, and everything this class ADDS is command.
//
// Every phase mechanic is an ORDER given to adds, and the player's job is to
// read who is being told to do what. When they realise the adds are responding
// to it, the story fact lands mechanically before any dialogue says it. The
// corollary §3.1 draws is load-bearing: it must NOT be a damage sponge,
// because its interest lives in the adds.
//
// TELEGRAPH VOCABULARY — three tells, and the player learns all three:
//   1. The shield draw-back (inherited)  -> a sweep is coming to your front.
//   2. The growing ground ring (inherited) -> a slam is coming to your feet.
//   3. The apparatus raise (new)          -> an ORDER is coming, and WHERE it
//      points says which. §3.4 deliberately reuses ONE animation for both
//      orders so the player has to read the direction rather than the pose.
// The apparatus raise is also the punish window: it lifts the rear weak point
// above the shoulder line so it is visible and hittable FROM THE FRONT.
//
// WHAT IS NOT BUILT HERE, and why: the arena. §3.3 specs a 4000x4000 room with
// two pillars, two +600 galleries and four corner alcoves. Levels are editor
// work and this lane does not own Game/ — so the boss takes its alcove and
// gallery positions as AUTHORED OFFSETS from its own spawn point, which means
// it works in the flat gym today and snaps onto the real arena the moment one
// exists by retuning six vectors.
//
// DIVERGENCE [O18] carried forward from §3.2 and NOT resolved here: the fight
// has a script floor. Even a player who bursts a phase down instantly waits on
// the order cadence, so the phase cadence imposes its own length independent of
// health. Whether the composition lands inside O18's 20-45s band is a
// measurement, and O2 freezes the values until wave mode reports.
//
// EVERY NUMBER IS AN O2 PLACEHOLDER. Nothing here is playtested; automation
// proves the phase machine and cannot see whether the apparatus reads.
UCLASS(Blueprintable)
class RIORSEDGE_API ABreakerBossEnemy : public ABreakerWardenEnemy
{
    GENERATED_BODY()

public:
    ABreakerBossEnemy();

    UFUNCTION(BlueprintPure, Category="Boss") EBreakerBossPhase GetPhase() const { return Phase; }
    UFUNCTION(BlueprintPure, Category="Boss") EBreakerBossOrder GetActiveOrder() const { return ActiveOrder; }
    UFUNCTION(BlueprintPure, Category="Boss") bool IsGivingOrder() const { return bOrderRaiseActive; }
    UFUNCTION(BlueprintPure, Category="Boss") bool IsApparatusExposed() const;
    UFUNCTION(BlueprintPure, Category="Boss") int32 GetOrdersGiven() const { return OrdersGiven; }

    UPROPERTY(BlueprintAssignable, Category="Boss") FBreakerBossPhaseChanged OnPhaseChanged;
    UPROPERTY(BlueprintAssignable, Category="Boss") FBreakerBossDefeated OnBossDefeated;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss") FBreakerBossPhaseParams PhaseParams;

    // --- Arena geometry, authored as offsets from the spawn point ----------
    // §3.3's four corner alcoves and two galleries. Offsets rather than world
    // points so the boss is placeable anywhere, including the flat gym, and so
    // the real arena is a retune rather than a rewrite.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Arena")
    TArray<FVector> AlcoveOffsets;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Arena")
    TArray<FVector> GalleryOffsets;

    // --- DEPLOY (phase 1) --------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Deploy", meta=(ClampMin="0"))
    int32 AddsPerDeploy = 2;   // O2 PLACEHOLDER (§3.4, solo)
    // What DEPLOY spawns. Left null in the constructor and defaulted to the
    // plain melee enemy at BeginPlay, so a Blueprint can swap the add without
    // this class knowing what a Skitter is.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Deploy")
    TSubclassOf<ABreakerEnemy> DeployAddClass;
    // §5.3's density ceiling, enforced at the SOURCE. A boss that keeps
    // deploying into a field the player has not cleared is how a 12-enemy cap
    // becomes 30.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Deploy", meta=(ClampMin="0"))
    int32 MaximumLiveAdds = 8;   // O2 PLACEHOLDER

    // --- Phase 2 gallery Lattices -----------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Suppression")
    TSubclassOf<ABreakerRangedEnemy> GalleryLatticeClass;
    // §5.3 caps live Lattices at 3 regardless of party size — "the single most
    // dangerous scaling knob".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Suppression", meta=(ClampMin="0", ClampMax="3"))
    int32 GalleryLatticeCount = 2;   // O2 PLACEHOLDER (§6.4, solo)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Suppression", meta=(ClampMin="0"))
    float GalleryRespawnSeconds = 12.0f;   // O2 PLACEHOLDER (§3.4)

    // --- The command apparatus (the third tell) ---------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Apparatus", meta=(ClampMin="0"))
    float ApparatusRaiseCm = 120.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Apparatus")
    FLinearColor ApparatusIdleColor = FLinearColor(0.18f, 0.16f, 0.22f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Apparatus")
    FLinearColor ApparatusDeployColor = FLinearColor(1.00f, 0.66f, 0.16f);
    // A DIFFERENT colour per order, on top of the different pointing direction.
    // §3.4 wants the player to read WHICH order from where it points; a second
    // channel is insurance, not a replacement, because a player standing behind
    // the boss cannot see which way it is pointing at all.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Apparatus")
    FLinearColor ApparatusFireColor = FLinearColor(0.36f, 0.72f, 1.00f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Apparatus", meta=(ClampMin="0"))
    float ApparatusLightIntensity = 5200.0f;

    // §3.2: DoTs are capped at 3 stacks on the boss (master sheet 7.10 risk 5).
    // They still deal full snapshot damage; they just do not stack unbounded.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss", meta=(ClampMin="1"))
    int32 BossDamageOverTimeStackCap = 3;   // O2 PLACEHOLDER

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void TickEngagedBehaviour(class ABreakerCharacter* Player, float Distance, float DeltaSeconds,
        FVector& OutDirection, float& OutSpeedScale) override;
    virtual void HandleDeath() override;
    virtual void SetBodyVisible(bool bVisible) override;

    // Phase transitions. Idempotent per phase: entering a phase runs its
    // one-time setup exactly once even if the health fraction wobbles.
    void UpdatePhase();
    void EnterPhase(EBreakerBossPhase NewPhase);

    void BeginOrder();
    void ResolveOrder();
    void UpdateApparatus(float Alpha);

    void SpawnDeployAdds(const FVector& AlcoveWorldLocation);
    void SpawnGalleryLattices();
    void TickGalleryRespawn(float DeltaSeconds);
    void CommandGalleryVolley();
    // The alcove or gallery this order points at, chosen before the raise so
    // the pointing direction is stable for the whole tell (§3.4: "the pointed
    // alcove is telegraphed before the spawn").
    FVector PickOrderTargetOffset();

    // The rear weak point, exposed only during orders in phases 1-2 and
    // permanently in phase 3.
    void SetApparatusExposed(bool bExposed);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> ApparatusVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPointLightComponent> ApparatusLight;

private:
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> ApparatusMaterial;
    UPROPERTY() TArray<TWeakObjectPtr<ABreakerEnemy>> LiveAdds;
    UPROPERTY() TArray<TWeakObjectPtr<ABreakerRangedEnemy>> GalleryLattices;

    EBreakerBossPhase Phase = EBreakerBossPhase::Deployment;
    EBreakerBossOrder ActiveOrder = EBreakerBossOrder::None;
    FVector ApparatusRestLocation = FVector::ZeroVector;
    FVector PendingOrderOffset = FVector::ZeroVector;
    float TimeSinceLastOrder = 0.0f;
    float OrderRaiseElapsed = 0.0f;
    float DeploySpawnCountdown = -1.0f;
    float GalleryRespawnCountdown = -1.0f;
    bool bOrderRaiseActive = false;
    bool bApparatusExposed = false;
    int32 OrdersGiven = 0;
    int32 OrderTargetIndex = 0;
    float BaseFrontalArmor = 90.0f;
    float BaseSweepCooldown = 2.2f;
    float BaseSlamCooldown = 7.0f;
    float BaseBossMoveSpeed = 300.0f;
};
