#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerManaComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerProgressionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerOvercastChanged, bool, bOvercast);

// Caster's Mana loop: an accumulating bank, the deliberate opposite of Swift's
// Momentum state machine (Class-Kits 2.1). Weapons generate, spells spend, and
// nothing decays — a banked bar is never taken back by standing still.
// Server-authority only; inert unless the owner's permanent class is Caster.
//
// Overcast (Class-Kits 2.1, Ability-Implementation-Spec 1.8) is grounded here
// only as far as this component owns it: the bank may be driven to a negative
// OvercastFloor, generation doubles while negative, and the incoming-damage
// penalty is *published* for the combat side to read.
// COMBAT WIRED: entering and leaving Overcast pushes/removes a keyed entry on
// UBreakerCombatComponent's incoming-damage chain, so the +15% lands in the
// same stage as gear-rolled reduction, before armour and shields. This
// component still never touches a damage request itself.
// AWAITING ATTRIBUTES — THE OVERCAST BLOCKER: UBreakerAttributeSet::
// PreAttributeChange clamps ClassResource to [0, Max], and GAS ability costs
// are GameplayEffects, which pass through that clamp. So no ability can
// currently drive the bank negative, which means the whole Overcast path here
// (doubling, the damage penalty, the cast lockout) is correct but never
// entered in play. The fix is the ClassResourceFloor attribute in
// Ability-Implementation-Spec 1.8 (D8), which lives in Attributes/. Do not
// work around it with a second, non-GAS spend path (spec D3).
UCLASS(ClassGroup=Classes, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerManaComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerManaComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintPure, Category="Mana") bool IsActiveForOwner() const;
    UFUNCTION(BlueprintPure, Category="Mana") float GetMana() const;
    UFUNCTION(BlueprintPure, Category="Mana") float GetManaFraction() const;
    UFUNCTION(BlueprintPure, Category="Mana") bool IsOvercast() const;
    // The fraction of extra damage the owner should take while Overcast, e.g.
    // 0.15 for +15%. Zero when not Overcast. Combat consumes this later; this
    // component only publishes it.
    UFUNCTION(BlueprintPure, Category="Mana") float GetOvercastIncomingDamageTaken() const;
    // Deepest the bank may be driven; negative. Spellblade SB4 lowers it.
    UFUNCTION(BlueprintPure, Category="Mana") float GetOvercastFloor() const { return FMath::Min(0.0f, OvercastFloor); }
    // "No further ability may be cast until Mana is at or above zero"
    // (Class-Kits 2.1). The tag-driven form lives in the ability layer; this is
    // the authoritative predicate it will mirror.
    UFUNCTION(BlueprintPure, Category="Mana") bool CanAffordSpend(float Cost) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mana") bool TrySpendMana(float Cost);
    // Direct credit. bIgnoreGlobalCap pays it this frame instead of metering it
    // through the 20/s budget: refunds and payback are not generation, and
    // trickling them in reads as a bug. Overcast doubling still applies, since
    // clearing a debt faster is exactly what the doubling is for.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mana") void GrantMana(float Amount, bool bIgnoreGlobalCap);

    // Generation suspension (Class-Kits §2.2 UNMAKE: "Mana generation is
    // suspended"). Keyed push/pop rather than a bool so overlapping sources
    // each revert independently; queued credits are discarded on push, not
    // banked for later, or the bar would jump when the window closes.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mana") void PushGenerationSuspension(FName Key);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mana") void PopGenerationSuspension(FName Key);
    UFUNCTION(BlueprintPure, Category="Mana") bool IsGenerationSuspended() const { return GenerationSuspensions.Num() > 0; }

    // Pure loop rules, exposed for tests and for the eventual DA_ManaPolicy
    // asset that will own these numbers.
    //
    // Anti-Multishot clause (Class-Kits 2.1, mandatory): each pellet banks
    // 1/n of the weapon-hit gain, so a shotgun and a rifle bank at comparable
    // rates. A weak point replaces the weapon-hit gain for the pellet that
    // scored it and does not stack with it. ProcCoefficient of 0 — what a DoT
    // tick carries — generates exactly nothing.
    static float HitGeneration(bool bWeakPoint, int32 LandedPellets, int32 PelletsPerShot, float WeaponHitGain, float WeakPointGain, float ProcCoefficient);
    static float ClampGeneration(float RequestedAmount, float GlobalCap);
    static bool IsOvercastValue(float Mana);
    // Generation is doubled while the bank is negative, until it returns to zero.
    static float GenerationMultiplierForMana(float Mana, float OvercastMultiplier);
    static float ClampToBank(float Value, float Floor, float MaxMana);
    static bool CanSpendFrom(float Mana, float Cost, float Floor);

    UPROPERTY(BlueprintAssignable, Category="Mana") FBreakerOvercastChanged OnOvercastChanged;

    // O2 placeholders, straight from Class-Kits 2.1. No passive regeneration
    // and no decay: this slice ships the conditional bank only, and the +2.0/s
    // idle trickle is deliberately deferred with the rest of the O2 re-anchor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Generation", meta=(ClampMin="0")) float WeaponHitGain = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Generation", meta=(ClampMin="0")) float WeakPointGain = 4.0f;
    // Lower than Swift's 25 because Caster generation is target-dependent and a
    // dense pack would otherwise fill the bar instantly.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Generation", meta=(ClampMin="0")) float GlobalGenerationCap = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Overcast", meta=(ClampMax="0")) float OvercastFloor = -20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Overcast", meta=(ClampMin="1")) float OvercastGenerationMultiplier = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Overcast", meta=(ClampMin="0")) float OvercastIncomingDamageTaken = 0.15f;

private:
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    UFUNCTION() void HandleProgressionChanged();

    bool IsInSafeZone() const;
    void ApplyManaDelta(float Delta);
    void RefreshOvercastState();
    // Pushes or pops the Overcast incoming-damage penalty on the owner's combat
    // component. Called from exactly one place (RefreshOvercastState) so the
    // penalty can never outlive the debt that justified it.
    void SyncOvercastDamagePenalty();

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    bool bIsCaster = false;
    bool bOvercast = false;
    float PendingGrants = 0.0f;
    TSet<FName> GenerationSuspensions;

public:
    // The key this component uses on UBreakerCombatComponent's incoming-damage
    // chain. Exposed so a test can assert the wiring without a world.
    static FName OvercastDamageModifierKey();
    // Pure rule: the incoming-damage multiplier for a given Overcast penalty.
    // 0.15 means the Caster takes 1.15x. Never below 1 — Overcast is a cost.
    static float OvercastIncomingMultiplier(bool bOvercastNow, float PenaltyFraction);
};
