#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerDropOverflowMath.h"

// ---------------------------------------------------------------------------
// O249: "Drop Chance above its cap converts to rarity weight. A stat that
// saturates is a stat the player stops reading."
//
// The ruling has two halves and a test that pins only one of them is worse than
// no test, because the dangerous half is the quiet one. A conversion bolted
// onto the top of a stat is an invitation to retune the bottom of it by
// accident, and nothing in a statistical sweep would ever notice a below-cap
// table that had drifted by half a percent.
//
//   BELOW THE CAP the arithmetic is untouched. Asserted twice: the overflow is
//   EXACTLY 0.0f everywhere at or under the saturation point (which is the
//   guard BreakerDropBuildWeights branches on, so zero overflow means the
//   conversion's additions never execute), and the shipped probability vector
//   still matches the pre-ruling formula recomputed here from first principles.
//
//   ABOVE THE CAP a further point of Drop Chance measurably moves the rarity
//   outcome, on a rank where step 1 provably cannot move at all.
//
// The second half is checked analytically first and then by seeded rolls. The
// analytic vector is exact, so it can resolve a single point of Drop Chance;
// the sweep proves the roll actually consumes the weights the vector describes,
// which no amount of arithmetic on its own can establish.
// ---------------------------------------------------------------------------

namespace
{
    // Anonymous-namespace helpers carry the subject prefix: unity builds merge
    // translation units, and this project has shipped that collision twice.
    constexpr int32 BreakerDropChanceOverflowSweepRolls = 40000;

    // Derived from the enum, never spelled 5: EBreakerItemRarity is serialized
    // by value and append-only, so a sixth rarity must widen this on its own.
    constexpr int32 BreakerDropChanceOverflowRarityCount = static_cast<int32>(EBreakerItemRarity::Unwritten) + 1;

    int32 BreakerDropChanceOverflowSeed(int32 Index)
    {
        return HashCombine(Index * 2654435761u, 0x0249F00D);
    }

    // THE PRE-RULING TABLE, written out rather than referenced. Its whole value
    // is that it is an independent copy: if someone edits the shipped shift,
    // this disagrees and says so. Read it as the definition of "as before".
    void BreakerDropChanceOverflowLegacyProbabilities(int32 ItemLevel, EBreakerMonsterRank Rank,
        float DropChanceBonusPercent, const FBreakerDropTableParams& Params, TArray<float>& OutProbabilities)
    {
        float Weights[BreakerDropChanceOverflowRarityCount];
        float StandardWeight = Params.StandardWeight;
        float UncommonWeight = Params.UncommonWeight;
        float ExceptionalWeight = Params.ExceptionalWeight;
        float AberrantWeight = Params.AberrantWeight;
        float UnwrittenWeight = Params.UnwrittenWeight;

        const float Bonus = FMath::Clamp(DropChanceBonusPercent, 0.0f, 100.0f) / 100.0f;
        const float Shifted = StandardWeight * Bonus * 0.5f;
        StandardWeight -= Shifted;
        UncommonWeight += Shifted * 0.55f;
        ExceptionalWeight += Shifted * 0.30f;
        AberrantWeight += Shifted * 0.12f;
        UnwrittenWeight += Shifted * 0.03f;

        // THE EARLY-LEVEL EXCEPTIONAL RAMP, applied here too. This helper is a
        // DELIBERATE second copy of the pre-O249 weight formula, kept so this
        // file can prove that O249 changed nothing below the Drop Chance cap.
        // The ramp is a different and later ruling, so it belongs on BOTH
        // sides of that comparison — carrying it on one side only would make
        // the ramp look like an O249 side effect, which it is not. The suite
        // caught exactly that, and it is the repeated-shape defect
        // Scripts/shapecheck.py exists to find.
        ExceptionalWeight *= UBreakerDropTableLibrary::ExceptionalWeightScalar(ItemLevel, Params);

        Weights[static_cast<int32>(EBreakerItemRarity::Standard)] = StandardWeight;
        Weights[static_cast<int32>(EBreakerItemRarity::Uncommon)] = UncommonWeight;
        Weights[static_cast<int32>(EBreakerItemRarity::Exceptional)] = ExceptionalWeight;
        Weights[static_cast<int32>(EBreakerItemRarity::Aberrant)] = AberrantWeight;
        Weights[static_cast<int32>(EBreakerItemRarity::Unwritten)] = UnwrittenWeight;

        float Total = 0.0f;
        for (int32 Index = 0; Index < BreakerDropChanceOverflowRarityCount; ++Index)
        {
            if (!UBreakerDropTableLibrary::IsRarityUnlocked(static_cast<EBreakerItemRarity>(Index), ItemLevel, Rank, Params))
            {
                Weights[Index] = 0.0f;
            }
            Total += FMath::Max(Weights[Index], 0.0f);
        }

        OutProbabilities.SetNumZeroed(BreakerDropChanceOverflowRarityCount);
        if (Total <= 0.0f)
        {
            OutProbabilities[static_cast<int32>(EBreakerItemRarity::Standard)] = 1.0f;
            return;
        }
        for (int32 Index = 0; Index < BreakerDropChanceOverflowRarityCount; ++Index)
        {
            OutProbabilities[Index] = FMath::Max(Weights[Index], 0.0f) / Total;
        }
    }

    // One number for "how good was that drop": the probability-weighted rarity
    // index. Comparing whole vectors term by term answers "did anything move";
    // this answers "did it move UP", which is the ruling's actual claim.
    float BreakerDropChanceOverflowMeanRarity(const TArray<float>& Probabilities)
    {
        float Mean = 0.0f;
        for (int32 Index = 0; Index < Probabilities.Num(); ++Index)
        {
            Mean += Probabilities[Index] * static_cast<float>(Index);
        }
        return Mean;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDropChanceOverflowTest,
    "RiorsEdge.Items.Drops.DropChanceOverflowConvertsToRarity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDropChanceOverflowTest::RunTest(const FString& Parameters)
{
    const FBreakerDropTableParams Params;

    // ---- The shipped configuration, asserted against the default-constructed
    // state, because the ruling only bites if the shipped table actually
    // saturates. ABreakerEnemy::DropTable is a default-constructed
    // FBreakerDropTableParams, so this IS the table a kill rolls against.
    TestTrue(TEXT("The shipped boss chance is authored at the probability ceiling"),
        FMath::IsNearlyEqual(Params.BossDropChance, 1.0f));
    TestTrue(TEXT("A boss therefore saturates at zero bonus: every rolled point is overflow"),
        UBreakerDropTableLibrary::GetDropChanceSaturationPercent(EBreakerMonsterRank::Boss, Params) <= 0.0f);
    // Trash is the far end of the same scale and is nowhere near its cap, so
    // the two halves below are both reachable in the shipped game rather than
    // one of them being an arithmetic curiosity.
    TestTrue(TEXT("Trash saturates far above any roll a player can wear"),
        UBreakerDropTableLibrary::GetDropChanceSaturationPercent(EBreakerMonsterRank::Trash, Params) > 100.0f);

    // ---- HALF ONE: below the cap, nothing moved. ---------------------------
    {
        const float TrashSaturation =
            UBreakerDropTableLibrary::GetDropChanceSaturationPercent(EBreakerMonsterRank::Trash, Params);
        AddInfo(FString::Printf(TEXT("Trash saturates at +%.1f%% Drop Chance; sweeping the whole range under it."),
            TrashSaturation));

        // EXACTLY zero, not nearly zero. BreakerDropBuildWeights branches on
        // the drain being above zero, so an exact zero here is the statement
        // that the conversion's additions do not execute at all below the cap.
        for (float BonusPercent = 0.0f; BonusPercent <= TrashSaturation; BonusPercent += 25.0f)
        {
            const float Overflow =
                UBreakerDropTableLibrary::GetDropChanceOverflowPercent(EBreakerMonsterRank::Trash, BonusPercent, Params);
            if (Overflow != 0.0f)
            {
                AddError(FString::Printf(
                    TEXT("Below the cap the overflow must be exactly zero, not %.9g (+%.1f%% at Trash)"),
                    Overflow, BonusPercent));
                break;
            }
            if (BreakerDropOverflow::DrainFraction(Overflow) != 0.0f)
            {
                AddError(TEXT("A zero overflow must drain exactly nothing"));
                break;
            }
        }
        TestTrue(TEXT("Standing exactly on the cap still spends nothing on rarity"),
            UBreakerDropTableLibrary::GetDropChanceOverflowPercent(EBreakerMonsterRank::Trash, TrashSaturation, Params) == 0.0f);

        // And the table itself, against the pre-ruling formula recomputed above.
        // The tolerance is an ulp guard rather than a slack allowance: the two
        // copies live in different translation units, so float contraction can
        // legitimately differ in the last place. The exact-zero assertions
        // above are what carry the bit-identical claim; this carries the claim
        // that the FORMULA under the cap is the one it always was.
        const int32 ItemLevels[] = { 1, 10, 25, 50 };
        const float Bonuses[] = { 0.0f, 12.0f, 50.0f, 100.0f, 400.0f, 899.0f };
        for (const int32 ItemLevel : ItemLevels)
        {
            for (const float BonusPercent : Bonuses)
            {
                TArray<float> Shipped;
                TArray<float> Legacy;
                UBreakerDropTableLibrary::GetGatedRarityProbabilities(
                    ItemLevel, EBreakerMonsterRank::Trash, BonusPercent, Params, Shipped);
                BreakerDropChanceOverflowLegacyProbabilities(
                    ItemLevel, EBreakerMonsterRank::Trash, BonusPercent, Params, Legacy);
                for (int32 Index = 0; Index < Shipped.Num(); ++Index)
                {
                    if (FMath::Abs(Shipped[Index] - Legacy[Index]) > 1.0e-6f)
                    {
                        AddError(FString::Printf(
                            TEXT("Below the cap the table changed: rarity %d at ilvl %d with +%.0f%% is %.9g, was %.9g"),
                            Index, ItemLevel, BonusPercent, Shipped[Index], Legacy[Index]));
                    }
                }
            }
        }
    }

    // ---- HALF TWO: above the cap, a further point buys something. ----------
    {
        // A boss at an item level where every gate is open, so the conversion
        // has the whole ladder to move weight along and a failure cannot be
        // blamed on a gated-out tier absorbing the shift.
        constexpr int32 OpenItemLevel = 50;
        const EBreakerMonsterRank Rank = EBreakerMonsterRank::Boss;

        // The premise of the ruling, stated as an assertion rather than assumed:
        // step 1 is immovable here, so anything that moves below is the
        // conversion and nothing else.
        const float ChanceAtHundred = UBreakerDropTableLibrary::GetEffectiveDropChance(Rank, 100.0f, Params);
        const float ChanceAtHundredOne = UBreakerDropTableLibrary::GetEffectiveDropChance(Rank, 101.0f, Params);
        TestTrue(TEXT("Step 1 is saturated at a boss and the extra point cannot move it"),
            ChanceAtHundred == ChanceAtHundredOne && FMath::IsNearlyEqual(ChanceAtHundred, 1.0f));

        // ONE point, resolved analytically. This is the ruling in a single
        // assertion: the point step 1 cannot spend still buys rarity.
        TArray<float> AtHundred;
        TArray<float> AtHundredOne;
        UBreakerDropTableLibrary::GetGatedRarityProbabilities(OpenItemLevel, Rank, 100.0f, Params, AtHundred);
        UBreakerDropTableLibrary::GetGatedRarityProbabilities(OpenItemLevel, Rank, 101.0f, Params, AtHundredOne);

        const int32 StandardIndex = static_cast<int32>(EBreakerItemRarity::Standard);
        AddInfo(FString::Printf(TEXT("Boss at +100%%: P(Standard) %.6f, mean rarity %.6f. At +101%%: %.6f / %.6f."),
            AtHundred[StandardIndex], BreakerDropChanceOverflowMeanRarity(AtHundred),
            AtHundredOne[StandardIndex], BreakerDropChanceOverflowMeanRarity(AtHundredOne)));
        TestTrue(TEXT("One more point of Drop Chance past the cap takes weight out of Standard"),
            AtHundredOne[StandardIndex] < AtHundred[StandardIndex]);
        TestTrue(TEXT("One more point of Drop Chance past the cap raises the mean rarity"),
            BreakerDropChanceOverflowMeanRarity(AtHundredOne) > BreakerDropChanceOverflowMeanRarity(AtHundred));

        // And it keeps buying, rather than reaching a second cap further out —
        // which is the failure mode the ruling names. Monotone across a ladder
        // that runs well past anything a player can wear.
        const float Ladder[] = { 0.0f, 50.0f, 100.0f, 200.0f, 400.0f, 800.0f, 5000.0f };
        float PreviousMean = -1.0f;
        for (const float BonusPercent : Ladder)
        {
            TArray<float> Probabilities;
            UBreakerDropTableLibrary::GetGatedRarityProbabilities(OpenItemLevel, Rank, BonusPercent, Params, Probabilities);
            const float Mean = BreakerDropChanceOverflowMeanRarity(Probabilities);
            TestTrue(FString::Printf(TEXT("Drop Chance still buys rarity at +%.0f%% (mean rarity %.6f)"), BonusPercent, Mean),
                Mean > PreviousMean);
            // No amount of conversion may take a weight negative, which is why
            // the drain is a share of what remains rather than a flat subtraction.
            for (const float Probability : Probabilities)
            {
                if (Probability < 0.0f)
                {
                    AddError(FString::Printf(TEXT("A converted weight went negative at +%.0f%%"), BonusPercent));
                    break;
                }
            }
            PreviousMean = Mean;
        }

        // ---- THE SWEEP. The vector above is arithmetic; this is the roll the
        // game actually performs, and it is the only thing that proves the
        // conversion reaches a player rather than only a probability table.
        // A single point is below this sweep's resolution, so the sweep is run
        // at a separation it can actually resolve and the one-point claim is
        // left to the exact vector above. Widening the step here would be
        // measuring nothing and calling it a pass.
        constexpr float SweepLow = 100.0f;
        constexpr float SweepHigh = 800.0f;
        int32 HighTierAtLow = 0;
        int32 HighTierAtHigh = 0;
        for (int32 Index = 0; Index < BreakerDropChanceOverflowSweepRolls; ++Index)
        {
            const int32 Seed = BreakerDropChanceOverflowSeed(Index);
            if (UBreakerDropTableLibrary::RollGatedRarity(Seed, OpenItemLevel, Rank, SweepLow, Params) >= EBreakerItemRarity::Exceptional) ++HighTierAtLow;
            if (UBreakerDropTableLibrary::RollGatedRarity(Seed, OpenItemLevel, Rank, SweepHigh, Params) >= EBreakerItemRarity::Exceptional) ++HighTierAtHigh;
        }
        AddInfo(FString::Printf(
            TEXT("Exceptional or better over %d seeded boss rolls: %d at +%.0f%%, %d at +%.0f%%."),
            BreakerDropChanceOverflowSweepRolls, HighTierAtLow, SweepLow, HighTierAtHigh, SweepHigh));
        TestTrue(TEXT("Overflow Drop Chance raises Exceptional-or-better through the real roll"),
            HighTierAtHigh > HighTierAtLow);

        // Determinism survives the second lane. A seed reproducing an item
        // exactly is a locked property of this pipeline, and a conversion that
        // read anything but its own inputs would break it.
        TestEqual(TEXT("The converted roll is still deterministic"),
            static_cast<int32>(UBreakerDropTableLibrary::RollGatedRarity(42, OpenItemLevel, Rank, 600.0f, Params)),
            static_cast<int32>(UBreakerDropTableLibrary::RollGatedRarity(42, OpenItemLevel, Rank, 600.0f, Params)));
    }

    // ---- The lane switch. FBreakerDropTableParams documents a zero quantity
    // scale as "the affix goes back to being a pure quality stat"; the
    // conversion must not quietly pay those points a second time.
    {
        FBreakerDropTableParams QualityOnly;
        QualityOnly.DropChanceQuantityScale = 0.0f;
        TestTrue(TEXT("A switched-off quantity lane routes nothing through the conversion"),
            UBreakerDropTableLibrary::GetDropChanceOverflowPercent(EBreakerMonsterRank::Boss, 500.0f, QualityOnly) == 0.0f);

        FBreakerDropTableParams NeverDrops;
        NeverDrops.BossDropChance = 0.0f;
        TestTrue(TEXT("A rank that never drops converts nothing, because there is no drop to be rare"),
            UBreakerDropTableLibrary::GetDropChanceOverflowPercent(EBreakerMonsterRank::Boss, 500.0f, NeverDrops) == 0.0f);
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
