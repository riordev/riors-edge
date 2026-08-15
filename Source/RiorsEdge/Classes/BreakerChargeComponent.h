#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerChargeComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerProgressionComponent;

UENUM(BlueprintType)
enum class EBreakerChargeBand : uint8
{
    Cold,
    Attuned,
    Resonant
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerChargeBandChanged, EBreakerChargeBand, NewBand);

// Support's Charge loop (Docs/Design/Class-Kits-Support.md §1.1).
//
// A BANK LIKE MANA, BUT WITH THE OPPOSITE STARTING POSITION. Charge ACCUMULATES
// from zero and has **no passive regeneration at all** — it is 100% event-driven
// — where Mana starts full and regenerates as its primary path. The reason is
// specific: the ultimate costs a full bar and is the strongest opening the class
// has, so a Support who banks a full bar before the fight starts arrives with a
// free CONDUIT. No idle income is how that is refused.
//
// It also does not DECAY, which looks like an inconsistency next to that and is
// not. Support's generation is the least self-directed of the five classes; a
// decaying bar would punish the player for a lull the enemy chose, and decay on
// top of no-idle-income produces a class that can sit at zero through no fault
// of its own. Banking is bounded a different way instead: an OUT-OF-COMBAT CLAMP
// that decays the bar down to a ceiling and then holds. A Support may open a
// fight with a strong bar and never with a free ultimate.
//
// THE CLAUSE THIS COMPONENT EXISTS TO ENFORCE, and the one that is not a tuning
// value: **self-directed generation pays exactly what ally-directed generation
// pays.** Healing yourself, shielding yourself and buffing yourself credit at
// the identical rate, and the buff-uptime source is a BOOLEAN rather than a
// count so that buffing five allies generates exactly what buffing yourself
// generates. Without those, Support is a party class with a solo penalty, which
// is the named failure this whole loop is shaped around and which O31 has since
// made a ruling ("every build must be able to make an impact") rather than a
// preference.
//
// O31 CAVEAT, RECORDED HONESTLY: the party layer does not exist. There is no
// ally, no party membership, no ally query, no buff propagation and no mark
// component. Every ALLY-facing half of every source below is therefore
// unreachable, and every SELF-facing half is the half that could run today. That
// is the reverse of the order the design implies, and it is why this component
// takes generation through explicit Notify* entry points describing the EVENT
// rather than binding a healing delegate that does not yet report overheal
// separately.
//
// Server-authority only; inert unless the owner's permanent class is Support.
// EVERY MAGNITUDE IS O2 PLACEHOLDER, quoted from §1.1.
UCLASS(ClassGroup=Classes, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerChargeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerChargeComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // The whole per-frame loop, split from TickComponent for the reason the
    // other four loops record. Charge's is the only one whose decay term runs
    // ONLY out of combat.
    void AdvanceLoop(float DeltaTime);

    UFUNCTION(BlueprintPure, Category="Charge") EBreakerChargeBand GetChargeBand() const { return CachedBand; }
    UFUNCTION(BlueprintPure, Category="Charge") bool IsActiveForOwner() const;
    UFUNCTION(BlueprintPure, Category="Charge") float GetCharge() const;
    UFUNCTION(BlueprintPure, Category="Charge") float GetChargeFraction() const;

    // Support abilities cost Charge AND carry a cooldown, per the shared rule
    // for event-driven, spiky generation. Charge is NEVER spent to zero
    // involuntarily: no node, ability, or enemy modifier drains it, and the only
    // ways it decreases are ability costs, the ultimate, and the out-of-combat
    // clamp. That is what makes the bank read as a bank.
    UFUNCTION(BlueprintPure, Category="Charge") bool CanAffordSpend(float Cost) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge") bool TrySpendCharge(float Cost);

    // Generation entry points.
    //
    // EffectiveHeal is the amount that ACTUALLY MOVED THE HEALTH BAR. Overheal
    // is passed separately and pays nothing — it is a parameter rather than an
    // omission so the caller states it and the rule is auditable at the call
    // site. bSelfTargeted exists only to route the self-heal sub-cap; it does
    // NOT change the rate, and nothing in this component may ever make it do so.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge|Generation") void NotifyHealingDone(float EffectiveHeal, float Overheal, float TargetMaxHealth, bool bSelfTargeted, float ProcCoefficient);
    // The shield-side twin. Shield granted above the target's cap generates
    // nothing, for the same reason overheal does not.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge|Generation") void NotifyShieldingDone(float EffectiveShield, float OverShield, float TargetMaxHealth, bool bSelfTargeted);
    // The offensive conversion path, available in all three branches because
    // Mark is a starter. Percentage-of-target-max means a boss pays the same
    // total for the same fraction, so a long boss fight yields a bounded,
    // predictable bar rather than an unbounded one.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge|Generation") void NotifyMarkedTargetDamage(float DamageDealt, float TargetMaxHealth);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge|Generation") void NotifyStatusCleansed(int32 StatusesRemoved);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge|Generation") void NotifyAssist();
    // COUNT-INDEPENDENT BY CONSTRUCTION. A bool, never a count — the same
    // type-level guarantee the Grit loop's proximity source carries, and here it
    // closes "stack five buffs on yourself for 5x" and "party of five for 5x" in
    // one rule. The symmetry is the point.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge|Buffs") void SetAnyBuffActive(bool bActive);
    UFUNCTION(BlueprintPure, Category="Charge|Buffs") bool HasAnyBuffActive() const { return bAnyBuffActive; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge|Combat") void SetInCombat(bool bNowInCombat);
    UFUNCTION(BlueprintPure, Category="Charge|Combat") bool IsInCombat() const { return bInCombat; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Charge") void GrantCharge(float Amount);

    // Loop overrides, the Momentum push/pop shape. CONDUIT does NOT push one:
    // its rewrite is that abilities cost nothing, which is an ABILITY-LAYER
    // cost multiplier (FBreakerAbilityVariant::AbilityCostMultiplier already
    // models exactly that for Caster's Unmake) and not a resource-loop change.
    // Generation deliberately CONTINUES during CONDUIT, so unlike Unmake there
    // is no suspension for this component to implement — a well-played CONDUIT
    // partially refunds itself and, bounded by ability cooldowns, cannot fully
    // refund itself.
    UFUNCTION(BlueprintCallable, Category="Charge|Loop") void PushLoopOverride(FName Key, bool bSuspendClamp, float GenerationMultiplier, float Duration);
    UFUNCTION(BlueprintCallable, Category="Charge|Loop") void PopLoopOverride(FName Key);
    UFUNCTION(BlueprintPure, Category="Charge|Loop") bool IsClampSuspended() const;
    UFUNCTION(BlueprintPure, Category="Charge|Loop") float GetGenerationMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Charge|Loop") int32 GetActiveLoopOverrideCount() const;

    // Pure loop rules, exposed for tests and for the eventual DA_ChargePolicy
    // asset that the design already names as the home for all Charge tuning.
    // Threshold-taking form is the real rule; the one-argument form carries the
    // shipped seeds for callers with no component in hand. The component always
    // calls the explicit form with its own tunables, so moving a band in the
    // editor actually moves it.
    static EBreakerChargeBand BandForFraction(float Fraction);
    static EBreakerChargeBand BandForFraction(float Fraction, float AttunedAt, float ResonantAt);
    static float ComposeGenerationMultipliers(const TArray<float>& Multipliers);
    static bool IsLoopOverrideExpired(double ExpiryTime, double Now);
    static float ClampGeneration(float RequestedRate, float GlobalCap);
    static float ClampToBank(float Value, float MaxCharge);
    // Overheal generates zero. The parameter exists so the rule is stated in the
    // signature: a caller that has not separated the two cannot call this.
    static float HealingGeneration(float EffectiveHeal, float TargetMaxHealth, float HealthFractionPerCharge, float ProcCoefficient);
    static float ShieldingGeneration(float EffectiveShield, float TargetMaxHealth, float HealthFractionPerCharge);
    static float MarkedDamageGeneration(float DamageDealt, float TargetMaxHealth, float HealthFractionPerCharge);
    static float CleanseGeneration(int32 StatusesRemoved, float PerStatusGrant);
    static float BuffUptimeGeneration(bool bAnyBuffActive, bool bInCombat, float UptimeRate);
    // The out-of-combat clamp: the bar falls TO a ceiling and then holds. It
    // never falls below the ceiling, which is why this is not a violation of
    // "no resource decays in a menu, at a Forge, or in the Anchor" — in those
    // states the clamp is already satisfied and never ticks.
    static float OutOfCombatDecay(float Current, float Ceiling, float DecayPerSecond, float DeltaTime);
    static bool CanSpendFrom(float Charge, float Cost);
    static float DrawFromBudget(float Requested, float& InOutBudgetRemaining);

    void BindAttributes(UBreakerAttributeSet* InAttributes);

    UPROPERTY(BlueprintAssignable, Category="Charge") FBreakerChargeBandChanged OnChargeBandChanged;

    // +1 per 3% of the TARGET'S maximum health restored or granted as shield.
    // Percentage-of-target rather than absolute, so healing a Tank is not a
    // Charge engine.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0.0001")) float HealthFractionPerCharge = 0.03f;   // O2 PLACEHOLDER
    // +1 per 2% of a marked target's maximum health dealt. A lower denominator
    // than healing because it is the class's only offensive lane and the loop's
    // ignition has to be affordable.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0.0001")) float MarkedHealthFractionPerCharge = 0.02f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0")) float BuffUptimeRate = 2.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0")) float AssistGrant = 8.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0")) float AssistInterval = 1.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0")) float CleansePerStatusGrant = 4.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0")) float CleanseInterval = 0.5f;   // O2 PLACEHOLDER
    // Lower than Swift's 25/s because Support generation is partly TIME-based
    // (buff uptime) rather than wholly event-based, and a time-based source
    // under a high cap is a passive drip by another name.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0")) float GlobalGenerationCap = 18.0f;   // O2 PLACEHOLDER
    // Self-heal sub-cap, INDEPENDENT of the global cap. This is the Life-on-Hit
    // guard: a Support running heavy Life on Hit affixes with a high-cadence
    // weapon heals dozens of times a second, and without this the ITEM layer
    // becomes the class's primary generator — affixes scale verbs, they do not
    // own the class loop.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Generation", meta=(ClampMin="0")) float SelfHealRateCap = 6.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Clamp", meta=(ClampMin="0")) float OutOfCombatCeiling = 60.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Clamp", meta=(ClampMin="0")) float OutOfCombatClampSeconds = 4.0f;   // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Bands", meta=(ClampMin="0", ClampMax="1")) float AttunedFraction = 0.34f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Charge|Bands", meta=(ClampMin="0", ClampMax="1")) float ResonantFraction = 0.75f;   // O2 PLACEHOLDER

    UFUNCTION() void HandleProgressionChanged();

private:
    bool IsInSafeZone() const;
    void ApplyChargeDelta(float Delta);
    void RefreshBand();
    void QueueGrant(float Amount);

    struct FLoopOverrideEntry
    {
        bool bSuspendClamp = false;
        float GenerationMultiplier = 1.0f;
        double ExpiryTime = -1.0;
    };
    mutable TMap<FName, FLoopOverrideEntry> LoopOverrides;
    void PruneLoopOverrides() const;

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    bool bIsSupport = false;
    bool bAnyBuffActive = false;
    bool bInCombat = false;
    EBreakerChargeBand CachedBand = EBreakerChargeBand::Cold;
    float PendingGrants = 0.0f;
    float SelfHealBudget = 0.0f;
    double LastAssistTime = -1000.0;
    double LastCleanseTime = -1000.0;
};
