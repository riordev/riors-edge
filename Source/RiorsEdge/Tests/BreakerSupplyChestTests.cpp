#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/BreakerSupplyChestMath.h"
#include "Items/BreakerItemTypes.h"

// ---------------------------------------------------------------------------
// WHAT IS IN A CHEST. The owner's sentence was "either currency or an item in
// there weighted more towards lower value stuff", and both halves are
// assertable without a world:
//
//   EITHER/OR      one chest pays one thing
//   WEIGHTED       currency outweighs items, over the whole seed space
//   LOWER VALUE    an item a chest hands over is never BETTER than the roll
//
// What is NOT assertable here is whether opening one feels worth the walk.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerSupplyChestTest,
    "RiorsEdge.Items.SupplyChest.Contents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    // The rarity ladder, low to high, held once so the two sweeps below cannot
    // disagree about which direction "down" is. Deliberately NOT the
    // enumerator's numeric order: that enum is serialized and append-only, so
    // its integers are a storage detail.
    int32 BreakerSupplyChestRank(EBreakerItemRarity Rarity)
    {
        switch (Rarity)
        {
            case EBreakerItemRarity::Uncommon:    return 1;
            case EBreakerItemRarity::Exceptional: return 2;
            case EBreakerItemRarity::Aberrant:    return 3;
            case EBreakerItemRarity::Unwritten:   return 4;
            default:                              return 0;   // Standard
        }
    }
}

bool FBreakerSupplyChestTest::RunTest(const FString& Parameters)
{
    using namespace BreakerSupplyChest;
    constexpr int32 Samples = 20000;

    // ---- THE SPLIT, over the seed space rather than at three samples -------
    int32 Currency = 0;
    for (int32 Seed = 0; Seed < Samples; ++Seed) Currency += PaysCurrency(Seed) ? 1 : 0;
    const float Share = 100.0f * Currency / Samples;
    const float Authored = 100.0f * CurrencyWeight / (CurrencyWeight + ItemWeight);
    AddInfo(FString::Printf(TEXT("CHEST CONTENTS  currency %.1f%% of %d seeds, authored %.1f%%"),
        Share, Samples, Authored));
    TestTrue(*FString::Printf(TEXT("the split matches its own weights (%.1f%% vs %.1f%%)"), Share, Authored),
        FMath::Abs(Share - Authored) < 2.0f);
    // AND IT IS ACTUALLY WEIGHTED TOWARD CURRENCY. A hash that drifted to an
    // even split would still pass the tolerance above if the weights were ever
    // edited to 50/50; this is the owner's sentence, asserted directly.
    TestTrue(TEXT("currency outweighs items, which is what 'lower value' means here"),
        CurrencyWeight > ItemWeight);

    // ---- DETERMINISM ------------------------------------------------------
    // A chest's contents are a pure function of its seed. Without that, a
    // capture cannot be compared with the one before it and a bug report
    // cannot be reproduced from the seed the log printed.
    for (int32 Seed = -50; Seed < 50; ++Seed)
    {
        TestEqual(*FString::Printf(TEXT("seed %d always pays the same kind"), Seed),
            PaysCurrency(Seed), PaysCurrency(Seed));
        TestEqual(*FString::Printf(TEXT("and tempers the same way at seed %d"), Seed),
            static_cast<int32>(Temper(EBreakerItemRarity::Aberrant, Seed)),
            static_cast<int32>(Temper(EBreakerItemRarity::Aberrant, Seed)));
    }

    // ---- NEVER UPWARD -----------------------------------------------------
    // The one property that must hold for every rarity at every seed: a chest
    // cannot improve a roll. Swept rather than sampled, because a StepDown
    // that skipped a case would still pass at any single rarity.
    const EBreakerItemRarity Ladder[] = { EBreakerItemRarity::Standard, EBreakerItemRarity::Uncommon,
        EBreakerItemRarity::Exceptional, EBreakerItemRarity::Aberrant, EBreakerItemRarity::Unwritten };
    for (const EBreakerItemRarity Rarity : Ladder)
    {
        TestTrue(TEXT("stepping down never goes up"),
            BreakerSupplyChestRank(StepDown(Rarity)) <= BreakerSupplyChestRank(Rarity));
        // Standard is the floor: a chest cannot pay less than the least thing.
        if (Rarity != EBreakerItemRarity::Standard)
        {
            TestEqual(TEXT("and it moves exactly one tier"),
                BreakerSupplyChestRank(StepDown(Rarity)), BreakerSupplyChestRank(Rarity) - 1);
        }
        int32 Stepped = 0;
        for (int32 Seed = 0; Seed < Samples; ++Seed)
        {
            const EBreakerItemRarity Out = Temper(Rarity, Seed);
            if (!TestTrue(TEXT("tempering never improves a roll"),
                BreakerSupplyChestRank(Out) <= BreakerSupplyChestRank(Rarity))) return false;
            Stepped += BreakerSupplyChestRank(Out) < BreakerSupplyChestRank(Rarity) ? 1 : 0;
        }
        if (Rarity != EBreakerItemRarity::Standard)
        {
            const float Rate = 100.0f * Stepped / Samples;
            TestTrue(*FString::Printf(TEXT("and does so at its authored rate (%.1f%% vs %d%%)"),
                    Rate, TemperPercent),
                FMath::Abs(Rate - TemperPercent) < 2.0f);
        }
        else
        {
            TestEqual(TEXT("Standard has nowhere lower to go"), Stepped, 0);
        }
    }

    // ---- A CHEST NEVER PAYS NOTHING ---------------------------------------
    // The defect this pins actually shipped for one suite run: the chest paid
    // the kill roll straight through, a trash body is worth 0 to 1 Riftglass,
    // and a chest credited zero. A bad roll on a kill is one of forty; a bad
    // roll on a chest is the whole interaction.
    for (int32 AreaLevel = 1; AreaLevel <= 60; ++AreaLevel)
    {
        for (int32 PerKill = 0; PerKill <= 12; ++PerKill)
        {
            const int32 Paid = CurrencyPayout(PerKill, AreaLevel);
            if (!TestTrue(*FString::Printf(TEXT("a chest at level %d always pays something"), AreaLevel),
                Paid >= MinimumRiftglass)) return false;
            if (!TestTrue(TEXT("and never less than the kill it is worth several of"),
                Paid >= PerKill)) return false;
        }
        // A deeper yard is a longer walk, so the floor may not fall as the
        // level rises.
        if (AreaLevel > 1)
        {
            TestTrue(TEXT("the floor never falls as the yard gets deeper"),
                CurrencyPayout(0, AreaLevel) >= CurrencyPayout(0, AreaLevel - 1));
        }
    }
    // A negative roll is a defect upstream, not a debt: it cannot make a chest
    // charge the player.
    TestTrue(TEXT("a negative roll cannot become a negative payout"),
        CurrencyPayout(-40, 5) >= MinimumRiftglass);
    AddInfo(FString::Printf(TEXT("CHEST CURRENCY  level 5 pays %d-%d, level 13 pays %d-%d"),
        CurrencyPayout(0, 5), CurrencyPayout(1, 5), CurrencyPayout(0, 13), CurrencyPayout(1, 13)));

    // ---- WHERE ONE STANDS -------------------------------------------------
    // The placement must land inside the yard's own band on every seed, or a
    // chest rolls into a wall and simply is not there — a silent absence the
    // player reads as "chests are rare" rather than as a bug.
    constexpr float HalfWidth = 2000.0f;
    for (int32 Seed = 0; Seed < 4000; ++Seed)
    {
        for (int32 Index = 0; Index < ChestsPerYard; ++Index)
        {
            const float Fraction = PlacementFraction(Seed, Index);
            if (!TestTrue(*FString::Printf(TEXT("chest %d of seed %d lands inside the band"), Index, Seed),
                Fraction >= BandInset - KINDA_SMALL_NUMBER
                && Fraction <= 1.0f - BandInset + KINDA_SMALL_NUMBER)) return false;
            const float Lateral = PlacementLateral(Seed, Index, HalfWidth);
            if (!TestTrue(*FString::Printf(TEXT("and inside its half-width (%.0f)"), Lateral),
                FMath::Abs(Lateral) <= HalfWidth + KINDA_SMALL_NUMBER)) return false;
        }
    }
    // A zero half-width puts every chest on the centreline rather than
    // anywhere: a dial turned to nothing must do nothing, not misbehave.
    TestEqual(TEXT("a zero half-width places on the lane"), PlacementLateral(7, 0, 0.0f), 0.0f);
    TestEqual(TEXT("and a negative one does not mirror"), PlacementLateral(7, 0, -900.0f), 0.0f);

    // THE TWO CHESTS IN A YARD ARE NOT IN THE SAME PLACE. They take different
    // salts, and a salt collision would stack them inside one another.
    {
        int32 Apart = 0;
        for (int32 Seed = 0; Seed < 2000; ++Seed)
            if (FMath::Abs(PlacementFraction(Seed, 0) - PlacementFraction(Seed, 1)) > 0.05f) ++Apart;
        TestTrue(*FString::Printf(TEXT("the two chests in a yard land apart (%d of 2000)"), Apart),
            Apart > 1600);
    }
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
