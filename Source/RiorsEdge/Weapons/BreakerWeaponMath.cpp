#include "Weapons/BreakerWeaponMath.h"
#include "Weapons/BreakerWeaponDefinition.h"

float FBreakerWeaponMath::FireInterval(float RoundsPerMinute)
{
    return 60.0f / FMath::Max(1.0f, RoundsPerMinute);
}

float FBreakerWeaponMath::DamageMultiplierAtDistance(const UBreakerWeaponDefinition* Definition, float Distance)
{
    if (!Definition || Distance <= Definition->FalloffStart) return 1.0f;
    if (Distance >= Definition->FalloffEnd) return Definition ? Definition->MinimumFalloffMultiplier : 1.0f;
    const float Alpha = FMath::GetRangePct(Definition->FalloffStart, Definition->FalloffEnd, Distance);
    return FMath::Lerp(1.0f, Definition->MinimumFalloffMultiplier, Alpha);
}

FVector FBreakerWeaponMath::ApplyConeSpread(const FVector& Direction, float SpreadDegrees, int32 RandomSeed)
{
    if (SpreadDegrees <= 0.0f) return Direction.GetSafeNormal();
    FRandomStream Random(RandomSeed);
    return Random.VRandCone(Direction.GetSafeNormal(), FMath::DegreesToRadians(SpreadDegrees));
}

float FBreakerWeaponMath::DistanceFromRayToPoint(const FVector& RayOrigin, const FVector& RayDirection, const FVector& Point)
{
    const FVector Direction = RayDirection.GetSafeNormal();
    if (Direction.IsNearlyZero()) return static_cast<float>(FVector::Dist(RayOrigin, Point));
    const FVector ToPoint = Point - RayOrigin;
    // Clamped at zero: only what is downrange of the muzzle can be shot.
    const double Along = FMath::Max(0.0, FVector::DotProduct(ToPoint, Direction));
    return static_cast<float>(FVector::Dist(RayOrigin + Direction * Along, Point));
}

bool FBreakerWeaponMath::IsWithinWeakPointTolerance(const FVector& RayOrigin, const FVector& RayDirection, const FVector& WeakPointCenter, float WeakPointRadius, float ToleranceCm)
{
    const float Accept = FMath::Max(0.0f, WeakPointRadius) + FMath::Max(0.0f, ToleranceCm);
    if (Accept <= 0.0f) return false;
    return DistanceFromRayToPoint(RayOrigin, RayDirection, WeakPointCenter) <= Accept;
}

float FBreakerWeaponMath::ItemLevelDamageScalar(int32 ItemLevel, float GrowthPerLevel)
{
    const int32 Level = FMath::Clamp(ItemLevel, 1, MaxSupportedItemLevel);
    // A growth of 0 (or a nonsense negative) means "item level does nothing",
    // which is the pre-curve behaviour and a legitimate A/B setting.
    const float Growth = FMath::Max(0.0f, GrowthPerLevel);
    if (Growth <= 0.0f || Level <= 1) return 1.0f;
    return FMath::Pow(1.0f + Growth, static_cast<float>(Level - 1));
}

float FBreakerWeaponMath::WeaponBaseDamage(float ArchetypeBase, int32 ItemLevel, float GrowthPerLevel)
{
    return FMath::Max(0.0f, ArchetypeBase) * ItemLevelDamageScalar(ItemLevel, GrowthPerLevel);
}

int32 FBreakerWeaponMath::ConsumeMultishot(float AdditionalProjectiles, float& Accumulator)
{
    if (AdditionalProjectiles <= 0.0f) return 0;
    // Bank the whole request, then pay out the whole pellets. The accumulator
    // keeps only the sub-pellet remainder, so it can never grow unboundedly
    // and a channel dropping back to zero leaves at most a fraction stranded.
    Accumulator += AdditionalProjectiles;
    const int32 WholePellets = FMath::FloorToInt32(Accumulator);
    Accumulator -= static_cast<float>(WholePellets);
    return WholePellets;
}

float FBreakerWeaponMath::NextPierceMultiplier(float CurrentMultiplier, float FalloffPerTarget, bool bPreviousHitKilled, bool bOverpenetration)
{
    // M10: "a shot that kills carries on at full damage instead of falling
    // off" — the falloff STEP is skipped, not the whole ladder reset.
    if (bOverpenetration && bPreviousHitKilled) return CurrentMultiplier;
    return CurrentMultiplier * FMath::Clamp(FalloffPerTarget, 0.0f, 1.0f);
}

int32 FBreakerWeaponMath::SelectNearestTarget(const FVector& Origin, const TArray<FVector>& Candidates, float MaxRadiusCm)
{
    int32 BestIndex = INDEX_NONE;
    double BestDistanceSquared = FMath::Square(static_cast<double>(FMath::Max(0.0f, MaxRadiusCm)));
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        const double DistanceSquared = FVector::DistSquared(Origin, Candidates[Index]);
        // Strictly-less keeps the tie deterministic at the lower index.
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestIndex = Index;
        }
    }
    return BestIndex;
}

float FBreakerWeaponMath::SteadyMovementSpreadDegrees(float MovementSpreadDegrees, float AimAlpha, int32 SteadyRank, bool bAirborne)
{
    if (SteadyRank <= 0 || AimAlpha <= 0.0f || MovementSpreadDegrees <= 0.0f) return MovementSpreadDegrees;
    // §1.5 M2's rank split: R1 is the grounded rule, R2 extends it airborne.
    if (bAirborne && SteadyRank < 2) return MovementSpreadDegrees;
    // Relief scales with aim progress so the rule arrives at the pace every
    // other ADS benefit does — full sights, no movement penalty at all.
    return MovementSpreadDegrees * (1.0f - FMath::Clamp(AimAlpha, 0.0f, 1.0f));
}

float FBreakerWeaponMath::LeadRangeGateCm(float BaseGateCm, bool bCalledShotOwned, bool bRedline)
{
    // Class-Kits §1.5 M11 / node text: 25 m -> 10 m. The 10 m is transcribed;
    // the base gate stays whatever Lead authored, so retuning Lead retunes
    // the un-rewritten case without touching this rule.
    constexpr float CalledShotGateCm = 1000.0f;   // §1.5 M11: 10 m
    return (bCalledShotOwned && bRedline) ? CalledShotGateCm : BaseGateCm;
}

float FBreakerWeaponMath::LedgerRefundFraction(int32 LedgerRank)
{
    if (LedgerRank <= 0) return 0.0f;
    return LedgerRank >= 2 ? 0.50f : 0.25f;   // Class-Kits §1.5 M3 R1/R2
}

float FBreakerWeaponMath::MarkJumpRadiusCm(int32 MarkEconomyRank)
{
    if (MarkEconomyRank <= 0) return 0.0f;
    return MarkEconomyRank >= 2 ? 2500.0f : 1500.0f;   // Class-Kits §1.5 M5: 15 m / 25 m
}

int32 FBreakerWeaponMath::LoadedRefundRounds(int32 ShotsInWindow, int32 LoadedRank)
{
    if (LoadedRank <= 0 || ShotsInWindow <= 0) return 0;
    return LoadedRank >= 2 ? ShotsInWindow : ShotsInWindow / 2;   // Class-Kits §1.3 F2 R1/R2
}

int32 FBreakerWeaponMath::MagazineDebitRounds(bool bChamberedRoundArmed)
{
    // AR3: the armed chambered round is free; everything else costs exactly
    // the one round it always has.
    return bChamberedRoundArmed ? 0 : 1;
}

int32 FBreakerWeaponMath::MagazineDumpThresholdRounds(bool bLastRoundOwned)
{
    // AR5: the payout moves from "the magazine emptied" to "your last round".
    return bLastRoundOwned ? 1 : 0;
}

int32 FBreakerWeaponMath::ReserveCapRounds(int32 StartingReserve, float CapMultiplier, bool bNoReserveOwned)
{
    // AR11: the halving applies to the CEILING, inside the ceil, so 2x of an
    // odd starting reserve halves exactly rather than rounding twice.
    const float Multiplier = FMath::Max(0.0f, CapMultiplier) * (bNoReserveOwned ? 0.5f : 1.0f);
    return FMath::CeilToInt(FMath::Max(0, StartingReserve) * Multiplier);
}

bool FBreakerWeaponMath::SpreadReadsStationary(bool bEmplacementOwned, float AnchorDistanceCm, float AnchorNearRadiusCm)
{
    // B7: owned, and physically at the anchor by the Grit layer's own radius.
    // No anchor standing reads as an infinite distance, which fails here
    // honestly rather than needing a separate "none" case.
    return bEmplacementOwned && AnchorNearRadiusCm > 0.0f && AnchorDistanceCm <= AnchorNearRadiusCm;
}

int32 FBreakerWeaponMath::ClampMagazineCapacityDelta(int32 EffectiveSizeWithoutEntry, int32 DeltaRounds)
{
    if (DeltaRounds >= 0) return DeltaRounds;
    // Shrink form: never below one chambered round. A shrink against a
    // magazine already at 1 clamps to zero, which the push refuses to store.
    return FMath::Max(DeltaRounds, 1 - FMath::Max(1, EffectiveSizeWithoutEntry));
}

int32 FBreakerWeaponMath::SecondaryShotSeed(uint32 OwnerHash, int32 ShotSequence, uint32 Salt, int32 Index)
{
    // Same seed material as the primary draws, salted twice: once by the
    // channel (multishot vs chain vs ricochet) and once by the index within
    // it, so no sub-stream can collide with the primary sequence or with a
    // sibling.
    return static_cast<int32>(HashCombine(HashCombine(OwnerHash, static_cast<uint32>(ShotSequence)), Salt + static_cast<uint32>(Index)));
}
