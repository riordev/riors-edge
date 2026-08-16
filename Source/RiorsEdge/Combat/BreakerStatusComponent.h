#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerStatusComponent.generated.h"

class UBreakerCombatComponent;

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerActiveStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FBreakerStatusApplicationSpec Spec;
    UPROPERTY(BlueprintReadOnly) EBreakerDamageFamily DamageFamily = EBreakerDamageFamily::Physical;
    UPROPERTY(BlueprintReadOnly) int32 Stacks = 1;
    UPROPERTY(BlueprintReadOnly) float RemainingDuration = 0.0f;
    UPROPERTY(BlueprintReadOnly) float TimeUntilNextTick = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 TicksDelivered = 0;
    // Who applied this status. Weak: a DoT outliving its applier keeps
    // ticking, it just stops crediting anyone.
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Instigator = nullptr;
    // WHERE the applier stood when the status landed, snapshotted at
    // application exactly like the offensive stats in Spec.Snapshot. Every
    // tick's damage request carries this as its source location so the
    // facing-armour step judges the tick from the angle the wound was
    // inflicted at — without it MakeSnapshotDotTick set no source location,
    // the combat component skipped the facing multiplier entirely, and a
    // Bleed applied to a Warden's exposed back ate the frontal mitigation.
    UPROPERTY(BlueprintReadOnly) FVector SourceLocationSnapshot = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bHasSourceLocationSnapshot = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerStatusEvent, const FBreakerActiveStatus&, Status);

// Runs active damage-over-time statuses on the owning actor. Bleed and
// Poison are physical, bypass shields, and take half armour mitigation —
// all of that is already encoded in the damage request each tick builds.
// Ticks use the snapshot taken at application; the tick interval is part of
// that snapshot, consistent with the DoT contract.
UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerStatusComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerStatusComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Reapplying a status the target already has adds stacks (capped) and
    // refreshes duration, but keeps the ORIGINAL snapshot: an application
    // either critically ticks for its whole lifetime or never does.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
    // Instigator is remembered weakly so every tick this application produces
    // credits the applier through the attacker-side hit events.
    void ApplyStatus(const FBreakerStatusApplicationSpec& Spec, EBreakerDamageFamily DamageFamily, AActor* Instigator);

    UFUNCTION(BlueprintPure, Category="Combat|Status") const TArray<FBreakerActiveStatus>& GetActiveStatuses() const { return ActiveStatuses; }
    UFUNCTION(BlueprintPure, Category="Combat|Status") bool HasStatus(FGameplayTag StatusTag) const;

    // --- Consumption ------------------------------------------------------
    // Reading a status is not the same verb as CONSUMING one, and until this
    // block existed the component could only be read. That is the difference
    // between status as a damage-over-time footnote and status as a build axis:
    // Resonance detonates, Multispell sequences, and Affliction deepens, and
    // all three need to take a status away and be told exactly what they took.

    // Distinct TYPES, never stacks. Class-Kits C6 makes this an explicit
    // anti-stacking rule: Resonance scales on how many different statuses are
    // on the target, so a 10-stack Bleed counts once.
    UFUNCTION(BlueprintPure, Category="Combat|Status") int32 GetDistinctStatusTypeCount() const;

    // Removes everything and RETURNS what was removed, so the detonator can
    // compute its damage from the same data it just destroyed rather than
    // reading the list twice and racing itself. Returns an empty array when
    // there was nothing to consume — consuming nothing is a legal no-op, not an
    // error, because Cascade and Resonance both fire into whatever is there.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
    TArray<FBreakerActiveStatus> ConsumeAllStatuses();

    // Single-type consumption for conversion effects. bOutFound distinguishes
    // "consumed a status worth nothing" from "there was no such status".
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
    FBreakerActiveStatus ConsumeStatus(FGameplayTag StatusTag, bool& bOutFound);

    // MS8's rewrite: do not consume, shorten. A scalar of 0 removes outright,
    // which is the same observable outcome as consumption and keeps the two
    // paths from disagreeing about what "gone" means.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
    void ScaleRemainingDurations(float Scalar);

    // Affliction A1 Deepen. Additive against the authored cap; lowering the cap
    // trims live stacks immediately so the readout cannot show more stacks than
    // the cap allows.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
    void SetStackCapDelta(int32 Delta);

    UFUNCTION(BlueprintPure, Category="Combat|Status") int32 GetEffectiveStackCap() const;

    // --- Ailment avoidance (defense triad, owner ruling 2026-08-16/17) ----
    // A chance to refuse a status infliction ENTIRELY, rolled exactly once
    // per application at the top of ApplyStatus — BEFORE the immunity
    // primitive, which stays the absolute refusal it always was. The roll is
    // seeded/deterministic like dodge (an FRandomStream over the application
    // seed plus a salt, never FMath::FRand), and it exists only at the
    // application door: ticks of a status that already landed never re-roll,
    // and a refused application refuses everything — the DoT, the stack add,
    // the refresh — because "avoided" means the wound never happened.
    //
    // The chance is baseline-plus-gear, the DodgeChance pattern on the combat
    // component: classes/tests set the baseline here, gear supplies
    // FBreakerEquipmentStats::AilmentAvoidanceChancePercent (already capped
    // at aggregation), and the composed total is clamped below 1 so gear can
    // never rebuild GrantStatusImmunity out of suffixes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status", meta=(ClampMin="0", ClampMax="1"))
    float AilmentAvoidanceChance = 0.0f;

    // The composed ceiling. O2 PLACEHOLDER, and deliberately the SAME number
    // as the gear cap (FBreakerEquipmentStats::AilmentAvoidanceCapPercent /
    // 100): one ceiling, stated twice only because Items/ cannot be included
    // from this header — RiorsEdge.Combat.DefenseTriad.AvoidanceGate pins
    // that the two agree so they cannot drift apart silently.
    static constexpr float MaxAilmentAvoidanceChance = 0.75f;

    // Baseline + gear, clamped. Pure and world-free so the HUD, the tooltip
    // and the tests all read the number the roll actually uses.
    UFUNCTION(BlueprintPure, Category="Combat|Status")
    float GetEffectiveAilmentAvoidanceChance() const;

    // --- Immunity ---------------------------------------------------------
    // The status-immunity primitive Purge's §U2 window was recorded absent
    // without (2026-08-16, built for Medic MD7 Field Kit — a cleanse rule,
    // which is why it lives here). While the window is open, NEW status
    // applications are refused outright; statuses already running are
    // untouched (cleansing them is the cleanse's own job). Refreshes, never
    // stacks: a second grant extends to the longer remaining window.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Status")
    void GrantStatusImmunity(float DurationSeconds);

    UFUNCTION(BlueprintPure, Category="Combat|Status") bool IsStatusImmune() const { return StatusImmunityRemaining > 0.0f; }

    // Drives the DoT clocks. Called from TickComponent, and directly by tests
    // so the whole component is exercisable without a world — the same
    // precedent as UBreakerAbilityStateComponent::AdvanceTime.
    UFUNCTION(BlueprintCallable, Category="Combat|Status") void AdvanceStatuses(float DeltaSeconds);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Status", meta=(ClampMin="1")) int32 MaximumStacksPerStatus = 10;
    UPROPERTY(BlueprintAssignable, Category="Combat|Status") FBreakerStatusEvent OnStatusApplied;
    UPROPERTY(BlueprintAssignable, Category="Combat|Status") FBreakerStatusEvent OnStatusExpired;
    // Deliberately NOT OnStatusExpired: a listener must be able to tell a
    // detonation from a status that simply ran out. Support's Charge loop and
    // the Affliction nodes both care about the difference.
    UPROPERTY(BlueprintAssignable, Category="Combat|Status") FBreakerStatusEvent OnStatusConsumed;
    // Raised when the avoidance roll refuses an application, carrying a
    // transient status built from the refused spec (it was never added to the
    // list). A refused ailment that refuses SILENTLY reads as a broken status
    // system, so the HUD gets a hook — the project's "visible tell" rule.
    UPROPERTY(BlueprintAssignable, Category="Combat|Status") FBreakerStatusEvent OnStatusAvoided;

private:
    UPROPERTY() TArray<FBreakerActiveStatus> ActiveStatuses;
    UPROPERTY() TObjectPtr<UBreakerCombatComponent> Combat;
    // Additive against MaximumStacksPerStatus. Separate from the authored cap
    // so a node granting +2 stacks does not permanently rewrite the value an
    // owner authored.
    int32 StackCapDelta = 0;
    float StatusImmunityRemaining = 0.0f;
    // The avoidance roll's application ordinal. FBreakerStatusApplicationSpec
    // carries no seed field (it lives in Progression/, another lane's file),
    // so the application seed is derived HERE from what ApplyStatus already
    // receives: the status tag's hash mixed with this counter. That keeps
    // the dodge property — same component, same application sequence, same
    // outcomes, and never a shared engine RNG — while consecutive
    // applications of the same status still roll independently instead of
    // all sharing one verdict. Counted per application ATTEMPT (valid specs
    // only), whether or not it lands, so equipping avoidance mid-fight does
    // not shift the seeds of everything after it.
    uint32 ApplicationsAttempted = 0;
};
