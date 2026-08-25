#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/BreakerCombatTypes.h"
#include "BreakerDamageLibrary.generated.h"

UCLASS()
class RIORSEDGE_API UBreakerDamageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // O34: weak point is ruled THE aim-skill lane — crit and weak point are the
    // two site multipliers (crit is build-gated, weak point is skill-gated, and
    // nothing else may multiply at the site). It sits deliberately OUTSIDE the
    // O3 More budget, and the price of living outside a budget is a hard bound:
    // every archetype's weak-point multiplier must land inside
    // [WeakPointMultiplierFloor, WeakPointMultiplierCeiling], and the resolve
    // site clamps to the same pair so an out-of-bounds author cannot ship.
    // The endpoints are the ruling's seed values. O2 PLACEHOLDER
    // O104, AS A FUNCTION. "Removing a multiplier's gate is a canon event. The
    // multiplier moves into the accounting its gate stood in for: a guaranteed
    // weak point is a build multiplier, and crit does not also multiply on that
    // hit."
    //
    // A weak point can arrive two ways and they are NOT the same event. EARNED
    // means the round actually struck a weak-point primitive; GRANTED means a
    // rule handed it over without one -- Swift.Marksman.Lead's mark, whose only
    // gate is range. The ruling forbids crit on the granted kind and explicitly
    // permits it on the earned kind.
    //
    // Both used to collapse into one bool at the call site before anything
    // could tell them apart, and the weapon path never set bCanCritical at all
    // -- the request struct defaults it to true, so every Lead-marked shot took
    // the weak-point multiplier AND a live crit roll. The guard test passed
    // because it hand-set bCanCritical = false and then checked the library
    // honoured it: the library half of the rule, proved, with the caller half
    // untested. That is the one this project keeps finding.
    static bool CanCriticalOnWeakPoint(bool bEarnedWeakPoint, bool bGrantedWeakPoint, bool bWeakPointKeepsCritRoll = false)
    {
        // An ordinary hit is neither, and crits normally. Earning it always
        // wins: a round that struck the weak point earned its crit even if a
        // mark would have granted the same hit anyway.
        //
        // The third parameter is Core.Precision.Deadeye — the PURCHASED
        // exception to O104's granted-weak-point rule, passed by the one
        // caller that owns a progression component to read it from. Default
        // false, so every pre-existing call keeps the ruling's behaviour
        // bit-identically.
        return bEarnedWeakPoint || (bGrantedWeakPoint && bWeakPointKeepsCritRoll) || !bGrantedWeakPoint;
    }

    static constexpr float WeakPointMultiplierFloor = 1.0f;
    static constexpr float WeakPointMultiplierCeiling = 2.0f;

    UFUNCTION(BlueprintPure, Category="Combat|Damage")
    static FBreakerDamageResult ResolveDamage(const FBreakerDamageRequest& Request, const FBreakerDefenseState& Defense);

    // ---- O54: the three pools, composed ----------------------------------
    // The whole rule, world-free so it can be proved with no actor, no world
    // and no attribute set — which is the only reason the aggregation law is
    // testable at all.
    //
    //   weapon-delivered  = (1 + (Weapon  + Shared) / 100) x MoreProduct
    //   ability-delivered = (1 + (Ability + Shared) / 100) x MoreProduct
    //
    // A hit reads ONE lane. The shared percentage appears in both formulas and
    // is never counted twice inside one of them, which is what makes it a legal
    // contributor to two buckets rather than a second bucket of its own.
    //
    // Callers pass the SHARED sum separately from the two specific sums even
    // though the live attributes already contain it folded in, because that is
    // the form the law is written in and the form a test can falsify. The
    // attribute-reading caller below passes zero for Shared for exactly that
    // reason: by the time a lane is composed, its share is already inside it.
    static float ComposeSourcePools(float WeaponIncreasedPercent, float AbilityIncreasedPercent,
        float SharedIncreasedPercent, float MoreProduct, EBreakerDamageDelivery Delivery);

    // Fills a request's ENTIRE source block — the composed multiplier, the
    // Stage 6 split, and the delivery — from a live attribute set. Every
    // submission site in the game calls this instead of assembling the four
    // fields itself; that is the difference between one rule and thirteen
    // copies of it, and the conformance scan enforces it.
    //
    // Each lane's Increased sum is recovered by dividing the composed attribute
    // by that lane's own post-clamp More product. The aggregator's fold is
    // (Base + Flat) x (1 + Increased/100) x prod(More), so the division is
    // exact — the same recovery UBreakerCombatComponent::ComposeDotSourcePower
    // already performs for the tick path.
    //
    // A null attribute set (an enemy, a hazard, a bare test rig) leaves the
    // request at the identity: multiplier 1.0, no split, delivery unchanged
    // except as named.
    static void FillSourcePools(const class UBreakerAttributeSet* SourceAttributes,
        EBreakerDamageDelivery Delivery, FBreakerDamageRequest& Request);

    UFUNCTION(BlueprintPure, Category="Combat|Damage")
    static float CalculateArmorMitigation(float Armor, float ArmorPenetration);

    // Facing-dependent armour, Encounter-Design §7. Pure and world-free for the
    // same reason ResolveDamage is: "is this hit in the rear arc" is geometry,
    // and geometry with an actor in it is geometry nobody can test.
    //
    // Returns the multiplier to apply to armour BEFORE the mitigation step, so
    // a rear multiplier of 0 means the hit lands on unarmoured flesh and a
    // multiplier of 1 means the arc did nothing. RearCosine is the cosine of
    // the boundary: 0 splits front from back exactly, positive values widen the
    // vulnerable arc onto the flanks.
    UFUNCTION(BlueprintPure, Category="Combat|Damage")
    static float GetFacingArmorMultiplier(const FVector& Forward, const FVector& SelfLocation,
        const FVector& SourceLocation, float RearArmorMultiplier, float RearCosine);

    UFUNCTION(BlueprintPure, Category="Combat|Status")
    // Instigator is the actor that applied the status; it is carried weakly on
    // the tick request so attacker-side hit events fire for DoT damage too.
    // SourceLocation/bHasSourceLocation are the APPLICATION-TIME source
    // position (snapshotted by the status component when the status landed),
    // so every tick answers the facing-armour check the same way the applying
    // hit did — a Bleed put into a Warden's exposed back keeps reading as a
    // rear hit instead of silently re-acquiring the frontal mitigation.
    static FBreakerDamageRequest MakeSnapshotDotTick(const FBreakerStatusApplicationSpec& StatusSpec, EBreakerDamageFamily DamageFamily, int32 TickIndex, AActor* Instigator,
        const FVector& SourceLocation, bool bHasSourceLocation);

    // The healing half of the contract, pure and world-free for exactly the
    // reason ResolveDamage is: the arithmetic of "how much of this heal lands,
    // how much is overheal, how much of the overheal becomes shield" is
    // testable with no actor, and every healing source in the game must agree
    // on it. Health first, then overheal — never shield first, or a target at
    // full health with a broken shield would consume the heal on the shield and
    // report zero healing to Support's Charge loop.
    UFUNCTION(BlueprintPure, Category="Combat|Healing")
    static FBreakerHealResult ResolveHealing(const FBreakerHealRequest& Request, const FBreakerVitalsState& Vitals);
};
