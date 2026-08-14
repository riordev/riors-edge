#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/BreakerProgressionTypes.h"
#include "Weapons/BreakerWeaponComponent.h"
#include "BreakerManaComponent.generated.h"

class UBreakerAttributeSet;
class UBreakerProgressionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerOvercastChanged, bool, bOvercast);

// Caster's Mana loop.
//
// OWNER RULING 2026-08-14 INVERTED THIS: "Caster's mana bar should be full and
// go down when using spells, and affixes like resource efficiency and resource
// regeneration should exist." It supersedes Class-Kits §2.1's accumulating-bank
// model, in which the bar started EMPTY and the class fantasy was "the Caster
// shoots to pay for spells". The bar now starts FULL, spends DOWN, and
// REGENERATES back up; passive regeneration is the primary recovery path and
// the conditional sources (weapon hits, weak points, kills, status
// applications, reloads) are ACCELERATORS on top of it, not the income.
//
// What did not change, and must not: the anti-Multishot 1/n pellet rule
// (mandatory, Class-Kits §2.1), DoT ticks generating exactly nothing, and the
// Overcast debt machinery. Only the WEIGHTING moved — see GlobalGenerationCap.
//
// Still the deliberate opposite of Swift's Momentum state machine: nothing
// decays here, and a Caster standing still recovers rather than draining.
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
// OVERCAST IS REACHABLE (spec D8, landed). This component OWNS the floor: it
// writes the ClassResourceFloor attribute to the Overcast floor while the owner
// is a Caster and back to 0 the moment it is not, so
// UBreakerAttributeSet::PreAttributeChange clamps the bank to [Floor, Max] and
// an ordinary GAS cost effect can drive it negative. Ability costs remain
// GameplayEffects; there is no second, non-GAS spend path (spec D3).
// The floor is closed on every class change and re-synced from the tick, so a
// respec, a dev class swap, or a save load cannot strand a character with a
// negative floor — or, worse, with a negative bank they can never climb out of.
UCLASS(ClassGroup=Classes, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerManaComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerManaComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // The whole per-frame loop: class-ownership poll, metered generation, and
    // the Overcast state refresh. Separated from TickComponent because
    // UActorComponent::TickComponent asserts on an unregistered component, and
    // this loop is worth running in an automation test with no world.
    void AdvanceLoop(float DeltaTime);

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
    // Deepens (or restores) the floor at runtime — SB4 and MS10 (spec §5,
    // Class-Kits §2.1). Re-publishes the attribute immediately, so a node that
    // shallows the floor cannot leave the bank below it.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mana") void SetOvercastFloor(float Floor);
    // The floor this component is currently publishing to the attribute set:
    // the Overcast floor while the owner is a Caster, and 0 otherwise. This is
    // the value that actually gates spending, because it is what the attribute
    // clamp and the ability CheckCost both read.
    UFUNCTION(BlueprintPure, Category="Mana") float GetPublishedFloor() const { return IsActiveForOwner() ? GetOvercastFloor() : 0.0f; }
    // Binds the attribute set directly. BeginPlay resolves it from the owner's
    // ability system; this is the same wiring for a component built outside a
    // world (the pattern equipment and progression already use).
    void BindAttributes(UBreakerAttributeSet* InAttributes);
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

    // Fills the bank to MaxClassResource. The owner ruling's first clause: a
    // fresh Caster can cast immediately. Called when the owner BECOMES a Caster
    // (class lock, save load, dev swap) and on every vitals restore — the
    // playtest reset and respawn path — so "start full" holds after F1 as well
    // as at spawn. No-op for any other class: nothing may fill a Swift's bar,
    // which is earned by moving.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mana") void FillToMaximum();

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

    // THE primary recovery path under the 2026-08-14 owner ruling, and the one
    // number to turn when a Caster feels resource-starved or resource-free.
    //
    // O2 PLACEHOLDER 6.0/s. Derived, not guessed: Class-Kits §2.1 authored
    // +2.0/s as a FLOOR under an accumulating loop (a full 100 bar in 50s,
    // "usable but never sufficient"), and §2.7's first acceptance criterion
    // reads one 25-cost cast every ~13s from regeneration alone. As the primary
    // path that is far too slow — it is a floor being asked to be a ceiling.
    // 6.0/s refills the bar in ~17s and sustains one mid-cost (30 Mana) cast
    // every 5s with no target present, which is a cadence a player can actually
    // fight at while still making a three-cast burst something they have to
    // stop and recover from. §2.7 criterion 1 needs restating against this.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Generation", meta=(ClampMin="0")) float PassiveRegenPerSecond = 6.0f; // O2 PLACEHOLDER
    // Unchanged from Class-Kits §2.1 on purpose. The per-source magnitudes are
    // what SB1/SB3/VW1/MS1 and §2.7's shotgun-vs-rifle criterion are authored
    // against, and the anti-Multishot 1/n ratio lives between them — moving
    // them would silently rewrite a dozen branch nodes. The re-weighting the
    // ruling asks for is done at the CAP below instead: one number, and every
    // relative rate the acceptance criteria measure is preserved.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Generation", meta=(ClampMin="0")) float WeaponHitGain = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Generation", meta=(ClampMin="0")) float WeakPointGain = 4.0f;
    // O2 PLACEHOLDER 6.0/s, was 20.0/s. This is the re-weight: it is the
    // ceiling on how much CONDITIONAL income can arrive per second, and at par
    // with PassiveRegenPerSecond it says exactly what the ruling asks — a
    // Caster who fights well recovers at most twice as fast as one who stands
    // still, so the weapon is an accelerator rather than the income. At the old
    // 20/s a rifle out-earned the new regeneration threefold and the bar would
    // have inverted straight back into a bank that happened to start full.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Generation", meta=(ClampMin="0")) float GlobalGenerationCap = 6.0f; // O2 PLACEHOLDER

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Overcast", meta=(ClampMax="0")) float OvercastFloor = -20.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Overcast", meta=(ClampMin="1")) float OvercastGenerationMultiplier = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana|Overcast", meta=(ClampMin="0")) float OvercastIncomingDamageTaken = 0.15f;

    // Public for the same reason the Momentum component's is: BeginPlay is the
    // only thing that binds it, the suite has no world, and the contract it
    // implements (a class change re-syncing the floor) silently broke once
    // already. Idempotent; calling it spuriously costs a component lookup.
    UFUNCTION() void HandleProgressionChanged();

private:
    UFUNCTION() void HandleShot(const FBreakerShotResult& Shot);
    UFUNCTION() void HandleVitalsRestored();
    // Binds the owner's shot and vitals-restore events. Called from BeginPlay
    // and from BindAttributes, because a component wired up outside a world
    // never gets a BeginPlay and would otherwise miss the reset path entirely.
    // Guarded against double-binding: AddDynamic on an already-bound delegate
    // is a duplicate-listener bug, not a no-op.
    void BindOwnerEvents();

    bool IsInSafeZone() const;
    void ApplyManaDelta(float Delta);
    void RefreshOvercastState();
    // Writes GetPublishedFloor() to the attribute set. Called from every place
    // the answer can change (bind, class change, SB4-style floor edits), and it
    // is the attribute write that raises a stranded negative bank back to the
    // new floor.
    void SyncClassResourceFloor();
    // Re-reads the owner's permanent class and re-syncs everything that depends
    // on it. Bound to OnProgressionChanged AND polled from the tick, because
    // UBreakerProgressionComponent::DevForceClass changes the class without
    // broadcasting — a dev class swap out of Caster must not leave the floor
    // open on a Swift.
    void RefreshClassOwnership();
    // Pushes or pops the Overcast incoming-damage penalty on the owner's combat
    // component. Called from exactly one place (RefreshOvercastState) so the
    // penalty can never outlive the debt that justified it.
    void SyncOvercastDamagePenalty();

    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    TWeakObjectPtr<UBreakerProgressionComponent> CachedProgression;

    bool bIsCaster = false;
    bool bOvercast = false;
    // Last class this component synced against, so the tick poll is a cheap
    // comparison rather than a resync every frame.
    EBreakerClassId ObservedClass = EBreakerClassId::None;
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
