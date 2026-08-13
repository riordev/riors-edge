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
// AWAITING COMBAT: UBreakerCombatComponent must multiply incoming damage by
// (1 + GetOvercastIncomingDamageTaken()) — this component never touches damage.
// AWAITING ATTRIBUTES: UBreakerAttributeSet::PreAttributeChange still clamps
// ClassResource at 0, so GAS-routed spends cannot go negative yet; the
// ClassResourceFloor attribute in Ability-Implementation-Spec 1.8 is the fix.
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

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    bool bIsCaster = false;
    bool bOvercast = false;
    float PendingGrants = 0.0f;
};
