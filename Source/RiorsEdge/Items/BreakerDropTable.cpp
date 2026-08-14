#include "Items/BreakerDropTable.h"

#include "Items/BreakerAffixLibrary.h"

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

    // Currency's own salts. Three, one per currency stream, for the same
    // reason the item pipeline splits chance from rarity: three grants
    // drawn from the same stream would correlate, and a kill that rolled a
    // generous Slag amount would also tend to roll a generous Flux amount.
    constexpr int32 BreakerCurrencySlagSalt = 0x5A1C0001;
    constexpr int32 BreakerCurrencyFluxSalt = 0x5A1C0002;
    constexpr int32 BreakerCurrencySigilSalt = 0x5A1C0003;

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

    // ---- Currency helpers, shared between RollCurrencyDrop (the roll) and
    // ProjectCurrencyRate (the analytic average) so the two cannot disagree
    // about which rank pays what — the same discipline BreakerDropBuildWeights
    // and BreakerDropApplyGates keep for items.

    void BreakerCurrencySlagRangeForRank(EBreakerMonsterRank Rank, const FBreakerCurrencyDropParams& Params, int32& OutMin, int32& OutMax)
    {
        switch (Rank)
        {
        case EBreakerMonsterRank::Trash:           OutMin = Params.TrashSlagMin; OutMax = Params.TrashSlagMax; return;
        case EBreakerMonsterRank::Elite:           OutMin = Params.EliteSlagMin; OutMax = Params.EliteSlagMax; return;
        case EBreakerMonsterRank::ModifierBearing: OutMin = Params.ModifierBearingSlagMin; OutMax = Params.ModifierBearingSlagMax; return;
        case EBreakerMonsterRank::Boss:            OutMin = Params.BossSlagMin; OutMax = Params.BossSlagMax; return;
        }
        OutMin = 0; OutMax = 0;
    }

    float BreakerCurrencyLevelScalar(int32 ItemLevel, const FBreakerCurrencyDropParams& Params)
    {
        // Same shape as UBreakerForgeLibrary's file-private salvage scalar
        // (BreakerForgeLevelScalar): 1.0 at ilvl 1, growing linearly with the
        // O29 item-level ceiling rather than the old character cap.
        return 1.0f + (FMath::Clamp(ItemLevel, 1, UBreakerAffixLibrary::MaxItemLevel) - 1) * Params.SlagLevelScalarPerLevel;
    }

    // One bound of a currency range, scaled by item level. Shared by the roll
    // and the projection so the two cannot round differently — the whole point
    // of the pairing.
    int32 BreakerCurrencyScaleBound(int32 Bound, float LevelScalar)
    {
        return FMath::RoundToInt(static_cast<float>(Bound) * LevelScalar);
    }

    bool BreakerCurrencyFluxUnlockedForRank(EBreakerMonsterRank Rank, int32 ItemLevel, const FBreakerCurrencyDropParams& Params)
    {
        return UBreakerDropTableLibrary::GetRankLootOrder(Rank) >= UBreakerDropTableLibrary::GetRankLootOrder(Params.FluxMinimumRank)
            && ItemLevel >= Params.FluxMinimumItemLevel;
    }

    void BreakerCurrencyFluxRangeForRank(EBreakerMonsterRank Rank, const FBreakerCurrencyDropParams& Params, int32& OutMin, int32& OutMax)
    {
        // Only Boss authors its own band; every other rank that
        // BreakerCurrencyFluxUnlockedForRank lets through (ModifierBearing at
        // the O2 defaults, or Elite/Trash if FluxMinimumRank is retuned down)
        // uses the ModifierBearing band, because Flux is meant to have one
        // "elite-plus" band and one "boss" band, not four authored ranges.
        if (Rank == EBreakerMonsterRank::Boss) { OutMin = Params.BossFluxMin; OutMax = Params.BossFluxMax; return; }
        OutMin = Params.ModifierBearingFluxMin;
        OutMax = Params.ModifierBearingFluxMax;
    }

    bool BreakerCurrencySigilUnlockedForRank(EBreakerMonsterRank Rank, int32 ItemLevel, const FBreakerCurrencyDropParams& Params)
    {
        return UBreakerDropTableLibrary::GetRankLootOrder(Rank) >= UBreakerDropTableLibrary::GetRankLootOrder(Params.SigilMinimumRank)
            && ItemLevel >= Params.SigilMinimumItemLevel;
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

FBreakerForgeWallet UBreakerDropTableLibrary::RollCurrencyDrop(int32 RandomSeed, int32 ItemLevel, EBreakerMonsterRank Rank,
    const FBreakerCurrencyDropParams& Params)
{
    FBreakerForgeWallet Wallet;
    const float LevelScalar = BreakerCurrencyLevelScalar(ItemLevel, Params);

    // Step 1/3: Slag. Ranged and rank-scaled, paying on every kill — the
    // owner's finding was that NOTHING paid currency from a kill, not that
    // kills should sometimes pay zero the way an item drop deliberately does.
    int32 SlagMin = 0, SlagMax = 0;
    BreakerCurrencySlagRangeForRank(Rank, Params, SlagMin, SlagMax);
    if (SlagMax > 0)
    {
        // THE SCALAR IS APPLIED TO THE RANGE, NOT TO THE SAMPLE, and the
        // difference is not cosmetic. Scaling the sample —
        // Round(RandRange(0,1) * 1.54) at ilvl 10 — can only ever produce 0 or
        // 2, so a trash kill paid an even number of Slag and never an odd one,
        // and the mean landed 30% above the analytic projection. Scaling the
        // ENDPOINTS first draws uniformly across the whole scaled band (0,1,2),
        // which both restores the intended distribution and makes the mean
        // exactly the projection's (Min+Max)/2 — which is what lets
        // RiorsEdge.Items.ForgeDrops.CurrencyPerHour hold the two in agreement
        // at all. Caught by that test, not by inspection.
        const int32 ScaledMin = BreakerCurrencyScaleBound(SlagMin, LevelScalar);
        const int32 ScaledMax = BreakerCurrencyScaleBound(SlagMax, LevelScalar);
        const FRandomStream SlagStream(HashCombine(RandomSeed, BreakerCurrencySlagSalt));
        const int32 SlagAmount = SlagStream.RandRange(ScaledMin, ScaledMax);
        if (SlagAmount > 0) Wallet.Add(EBreakerForgeCurrency::Slag, SlagAmount);
    }

    // Step 2/3: Flux, gated exactly like IsRarityUnlocked — a rank AND an
    // item level, both required, or the grant is zero rather than rerolled.
    if (BreakerCurrencyFluxUnlockedForRank(Rank, ItemLevel, Params))
    {
        int32 FluxMin = 0, FluxMax = 0;
        BreakerCurrencyFluxRangeForRank(Rank, Params, FluxMin, FluxMax);
        if (FluxMax > 0)
        {
            const FRandomStream FluxStream(HashCombine(RandomSeed, BreakerCurrencyFluxSalt));
            const int32 FluxAmount = FluxStream.RandRange(FluxMin, FluxMax);
            if (FluxAmount > 0) Wallet.Add(EBreakerForgeCurrency::Flux, FluxAmount);
        }
    }

    // Step 3/3: Sigil, gated the same way and, at the O2 defaults, reachable
    // from exactly one rank (Boss) — mirroring
    // RiorsEdge.Items.Drops.TrashCannotRollAberrant's "not at any item level
    // whatsoever" for the rarest currency instead of the rarest item.
    if (BreakerCurrencySigilUnlockedForRank(Rank, ItemLevel, Params) && Params.BossSigilMax > 0)
    {
        const FRandomStream SigilStream(HashCombine(RandomSeed, BreakerCurrencySigilSalt));
        const int32 SigilAmount = SigilStream.RandRange(Params.BossSigilMin, Params.BossSigilMax);
        if (SigilAmount > 0) Wallet.Add(EBreakerForgeCurrency::Sigil, SigilAmount);
    }

    return Wallet;
}

FBreakerCurrencyRateProjection UBreakerDropTableLibrary::ProjectCurrencyRate(const FBreakerKillRateSample& Kills, int32 ItemLevel,
    const FBreakerCurrencyDropParams& Params)
{
    FBreakerCurrencyRateProjection Projection;
    const float LevelScalar = BreakerCurrencyLevelScalar(ItemLevel, Params);

    const EBreakerMonsterRank Ranks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };
    const float KillsPerHour[] = {
        Kills.TrashKillsPerHour, Kills.EliteKillsPerHour,
        Kills.ModifierBearingKillsPerHour, Kills.BossKillsPerHour };

    for (int32 Index = 0; Index < 4; ++Index)
    {
        int32 SlagMin = 0, SlagMax = 0;
        BreakerCurrencySlagRangeForRank(Ranks[Index], Params, SlagMin, SlagMax);
        // Scales the BOUNDS and then averages, in exactly the order the roll
        // does it, so the projection is the roll's true mean rather than a
        // continuous approximation of it. See the roll's comment for the defect
        // this shape exists to prevent.
        Projection.SlagPerHour += KillsPerHour[Index]
            * (BreakerCurrencyScaleBound(SlagMin, LevelScalar) + BreakerCurrencyScaleBound(SlagMax, LevelScalar)) * 0.5f;

        if (BreakerCurrencyFluxUnlockedForRank(Ranks[Index], ItemLevel, Params))
        {
            int32 FluxMin = 0, FluxMax = 0;
            BreakerCurrencyFluxRangeForRank(Ranks[Index], Params, FluxMin, FluxMax);
            Projection.FluxPerHour += KillsPerHour[Index] * (FluxMin + FluxMax) * 0.5f;
        }

        if (BreakerCurrencySigilUnlockedForRank(Ranks[Index], ItemLevel, Params))
        {
            Projection.SigilPerHour += KillsPerHour[Index] * (Params.BossSigilMin + Params.BossSigilMax) * 0.5f;
        }
    }

    return Projection;
}
