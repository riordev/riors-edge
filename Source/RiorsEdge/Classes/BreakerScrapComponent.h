#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "BreakerScrapComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerProgressionComponent;

UENUM(BlueprintType)
enum class EBreakerScrapState : uint8
{
    Dry,
    Stocked,
    Surplus
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerScrapStateChanged, EBreakerScrapState, NewState);

// Gunsmith's Scrap loop (Docs/Design/Class-Kits-Gunsmith.md §1).
//
// THE THIRD SHAPE, AND THE REASON IT IS NOT A FOURTH. Momentum is a state that
// decays; Mana is a wallet that regenerates. Scrap is a LEDGER OF WORK ALREADY
// DONE: it ACCUMULATES from zero, it has **no passive regeneration and no
// decay**, and that pairing is structural rather than flavour. Gunsmith's spend
// converts into deployables that persist in the world until destroyed or
// expired, so idle generation would be free permanent power accruing out of
// combat — the Master 7.10.1 failure mode by another door. The compensation for
// zero idle income is that nothing is ever lost: Scrap earned in one encounter
// is spendable in the next.
//
// Server-authority only; inert unless the owner's permanent class is Gunsmith,
// exactly as the Momentum and Mana loops are gated on Swift and Caster.
//
// O30: MINIONS AND DEPLOYABLES DO NOT EXIST IN ANY FORM, AND THIS COMPONENT
// DOES NOT PRETEND OTHERWISE. Two of the five generation sources in §1.1 are
// deployable-driven (destruction refund, deployable damage). They are authored
// below as PURE STATIC ARITHMETIC and nothing calls them, because the alternative
// — omitting them — would leave the loop's real shape untestable, and the other
// alternative — spawning something — would be building a system the ledger says
// does not exist. Every deployable-facing entry point is a NotifyX() the
// deployable system will call when it lands; today only a test does.
//
// EVERY MAGNITUDE IS O2 PLACEHOLDER. The numbers below are QUOTED from
// Class-Kits-Gunsmith.md §1.1-§1.5, which is itself frozen under O2 pending
// wave mode. Nothing here is authored balance.
UCLASS(ClassGroup=Classes, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerScrapComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerScrapComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // The whole per-frame loop, split out of TickComponent for the same reason
    // UBreakerManaComponent::AdvanceLoop and UBreakerMomentumComponent::AdvanceLoop
    // are: UActorComponent::TickComponent asserts on an unregistered component,
    // which is what every component in the automation suite is. Scrap's loop is
    // the shortest of the three — there is no regeneration to apply and no decay
    // to charge, so all it does is meter queued grants against the per-second
    // budget — and that shortness is the design, not an omission.
    void AdvanceLoop(float DeltaTime);

    UFUNCTION(BlueprintPure, Category="Scrap") EBreakerScrapState GetScrapState() const { return CachedState; }
    UFUNCTION(BlueprintPure, Category="Scrap") bool IsActiveForOwner() const;
    UFUNCTION(BlueprintPure, Category="Scrap") float GetScrap() const;
    UFUNCTION(BlueprintPure, Category="Scrap") float GetScrapFraction() const;

    // Spending. Deployable abilities cost Scrap and carry NO cooldown (Scrap is
    // the cooldown); personal abilities carry a cooldown and cost NO Scrap
    // (§1.5). This component owns only the first half — a refused spend is the
    // only failure state the loop has, because there is no floor below zero to
    // borrow against the way Caster's Overcast borrows.
    UFUNCTION(BlueprintPure, Category="Scrap") bool CanAffordSpend(float Cost) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Scrap") bool TrySpendScrap(float Cost);

    // Generation entry points. Each mirrors one row of §1.1 and each is a
    // NOTIFY rather than a poll, because none of the events has a world-free
    // source this component could read for itself.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Scrap|Generation") void NotifyKill(float ProcCoefficient);
    // bAnyRoundFired is the anti-farm clause stated as a parameter: reloading a
    // magazine nothing has left credits nothing.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Scrap|Generation") void NotifyReloadCompleted(bool bAnyRoundFired);
    // Fires on the LAST ROUND LEAVING the magazine, not on the reload, and only
    // when the magazine was full at the start of the cycle — topping off at 1/30
    // and firing one round does not re-arm it.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Scrap|Generation") void NotifyMagazineEmptied(bool bStartedFull);
    // O30: NOTHING CALLS THESE TWO. They are the deployable system's half of the
    // contract, written now so the arithmetic is pinned by a test before the
    // system that needs it exists, and so a future deployable cannot invent its
    // own economy.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Scrap|Generation") void NotifyDeployableDestroyed(float DeployableScrapCost);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Scrap|Generation") void NotifyDeployableDamageDealt(float DamageAppliedToHealth);

    // Direct credit, mirroring UBreakerMomentumComponent::GrantMomentum and
    // UBreakerManaComponent::GrantMana(bIgnoreGlobalCap=true): it bypasses the
    // metered per-second budget entirely rather than queuing, because a node
    // refund is not generation and trickling it in reads as a bug.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Scrap") void GrantScrap(float Amount);

    // Loop overrides, the same keyed push/pop the Momentum loop uses. Scrap has
    // no decay to suspend, so only the generation multiplier is meaningful here
    // and the suspend-decay half of Momentum's signature is deliberately absent
    // rather than present-and-ignored. FIELD ASSEMBLY does not push one of
    // these: its rewrite is a deployable DENSITY CAP override, which lives on a
    // system that does not exist (O30).
    UFUNCTION(BlueprintCallable, Category="Scrap|Loop") void PushLoopOverride(FName Key, float GenerationMultiplier, float Duration);
    UFUNCTION(BlueprintCallable, Category="Scrap|Loop") void PopLoopOverride(FName Key);
    UFUNCTION(BlueprintPure, Category="Scrap|Loop") float GetGenerationMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Scrap|Loop") int32 GetActiveLoopOverrideCount() const;

    // Pure loop rules, exposed for tests and for the eventual DA_ScrapPolicy
    // asset that will own these numbers — the same escape hatch the Mana and
    // Momentum components carry.
    // The threshold-taking form is the real rule; the one-argument form is the
    // convenience wrapper carrying the shipped seeds, and exists because a
    // caller with no component in hand still has to be able to ask. The
    // component itself always calls the explicit form with its own EditAnywhere
    // tunables, so a designer moving a band in the editor actually moves it —
    // the alternative, literals inside the static, is how a tunable becomes
    // decoration.
    static EBreakerScrapState StateForFraction(float Fraction);
    static EBreakerScrapState StateForFraction(float Fraction, float StockedAt, float SurplusAt);
    static float ComposeGenerationMultipliers(const TArray<float>& Multipliers);
    static bool IsLoopOverrideExpired(double ExpiryTime, double Now);
    static float ClampGeneration(float RequestedRate, float GlobalCap);
    // Overflow is DISCARDED, not banked (§1.2): there is no stored-surplus
    // mechanic and no node grants one, because a bank above the cap lets a
    // Gunsmith arrive at a boss with two full deployable sets.
    static float ClampToBank(float Value, float MaxScrap);
    // A kill credits at the KILLING INSTANCE'S proc coefficient, so a DoT tick
    // that lands the kill pays at its coefficient and Tick Frequency cannot
    // become a Scrap engine (§1.1, Master 6.4).
    static float KillGeneration(float KillGrant, float ProcCoefficient);
    static float ReloadGeneration(bool bAnyRoundFired, float ReloadGrant);
    static float MagazineDumpGeneration(bool bStartedFull, float DumpGrant);
    // REFUND, NOT PROFIT — and identical whether the deployable expired, was
    // shot, was culled by the density cap, or was recalled by a node. One
    // destruction path, one refund path.
    static float DestructionRefund(float DeployableScrapCost, float RefundFraction);
    // Credited on damage ACTUALLY APPLIED to health/shield: overkill pays
    // nothing, matching the post-mitigation principle Grit is built on.
    static float DeployableDamageGeneration(float DamageAppliedToHealth, float DamagePerScrap);
    static bool CanSpendFrom(float Scrap, float Cost);

    void BindAttributes(UBreakerAttributeSet* InAttributes);

    UPROPERTY(BlueprintAssignable, Category="Scrap") FBreakerScrapStateChanged OnScrapStateChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Generation", meta=(ClampMin="0")) float KillGrant = 12.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Generation", meta=(ClampMin="0")) float ReloadGrant = 4.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Generation", meta=(ClampMin="0")) float MagazineDumpGrant = 8.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Generation", meta=(ClampMin="0", ClampMax="1")) float DestructionRefundFraction = 0.5f;   // O2 PLACEHOLDER
    // "+1 Scrap per 500 damage dealt by your deployables" (§1.1) expressed as
    // the divisor it actually is, so the acceptance invariant — a deployable
    // must never generate more Scrap over its life than it cost — is one
    // division rather than a hidden constant.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Generation", meta=(ClampMin="1")) float DeployableDamagePerScrap = 500.0f;   // O2 PLACEHOLDER
    // Per-deployable internal cooldown on the damage source (§1.1): four
    // turrets are four independent trickles. NOT ENFORCED HERE — the ICD is
    // per-deployable and this component has no deployable to key it to (O30).
    // Recorded as the tunable it will be so the number does not get reinvented.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Generation", meta=(ClampMin="0")) float DeployableDamageInterval = 0.5f;   // O2 PLACEHOLDER
    // Lower than Swift's 25/s and Caster's 20/s because Gunsmith generation is
    // kill- and damage-weighted, and a dense pack would otherwise refund a full
    // deployable set mid-wave.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Generation", meta=(ClampMin="0")) float GlobalGenerationCap = 15.0f;   // O2 PLACEHOLDER

    // Band thresholds as fractions of MaxClassResource, not absolutes, so a
    // Maximum Resource roll does not accidentally move what Surplus means
    // (the rule Tank states explicitly for IRONCLAD and every band obeys).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Bands", meta=(ClampMin="0", ClampMax="1")) float StockedFraction = 0.25f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scrap|Bands", meta=(ClampMin="0", ClampMax="1")) float SurplusFraction = 0.60f;   // O2 PLACEHOLDER

    // Public for the same reason the Mana and Momentum equivalents are:
    // BeginPlay is the only thing that binds the delegate, the suite has no
    // world, and this contract has silently broken once already elsewhere.
    UFUNCTION() void HandleProgressionChanged();

private:
    bool IsInSafeZone() const;
    void ApplyScrapDelta(float Delta);
    void RefreshState();
    void QueueGrant(float Amount);

    struct FLoopOverrideEntry
    {
        float GenerationMultiplier = 1.0f;
        double ExpiryTime = -1.0;   // Negative = no expiry; popped explicitly.
    };
    mutable TMap<FName, FLoopOverrideEntry> LoopOverrides;
    void PruneLoopOverrides() const;

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    bool bIsGunsmith = false;
    EBreakerScrapState CachedState = EBreakerScrapState::Dry;
    float PendingGrants = 0.0f;
};
