#include "Playtest/BreakerKillBuckets.h"

EBreakerKillBucket UBreakerKillBucketLibrary::ClassifyKill(EBreakerMonsterRank Rank, bool bRanged, int32 ModifierCount)
{
    switch (Rank)
    {
    case EBreakerMonsterRank::Boss:
        return EBreakerKillBucket::Boss;
    case EBreakerMonsterRank::Elite:
        // Deliberately ABOVE ModifierBearing. The gym's arena anchor is an
        // elite that also carries modifiers, and its health came from the elite
        // rank row (x3.0), not the modifier row (x2.5) — so its kill time
        // belongs to the elite population. Rank multiplies the chassis;
        // modifiers change the verb.
        return EBreakerKillBucket::Elite;
    default:
        break;
    }

    // Rank ModifierBearing and a nonzero count are supposed to be the same
    // statement (GetRankForModifierCount maps count > 0 onto it), but they are
    // set by two different call paths and either one alone is enough to make
    // this a Veteran kill. Asking for both would let a rank restored by a
    // spawner silently demote the sample.
    if (Rank == EBreakerMonsterRank::ModifierBearing || ModifierCount > 0)
    {
        return EBreakerKillBucket::ModifierBearing;
    }

    return bRanged ? EBreakerKillBucket::RangedTrash : EBreakerKillBucket::MeleeTrash;
}

EBreakerKillBucket UBreakerKillBucketLibrary::ClassifyLegacyKill(bool bElite, bool bRanged)
{
    // Elite wins over ranged, unchanged: an elite is the elite sample whatever
    // its archetype.
    if (bElite) return EBreakerKillBucket::Elite;
    return bRanged ? EBreakerKillBucket::RangedTrash : EBreakerKillBucket::MeleeTrash;
}

FString UBreakerKillBucketLibrary::GetKillBucketName(EBreakerKillBucket Bucket)
{
    switch (Bucket)
    {
    case EBreakerKillBucket::MeleeTrash:      return TEXT("Melee trash");
    case EBreakerKillBucket::RangedTrash:     return TEXT("Ranged trash");
    case EBreakerKillBucket::Elite:           return TEXT("Elite");
    case EBreakerKillBucket::ModifierBearing: return TEXT("Modifier-bearing");
    case EBreakerKillBucket::Boss:            return TEXT("Boss");
    default:                                  return TEXT("Unknown");
    }
}
