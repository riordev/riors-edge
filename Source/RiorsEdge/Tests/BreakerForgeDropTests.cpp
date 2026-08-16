#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerForgeLibrary.h"

// Coverage for the currency-drop pipeline (Items/BreakerDropTable.h,
// UBreakerDropTableLibrary::RollCurrencyDrop / ProjectCurrencyRate), which
// exists because of one owner playtest finding: "resources for crafting
// should drop from mobs at a reasonable rate." Before this pass,
// UBreakerForgeLibrary::SalvageValue was the ONLY currency source in the
// game — the wallet only filled by destroying loot the player already owned.
//
// Since the one-currency consolidation (owner ruling 2026-08-16) the pipeline
// pays a single ranged, rank-scaled Riftglass grant per kill. The old
// three-stream gates are gone; what this file now pins is the shape that
// replaced them: rank ordering, every-kill payment, and the documented
// per-hour rate staying in agreement with what the roll actually pays.
//
// Deliberately mirrors BreakerDropTableTests.cpp's style, macros and naming.

namespace
{
    // Same shape as BreakerDropSweepKills/BreakerDropSweepSeed in
    // BreakerDropTableTests.cpp (private to that file, so re-declared here).
    constexpr int32 BreakerForgeDropSweepKills = 20000;

    int32 BreakerForgeDropSweepSeed(int32 Index, int32 Salt)
    {
        return HashCombine(Index * 2654435761u, Salt);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeCurrencyRankScalingTest,
    "RiorsEdge.Items.ForgeDrops.CurrencyRankScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeCurrencyRankScalingTest::RunTest(const FString& Parameters)
{
    const FBreakerCurrencyDropParams Params;
    constexpr int32 ItemLevel = 50;

    const EBreakerMonsterRank Ranks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };

    float AverageByRank[4] = {};

    for (int32 RankIndex = 0; RankIndex < 4; ++RankIndex)
    {
        double Total = 0.0;
        for (int32 Index = 0; Index < BreakerForgeDropSweepKills; ++Index)
        {
            const int32 Seed = BreakerForgeDropSweepSeed(Index, 0x100 + RankIndex);
            Total += UBreakerDropTableLibrary::RollCurrencyDrop(Seed, ItemLevel, Ranks[RankIndex], Params).Get();
        }
        AverageByRank[RankIndex] = static_cast<float>(Total / BreakerForgeDropSweepKills);
        AddInfo(FString::Printf(TEXT("Rank %d @ ilvl %d: avg Riftglass %.3f"),
            RankIndex, ItemLevel, AverageByRank[RankIndex]));
    }

    // THE RANK ORDER is the design; the values are O2 placeholders. A boss
    // kill must pay at least as much as a modifier-bearer, which must pay at
    // least as much as an elite, which must pay at least as much as trash —
    // the same "difficulty follows loot" rule ChanceByRank pins for items
    // (O27: trash is trivialized, difficulty lives above it).
    TestTrue(TEXT("Boss Riftglass >= ModifierBearing Riftglass"), AverageByRank[3] >= AverageByRank[2]);
    TestTrue(TEXT("ModifierBearing Riftglass >= Elite Riftglass"), AverageByRank[2] >= AverageByRank[1]);
    TestTrue(TEXT("Elite Riftglass >= Trash Riftglass"), AverageByRank[1] >= AverageByRank[0]);
    // Strict at the O2 defaults (distinct ranges per rank), so a retune that
    // collapsed two ranks to the same band would be caught rather than
    // silently passing the >= checks above. The boss band folds in what the
    // old gated Flux/Sigil streams paid, so it sits an order above trash.
    TestTrue(TEXT("Boss Riftglass is strictly more than 5x Trash Riftglass"), AverageByRank[3] > AverageByRank[0] * 5.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeCurrencyEveryKillPaysTest,
    "RiorsEdge.Items.ForgeDrops.EveryKillPays",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeCurrencyEveryKillPaysTest::RunTest(const FString& Parameters)
{
    const FBreakerCurrencyDropParams Params;

    // THE INVARIANT THAT LANDED WITH THE KILL-CREDIT PASS AND MUST NOT
    // REGRESS: the credit path pays on every kill. Elite and above have a
    // nonzero range minimum at the O2 defaults, so every single roll pays;
    // Trash's minimum is authored at zero (a trash kill may pay nothing),
    // but the RANK must still be able to pay, or "resources drop from mobs
    // at a reasonable rate" would be silently unaddressed for it.
    for (int32 ItemLevel = 1; ItemLevel <= 120; ItemLevel += 7)
    {
        for (const EBreakerMonsterRank Rank : { EBreakerMonsterRank::Elite,
            EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss })
        {
            for (int32 Index = 0; Index < 200; ++Index)
            {
                const int32 Paid = UBreakerDropTableLibrary::RollCurrencyDrop(
                    BreakerForgeDropSweepSeed(Index, 0x200 + ItemLevel * 7 + static_cast<int32>(Rank)),
                    ItemLevel, Rank, Params).Get();
                if (Paid <= 0)
                {
                    AddError(FString::Printf(TEXT("Rank %d paid nothing at ilvl %d"), static_cast<int32>(Rank), ItemLevel));
                    break;
                }
            }
        }
    }

    bool bTrashPaid = false;
    for (int32 Index = 0; Index < 200 && !bTrashPaid; ++Index)
    {
        bTrashPaid = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(Index, 0x230), 1, EBreakerMonsterRank::Trash, Params).Get() > 0;
    }
    TestTrue(TEXT("Trash can pay Riftglass"), bTrashPaid);

    // Deterministic from the kill seed, like every other roll in the drop
    // pipeline: the same seed always produces the same credit.
    const int32 First = UBreakerDropTableLibrary::RollCurrencyDrop(1234, 50, EBreakerMonsterRank::Boss, Params).Get();
    const int32 Second = UBreakerDropTableLibrary::RollCurrencyDrop(1234, 50, EBreakerMonsterRank::Boss, Params).Get();
    TestEqual(TEXT("A seed reproduces a currency credit exactly"), First, Second);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeCurrencyPerHourTest,
    "RiorsEdge.Items.ForgeDrops.CurrencyPerHour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeCurrencyPerHourTest::RunTest(const FString& Parameters)
{
    // THE IMPORTANT TEST: proves ProjectCurrencyRate's analytic answer by
    // SIMULATING the same hours through the real RollCurrencyDrop, mirroring
    // RiorsEdge.Items.Drops.LootPerHour — this is what stops the documented
    // currency rate from drifting away from what the pipeline actually pays.
    const FBreakerCurrencyDropParams Params;
    const FBreakerKillRateSample Kills;

    struct FSweepRow { EBreakerMonsterRank Rank; float KillsPerHour; };
    const FSweepRow Rows[] = {
        { EBreakerMonsterRank::Trash,           Kills.TrashKillsPerHour },
        { EBreakerMonsterRank::Elite,           Kills.EliteKillsPerHour },
        { EBreakerMonsterRank::ModifierBearing, Kills.ModifierBearingKillsPerHour },
        { EBreakerMonsterRank::Boss,            Kills.BossKillsPerHour } };

    // The same documented stops LootPerHour uses; there is no gate to open
    // any more, so these now only exercise the level scalar.
    const int32 ItemLevels[] = { 5, 10, 25, 50 };
    for (const int32 ItemLevel : ItemLevels)
    {
        const FBreakerCurrencyRateProjection Projection = UBreakerDropTableLibrary::ProjectCurrencyRate(Kills, ItemLevel, Params);

        constexpr int32 SimulatedHours = 200;
        double Riftglass = 0.0;
        for (const FSweepRow& Row : Rows)
        {
            const int32 KillCount = FMath::RoundToInt(Row.KillsPerHour) * SimulatedHours;
            for (int32 Index = 0; Index < KillCount; ++Index)
            {
                const int32 Seed = BreakerForgeDropSweepSeed(Index, ItemLevel * 977 + static_cast<int32>(Row.Rank) + 0x300);
                Riftglass += UBreakerDropTableLibrary::RollCurrencyDrop(Seed, ItemLevel, Row.Rank, Params).Get();
            }
        }
        const float Measured = static_cast<float>(Riftglass / SimulatedHours);

        AddInfo(FString::Printf(TEXT("ilvl %d | Riftglass/h %.1f (proj %.1f)"),
            ItemLevel, Measured, Projection.RiftglassPerHour));

        // Riftglass reaches thousands/hour at ilvl 50, so the tolerance is
        // relative rather than the ~2-item absolute band LootPerHour uses.
        TestTrue(FString::Printf(TEXT("Riftglass per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(Measured - Projection.RiftglassPerHour) < FMath::Max(5.0f, Projection.RiftglassPerHour * 0.05f));
    }

    // THE DOCUMENTED FIGURES the ladder table in BreakerForgeLibrary.cpp
    // leans on, so a retune that moves them fails this loudly instead of
    // silently. O2 PLACEHOLDER bands, wide because the exact values are the
    // owner's to move — the SHAPE (visible within minutes, thousands per
    // endgame hour, boss-heavy) is what is being asserted.
    const FBreakerCurrencyRateProjection AtFifty = UBreakerDropTableLibrary::ProjectCurrencyRate(Kills, 50, Params);
    TestTrue(TEXT("Riftglass per hour at ilvl 50 is in the low thousands, not near zero and not tens of thousands"),
        AtFifty.RiftglassPerHour > 2000.0f && AtFifty.RiftglassPerHour < 6000.0f);

    // PERCEPTIBLE IN THE GYM (O2's second half): a few minutes of on-level
    // trash-and-elite fighting must visibly move the wallet. At ilvl 1 the
    // full mix must clear a modest total within five minutes, which is the
    // shortest plausible gym session.
    const FBreakerCurrencyRateProjection AtOne = UBreakerDropTableLibrary::ProjectCurrencyRate(Kills, 1, Params);
    const float FiveMinuteRiftglass = AtOne.RiftglassPerHour / 12.0f;
    TestTrue(TEXT("Five minutes of on-level killing visibly moves the wallet (>= 20 Riftglass)"), FiveMinuteRiftglass >= 20.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeWalletCreditTest,
    "RiorsEdge.Items.ForgeDrops.WalletCreditAdditiveNoNegative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeWalletCreditTest::RunTest(const FString& Parameters)
{
    // The wallet-credit contract UBreakerEquipmentComponent::CreditForgeCurrency
    // relies on: FBreakerForgeWallet::Add is additive across repeated calls
    // (a kill's currency stacks onto whatever the player already has) and
    // clamps at zero rather than going negative (a bug in a spend path must
    // not be able to leave the wallet in an unrepresentable state).
    FBreakerForgeWallet Wallet;
    TestEqual(TEXT("A fresh wallet starts at zero"), Wallet.Get(), 0);

    Wallet.Add(12);
    Wallet.Add(8);
    TestEqual(TEXT("Repeated grants are additive"), Wallet.Get(), 20);

    // The clamp: a negative Add larger than the balance must floor at zero,
    // never go negative.
    Wallet.Add(-100);
    TestEqual(TEXT("A negative grant larger than the balance clamps to zero, not negative"), Wallet.Get(), 0);

    // A rolled kill credit is a plain wallet whose one balance is within the
    // authored trash band at ilvl 1 — the exact shape CreditForgeCurrency
    // reads with a single Get().
    const FBreakerCurrencyDropParams Params;
    const int32 TrashYield = UBreakerDropTableLibrary::RollCurrencyDrop(7, 1, EBreakerMonsterRank::Trash, Params).Get();
    TestTrue(TEXT("A trash credit stays within its authored band"),
        TrashYield >= Params.TrashRiftglassMin && TrashYield <= Params.TrashRiftglassMax);
    return true;
}

#endif
