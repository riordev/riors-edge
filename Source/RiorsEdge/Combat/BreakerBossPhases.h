#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerBossPhases.generated.h"

// THE FIELD MARSHAL's phase and order machine, as pure world-free maths.
//
// The precedent is Combat/BreakerRangedBehavior.h and
// Combat/BreakerMonsterChassis.h, and the reason is the same: a boss fight is
// exactly the kind of thing nobody can test in a running game without playing
// it forty times, and exactly the kind of thing that breaks silently. Phase
// regression, an order that fires twice on one frame, a phase-3 boss that
// still spawns adds — all of those are arithmetic bugs wearing a boss costume,
// and all of them are checkable here with no actor and no world.
//
// It also cannot read the player, for the same O27 reason everything else in
// this lane cannot: phase is a function of the boss's OWN health fraction, and
// order cadence is a function of phase. Nothing about the player's level, gear
// or build enters, so a well-built character does not get a longer fight.

UENUM(BlueprintType)
enum class EBreakerBossPhase : uint8
{
    // 100% -> 66%. It fights like a Warden and gives the DEPLOY order.
    Deployment,
    // 66% -> 33%. It holds gallery Lattices and gives the FIRE order. It also
    // starts turning to face the player continuously, so the rear weak point
    // has to be earned.
    Suppression,
    // 33% -> 0%. It stops commanding and fights: faster, shorter slam
    // cooldown, hazards accumulate, frontal armour halves, and the command
    // apparatus stays permanently exposed.
    Commitment
};

// What the boss is telling its adds to do. The player's whole job in phases 1
// and 2 is reading WHICH order this is, and both orders share the same
// apparatus-raise animation — the difference is where it points.
UENUM(BlueprintType)
enum class EBreakerBossOrder : uint8
{
    // Phase 3. It has stopped commanding, which is exactly when it is
    // dangerous.
    None,
    // Points at an alcove. Adds spawn there.
    Deploy,
    // Points at a gallery. Every live ranged ally volleys at once.
    Fire
};

// Every tunable of the fight in one authored block. O2: all placeholders.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerBossPhaseParams
{
    GENERATED_BODY()

    // HEALTH GATES, not timers. Encounter-Design §3.4: "timers punish low-DPS
    // builds; the gear thesis needs boss pacing to respond to player power."
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Phases", meta=(ClampMin="0", ClampMax="1"))
    float SuppressionGate = 0.66f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Phases", meta=(ClampMin="0", ClampMax="1"))
    float CommitmentGate = 0.33f;   // O2 PLACEHOLDER

    // Order cadence. §3.4 authors 20s DEPLOY and 15s FIRE.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0.1"))
    float DeployIntervalSeconds = 20.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0.1"))
    float FireIntervalSeconds = 15.0f;   // O2 PLACEHOLDER

    // The apparatus raise. This is the punish window and §3.4 calls it
    // "generous" on purpose: it is the only time in phases 1 and 2 that the
    // rear weak point is visible from the FRONT, and O1's passive defence means
    // the player answers with position rather than a reaction.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0"))
    float DeployRaiseSeconds = 2.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0"))
    float FireRaiseSeconds = 2.0f;   // O2 PLACEHOLDER
    // Adds appear this long after the raise completes, so the pointed alcove is
    // previewed before anything comes out of it (§5.1: spawns are always
    // previewed).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0"))
    float DeploySpawnDelaySeconds = 1.5f;   // O2 PLACEHOLDER

    // Phase 3's rewrites (§3.4).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Commitment", meta=(ClampMin="1"))
    float CommitmentSpeedMultiplier = 1.4f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Commitment", meta=(ClampMin="0", ClampMax="1"))
    float CommitmentSweepCadenceScale = 0.70f;   // O2 PLACEHOLDER (-30%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Commitment", meta=(ClampMin="0"))
    float CommitmentSlamCooldownSeconds = 4.0f;   // O2 PLACEHOLDER (7s -> 4s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Commitment", meta=(ClampMin="0", ClampMax="1"))
    float CommitmentArmorFraction = 0.50f;   // O2 PLACEHOLDER (90 -> 45)
};

UCLASS()
class RIORSEDGE_API UBreakerBossPhaseLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The phase a health fraction alone implies, ignoring history.
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static EBreakerBossPhase GetPhaseForHealthFraction(float HealthFraction, const FBreakerBossPhaseParams& Params);

    // The phase the boss is ACTUALLY in, which is monotonic: a boss that is
    // healed, shielded, or whose max health is rebuilt under it does not walk
    // back into a phase it already left. Without this, a Support player or a
    // chassis rebuild could re-run the DEPLOY script and the fight would have
    // no defined length at all.
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static EBreakerBossPhase AdvancePhase(EBreakerBossPhase Current, float HealthFraction, const FBreakerBossPhaseParams& Params);

    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static EBreakerBossOrder GetOrderForPhase(EBreakerBossPhase Phase);

    // Negative means "this phase gives no orders", which is Commitment.
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static float GetOrderIntervalSeconds(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params);

    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static float GetOrderRaiseSeconds(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params);

    // One step of the order clock. Returns true on the frame the order should
    // BEGIN (the apparatus starts rising). Advancing by a huge delta fires at
    // most once — a hitch must not deploy six packs.
    UFUNCTION(BlueprintCallable, Category="Boss|Phases")
    static bool AdvanceOrderClock(UPARAM(ref) float& TimeSinceLastOrder, float DeltaSeconds, float IntervalSeconds);

    // Phase 3 rewrites, read out of the params so there is one source of truth.
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static float GetPhaseSpeedMultiplier(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params);
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static float GetPhaseSweepCooldown(EBreakerBossPhase Phase, float BaseCooldown, const FBreakerBossPhaseParams& Params);
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static float GetPhaseSlamCooldown(EBreakerBossPhase Phase, float BaseCooldown, const FBreakerBossPhaseParams& Params);
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static float GetPhaseFrontalArmor(EBreakerBossPhase Phase, float BaseArmor, const FBreakerBossPhaseParams& Params);

    // The rear weak point is exposed during an order raise in phases 1 and 2,
    // and PERMANENTLY in phase 3 — because it has stopped commanding. §3.4's
    // unstated narrative beat: "it fights hardest when it stops being a
    // commander."
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static bool IsApparatusExposed(EBreakerBossPhase Phase, bool bOrderRaiseActive);

    // Phase 3 stops spawning; anything alive stays alive.
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static bool ShouldSpawnAdds(EBreakerBossPhase Phase);

    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static FString GetPhaseName(EBreakerBossPhase Phase);
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static FString GetOrderName(EBreakerBossOrder Order);
};
