#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerCombatComponent.generated.h"

class UBreakerAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerDamageReceived, const FBreakerDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerDeathEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerHitDealt, const FBreakerHitContext&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerKillDealt, const FBreakerHitContext&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerHealEvent, const FBreakerHealResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakerHealDealt, const FBreakerHealContext&, Heal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBreakerVitalsRestored);

// One push/pop-able outgoing damage modifier. Keyed so the pusher (an ability
// window, a node) can remove exactly its own entry; expiry is a safety net for
// windows whose owner never gets to pop.
USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerOutgoingModifier
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName Key;
    UPROPERTY(BlueprintReadOnly) float FlatBonus = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MoreMultiplier = 1.0f;
    // Negative means "never expires on its own".
    UPROPERTY(BlueprintReadOnly) float ExpiryTime = -1.0f;
};

UCLASS(ClassGroup=Combat, BlueprintType, meta=(BlueprintSpawnableComponent))
class RIORSEDGE_API UBreakerCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBreakerCombatComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Normally resolved from the owner's ability system in BeginPlay. Exposed
    // for the same reason UBreakerEquipmentComponent and
    // UBreakerProgressionComponent expose it: a component that can only find
    // its attributes through a live actor in a live world cannot be tested at
    // all, and the vitals paths are exactly the ones that must be.
    void BindAttributes(UBreakerAttributeSet* InAttributes);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") FBreakerDamageResult ReceiveDamage(const FBreakerDamageRequest& Request);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool SpendClassResource(float Cost);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void AddClassResource(float Amount);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") void RestoreVitals();
    UFUNCTION(BlueprintPure, Category="Combat") bool IsDead() const;
    UFUNCTION(BlueprintPure, Category="Combat") float GetSecondsSinceDamage() const;

    // Outgoing-damage modifier chain (SI-7 partial). Pushed by ability windows
    // and class nodes; applied to a request just before it is submitted.
    // Each modifier's More is clamped at the single-More ceiling on push
    // (FBreakerAttributeAggregator::SingleMoreCeiling — O3's per-source cap,
    // shared with the tree's selection, never restated here).
    UFUNCTION(BlueprintCallable, Category="Combat|Outgoing")
    void PushOutgoingModifier(FName Key, float FlatBonus, float MoreMultiplier, float ExpirySeconds);

    UFUNCTION(BlueprintCallable, Category="Combat|Outgoing")
    void RemoveOutgoingModifier(FName Key);

    // Adds the composed flat bonus to BaseDamage and folds the More product
    // into SourceDamageMultiplier. O34: the chain's product counts against the
    // SAME aggregator-derived budget as the attribute-side Mores — total
    // effective More (attribute-side product x chain product) never exceeds
    // FBreakerAttributeAggregator::ComposedMoreCeiling(). There is no second
    // ceiling constant here any more; the 2.20 local was deleted by ruling.
    UFUNCTION(BlueprintCallable, Category="Combat|Outgoing")
    void ApplyOutgoingModifiers(UPARAM(ref) FBreakerDamageRequest& Request);

    // The chain's effective More product AFTER the O34 budget: clamped so that
    // (attribute-side More product) x (this) stays at or under the one ceiling.
    // With no bound attributes (enemies, bare test rigs) the attribute side is
    // 1.0 and the whole budget is available to the chain.
    UFUNCTION(BlueprintPure, Category="Combat|Outgoing") float GetComposedMoreMultiplier() const;

    // The attribute side of the O34 budget: the post-clamp More product the
    // aggregator composed into DamageMultiplier (equipment and progression
    // submissions alike), 1.0 when no attribute set is bound.
    UFUNCTION(BlueprintPure, Category="Combat|Outgoing") float GetAttributeSideMoreProduct() const;

    // Snapshot-time source power for a DoT APPLICATION: the composed
    // DamageMultiplier attribute times the outgoing chain's budgeted More
    // product. DoTs snapshot offensive stats at application (locked rule), and
    // before this the chain's product was NOT in the snapshot — a bleed applied
    // inside an Overdrive window ticked as if the window did not exist. Called
    // at application time only, never per tick; ticks already resolved read
    // their spec's captured value and stay untouched. Null-safe on both
    // arguments so every application site can call it unconditionally.
    static float ComposeDotSourcePower(const UBreakerAttributeSet* SourceAttributes, const UBreakerCombatComponent* OwnerCombat);

    // Incoming-damage modifier chain (Ability-Implementation-Spec §4.4). Keyed
    // push/remove, composed multiplicatively into FBreakerDefenseState::
    // IncomingDamageMultiplier at the top of ReceiveDamage, so it lands before
    // armour, shields, and the passive rolls — the same stage gear-rolled
    // physical reduction already occupies.
    //   1.0 = no change, 1.15 = takes 15% more, 0.0 = immune.
    // First consumer: Caster's Overcast penalty (Class-Kits §2.1).
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Incoming")
    void PushIncomingDamageModifier(FName Key, float Multiplier);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Incoming")
    void RemoveIncomingDamageModifier(FName Key);

    UFUNCTION(BlueprintPure, Category="Combat|Incoming") float GetComposedIncomingDamageMultiplier() const;

    // Flat armour reduction, keyed (Ability-Implementation-Spec §5.3). Rot
    // strips 40 armour and VW7 strips another 40 against a target already
    // affected by a DoT; Gunsmith's Disruptor strips more.
    //
    // Two rules are load-bearing here and both exist to stop a known bug:
    //  * FLAT, never percentage — Class-Kits VW7 is explicit that this is what
    //    protects the boss armour cap (Master 7.10.5).
    //  * KEYED, so two overlapping Rots do not double-strip. A naive additive
    //    −40 GameplayEffect stacks with itself and drives armour negative,
    //    which under the mitigation formula becomes a damage BONUS.
    // Effective armour is clamped at zero at the point of use, so no
    // combination of strippers can ever invert mitigation.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Armor")
    void PushArmorReduction(FName Key, float FlatAmount);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Armor")
    void PopArmorReduction(FName Key);

    UFUNCTION(BlueprintPure, Category="Combat|Armor") float GetComposedArmorReduction() const;
    UFUNCTION(BlueprintPure, Category="Combat|Armor") float GetEffectiveArmor() const;

    // --- Healing ---------------------------------------------------------
    // The one healing path (Ability-Implementation-Spec §5.4). Every heal in
    // the game routes through here for the same reason every hit routes through
    // ReceiveDamage: an ability that writes Health directly is invisible to
    // overheal rules, to healing-modifier affixes, and to every listener.
    //
    // Authority-only, and it will NOT resurrect: a dead actor is dead, and
    // healing is not the revive system.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Healing")
    FBreakerHealResult ApplyHealing(const FBreakerHealRequest& Request);

    // Convenience form matching the spec's signature, for callers with nothing
    // to say beyond "heal this much".
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat|Healing")
    FBreakerHealResult ApplyHealingAmount(float Amount, AActor* Healer, FGameplayTag SourceTag);

    // Raised on the HEALED actor.
    UPROPERTY(BlueprintAssignable, Category="Combat|Healing") FBreakerHealEvent OnHealed;
    // Raised on the HEALER, the twin of OnHitDealt. Support's Charge loop binds
    // this one.
    UPROPERTY(BlueprintAssignable, Category="Combat|Healing") FBreakerHealDealt OnHealingDealt;

    // O34: there is deliberately NO More ceiling constant on this class. The
    // one budget is FBreakerAttributeAggregator::ComposedMoreCeiling() and the
    // chain queries it; a second constant here is exactly the drift the ruling
    // deleted (the old local 2.20 silently multiplied with the aggregator's
    // 2.197 into a ~4.83x total nothing ever authored).

    // --- Facing-dependent armour -----------------------------------------
    // Encounter-Design §7 names this "the one genuinely new combat-pipeline
    // requirement in this document": the Severed Warden and the slice boss are
    // both designed around "flank it and its defence disappears", and without
    // it both collapse into ordinary armoured enemies and lose their whole
    // teaching value.
    //
    // It is a PER-HIT check, not a per-frame one, because the request already
    // carries where the damage came from (SourceLocation / bHasSourceLocation,
    // which the weapon, both projectiles, the zones and the melee sweep all
    // fill in). A per-frame "is the player behind me" approximation would let a
    // shot fired from the front resolve as a rear hit because the shooter moved
    // during the bullet's flight, which is the kind of silent inconsistency
    // that teaches players the mechanic is random.
    //
    // 1.0 is OFF and is the default, so no existing enemy changes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Facing", meta=(ClampMin="0"))
    float RearArcArmorMultiplier = 1.0f;
    // Cosine of the half-angle that counts as "behind". 0 is the strict rear
    // hemisphere; a small positive number widens the vulnerable arc onto the
    // flanks, which is what makes "circle it" rather than "stand exactly
    // behind it" the answer. O2 PLACEHOLDER.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Facing", meta=(ClampMin="-1", ClampMax="1"))
    float RearArcCosine = 0.15f;

    // Whether a hit from this location lands in the rear arc. Public so an
    // archetype can drive presentation (a seam flash) off the same decision the
    // damage used, instead of a second copy of the geometry.
    UFUNCTION(BlueprintPure, Category="Defense|Facing")
    bool IsRearArcHit(const FVector& SourceLocation) const;

    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDamageReceived OnDamageReceived;
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerDeathEvent OnDeath;
    // Raised by RestoreVitals — spawn, respawn, and the F1 playtest reset.
    // Combat deliberately does NOT refill class resources itself: filling a
    // Swift's Momentum bar on reset would hand out a resource the class exists
    // to make you earn. The class-resource loop that owns the mechanic listens
    // and decides for itself, which is how a Caster starts full (owner ruling
    // 2026-08-14) without Combat/ knowing what a Caster is.
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerVitalsRestored OnVitalsRestored;
    // Attacker-side (SI-8): raised on the component of whoever dealt the hit.
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHitDealt OnHitDealt;
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerKillDealt OnKillDealt;
    // VICTIM-side, carrying the same context. OnDamageReceived hands out only a
    // FBreakerDamageResult, which does not say WHO dealt the hit — so a
    // listener that needs to answer the attacker (the Reflective modifier is
    // the first) had no way to find them. Broadcast alongside OnDamageReceived
    // on every hit that is not dodged.
    UPROPERTY(BlueprintAssignable, Category="Combat") FBreakerHitDealt OnDamageTaken;
    // Passive defensive layers: classes and gear raise these; no inputs.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float BlockChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float BlockMitigation = 0.5f;   // O2 PLACEHOLDER
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0", ClampMax="1")) float DodgeChance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0")) float DodgeResourceRefund = 5.0f;   // O2 PLACEHOLDER

private:
    void PruneExpiredOutgoingModifiers();
    void DispatchHitDealt(const FBreakerDamageRequest& Request, const FBreakerDamageResult& Result);

    UPROPERTY() TArray<FBreakerOutgoingModifier> OutgoingModifiers;
    // Keyed so a pusher removes exactly its own entry. No expiry: an incoming
    // modifier reflects a state (Overcast, a defensive window) whose owner is
    // responsible for removing it, and a silently expiring defence is worse
    // than one that is visibly stuck.
    TMap<FName, float> IncomingDamageModifiers;
    // Keyed for the same reason, and summed rather than multiplied because
    // armour reduction is authored FLAT.
    TMap<FName, float> ArmorReductions;
    UPROPERTY() TObjectPtr<UBreakerAttributeSet> Attributes;
    bool bDeathBroadcast = false;
    double LastDamageTime = -1000.0;
};
