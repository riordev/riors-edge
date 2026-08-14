#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BreakerStatusConsumption.generated.h"

// The arithmetic of DETONATING statuses (Class-Kits C6 / MS9,
// Ability-Implementation-Spec §5.6). Pure, world-free and actor-free in the
// precedent of Combat/BreakerRangedBehavior.h and Combat/BreakerMonsterChassis.h,
// because the thing that MUST be provable here is a bound, not a behaviour:
// Class-Kits §2.7.5 caps Resonance's damage against a 6-status target at 2.2x
// its damage against a 2-status target, and that is a statement about a
// function of one integer. It is checkable with no world, and it is checked.
//
// Damage scales on DISTINCT TYPES, never on stacks. That is the explicit
// anti-stacking rule: without it, the strongest Resonance build is "apply one
// status ten times", which collapses the whole Multispell branch into a
// stacking check (Master 7.10.1's recursion/explosion concern).

// Which curve turns a distinct-status count into a burst.
UENUM(BlueprintType)
enum class EBreakerDetonationCurve : uint8
{
    // Base row: flat per distinct status. Doubling the statuses doubles the
    // burst, which is exactly the shape MS9 exists to flatten.
    Linear,
    // MS9 Interference: a fixed value per status plus one flat bonus at or
    // above the threshold count. Deliberately re-shaped AWAY from a count
    // multiplier — the anti-explosion rewrite named in Class-Kits MS9.
    FixedPlusThreshold
};

USTRUCT(BlueprintType)
struct RIORSEDGE_API FBreakerDetonationParams
{
    GENERATED_BODY()

    // O2 PLACEHOLDER. Class-Kits §2.7.5 gives the BOUND and no values at all,
    // so every number here is a shape rather than balance.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float DamagePerDistinctStatus = 40.0f;
    // Paid once when at least one status is consumed. Zero statuses always
    // detonates for zero — detonating nothing must never pay out.
    //
    // This term is what BOUNDS the ability, and it is the reason the default is
    // large rather than zero: with no flat base, Linear's 2-to-6 ratio is
    // exactly 3.0 and breaches §2.7.5's 2.2x cap before a single node is taken.
    // A flat base flattens the ratio without flattening the incentive to
    // diversify, which is the design goal MS9 states in words.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float FlatDamageIfAny = 150.0f;
    // MS9 only.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 ThresholdCount = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float ThresholdFlatBonus = 40.0f;
    // Hard stop on how many distinct statuses can be paid for. Five elements
    // plus Bleed is six statuses today; the cap exists so a future seventh
    // status type cannot silently inflate an already-bounded ability.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) int32 MaximumCountedStatuses = 6;
};

UCLASS()
class RIORSEDGE_API UBreakerStatusConsumption : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The burst for a given number of DISTINCT statuses. Zero (and negative,
    // which cannot legally happen) returns zero.
    UFUNCTION(BlueprintPure, Category="Combat|Status")
    static float DetonationDamage(int32 DistinctCount, const FBreakerDetonationParams& Params, EBreakerDetonationCurve Curve);

    // Class-Kits §2.7.5's bound, expressed so a test can assert it directly:
    // damage(High) / damage(Low). Returns 0 when the low end is zero, which a
    // caller must treat as "unbounded" rather than "fine".
    UFUNCTION(BlueprintPure, Category="Combat|Status")
    static float DetonationRatio(int32 LowCount, int32 HighCount, const FBreakerDetonationParams& Params, EBreakerDetonationCurve Curve);

    // MS5 Payment: Mana refunded per distinct status consumed. Here rather than
    // on the ability so the refund and the damage read the same count.
    UFUNCTION(BlueprintPure, Category="Combat|Status")
    static float RefundForConsumed(int32 DistinctCount, float RefundPerStatus, int32 MaximumCountedStatuses);
};
