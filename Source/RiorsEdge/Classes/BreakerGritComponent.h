#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerGritComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerProgressionComponent;

UENUM(BlueprintType)
enum class EBreakerGritBand : uint8
{
    Winded,
    Braced,
    Ironclad
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerGritBandChanged, EBreakerGritBand, NewBand);

// Tank's Grit loop (Docs/Design/Class-Kits-Tank.md §1).
//
// THE THIRD DECAY SHAPE. Momentum decays on a STATE (are you moving?) and Mana
// does not decay at all. Grit decays on a TIMER: a 6s lapse window that any
// damage taken or any enemy inside 5 m refreshes, and outside which the bar
// bleeds. That makes it a BANKED STATE WITH A LAPSE TIMER — more forgiving than
// Momentum, less forgiving than Mana — and it exists because the Tank's "am I
// doing my job" signal is discrete (took a hit / enemy near) rather than
// continuous (going fast).
//
// It ACCUMULATES from zero, and the class's worst moment — opening an encounter
// at zero — is a designed problem rather than an accident: combat entry grants a
// one-off credit once per combat state so the first seconds are not dead.
//
// THE RULE THAT IS NOT A PLACEHOLDER, stated once at the top because everything
// else bends around it: **all damage-taken generation is computed on
// POST-MITIGATION damage.** A high-Armour Tank must not out-generate a
// low-Armour Tank on the same incoming hit, or Armour silently becomes a Grit
// multiplier and the class's own §1.3 rule 1 inverts. The magnitudes below are
// O2 PLACEHOLDER; that rule is a SHAPE and must survive re-costing.
//
// O1 IS LOAD-BEARING HERE IN A WAY IT IS NOT FOR SWIFT OR CASTER. The Tank has
// NO defensive input: block is an RNG proc off a passive chance layer, never a
// stance, never a button, and Parry belongs to the Bulwark constellation and is
// not part of this kit. The block source below is therefore a YIELD ON A PROC —
// gear-driven inflow the player cannot time, cannot bait, and cannot spend skill
// on — and its per-source cap exists so that a node shortening the proc's
// internal cooldown cannot quietly promote it into the loop's spine. A Tank with
// zero Block Chance must have a complete, playable economy; the spine is
// post-mitigation damage and proximity, the two sources that fire whenever the
// Tank is doing its job.
//
// Server-authority only; inert unless the owner's permanent class is Tank.
UCLASS(ClassGroup=Classes, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerGritComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerGritComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // The whole per-frame loop, split out of TickComponent for the reason the
    // Mana and Momentum loops record: TickComponent asserts on an unregistered
    // component. Grit's loop is the longest of the three new ones because it is
    // the only one with BOTH a rate source (proximity) and a decay term, and the
    // per-source budgets have to be refreshed before either is charged.
    void AdvanceLoop(float DeltaTime);

    UFUNCTION(BlueprintPure, Category="Grit") EBreakerGritBand GetGritBand() const { return CachedBand; }
    UFUNCTION(BlueprintPure, Category="Grit") bool IsActiveForOwner() const;
    UFUNCTION(BlueprintPure, Category="Grit") float GetGrit() const;
    UFUNCTION(BlueprintPure, Category="Grit") float GetGritFraction() const;
    // True while the lapse window is open — i.e. damage taken or an enemy within
    // the proximity radius inside the last LapseSeconds. Decay is suspended for
    // exactly as long as this is true.
    UFUNCTION(BlueprintPure, Category="Grit") bool IsLapseWindowOpen() const;

    // Tank abilities cost Grit AND carry the longest cooldown band in the game.
    // This component owns only the cost half; the cooldown is a GameplayEffect
    // like every other, and the pairing is deliberate — the cooldown stops a
    // full bar becoming one instant, the cost stops the cooldown being the only
    // constraint. Floor is a hard 0: Grit has no Overcast analogue.
    UFUNCTION(BlueprintPure, Category="Grit") bool CanAffordSpend(float Cost) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit") bool TrySpendGrit(float Cost);

    // Generation entry points, one per source row of §1.1.
    //
    // HealthDamage and ShieldDamage are POST-MITIGATION and are separate
    // parameters rather than one total precisely so the caller cannot collapse
    // them: shield absorption generates at half rate, and that halving is the
    // guard that stops the Leech overshield loop being a generation engine as
    // well as a survival one.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit|Generation") void NotifyDamageTaken(float HealthDamage, float ShieldDamage, bool bSelfInflicted, float ProcCoefficient);
    // The passive Block proc. NOT an input (O1) — the caller is the combat
    // layer's chance roll, not a player action.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit|Generation") void NotifyBlockProc();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit|Generation") void NotifyMeleeKill();
    // Count-independent by construction: this takes a BOOL, not a count, so no
    // caller can ever pass nine and be paid nine times. One enemy at 4.9 m
    // generates exactly what nine do, and the type system says so.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit|Proximity") void SetEnemyInProximity(bool bEnemyNear);
    UFUNCTION(BlueprintPure, Category="Grit|Proximity") bool IsEnemyInProximity() const { return bEnemyNear; }
    // Combat state drives the entry grant, the generation gate and the decay
    // gate together. Entering combat grants the one-off Winded credit exactly
    // once per combat state; leaving arms the drain.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit|Combat") void SetInCombat(bool bNowInCombat);
    UFUNCTION(BlueprintPure, Category="Grit|Combat") bool IsInCombat() const { return bInCombat; }
    // Death sets Grit to 0. No banking through a death and no free ultimate on
    // respawn — there is no hardcore mode (O16), but there is no free Hold
    // either.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit") void NotifyDeath();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Grit") void GrantGrit(float Amount);

    // Loop overrides, the Momentum push/pop shape verbatim. HOLD is the caller
    // this exists for — it suspends nothing but multiplies generation against a
    // raised cap — and the Tank kit explicitly needs the KEYED form because
    // Hold, a Bastion node suspending decay near an Anchor Point, and a Leech
    // node can all be live at once and must each revert independently.
    UFUNCTION(BlueprintCallable, Category="Grit|Loop") void PushLoopOverride(FName Key, bool bSuspendDecay, float GenerationMultiplier, float Duration);
    UFUNCTION(BlueprintCallable, Category="Grit|Loop") void PopLoopOverride(FName Key);
    UFUNCTION(BlueprintPure, Category="Grit|Loop") bool IsDecaySuspended() const;
    UFUNCTION(BlueprintPure, Category="Grit|Loop") float GetGenerationMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Grit|Loop") int32 GetActiveLoopOverrideCount() const;

    // Pure loop rules, exposed for tests and for the eventual DA_GritPolicy asset.
    static EBreakerGritBand BandForFraction(float Fraction);
    static float ComposeGenerationMultipliers(const TArray<float>& Multipliers);
    static bool IsLoopOverrideExpired(double ExpiryTime, double Now);
    static float ClampGeneration(float RequestedRate, float GlobalCap);
    static float ClampToBank(float Value, float MaxGrit);
    // The mandatory post-mitigation rule as arithmetic. HealthDamage pays at the
    // full rate, ShieldDamage at ShieldRateFraction of it, and the whole result
    // is scaled by the proc coefficient — a DoT tick landing on the Tank pays at
    // its coefficient, not at 1.0, or a stacked Bleed becomes the cheapest Grit
    // engine in the game.
    static float DamageTakenGeneration(float HealthDamage, float ShieldDamage, float MaxHealth,
        float GritPerHealthFraction, float HealthFractionPerGrit, float ShieldRateFraction, float ProcCoefficient);
    // Self-damage generates at a FRACTION of the ordinary rate and is never
    // exempted anywhere. Reducing self-damage (O13) therefore reduces the Grit
    // it produces, by the post-mitigation rule — so rocket-jumping gets strictly
    // WORSE as a Grit engine the more the player invests in the branch that
    // rocket-jumps. That direction is the point: it is what makes O13's
    // "tolerated, never required" true at the resource layer and not only at the
    // damage layer.
    static float SelfDamageScalar(float SelfDamageRate, bool bSelfInflicted);
    // Count-independent: a bool in, a rate out. There is deliberately no
    // overload taking a count.
    static float ProximityGeneration(bool bEnemyNear, float ProximityRate);
    // Decay is charged only outside the lapse window; inside it, Grit banks.
    static bool IsLapseOpen(float SecondsSinceContact, float LapseSeconds);
    static float DecayRate(bool bLapseOpen, bool bDecaySuspended, float DecayPerSecond);
    static bool CanSpendFrom(float Grit, float Cost);
    // Per-source budgets. Grit is the only one of the five loops with per-source
    // caps UNDER the global cap, and they encode the class thesis rather than
    // decorating it: with damage capped at half the global cap, a Tank CANNOT
    // reach the global cap by taking damage alone. Filling the bar fast needs
    // damage plus presence plus kills. Being hit is necessary and never
    // sufficient — which is the mechanical statement of "gets stronger by being
    // hit, without ever wanting to be hit more than necessary."
    static float DrawFromBudget(float Requested, float& InOutBudgetRemaining);

    void BindAttributes(UBreakerAttributeSet* InAttributes);

    UPROPERTY(BlueprintAssignable, Category="Grit") FBreakerGritBandChanged OnGritBandChanged;

    // +1 Grit per 2% of maximum health lost, expressed as the denominator so the
    // rule is one division rather than a hidden constant.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0")) float GritPerHealthFraction = 1.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0.0001")) float HealthFractionPerGrit = 0.02f;   // O2 PLACEHOLDER
    // Half rate for shield absorption: still post-mitigation damage, but it cost
    // no health.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0", ClampMax="1")) float ShieldRateFraction = 0.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0", ClampMax="1")) float SelfDamageRate = 0.25f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0")) float BlockProcGrant = 6.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0")) float BlockProcInterval = 0.4f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0")) float MeleeKillGrant = 10.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0")) float ProximityRate = 1.5f;   // O2 PLACEHOLDER
    // Softens Winded, the class's worst moment. The EXISTENCE of an entry grant
    // is the design commitment; the magnitude is a placeholder like every other.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Generation", meta=(ClampMin="0")) float CombatEntryGrant = 15.0f;   // O2 PLACEHOLDER

    // Per-source caps. DamageRateCap is SHARED by the health and shield sources
    // — the two together never exceed it — which is why there is one knob and
    // not two.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Caps", meta=(ClampMin="0")) float DamageRateCap = 10.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Caps", meta=(ClampMin="0")) float SelfDamageRateCap = 3.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Caps", meta=(ClampMin="0")) float BlockRateCap = 8.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Caps", meta=(ClampMin="0")) float GlobalGenerationCap = 20.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Decay", meta=(ClampMin="0")) float LapseSeconds = 6.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grit|Decay", meta=(ClampMin="0")) float DecayPerSecond = 5.0f;   // O2 PLACEHOLDER

    UFUNCTION() void HandleProgressionChanged();

private:
    bool IsInSafeZone() const;
    void ApplyGritDelta(float Delta);
    void RefreshBand();
    void RefreshBudgets(float DeltaTime);

    struct FLoopOverrideEntry
    {
        bool bSuspendDecay = false;
        float GenerationMultiplier = 1.0f;
        double ExpiryTime = -1.0;
    };
    mutable TMap<FName, FLoopOverrideEntry> LoopOverrides;
    void PruneLoopOverrides() const;

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    bool bIsTank = false;
    bool bEnemyNear = false;
    bool bInCombat = false;
    EBreakerGritBand CachedBand = EBreakerGritBand::Winded;
    float SecondsSinceContact = 0.0f;
    // Per-source budgets, refilled each frame to cap * DeltaTime and drawn down
    // by that frame's events. Held as members rather than recomputed inside the
    // notify handlers because a notify can arrive several times between ticks
    // and the cap is per SECOND, not per event.
    float DamageBudget = 0.0f;
    float SelfDamageBudget = 0.0f;
    float BlockBudget = 0.0f;
    float PendingGrants = 0.0f;
    double LastBlockGrantTime = -1000.0;
};
