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
// Deliberately mirrors BreakerDropTableTests.cpp's style, macros and naming:
// this is the same three-step pipeline (amount, gate, roll) applied to
// currency instead of items, and it is held to the same standard — the
// gates actually gate, and the documented rate cannot drift from the
// shipped one.

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
    // High enough item level that every gate (Flux at 8, Sigil at 25) is
    // open, so the averages below measure the RANK ordering and not a gate
    // that happens to be closed for one of the ranks under test.
    constexpr int32 ItemLevel = 50;

    const EBreakerMonsterRank Ranks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };

    float AverageSlagByRank[4] = {};
    float AverageFluxByRank[4] = {};
    float AverageSigilByRank[4] = {};

    for (int32 RankIndex = 0; RankIndex < 4; ++RankIndex)
    {
        double TotalSlag = 0.0, TotalFlux = 0.0, TotalSigil = 0.0;
        for (int32 Index = 0; Index < BreakerForgeDropSweepKills; ++Index)
        {
            const int32 Seed = BreakerForgeDropSweepSeed(Index, 0x100 + RankIndex);
            const FBreakerForgeWallet Wallet = UBreakerDropTableLibrary::RollCurrencyDrop(Seed, ItemLevel, Ranks[RankIndex], Params);
            TotalSlag += Wallet.Get(EBreakerForgeCurrency::Slag);
            TotalFlux += Wallet.Get(EBreakerForgeCurrency::Flux);
            TotalSigil += Wallet.Get(EBreakerForgeCurrency::Sigil);
        }
        AverageSlagByRank[RankIndex] = static_cast<float>(TotalSlag / BreakerForgeDropSweepKills);
        AverageFluxByRank[RankIndex] = static_cast<float>(TotalFlux / BreakerForgeDropSweepKills);
        AverageSigilByRank[RankIndex] = static_cast<float>(TotalSigil / BreakerForgeDropSweepKills);
        AddInfo(FString::Printf(TEXT("Rank %d @ ilvl %d: avg Slag %.3f, avg Flux %.3f, avg Sigil %.3f"),
            RankIndex, ItemLevel, AverageSlagByRank[RankIndex], AverageFluxByRank[RankIndex], AverageSigilByRank[RankIndex]));
    }

    // THE RANK ORDER is the design; the values are O2 placeholders. A boss
    // kill must pay at least as much Slag as a modifier-bearer, which must
    // pay at least as much as an elite, which must pay at least as much as
    // trash — the same "difficulty follows loot" rule ChanceByRank pins for
    // items (O27: trash is trivialized, difficulty lives above it).
    TestTrue(TEXT("Boss Slag >= ModifierBearing Slag"), AverageSlagByRank[3] >= AverageSlagByRank[2]);
    TestTrue(TEXT("ModifierBearing Slag >= Elite Slag"), AverageSlagByRank[2] >= AverageSlagByRank[1]);
    TestTrue(TEXT("Elite Slag >= Trash Slag"), AverageSlagByRank[1] >= AverageSlagByRank[0]);
    // Strict at the O2 defaults (distinct ranges per rank), so a retune that
    // collapsed two ranks to the same band would be caught rather than
    // silently passing the >= checks above.
    TestTrue(TEXT("Boss Slag is strictly more than Trash Slag"), AverageSlagByRank[3] > AverageSlagByRank[0] * 5.0f);

    // Flux: gated to zero below ModifierBearing at the O2 defaults, and Boss
    // must pay at least as much as ModifierBearing where both are unlocked.
    TestTrue(TEXT("Trash pays no Flux"), FMath::IsNearlyZero(AverageFluxByRank[0]));
    TestTrue(TEXT("Elite pays no Flux"), FMath::IsNearlyZero(AverageFluxByRank[1]));
    TestTrue(TEXT("ModifierBearing pays some Flux"), AverageFluxByRank[2] > 0.0f);
    TestTrue(TEXT("Boss Flux >= ModifierBearing Flux"), AverageFluxByRank[3] >= AverageFluxByRank[2]);

    // Sigil: reachable from exactly one rank (Boss) at the O2 defaults.
    TestTrue(TEXT("Trash pays no Sigil"), FMath::IsNearlyZero(AverageSigilByRank[0]));
    TestTrue(TEXT("Elite pays no Sigil"), FMath::IsNearlyZero(AverageSigilByRank[1]));
    TestTrue(TEXT("ModifierBearing pays no Sigil"), FMath::IsNearlyZero(AverageSigilByRank[2]));
    TestTrue(TEXT("Boss pays some Sigil"), AverageSigilByRank[3] > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerForgeCurrencyGateTest,
    "RiorsEdge.Items.ForgeDrops.GatesActuallyGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerForgeCurrencyGateTest::RunTest(const FString& Parameters)
{
    const FBreakerCurrencyDropParams Params;

    // THE HARD REQUIREMENT: a low-rank, low-item-level kill cannot produce a
    // gated currency. Checked exhaustively over the item level range rather
    // than sampled — a gate that leaks one in ten thousand is not a gate,
    // exactly the standard RiorsEdge.Items.Drops.TrashCannotRollAberrant
    // holds items to.
    for (int32 ItemLevel = 1; ItemLevel <= 120; ++ItemLevel)
    {
        const bool bTrashFlux = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(ItemLevel, 0x200), ItemLevel, EBreakerMonsterRank::Trash, Params).Get(EBreakerForgeCurrency::Flux) > 0;
        const bool bTrashSigil = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(ItemLevel, 0x201), ItemLevel, EBreakerMonsterRank::Trash, Params).Get(EBreakerForgeCurrency::Sigil) > 0;
        const bool bEliteFlux = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(ItemLevel, 0x202), ItemLevel, EBreakerMonsterRank::Elite, Params).Get(EBreakerForgeCurrency::Flux) > 0;
        const bool bEliteSigil = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(ItemLevel, 0x203), ItemLevel, EBreakerMonsterRank::Elite, Params).Get(EBreakerForgeCurrency::Sigil) > 0;
        const bool bModifierBearingSigil = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(ItemLevel, 0x204), ItemLevel, EBreakerMonsterRank::ModifierBearing, Params).Get(EBreakerForgeCurrency::Sigil) > 0;

        if (bTrashFlux) AddError(FString::Printf(TEXT("A Trash kill paid Flux at ilvl %d"), ItemLevel));
        if (bTrashSigil) AddError(FString::Printf(TEXT("A Trash kill paid Sigil at ilvl %d"), ItemLevel));
        if (bEliteFlux) AddError(FString::Printf(TEXT("An Elite kill paid Flux at ilvl %d"), ItemLevel));
        if (bEliteSigil) AddError(FString::Printf(TEXT("An Elite kill paid Sigil at ilvl %d"), ItemLevel));
        if (bModifierBearingSigil) AddError(FString::Printf(TEXT("A ModifierBearing kill paid Sigil at ilvl %d"), ItemLevel));
    }

    // Below the item-level half of the gate, on a rank that is otherwise
    // allowed, the currency must still be withheld.
    for (int32 ItemLevel = 1; ItemLevel < Params.FluxMinimumItemLevel; ++ItemLevel)
    {
        const bool bFlux = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(ItemLevel, 0x210), ItemLevel, EBreakerMonsterRank::Boss, Params).Get(EBreakerForgeCurrency::Flux) > 0;
        if (bFlux) AddError(FString::Printf(TEXT("A Boss kill paid Flux below FluxMinimumItemLevel (ilvl %d)"), ItemLevel));
    }
    for (int32 ItemLevel = 1; ItemLevel < Params.SigilMinimumItemLevel; ++ItemLevel)
    {
        const bool bSigil = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(ItemLevel, 0x211), ItemLevel, EBreakerMonsterRank::Boss, Params).Get(EBreakerForgeCurrency::Sigil) > 0;
        if (bSigil) AddError(FString::Printf(TEXT("A Boss kill paid Sigil below SigilMinimumItemLevel (ilvl %d)"), ItemLevel));
    }

    // A gate that closed everything would be a different bug: on-level kills
    // of the unlocking rank CAN produce the currency.
    bool bFoundFlux = false;
    bool bFoundSigil = false;
    for (int32 Index = 0; Index < BreakerForgeDropSweepKills && (!bFoundFlux || !bFoundSigil); ++Index)
    {
        const FBreakerForgeWallet ModifierBearingWallet = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(Index, 0x220), Params.FluxMinimumItemLevel, EBreakerMonsterRank::ModifierBearing, Params);
        if (ModifierBearingWallet.Get(EBreakerForgeCurrency::Flux) > 0) bFoundFlux = true;

        const FBreakerForgeWallet BossWallet = UBreakerDropTableLibrary::RollCurrencyDrop(
            BreakerForgeDropSweepSeed(Index, 0x221), Params.SigilMinimumItemLevel, EBreakerMonsterRank::Boss, Params);
        if (BossWallet.Get(EBreakerForgeCurrency::Sigil) > 0) bFoundSigil = true;
    }
    TestTrue(TEXT("An on-level ModifierBearing kill can produce Flux"), bFoundFlux);
    TestTrue(TEXT("An on-level Boss kill can produce Sigil"), bFoundSigil);

    // Slag is ungated by design — every rank must be able to pay it, or the
    // "resources drop from mobs at a reasonable rate" finding would still be
    // unaddressed for whichever rank was accidentally excluded.
    for (const EBreakerMonsterRank Rank : { EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss })
    {
        bool bFoundSlag = false;
        for (int32 Index = 0; Index < 200 && !bFoundSlag; ++Index)
        {
            bFoundSlag = UBreakerDropTableLibrary::RollCurrencyDrop(
                BreakerForgeDropSweepSeed(Index, 0x230 + static_cast<int32>(Rank)), 1, Rank, Params).Get(EBreakerForgeCurrency::Slag) > 0;
        }
        TestTrue(FString::Printf(TEXT("Rank %d can pay Slag"), static_cast<int32>(Rank)), bFoundSlag);
    }
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

    // Same documented stops as LootPerHour: 5 is below every gate, 10 opens
    // Flux (gate at 8), 25 opens Flux and Sigil (Sigil gate is also 25), 50 is
    // the headline endgame figure.
    const int32 ItemLevels[] = { 5, 10, 25, 50 };
    for (const int32 ItemLevel : ItemLevels)
    {
        const FBreakerCurrencyRateProjection Projection = UBreakerDropTableLibrary::ProjectCurrencyRate(Kills, ItemLevel, Params);

        constexpr int32 SimulatedHours = 200;
        double Slag = 0.0, Flux = 0.0, Sigil = 0.0;
        for (const FSweepRow& Row : Rows)
        {
            const int32 KillCount = FMath::RoundToInt(Row.KillsPerHour) * SimulatedHours;
            for (int32 Index = 0; Index < KillCount; ++Index)
            {
                const int32 Seed = BreakerForgeDropSweepSeed(Index, ItemLevel * 977 + static_cast<int32>(Row.Rank) + 0x300);
                const FBreakerForgeWallet Wallet = UBreakerDropTableLibrary::RollCurrencyDrop(Seed, ItemLevel, Row.Rank, Params);
                Slag += Wallet.Get(EBreakerForgeCurrency::Slag);
                Flux += Wallet.Get(EBreakerForgeCurrency::Flux);
                Sigil += Wallet.Get(EBreakerForgeCurrency::Sigil);
            }
        }
        const float MeasuredSlag = static_cast<float>(Slag / SimulatedHours);
        const float MeasuredFlux = static_cast<float>(Flux / SimulatedHours);
        const float MeasuredSigil = static_cast<float>(Sigil / SimulatedHours);

        AddInfo(FString::Printf(
            TEXT("ilvl %d | Slag/h %.1f (proj %.1f) | Flux/h %.2f (proj %.2f) | Sigil/h %.3f (proj %.3f)"),
            ItemLevel, MeasuredSlag, Projection.SlagPerHour, MeasuredFlux, Projection.FluxPerHour, MeasuredSigil, Projection.SigilPerHour));

        // Slag reaches thousands/hour at ilvl 50, so the tolerance is
        // relative rather than the ~2-item absolute band LootPerHour uses.
        TestTrue(FString::Printf(TEXT("Slag per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(MeasuredSlag - Projection.SlagPerHour) < FMath::Max(5.0f, Projection.SlagPerHour * 0.05f));
        TestTrue(FString::Printf(TEXT("Flux per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(MeasuredFlux - Projection.FluxPerHour) < 3.0f);
        TestTrue(FString::Printf(TEXT("Sigil per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(MeasuredSigil - Projection.SigilPerHour) < 1.0f);
    }

    // THE DOCUMENTED FIGURES the report states, so a retune that moves them
    // fails this loudly instead of silently. O2 PLACEHOLDER bands, wide
    // because the exact values are the owner's to move — the SHAPE (visible
    // within minutes, Sigil scarce) is what is being asserted.
    const FBreakerCurrencyRateProjection AtFifty = UBreakerDropTableLibrary::ProjectCurrencyRate(Kills, 50, Params);
    TestTrue(TEXT("Slag per hour at ilvl 50 is in the low thousands, not near zero and not tens of thousands"),
        AtFifty.SlagPerHour > 1500.0f && AtFifty.SlagPerHour < 5000.0f);
    TestTrue(TEXT("Flux per hour at ilvl 50 is tens per hour"),
        AtFifty.FluxPerHour > 20.0f && AtFifty.FluxPerHour < 100.0f);
    TestTrue(TEXT("Sigil per hour at ilvl 50 is a handful, not a stream"),
        AtFifty.SigilPerHour > 0.5f && AtFifty.SigilPerHour < 10.0f);

    // Gated content answers "zero", not "a trickle".
    const FBreakerCurrencyRateProjection AtFive = UBreakerDropTableLibrary::ProjectCurrencyRate(Kills, 5, Params);
    TestTrue(TEXT("No Flux at ilvl 5 (below FluxMinimumItemLevel)"), AtFive.FluxPerHour <= 0.0f);
    TestTrue(TEXT("No Sigil at ilvl 5 (below SigilMinimumItemLevel)"), AtFive.SigilPerHour <= 0.0f);

    // PERCEPTIBLE IN THE GYM (O2's second half): a few minutes of on-level
    // trash-and-elite fighting must visibly move the wallet. Trash and Elite
    // alone (no ModifierBearing/Boss needed) at ilvl 1 must clear a modest
    // Slag total within five minutes, which is the shortest plausible gym
    // session.
    const FBreakerCurrencyRateProjection AtOne = UBreakerDropTableLibrary::ProjectCurrencyRate(Kills, 1, Params);
    const float FiveMinuteSlag = AtOne.SlagPerHour / 12.0f;
    TestTrue(TEXT("Five minutes of on-level killing visibly moves the Slag wallet (>= 20)"), FiveMinuteSlag >= 20.0f);
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
    TestEqual(TEXT("A fresh wallet starts at zero"), Wallet.Get(EBreakerForgeCurrency::Slag), 0);

    Wallet.Add(EBreakerForgeCurrency::Slag, 12);
    Wallet.Add(EBreakerForgeCurrency::Slag, 8);
    TestEqual(TEXT("Repeated Slag grants are additive"), Wallet.Get(EBreakerForgeCurrency::Slag), 20);

    // A second currency stream must not disturb the first — the whole point
    // of RollCurrencyDrop returning a wallet rather than a single amount.
    Wallet.Add(EBreakerForgeCurrency::Flux, 3);
    TestEqual(TEXT("Slag is untouched by a Flux grant"), Wallet.Get(EBreakerForgeCurrency::Slag), 20);
    TestEqual(TEXT("Flux recorded its own grant"), Wallet.Get(EBreakerForgeCurrency::Flux), 3);

    // The clamp: a negative Add larger than the balance must floor at zero,
    // never go negative.
    Wallet.Add(EBreakerForgeCurrency::Sigil, 2);
    Wallet.Add(EBreakerForgeCurrency::Sigil, -10);
    TestEqual(TEXT("A negative grant larger than the balance clamps to zero, not negative"),
        Wallet.Get(EBreakerForgeCurrency::Sigil), 0);

    // A RollCurrencyDrop wallet at a rank/item level that gates out Flux and
    // Sigil must credit ONLY Slag, and crediting it must be additive against
    // whatever currency the wallet already held — the exact shape
    // UBreakerEquipmentComponent::CreditForgeCurrency assumes when it loops
    // FBreakerForgeWallet::CurrencyCount and calls Add per nonzero entry.
    const FBreakerCurrencyDropParams Params;
    const FBreakerForgeWallet TrashYield = UBreakerDropTableLibrary::RollCurrencyDrop(7, 1, EBreakerMonsterRank::Trash, Params);
    TestEqual(TEXT("A gated-out currency reads exactly zero, not merely unset"),
        TrashYield.Get(EBreakerForgeCurrency::Flux), 0);
    TestEqual(TEXT("A gated-out currency reads exactly zero, not merely unset"),
        TrashYield.Get(EBreakerForgeCurrency::Sigil), 0);
    return true;
}

#endif
