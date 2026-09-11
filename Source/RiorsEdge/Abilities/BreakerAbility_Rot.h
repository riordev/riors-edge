#pragma once

#include "CoreMinimal.h"
#include "Abilities/BreakerCasterAbility.h"
#include "BreakerAbility_Rot.generated.h"

class ABreakerCharacter;
class ABreakerZoneActor;
class UBreakerAttributeSet;
class UWorld;

// C3 Rot (Class-Kits §2.2, Ability-Implementation-Spec §5.3): 25 Mana, no
// cooldown. "A radius zone at the aim point, for a duration. Enemies inside take
// Entropy damage and have their Armour reduced by a flat 40. Zones are the Void
// Whisperer's whole grammar."
//
// The ability is deliberately thin: everything durable about it lives in
// ABreakerZoneActor, because Support's Suppress, Gunsmith's Disruptor, every
// boss telegraph and every environmental hazard need the same volume. What is
// Rot's and only Rot's is the aim solve and the payload. A recast spawns a new
// puddle; nothing merges into a live one (O271). Wellspring's following puddle
// is the one exception, renewed in place because there is only ever one.
// ---------------------------------------------------------------------------
// WHICH HIT IS THE FLOOR.
//
// Owner: "rot ... doesn't even place correctly half the time". The downward
// probe under the aim point used to be a SINGLE-hit trace with a normal gate
// on the one hit it returned, and that is wrong in two ways that between them
// cover most of a yard:
//
//   * Aim at a wall, a pillar, a railing or a ramp and the FIRST thing under
//     the aim point is that same surface. Its normal is not floor-like, the
//     gate rejects it, and the correction gives up — leaving the zone on the
//     wall's face. The floor two metres below was never looked at.
//   * When the aim point sits exactly ON a vertical face, the probe starts
//     inside that geometry. A trace that starts penetrating reports the START
//     POINT with an up normal, which PASSES the gate, and the zone is lifted to
//     the top of the probe — four metres in the air. Which of the two happens
//     is numerically decided, which is the "half the time".
//
// So the rule is: walk the hits from the top down, ignore anything that starts
// penetrating, and take the first surface flat enough to stand on. No floor in
// the list leaves the aim point exactly where it was — four ability fixtures
// cast Rot in empty worlds and assert where the zone lands, and a correction
// that fires on the ABSENCE of evidence is the wrong rule anyway.
// ---------------------------------------------------------------------------
namespace BreakerRotFloor
{
    // The floor-ness gate. A ramp at 45 degrees has a normal Z of 0.707, so
    // this admits every walkable slope and no wall. O2 PLACEHOLDER.
    inline constexpr float MinimumFloorNormalZ = 0.7f;

    struct FProbeHit
    {
        float PointZ = 0.0f;
        float NormalZ = 0.0f;
        // True when the trace began inside this geometry. Such a hit reports
        // the trace's own start and an invented up normal; it is evidence of
        // nothing.
        bool bStartPenetrating = false;
    };

    // Returns true and writes the floor height when one of the hits is a floor.
    // Hits are expected in trace order, which is top-down.
    inline bool PickFloorZ(TArrayView<const FProbeHit> Hits, float& OutZ)
    {
        for (const FProbeHit& Hit : Hits)
        {
            if (Hit.bStartPenetrating) continue;
            if (Hit.NormalZ < MinimumFloorNormalZ) continue;
            OutZ = Hit.PointZ;
            return true;
        }
        return false;
    }
}

UCLASS()
class RIORSEDGE_API UBreakerAbility_Rot : public UBreakerCasterAbility
{
    GENERATED_BODY()

public:
    UBreakerAbility_Rot();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // THE AIM IS READ AT THE PRESS. O266's wind-up delays the RESOLUTION, not
    // the press, and Rot was reading its whole aim — view, trace, floor probe,
    // Wellspring's follow decision — on the far side of the 0.6 s. Owner: "rot
    // has a tendency to not go off when pressed or off at another target
    // location"; the reticle had moved on and the puddle went where it was
    // pointing at the landing. The solve now runs here, at cast start, in
    // Resonance's shape: snapshot at the press, consume at the landing. Rot
    // never refuses on aim (AimPoint's own comment: a mis-aim is a puddle in
    // the wrong place, not a swallowed input), so this always returns true.
    virtual bool PrepareCast() override;

    // O271: a press during the wind-up queues one cast, and ITS aim is read
    // at that press too — into a second slot, because the first still belongs
    // to the cast winding up. Promotion moves it into the active slot at the
    // landing that fires the queued cast.
    virtual bool PrepareQueuedCast() override;
    virtual void PromoteQueuedCast() override;

    // Everything the aim decides, in one value: the centre after the floor
    // probe and whether Wellspring makes the puddle ride the caster.
    struct FAimSolve
    {
        FVector Center = FVector::ZeroVector;
        bool bFollowCaster = false;
    };

    // Pure rule: where the puddle lands. A zone dropped at the point the trace
    // hit is right for a floor; a zone dropped at the far end of a trace that
    // hit NOTHING must still be placed somewhere the player expects, which is
    // the end of the reticle line and not the caster's feet.
    UFUNCTION(BlueprintPure, Category="Rot")
    static FVector AimPoint(const FVector& ViewLocation, const FVector& ViewDirection, float MaximumRangeCm, bool bTraceHit, const FVector& HitLocation);

    // The geometry the activation actually spawns: the ability-geometry seam
    // (base-class AbilityArea / AbilityDuration accessors) applied to this
    // subclass's own differently-named UPROPERTYs — the shape the
    // EBreakerNodeStatTarget comments ask for. Actor-parameterised so a test
    // can pin them on a rigged owner with no world. With no owned ranks both
    // are exactly the authored numbers in Data/abilities.json.
    UFUNCTION(BlueprintPure, Category="Rot")
    float ComputeEffectiveRadiusCm(const AActor* OwnerActor) const;
    UFUNCTION(BlueprintPure, Category="Rot")
    float ComputeEffectiveDurationSeconds(const AActor* OwnerActor) const;
    bool ShouldFollowCaster(const AActor* OwnerActor, bool bGroundHit, const FVector& HitPoint, const FVector& HitNormal) const;

private:
    // The whole aim question in one place, so the cast start and the
    // no-cast-time path cannot ask it differently. Reads the world, writes
    // nothing on the ability.
    void SolveAim(const ABreakerCharacter& Character, const UWorld& World, FAimSolve& Out) const;

    FAimSolve CastAimSnapshot;
    bool bAimSnapshotValid = false;
    // The queued press's aim (O271), waiting behind the active snapshot.
    FAimSolve QueuedAimSnapshot;
    bool bQueuedAimValid = false;

public:
    // Class-Kits §2.2 C3 names the shape; the numbers live in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float RadiusCm {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float DurationSeconds {}; // O246: authored in Data/abilities.json.
    // O2 PLACEHOLDER: the doc gives radius and duration and nothing else. The
    // cadence is a shape, not balance.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0.05")) float TickIntervalSeconds {}; // O246: authored in Data/abilities.json.
    // O2 PLACEHOLDER: direct Entropy hits build the earned Rot status.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float ZoneDamagePerTick {}; // O246: authored in Data/abilities.json.
    // Class-Kits §2.2 C3: a FLAT 40. Flat and never percentage is the ruling —
    // it is what protects the boss armour cap (Master 7.10.5).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float FlatArmorReduction {}; // O246: authored in Data/abilities.json.
    // How far out the puddle can be placed. O2 PLACEHOLDER.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float MaximumRangeCm {}; // O246: authored in Data/abilities.json.
    // Vertical reach of the volume; see FBreakerZoneSpec::HalfHeightCm.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot", meta=(ClampMin="0")) float HalfHeightCm {}; // O246: authored in Data/abilities.json.
    // O2 PLACEHOLDER: conditional income shares the Caster generation cap.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot|Nodes", meta=(ClampMin="0")) float StandingWaterRankOneManaPerSecond {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot|Nodes", meta=(ClampMin="0")) float StandingWaterRankTwoManaPerSecond {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot|Nodes", meta=(ClampMin="0")) float ZoneworkAdditionalArmorReduction {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot|Nodes", meta=(ClampMin="0")) float LingeringRefreshGrowthCm {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot|Nodes", meta=(ClampMin="0")) float WellspringSelfPlacementRadiusCm {}; // O246: authored in Data/abilities.json.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rot|Nodes", meta=(ClampMin="0", ClampMax="1")) float WellspringMinimumGroundNormalZ {}; // O246: authored in Data/abilities.json.
};
