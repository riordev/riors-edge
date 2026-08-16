#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerSupportAbilities.generated.h"

class ABreakerZoneActor;
class UBreakerChargeComponent;
class UBreakerCombatComponent;

// ---------------------------------------------------------------------------
// THE SUPPORT KIT (Class-Kits-Support §3), built to PLAYABLE as O2
// placeholders under the owner's 2026-08-16 authorization.
//
// THE SOLO CASE IS THE FIRST-CLASS CASE. The party layer does not exist — no
// ally, no party membership, no buff propagation — so every ally-facing half
// below is honestly unreachable and every self-facing half is what runs. That
// is the reverse of the order the design implies (Class-Kits-Unbuilt §5.2) and
// it is exactly what §2's solo-viability section demands work anyway: every
// ability targets SELF at full value when no ally is under the crosshair, and
// the valid-target set always includes the Support, unconditionally.
//
// Costs/cooldowns live on the registry rows (quoted from §3). Magnitudes here
// are O2 PLACEHOLDER; doc-seeded rows cited per line.
// ---------------------------------------------------------------------------

// Shared base: while CONDUIT's window is open, Support ability costs multiply
// by the window's payload — 0 for the base ultimate (free casts), 1.0 under
// Triage/Blackout, which replace casting rather than enabling it. The same
// window-payload price rule UBreakerCasterAbility uses for Unmake, so the two
// cost-rewriting ultimates cannot drift apart in mechanism.
UCLASS(Abstract)
class RIORSEDGE_API UBreakerSupportAbility : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    static FName ConduitWindowKey();

    // Pure rule: authored cost times the live window scalar, never negative.
    static float CostUnderConduit(float AuthoredCost, float WindowScalar);

    virtual float GetResourceCost() const override;

    // The ally-or-self resolution every targeted Support verb uses (§3): the
    // ABreakerCharacter under the crosshair within RangeCm, or the caster.
    // Self-cast value is identical to ally-cast value by construction — one
    // code path, no self branch.
    static AActor* ResolveAllyTarget(ABreakerCharacter* Caster, float RangeCm);

    // Recomputes the count-independent buff-uptime bool from the live windows
    // (Cadence, Metronome). A BOOL, never a count: five buffs pay what one
    // pays (the anti-farm shape the Charge component's setter enforces).
    static void RefreshBuffUptime(ABreakerCharacter* Character);
};

// U1 Patch (§3 U1, starter, Medic): instant heal on the ally under the
// crosshair within 20 m, or on self with no target, at identical value. Heals
// a percentage of the TARGET'S maximum health, so it is equally meaningful on
// a Tank and on a Caster. Overheal is discarded and generates nothing.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Patch : public UBreakerSupportAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Patch();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Patch", meta=(ClampMin="0")) float TargetRangeCm = 2000.0f;   // §U1: 20 m
    // O2 PLACEHOLDER: §U1 authors "a percentage of the target's maximum
    // health" and never the percentage.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Patch", meta=(ClampMin="0", ClampMax="1")) float HealFractionOfTargetMax = 0.25f;   // O2 PLACEHOLDER
};

// U2 Purge (§3 U2, Medic): strips every status from the target (self with no
// target) and generates per status actually removed. THE IMMUNITY WINDOW IS
// RECORDED ABSENT: §U2 calls the 3s status immunity "the whole value" and no
// immunity primitive exists on the status component — the cleanse ships, the
// immunity stands as the recorded gap, and the row's WindowDuration (3s)
// keeps the number pinned for the day the primitive lands.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Purge : public UBreakerSupportAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Purge();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Purge", meta=(ClampMin="0")) float TargetRangeCm = 2000.0f;   // O2 PLACEHOLDER (mirrors Patch)
};

// U3 Cadence (§3 U3, Conductor): an 8s aura improving reload and swap tempo
// for everyone inside, starting with the Support. WHAT SHIPS: the buff WINDOW
// itself — it follows the Support, drives the count-independent uptime source,
// and reads on the HUD. WHAT IS RECORDED ABSENT: the tempo payload. No
// reload-speed or swap-speed multiplier exists anywhere on the weapon
// component, and this pass's weapon territory is the magazine hooks only. The
// buff is real and its payload is a named hole, not a fake.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Cadence : public UBreakerSupportAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Cadence();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName WindowKey();

private:
    FTimerHandle WindowTimer;
    bool bCadenceActive = false;
};

// U4 Metronome (§3 U4, Conductor): an 8s state in which consecutive weapon
// hits build a cadence ramp, per holder — solo, the Support's own ramp. The
// ramp pays as stacking FLAT weapon damage (flat sum stage, cannot double-dip
// with the Increased bucket), resets on a full second without a hit.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Metronome : public UBreakerSupportAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Metronome();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName WindowKey();
    static FName OutgoingModifierKey();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Metronome", meta=(ClampMin="0")) float FlatDamagePerStack = 2.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Metronome", meta=(ClampMin="1")) int32 MaximumStacks = 5;   // O2 PLACEHOLDER ("to a cap", §U4)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Metronome", meta=(ClampMin="0.1")) float StreakGapSeconds = 1.0f;   // §U4: resets on a full second without a hit

private:
    UFUNCTION() void HandleHitDealt(const FBreakerHitContext& Hit);
    void CloseMetronome();

    FTimerHandle WindowTimer;
    TWeakObjectPtr<UBreakerCombatComponent> BoundCombat;
    bool bMetronomeActive = false;
    int32 Stacks = 0;
    double LastHitTime = -1000.0;
};

// U5 Mark (§3 U5, starter, Warden): paints the target under the crosshair for
// 10s. It takes increased damage from all sources (a keyed incoming modifier
// on the target — every source's damage passes through it, so "including
// allies" is structural), and damage YOU deal to it generates Charge. The
// offensive conversion path, and the loop's deliberately cheap ignition.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Mark : public UBreakerSupportAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Mark();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName IncomingModifierKey();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mark", meta=(ClampMin="0")) float TargetRangeCm = 3000.0f;   // O2 PLACEHOLDER (§U5 authors no range)
    // O2 PLACEHOLDER: "takes increased damage" with no magnitude authored.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mark", meta=(ClampMin="1")) float MarkedDamageMultiplier = 1.15f;   // O2 PLACEHOLDER

private:
    UFUNCTION() void HandleHitDealt(const FBreakerHitContext& Hit);
    void CloseMark();

    FTimerHandle MarkTimer;
    TWeakObjectPtr<UBreakerCombatComponent> BoundCombat;
    TWeakObjectPtr<AActor> MarkedTarget;
    bool bMarkActive = false;
};

// U6 Suppress (§3 U6, Warden): a 6s zone, 6 m radius, at the aim point.
// Enemies inside are slowed; it deals no damage at all. THE ACCURACY HALF IS
// RECORDED ABSENT: enemy fire has no accuracy or spread concept to reduce, so
// the slow is the whole implementable payload today.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Suppress : public UBreakerSupportAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Suppress();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    UFUNCTION() void HandleOccupantEntered(AActor* Occupant);
    UFUNCTION() void HandleOccupantExited(AActor* Occupant);
    UFUNCTION() void HandleZoneExpired();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Suppress", meta=(ClampMin="0")) float AimRangeCm = 3000.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Suppress", meta=(ClampMin="0")) float RadiusCm = 600.0f;   // §U6: 6 m
    // Same enemy movement-profile mutator (and the same recorded Fleetfoot
    // restore limitation) as the Disruptor's slow.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Suppress", meta=(ClampMin="0", ClampMax="1")) float SlowMultiplier = 0.55f;   // O2 PLACEHOLDER

private:
    UPROPERTY() TObjectPtr<ABreakerZoneActor> ActiveZone;
    TArray<TWeakObjectPtr<AActor>> SlowedEnemies;
};

// CONDUIT (§3.1 ultimate): 100 Charge, no cooldown, 12s. For the duration
// every Support ability affects every valid target in radius and costs
// nothing; cooldowns still apply — breadth, not spam. THE VALID-TARGET SET
// ALWAYS INCLUDES THE SUPPORT, and solo (the only case that exists today) the
// set IS the Support, so the base window's whole observable behaviour is the
// free-cast payload plus continued generation.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Conduit : public UBreakerSupportAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Conduit();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName BlackoutModifierKey();
    static FName DownbeatModifierKey();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Conduit", meta=(ClampMin="0")) float RadiusCm = 1500.0f;   // §3.1: 15 m
    // Triage (§3.1): a continuous healing field — every valid target (solo:
    // self) healed each second. THE LETHAL-HIT SAVE IS RECORDED ABSENT: no
    // lethal-prevention hook exists on the damage pipeline.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Conduit", meta=(ClampMin="0", ClampMax="1")) float TriageHealFractionPerSecond = 0.04f;   // O2 PLACEHOLDER
    // Downbeat (§3.1): flat weapon damage for every buffed target (solo: one).
    // FLAT is load-bearing — flat-sum stage, cannot double-dip with gear.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Conduit", meta=(ClampMin="0")) float DownbeatFlatDamagePerBuffedTarget = 4.0f;   // O2 PLACEHOLDER
    // Blackout (§3.1): marks and suppresses every enemy in radius INSTEAD of
    // casting abilities; the marks are yours for generation purposes.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Conduit", meta=(ClampMin="1")) float BlackoutMarkedDamageMultiplier = 1.15f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Conduit", meta=(ClampMin="0", ClampMax="1")) float BlackoutSlowMultiplier = 0.55f;   // O2 PLACEHOLDER

private:
    UFUNCTION() void HandleBlackoutHit(const FBreakerHitContext& Hit);
    void HandleTriagePulse();
    void CloseConduit();

    FTimerHandle WindowTimer;
    FTimerHandle TriageTimer;
    TWeakObjectPtr<UBreakerCombatComponent> BoundCombat;
    TArray<TWeakObjectPtr<AActor>> BlackoutTargets;
    bool bConduitActive = false;
};
