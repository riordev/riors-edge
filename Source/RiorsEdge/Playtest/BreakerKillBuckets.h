#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/BreakerMonsterChassis.h"
#include "BreakerKillBuckets.generated.h"

// WHICH TTK AVERAGE A KILL BELONGS TO, as pure world-free maths — the precedent
// is Combat/BreakerRangedBehavior.h and Combat/BreakerCoverBehavior.h, and the
// reason is the same: this is a rule, and a rule with an actor in it is a rule
// nobody can test.
//
// WHY IT EXISTS. The playtest report is the instrument the whole O2 value
// freeze is waiting on, and an instrument that mixes two populations into one
// average reports a number that describes neither. That bug has now been found
// TWICE in this file's subject:
//
//   1. Ranged trash was averaged into the melee trash bucket. A LATTICE is
//      fought across a 9-19 m band and a Skitter is fought in contact; the
//      fixed version gave ranged its own bucket.
//   2. THE SAME CLASS OF BUG, still live before this header: a
//      ModifierBearing enemy (O9's Veteran, x2.5 health) and a BOSS (x25) both
//      fell into whichever bucket `IsElite()` and `IsRangedForTelemetry()`
//      happened to name. Worth stating precisely, because it is not what the
//      handover said: `IsElite()` is `MonsterRank == Elite`, and a Champion's
//      rank is ModifierBearing while the Field Marshal's is Boss — so NEITHER
//      was landing in the elite bucket. Both were landing in MELEE TRASH, and a
//      single boss kill at 25x health drags the sub-1s trash average that O18's
//      re-anchor reads.
//
// PRECEDENCE IS BY RANK, NOT BY DECORATION. Rank is what multiplies the
// chassis (Power-Curve §2: Elite x3.0, ModifierBearing x2.5, Boss x25), so rank
// is what decides which population a kill time belongs to. An ELITE that also
// carries modifiers — which is what the gym's arena anchor now is — is an elite
// sample, because its health came from the elite row. Modifiers change what the
// player has to DO, not what the health bar is worth.
UENUM(BlueprintType)
enum class EBreakerKillBucket : uint8
{
    // Contact-range trash. The bucket O18's "a little under 1s" target reads.
    MeleeTrash,
    // Trash fought across a band: LATTICE, and now the SKIRMISHER, which
    // declares itself ranged through IsRangedForTelemetry() for exactly this.
    RangedTrash,
    // Rank Elite: the authored pack anchor, x3.0 health. O18: ~3s.
    Elite,
    // Rank ModifierBearing — O9's Veteran, x2.5 health plus §1.1's +0.35x per
    // modifier beyond the first. Its kill time measures the modifier layer,
    // which is the thing O27 moved difficulty INTO, so it is the one number
    // that says whether that ruling worked.
    ModifierBearing,
    // Rank Boss: x25 health, and a fight with a script floor. O18: 20-45s.
    Boss,

    Count UMETA(Hidden)
};

UCLASS()
class RIORSEDGE_API UBreakerKillBucketLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // The whole rule. Takes ENEMY-side facts only — no player, no build, no
    // item level — so it composes with O27 rather than quietly reintroducing a
    // player read into the measurement layer.
    UFUNCTION(BlueprintPure, Category="Playtest|TTK")
    static EBreakerKillBucket ClassifyKill(EBreakerMonsterRank Rank, bool bRanged, int32 ModifierCount);

    // The pre-bucket call shape, kept because ABreakerEnemy::HandleDeath still
    // uses it and Combat/ is not this lane's to edit. It can only ever produce
    // the three buckets that existed before, which is precisely why anything
    // that wants the other two has to say so.
    UFUNCTION(BlueprintPure, Category="Playtest|TTK")
    static EBreakerKillBucket ClassifyLegacyKill(bool bElite, bool bRanged);

    // Short and stable — it is a column heading in the clipboard report the
    // owner pastes into the feedback log, so it must not drift between runs.
    UFUNCTION(BlueprintPure, Category="Playtest|TTK")
    static FString GetKillBucketName(EBreakerKillBucket Bucket);
};
