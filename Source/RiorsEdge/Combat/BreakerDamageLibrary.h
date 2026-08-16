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
    static constexpr float WeakPointMultiplierFloor = 1.0f;
    static constexpr float WeakPointMultiplierCeiling = 2.0f;

    UFUNCTION(BlueprintPure, Category="Combat|Damage")
    static FBreakerDamageResult ResolveDamage(const FBreakerDamageRequest& Request, const FBreakerDefenseState& Defense);

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
