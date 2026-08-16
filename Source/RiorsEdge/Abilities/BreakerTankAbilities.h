#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerGameplayAbility.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerTankAbilities.generated.h"

class UBreakerCombatComponent;

// ---------------------------------------------------------------------------
// THE TANK KIT (Class-Kits-Tank §2), built to PLAYABLE as O2 placeholders
// under the owner's 2026-08-16 authorization. Every Tank ability costs Grit
// AND carries a cooldown, and the 5-12s cooldown band is the longest in the
// game because the invulnerability audit leans on cooldown LENGTH as its
// primary guard — both facts live on the registry rows, not here.
//
// Costs/cooldowns are on the fallback registry (quoted from §2); every
// magnitude in this file is O2 PLACEHOLDER, doc-seeded rows cited per line.
// ---------------------------------------------------------------------------

// T1 Rend (§2 T1, starter, Leech): melee sweep, 3 m arc; heals for 35% of
// post-mitigation damage dealt; overheal converts to shield. Melee kills feed
// the Grit loop's aggression source (NotifyMeleeKill) — the one caller that
// source has, because no other melee kill verb exists for a Tank yet.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Rend : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Rend();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // §T1: 3 m arc.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rend", meta=(ClampMin="0")) float RangeCm = 300.0f;   // §T1
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rend", meta=(ClampMin="0", ClampMax="360")) float ArcDegrees = 120.0f;   // O2 PLACEHOLDER (L7 widens to 180, so base is narrower)
    // O2 PLACEHOLDER: "scaled by weapon damage" shape, the Cleave precedent.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rend", meta=(ClampMin="0")) float WeaponDamageCoefficient = 1.3f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rend", meta=(ClampMin="0")) float UnarmedDamage = 25.0f;   // O2 PLACEHOLDER
    // §T1: heals for 35% of post-mitigation damage dealt.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rend", meta=(ClampMin="0", ClampMax="1")) float HealFraction = 0.35f;   // §T1
    // §T1's overheal-to-shield cap is 25% OF MAXIMUM HEALTH; the healing
    // path's cap is a fraction OF MAX SHIELD, and the 4%/s-after-3s shield
    // decay has no primitive at all. Both differences are recorded on the
    // activation site rather than papered over.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rend", meta=(ClampMin="0", ClampMax="1")) float OverhealShieldFraction = 1.0f;   // O2 PLACEHOLDER
};

// T2 Bloodline (§2 T2, Leech): 8s window doubling Life on Hit and extending it
// to DoT ticks. NEAREST HONEST VERSION, recorded plainly: no Life on HIT stat
// exists anywhere (gear authors Life on KILL only), so the window pays the
// gear's LifeOnKill magnitude on every LANDED HIT instead — it still
// multiplies what you have and still grants nothing if you have none, which is
// the ability's whole identity ("a gear payoff"). When a Life on Hit affix
// lands, this should read it instead.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Bloodline : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Bloodline();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName WindowKey();

private:
    UFUNCTION() void HandleHitDealt(const FBreakerHitContext& Hit);
    void CloseBloodline();

    FTimerHandle WindowTimer;
    TWeakObjectPtr<UBreakerCombatComponent> BoundCombat;
    bool bBloodlineActive = false;
    // L11 EXSANGUINATE: the window expires 2s after the last landing hit
    // instead of on the authored clock. NEAREST HONEST FILTER, recorded: the
    // hit context cannot tell a melee swing from a bullet, so any non-DoT hit
    // sustains the window until a melee discriminator exists on the context.
    bool bExsanguinate = false;
};

// T3 Anchor Point (§2 T3, starter, Bastion): a frontal cover panel with its
// own health pool, deployed through the minimal deployable system. Not a Scrap
// object: destroying it refunds nothing.
UCLASS()
class RIORSEDGE_API UBreakerAbility_AnchorPoint : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_AnchorPoint();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // O2 PLACEHOLDER: placed a body-length ahead of the Tank, facing away.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AnchorPoint", meta=(ClampMin="0")) float PlacementRangeCm = 250.0f;
};

// T4 Provoke (§2 T4, Bastion): forces nearby enemies to target you, and each
// one provoked grants stacking FLAT damage.
//
// THE THREAT HALF IS HONESTLY ABSENT: enemy AI has no threat concept at all
// (Class-Kits-Unbuilt §5.3), so there is nothing to force. Solo — the mode the
// slice actually runs — forced targeting is vacuous anyway (every enemy is
// already targeting you). The SOLO CONVERSION is the implemented half: each
// enemy within radius grants a stacking flat damage bonus for the window,
// which is exactly §T4's "real damage window against one enemy".
UCLASS()
class RIORSEDGE_API UBreakerAbility_Provoke : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Provoke();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    static FName OutgoingModifierKey();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Provoke", meta=(ClampMin="0")) float RadiusCm = 1000.0f;   // §T4: 10 m
    // §T4: +4% of weapon base damage per enemy provoked, 6 stacks max, 6s.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Provoke", meta=(ClampMin="0", ClampMax="1")) float FlatDamagePerEnemyFraction = 0.04f;   // §T4 placeholder
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Provoke", meta=(ClampMin="1")) int32 MaximumStacks = 6;   // §T4
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Provoke", meta=(ClampMin="0")) float BonusDurationSeconds = 6.0f;   // §T4
};

// T5 Breach Charge (§2 T5, Demolitionist): thrown explosive, 1.2s fuse, 5 m
// radius. Strong self-knockback with full directional control; self-damage
// heavily reduced and NEVER zero (O13) — it routes through the Tank's own
// combat component as a real self-instigated hit, so the Grit loop's
// self-damage source (25% rate, its own sub-cap) sees it honestly.
UCLASS()
class RIORSEDGE_API UBreakerAbility_BreachCharge : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_BreachCharge();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0")) float ThrowRangeCm = 700.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0")) float FuseSeconds = 1.2f;   // §T5
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0")) float BlastRadiusCm = 500.0f;   // §T5: 5 m
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0")) float WeaponDamageCoefficient = 1.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0")) float UnarmedDamage = 40.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0", ClampMax="1")) float EdgeDamageFraction = 0.35f;   // O2 PLACEHOLDER
    // §T5: base self-damage reduction placeholder 50%, floor never below 20%
    // of the damage an enemy at the same distance would take.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0", ClampMax="1")) float SelfDamageFraction = 0.5f;   // §T5 placeholder
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="BreachCharge", meta=(ClampMin="0")) float KnockbackImpulse = 1400.0f;   // O2 PLACEHOLDER

    // D7 DEMOLITION: two charges sharing one cooldown. CheckCooldown admits one
    // extra cast while the first cooldown runs; ApplyCooldown declines to
    // restart the timer for that cast, so both charges return together when the
    // ONE cooldown expires — "sharing one cooldown", literally.
    virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
    void Detonate(FVector BlastLocation);
    FTimerHandle FuseTimer;
    // Demolition bookkeeping: true while the second charge has been spent into
    // a still-running cooldown. Mutable because CheckCooldown is const.
    mutable bool bDemolitionSecondSpent = false;
};

// T6 Ground Zero (§2 T6, Demolitionist): usable only while airborne; slams
// down, radial damage scaled by fall, applies Stagger.
//
// TWO NEAREST-HONEST SUBSTITUTIONS, recorded:
//  * "scaled by fall distance (cap 12 m)" reads the CURRENT DOWNWARD SPEED
//    instead — fall distance is not tracked anywhere and inventing a tracker
//    for one ability is a bigger lie than reading the honest kin quantity.
//  * ApplyStagger does not exist (Class-Kits-Unbuilt §5.3). The nearest
//    primitive is a full movement stop through the enemy's public
//    movement-profile mutator, restored on a timer. A stop is not a stagger —
//    no animation break, no attack interruption — and that difference stands
//    recorded here until a real stagger lands.
UCLASS()
class RIORSEDGE_API UBreakerAbility_GroundZero : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_GroundZero();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="0")) float BlastRadiusCm = 600.0f;   // §T6: 6 m
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="0")) float WeaponDamageCoefficient = 1.8f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="0")) float UnarmedDamage = 50.0f;   // O2 PLACEHOLDER
    // Downward speed that maps to full damage; a normal jump's fall reaches a
    // meaningful fraction, which is what makes rocket-jumping expressive
    // rather than mandatory (§T6).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="1")) float FullPowerFallSpeed = 1200.0f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="0", ClampMax="1")) float MinimumPowerFraction = 0.4f;   // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="0")) float StaggerSeconds = 1.5f;   // §T6
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="0")) float SlamDownSpeed = 2200.0f;   // O2 PLACEHOLDER
    // D8 TERMINAL DESCENT's raised power ceiling under the speed stand-in:
    // sqrt(25/12) ~= 1.44 — the fall speed a 25 m drop reaches relative to the
    // 12 m cap's, so the node's "cap 25 m instead of 12" survives the honest
    // kin quantity. O2 PLACEHOLDER like the rest of the scaling.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GroundZero", meta=(ClampMin="1")) float TerminalDescentPowerCap = 1.44f;
};

// HOLD (§2.1 ultimate): 100 Grit, no cooldown, 10s. Caps the damage any single
// hit can do, and triples Grit generation against a raised cap.
//
// THE PER-HIT CAP SUBSTITUTION, per the task's explicit instruction: no
// damage-pipeline hook can cap a single hit, so the base and Wall variants
// push a keyed INCOMING MULTIPLIER through UBreakerCombatComponent's chain.
// The difference is load-bearing and stands recorded: a multiplier helps
// against streams of small hits (which the real Hold deliberately does not)
// and caps nothing absolutely against a boss slam. When a per-hit cap hook
// exists, these two branches move onto it and compose as `min`, never as a
// product.
UCLASS()
class RIORSEDGE_API UBreakerAbility_Hold : public UBreakerGameplayAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Hold();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    static FName WindowKey();

    // §2.1: generation TRIPLES against a raised cap (20/s -> 60/s); the Grit
    // component multiplies its cap by the same override, so one push does both.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hold", meta=(ClampMin="1")) float GenerationMultiplier = 3.0f;   // §2.1
    // The multiplicative stand-ins for the per-hit cap (see the class comment).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hold", meta=(ClampMin="0", ClampMax="1")) float BaseIncomingMultiplier = 0.5f;    // O2 PLACEHOLDER
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hold", meta=(ClampMin="0", ClampMax="1")) float WallSoloIncomingMultiplier = 0.25f;   // O2 PLACEHOLDER (§2.1 Wall: solo, doubled effectiveness)
    // §2.1 Vein: converted to healing at 60% of the post-mitigation amount.
    // Instant rather than over 1.5s — no heal-over-time primitive exists.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hold", meta=(ClampMin="0", ClampMax="1")) float VeinHealFraction = 0.6f;   // §2.1 placeholder
    // §2.1 Detonation: releases 70% of absorbed damage, 8 m, no falloff.
    // Released at the WINDOW'S END rather than on command: the second input
    // binding §6.0 asks for does not exist, and this is the one ability in the
    // game that needs one. Recorded, not solved.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hold", meta=(ClampMin="0", ClampMax="1")) float DetonationReleaseFraction = 0.7f;   // §2.1 placeholder
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hold", meta=(ClampMin="0")) float DetonationRadiusCm = 800.0f;   // §2.1: 8 m

private:
    UFUNCTION() void HandleDamageTaken(const FBreakerHitContext& Hit);
    void CloseHold();

    FTimerHandle WindowTimer;
    TWeakObjectPtr<UBreakerCombatComponent> BoundCombat;
    bool bHoldActive = false;
    bool bVein = false;
    bool bDetonation = false;
    float AbsorbedDamage = 0.0f;
};
