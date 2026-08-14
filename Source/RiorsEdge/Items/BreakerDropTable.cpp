#include "Items/BreakerDropTable.h"

namespace
{
    // Named with the file's subject: this project has twice shipped a unity-
    // build collision between identically named helpers in two anonymous
    // namespaces (see the note in BreakerLootLibrary.cpp).
    constexpr int32 BreakerDropRarityCount = static_cast<int32>(EBreakerItemRarity::Anomalous) + 1;

    // Sub-seed salts. Arbitrary constants, but FIXED constants: changing one
    // re-rolls every drop in the game from the same kill seeds, which would
    // break the locked "a seed reproduces an item exactly" property for any
    // saved or logged seed.
    constexpr int32 BreakerDropChanceSalt = 0x00D20D19;
    constexpr int32 BreakerDropRaritySalt = 0x00A17E51;

    // The rarity weights BEFORE gating, with the Drop Chance affix's quality
    // effect applied. This is the original RollRarity table, moved verbatim
    // apart from reading the weights off the params struct: Drop Chance drains
    // weight OUT of Standard and pours it into the higher tiers in fixed
    // proportions. Kept as a drain rather than a flat add so a huge Drop Chance
    // roll cannot make Standard negative.
    void BreakerDropBuildWeights(float DropChanceBonusPercent, const FBreakerDropTableParams& Params, float(&OutWeights)[BreakerDropRarityCount])
    {
        float StandardWeight = Params.StandardWeight;
        float UncommonWeight = Params.UncommonWeight;
        float ExceptionalWeight = Params.ExceptionalWeight;
        float AberrantWeight = Params.AberrantWeight;
        float AnomalousWeight = Params.AnomalousWeight;

        const float Bonus = FMath::Clamp(DropChanceBonusPercent, 0.0f, 100.0f) / 100.0f;
        const float Shifted = StandardWeight * Bonus * 0.5f;
        StandardWeight -= Shifted;
        UncommonWeight += Shifted * 0.55f;
        ExceptionalWeight += Shifted * 0.30f;
        AberrantWeight += Shifted * 0.12f;
        AnomalousWeight += Shifted * 0.03f;

        OutWeights[static_cast<int32>(EBreakerItemRarity::Standard)] = StandardWeight;
        OutWeights[static_cast<int32>(EBreakerItemRarity::Uncommon)] = UncommonWeight;
        OutWeights[static_cast<int32>(EBreakerItemRarity::Exceptional)] = ExceptionalWeight;
        OutWeights[static_cast<int32>(EBreakerItemRarity::Aberrant)] = AberrantWeight;
        OutWeights[static_cast<int32>(EBreakerItemRarity::Anomalous)] = AnomalousWeight;
    }

    // Step 2. A gated rarity's weight becomes zero; nothing is redistributed
    // explicitly because the roll normalizes over the surviving total, which is
    // the same thing and cannot drift out of sync with the weights.
    void BreakerDropApplyGates(int32 ItemLevel, EBreakerMonsterRank Rank, const FBreakerDropTableParams& Params, float(&Weights)[BreakerDropRarityCount])
    {
        for (int32 Index = 0; Index < BreakerDropRarityCount; ++Index)
        {
            if (!UBreakerDropTableLibrary::IsRarityUnlocked(static_cast<EBreakerItemRarity>(Index), ItemLevel, Rank, Params))
            {
                Weights[Index] = 0.0f;
            }
        }
    }
}

int32 UBreakerDropTableLibrary::GetRankLootOrder(EBreakerMonsterRank Rank)
{
    switch (Rank)
    {
    case EBreakerMonsterRank::Trash:           return static_cast<int32>(EBreakerLootRankOrder::Trash);
    case EBreakerMonsterRank::Elite:           return static_cast<int32>(EBreakerLootRankOrder::Elite);
    case EBreakerMonsterRank::ModifierBearing: return static_cast<int32>(EBreakerLootRankOrder::ModifierBearing);
    case EBreakerMonsterRank::Boss:            return static_cast<int32>(EBreakerLootRankOrder::Boss);
    }
    return static_cast<int32>(EBreakerLootRankOrder::Trash);
}

float UBreakerDropTableLibrary::GetRankDropChance(EBreakerMonsterRank Rank, const FBreakerDropTableParams& Params)
{
    switch (Rank)
    {
    case EBreakerMonsterRank::Trash:           return Params.TrashDropChance;
    case EBreakerMonsterRank::Elite:           return Params.EliteDropChance;
    case EBreakerMonsterRank::ModifierBearing: return Params.ModifierBearingDropChance;
    case EBreakerMonsterRank::Boss:            return Params.BossDropChance;
    }
    return Params.TrashDropChance;
}

float UBreakerDropTableLibrary::GetEffectiveDropChance(EBreakerMonsterRank Rank, float DropChanceBonusPercent, const FBreakerDropTableParams& Params)
{
    const float Base = GetRankDropChance(Rank, Params);
    const float Bonus = FMath::Max(DropChanceBonusPercent, 0.0f) / 100.0f * FMath::Max(Params.DropChanceQuantityScale, 0.0f);
    return FMath::Clamp(Base * (1.0f + Bonus), 0.0f, 1.0f);
}

bool UBreakerDropTableLibrary::RollsDrop(int32 RandomSeed, EBreakerMonsterRank Rank, float DropChanceBonusPercent, const FBreakerDropTableParams& Params)
{
    const float Chance = GetEffectiveDropChance(Rank, DropChanceBonusPercent, Params);
    if (Chance <= 0.0f) return false;
    if (Chance >= 1.0f) return true;
    return FRandomStream(RandomSeed).FRand() < Chance;
}

bool UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity Rarity, int32 ItemLevel, EBreakerMonsterRank Rank, const FBreakerDropTableParams& Params)
{
    const int32 Order = GetRankLootOrder(Rank);
    switch (Rarity)
    {
    // Standard and Uncommon are ungated by design. Something must always be
    // rollable, or a drop that passed step 1 would have an empty table and the
    // pipeline would have to invent a fallback — which is exactly the kind of
    // silent special case that hides a balance bug.
    case EBreakerItemRarity::Standard:
    case EBreakerItemRarity::Uncommon:
        return true;
    case EBreakerItemRarity::Exceptional:
        return ItemLevel >= Params.ExceptionalMinimumItemLevel
            && Order >= GetRankLootOrder(Params.ExceptionalMinimumRank);
    case EBreakerItemRarity::Aberrant:
        return ItemLevel >= Params.AberrantMinimumItemLevel
            && Order >= GetRankLootOrder(Params.AberrantMinimumRank);
    case EBreakerItemRarity::Anomalous:
        return ItemLevel >= Params.AnomalousMinimumItemLevel
            && Order >= GetRankLootOrder(Params.AnomalousMinimumRank);
    }
    return false;
}

void UBreakerDropTableLibrary::GetGatedRarityProbabilities(int32 ItemLevel, EBreakerMonsterRank Rank, float DropChanceBonusPercent,
    const FBreakerDropTableParams& Params, TArray<float>& OutProbabilities)
{
    float Weights[BreakerDropRarityCount];
    BreakerDropBuildWeights(DropChanceBonusPercent, Params, Weights);
    BreakerDropApplyGates(ItemLevel, Rank, Params, Weights);

    float Total = 0.0f;
    for (const float Weight : Weights) Total += FMath::Max(Weight, 0.0f);

    OutProbabilities.SetNumZeroed(BreakerDropRarityCount);
    if (Total <= 0.0f)
    {
        // Cannot happen while Standard is ungated, but a params block with
        // every weight zeroed is authorable and would otherwise divide by zero.
        OutProbabilities[static_cast<int32>(EBreakerItemRarity::Standard)] = 1.0f;
        return;
    }
    for (int32 Index = 0; Index < BreakerDropRarityCount; ++Index)
    {
        OutProbabilities[Index] = FMath::Max(Weights[Index], 0.0f) / Total;
    }
}

EBreakerItemRarity UBreakerDropTableLibrary::RollGatedRarity(int32 RandomSeed, int32 ItemLevel, EBreakerMonsterRank Rank,
    float DropChanceBonusPercent, const FBreakerDropTableParams& Params)
{
    float Weights[BreakerDropRarityCount];
    BreakerDropBuildWeights(DropChanceBonusPercent, Params, Weights);
    BreakerDropApplyGates(ItemLevel, Rank, Params, Weights);

    float Total = 0.0f;
    for (const float Weight : Weights) Total += FMath::Max(Weight, 0.0f);
    if (Total <= 0.0f) return EBreakerItemRarity::Standard;

    // One FRand from one stream, walked low rarity to high — the same shape the
    // original flat RollRarity used, so a seed's position in the table is
    // unchanged and the determinism test still means what it meant.
    FRandomStream Random(RandomSeed);
    float Roll = Random.FRand() * Total;
    EBreakerItemRarity Last = EBreakerItemRarity::Standard;
    for (int32 Index = 0; Index < BreakerDropRarityCount; ++Index)
    {
        const float Weight = FMath::Max(Weights[Index], 0.0f);
        if (Weight <= 0.0f) continue;
        Last = static_cast<EBreakerItemRarity>(Index);
        if ((Roll -= Weight) < 0.0f) return Last;
    }
    // Float slop only. Falls to the highest UNLOCKED rarity rather than to
    // Anomalous unconditionally, which is the bug the original's bare
    // `return Anomalous` tail would have had the moment a weight hit zero.
    return Last;
}

bool UBreakerDropTableLibrary::RollDrop(int32 RandomSeed, int32 ItemLevel, EBreakerMonsterRank Rank,
    float DropChanceBonusPercent, const FBreakerDropTableParams& Params, EBreakerItemRarity& OutRarity)
{
    OutRarity = EBreakerItemRarity::Standard;
    if (!RollsDrop(HashCombine(RandomSeed, BreakerDropChanceSalt), Rank, DropChanceBonusPercent, Params)) return false;
    OutRarity = RollGatedRarity(HashCombine(RandomSeed, BreakerDropRaritySalt), ItemLevel, Rank, DropChanceBonusPercent, Params);
    return true;
}

FBreakerLootRateProjection UBreakerDropTableLibrary::ProjectLootRate(const FBreakerKillRateSample& Kills, int32 ItemLevel,
    float DropChanceBonusPercent, const FBreakerDropTableParams& Params)
{
    FBreakerLootRateProjection Projection;

    const EBreakerMonsterRank Ranks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };
    const float KillsPerHour[] = {
        Kills.TrashKillsPerHour, Kills.EliteKillsPerHour,
        Kills.ModifierBearingKillsPerHour, Kills.BossKillsPerHour };

    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float Drops = KillsPerHour[Index] * GetEffectiveDropChance(Ranks[Index], DropChanceBonusPercent, Params);
        Projection.ItemsPerHour += Drops;

        TArray<float> Probabilities;
        GetGatedRarityProbabilities(ItemLevel, Ranks[Index], DropChanceBonusPercent, Params, Probabilities);
        const float Aberrant = Probabilities[static_cast<int32>(EBreakerItemRarity::Aberrant)];
        const float Anomalous = Probabilities[static_cast<int32>(EBreakerItemRarity::Anomalous)];
        Projection.ExceptionalOrBetterPerHour += Drops * (Probabilities[static_cast<int32>(EBreakerItemRarity::Exceptional)] + Aberrant + Anomalous);
        Projection.AberrantPerHour += Drops * Aberrant;
        Projection.AnomalousPerHour += Drops * Anomalous;
    }

    // Infinity is the honest answer when the rarity is gated out of this
    // content entirely: not "eventually", never.
    Projection.HoursPerAberrant = Projection.AberrantPerHour > 0.0f
        ? 1.0f / Projection.AberrantPerHour : TNumericLimits<float>::Max();
    Projection.HoursPerAnomalous = Projection.AnomalousPerHour > 0.0f
        ? 1.0f / Projection.AnomalousPerHour : TNumericLimits<float>::Max();
    return Projection;
}
