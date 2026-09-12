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
    // cooldown, hazards accumulate, and the command apparatus stays
    // permanently exposed. The front is O198's pool, spent once and gone;
    // nothing about it changes with phase.
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

    // The apparatus raise. This is the ORDER punish window and §3.4 calls it
    // "generous" on purpose: it is the only time in phases 1 and 2 that the
    // rear weak point is visible from the FRONT, and O1's passive defence means
    // the player answers with position rather than a reaction.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0"))
    float DeployRaiseSeconds = 2.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0"))
    float FireRaiseSeconds = 2.0f;   // O2 PLACEHOLDER

    // The FRONT BREAK punish window (O198). The first punish window of the
    // fight is the one the player earns rather than waits for: spending the
    // front pool opens the apparatus for this long, once, in whatever phase
    // the break lands. Its punish is the exposed 1.75x weak point plus the
    // front that no longer mitigates — nothing more. combat.md leaves the
    // stagger/interrupt model open, so there is no stagger here and no damage
    // multiplier standing in for one; when that ruling lands it is a new beat
    // in the grammar below, not a number on this one.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Orders", meta=(ClampMin="0"))
    float FrontBreakPunishSeconds = 2.5f;   // O2 PLACEHOLDER
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

    // THE ADD GATE. While the boss has live adds and is not yet in Commitment,
    // incoming damage is scaled by (1 - this) and the next phase gate holds:
    // the adds are what the player has to answer, and a build that ignores
    // them does not out-DPS the fight. Zero means no gate at all — the Field
    // Marshal ships at zero and is byte-identical to a boss that never heard
    // of one. O31: the reduction is never a wall (BreakerAddGateMaxReduction),
    // so every build still lands damage through it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|AddGate", meta=(ClampMin="0", ClampMax="0.9"))
    float AddGateDamageReduction = 0.0f;   // O2 PLACEHOLDER
};

namespace BreakerBossGate
{
    // The most an add gate may take. 1.0 would be immunity, which O31 forbids
    // outright; anything above this reads as immunity in play. O2 PLACEHOLDER.
    constexpr float BreakerAddGateMaxReduction = 0.9f;
}

// THE BOSS GRAMMAR. Five kinds of beat, and every phase of the fight is a
// sentence in them. The grammar is DERIVED from the params and the actor's
// counts by MakeShippedGrammar, never authored beside them, so it cannot drift
// from the numbers the fight actually runs on: it is the fight's shape made
// checkable, and the place a new beat (a stagger, a second arena change) is
// added when its plumbing exists.
//
// NOT SERIALIZED, on purpose. This enum is a description of the runtime and is
// never written to a save or an asset, so it is free to grow and reorder; the
// append-only rule binds EBreakerBossPhase above, not this.
enum class EBreakerBossBeat : uint8
{
    // A tell the player can read before anything lands.
    Telegraph,
    // The weak point is open. Seconds < 0 means for the rest of the fight.
    PunishWindow,
    // A health fraction the boss crosses into the next phase.
    PhaseGate,
    // Adds arrive, AddCount of them, Seconds after the beat before it.
    AddWave,
    // The room itself changes what it is.
    ArenaChange,
    // The adds hold the boss: while any of the wave before this beat is alive,
    // incoming damage is scaled by (1 - Reduction) and the phase gate after it
    // does not open. Never emitted in Commitment, and never emitted at all
    // when the params author no reduction.
    AddGate
};

struct RIORSEDGE_API FBreakerBossBeat
{
    EBreakerBossBeat Beat = EBreakerBossBeat::Telegraph;
    // Duration for a Telegraph or PunishWindow; delay for an AddWave. -1 on a
    // PunishWindow is permanent.
    float Seconds = 0.0f;
    // PhaseGate only; -1 otherwise.
    float GateFraction = -1.0f;
    // AddWave only.
    int32 AddCount = 0;
    // AddGate only: the fraction of incoming damage the live adds take off.
    float Reduction = 0.0f;
    // Which tell, which window, which change — for a test or a log to name.
    FName Tag;
};

struct RIORSEDGE_API FBreakerBossGrammar
{
    // Beats that can land in any phase.
    TArray<FBreakerBossBeat> FightLevel;
    // Indexed by EBreakerBossPhase's value. Three because the phase enum has
    // three; the enum is not extended to carry this.
    TArray<FBreakerBossBeat> PerPhase[3];

    const TArray<FBreakerBossBeat>& ForPhase(EBreakerBossPhase Phase) const
    {
        return PerPhase[FMath::Clamp(static_cast<int32>(Phase), 0, 2)];
    }
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

    // AdvancePhase with the add gate: while the params author a reduction and
    // LiveAddCount is above zero, a boss outside Commitment holds its phase
    // whatever its health says. Still monotonic — the gate only ever delays
    // a step forward, never takes one back. With a zero reduction this IS
    // AdvancePhase. A separate name rather than an overload because UHT does
    // not overload a UFUNCTION; the actor calls this one and nothing else.
    static EBreakerBossPhase AdvancePhaseGated(EBreakerBossPhase Current, float HealthFraction,
        const FBreakerBossPhaseParams& Params, int32 LiveAddCount);

    // The incoming-damage multiplier the add gate applies to the boss:
    // (1 - Reduction) while LiveAddCount > 0 outside Commitment, else 1.0.
    // Never zero (O31): the reduction is capped at BreakerAddGateMaxReduction.
    UFUNCTION(BlueprintPure, Category="Boss|AddGate")
    static float AddGateIncomingMultiplier(EBreakerBossPhase Phase, int32 LiveAddCount, const FBreakerBossPhaseParams& Params);

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

    // The standoff ring this phase holds (O273). Identity in every phase for
    // now: O273 says phases tighten the ring, and that is cycle C's rewrite,
    // which lands here as a params term rather than on the actor. The actor
    // reads its ring through this and nowhere else, so the day it varies is
    // one edit and one test.
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static float GetPhaseHoldRing(EBreakerBossPhase Phase, float BaseRingCm, const FBreakerBossPhaseParams& Params);

    // The rear weak point is exposed during an order raise in phases 1 and 2,
    // and PERMANENTLY in phase 3 — because it has stopped commanding. §3.4's
    // unstated narrative beat: "it fights hardest when it stops being a
    // commander."
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static bool IsApparatusExposed(EBreakerBossPhase Phase, bool bOrderRaiseActive);

    // The full rule for the weak point: an order raise, a live front-break
    // window (O198), or Commitment. The actor reads this and nothing else.
    UFUNCTION(BlueprintPure, Category="Boss|Grammar")
    static bool IsPunishWindowOpen(EBreakerBossPhase Phase, bool bOrderRaiseActive, float FrontBreakWindowRemaining);

    // The health fraction that ends this phase; negative in Commitment, which
    // ends only with the boss.
    UFUNCTION(BlueprintPure, Category="Boss|Grammar")
    static float NextGate(EBreakerBossPhase Phase, const FBreakerBossPhaseParams& Params);

    // One step of the front-break window. Returns true on the step that CLOSES
    // it, exactly once; Remaining never goes below zero and a closed window
    // stays closed.
    UFUNCTION(BlueprintCallable, Category="Boss|Grammar")
    static bool AdvanceBreakWindow(UPARAM(ref) float& Remaining, float DeltaSeconds);

    // The shipped grammar, derived. SweepTellSeconds is the Warden's draw-back:
    // it lives on the archetype, not in the params, and is passed in rather
    // than authored a second time here. When the params author an add-gate
    // reduction, an AddGate follows every AddWave in Deployment and
    // Suppression; Commitment never has one, because it has no adds.
    static FBreakerBossGrammar MakeShippedGrammar(const FBreakerBossPhaseParams& Params,
        int32 AddsPerDeploy, int32 GalleryLatticeCount, float SweepTellSeconds);

    // The first PunishWindow in the grammar: fight-level beats first, then the
    // phases in order. Null if there is none.
    static const FBreakerBossBeat* FirstPunishWindow(const FBreakerBossGrammar& Grammar);

    // Phase 3 stops spawning; anything alive stays alive.
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static bool ShouldSpawnAdds(EBreakerBossPhase Phase);

    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static FString GetPhaseName(EBreakerBossPhase Phase);
    UFUNCTION(BlueprintPure, Category="Boss|Phases")
    static FString GetOrderName(EBreakerBossOrder Order);
};
