#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerDropTable.h"
#include "Items/BreakerLootLibrary.h"

// Coverage for the drop pipeline (Items/BreakerDropTable.h), which exists
// because of one owner playtest: "not every single enemy needs to drop an item
// ... I was getting way way too many Aberrants at this item level when it
// shouldn't even be fundamentally possible, and every single enemy dropped an
// item."
//
// The tests below are deliberately split along the two halves of that report
// plus the two properties the rewrite must not break.

namespace
{
    // A wide sweep. Drop rates are single-digit percentages at the trash end,
    // so a small sample cannot distinguish 10% from 12% and would either be
    // useless or flaky; 40k kills puts the standard error of a 10% rate at
    // about 0.15 percentage points.
    constexpr int32 BreakerDropSweepKills = 40000;

    int32 BreakerDropSweepSeed(int32 Index, int32 Salt)
    {
        return HashCombine(Index * 2654435761u, Salt);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDropChanceByRankTest,
    "RiorsEdge.Items.Drops.ChanceByRank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDropChanceByRankTest::RunTest(const FString& Parameters)
{
    const FBreakerDropTableParams Params;

    // The ORDERING is the design; the values are O2 placeholders. O27 says
    // trash exists to be trivialized and difficulty lives in elites, modifier
    // bearers and bosses, so the loot has to follow the difficulty. A retune
    // that inverted this ordering would be a design reversal, not a tweak.
    TestTrue(TEXT("Trash drops less often than elite"),
        Params.TrashDropChance < Params.EliteDropChance);
    TestTrue(TEXT("Elite drops less often than a modifier bearer"),
        Params.EliteDropChance < Params.ModifierBearingDropChance);
    TestTrue(TEXT("A boss is guaranteed"),
        FMath::IsNearlyEqual(Params.BossDropChance, 1.0f));
    TestTrue(TEXT("Trash is occasional, not routine (the owner's complaint)"),
        Params.TrashDropChance < 0.25f);
    TestTrue(TEXT("Elite is reliable"), Params.EliteDropChance >= 0.5f);

    // And the roll actually honours them, measured rather than asserted from
    // the parameter: a chance step that read the wrong field would still pass
    // every check above.
    const EBreakerMonsterRank Ranks[] = {
        EBreakerMonsterRank::Trash, EBreakerMonsterRank::Elite,
        EBreakerMonsterRank::ModifierBearing, EBreakerMonsterRank::Boss };
    for (const EBreakerMonsterRank Rank : Ranks)
    {
        int32 Drops = 0;
        for (int32 Index = 0; Index < BreakerDropSweepKills; ++Index)
        {
            if (UBreakerDropTableLibrary::RollsDrop(BreakerDropSweepSeed(Index, 0x11), Rank, 0.0f, Params)) ++Drops;
        }
        const float Measured = static_cast<float>(Drops) / BreakerDropSweepKills;
        const float Expected = UBreakerDropTableLibrary::GetRankDropChance(Rank, Params);
        AddInfo(FString::Printf(TEXT("Rank %d: measured %.4f vs authored %.4f"),
            static_cast<int32>(Rank), Measured, Expected));
        TestTrue(FString::Printf(TEXT("Measured drop chance matches the authored rank chance (rank %d)"),
            static_cast<int32>(Rank)), FMath::Abs(Measured - Expected) < 0.01f);
    }

    // The Drop Chance affix bids on QUANTITY as well as quality now. It must
    // raise the chance and must never push a guaranteed drop past 1.0, because
    // drops-per-kill above one is a separate unruled design.
    TestTrue(TEXT("Drop Chance raises the trash drop rate"),
        UBreakerDropTableLibrary::GetEffectiveDropChance(EBreakerMonsterRank::Trash, 100.0f, Params)
        > UBreakerDropTableLibrary::GetEffectiveDropChance(EBreakerMonsterRank::Trash, 0.0f, Params));
    TestTrue(TEXT("A guaranteed drop stays exactly one drop"),
        FMath::IsNearlyEqual(UBreakerDropTableLibrary::GetEffectiveDropChance(EBreakerMonsterRank::Boss, 100.0f, Params), 1.0f));

    // Setting every rank chance to 1.0 must recover the old "every enemy
    // drops" behaviour exactly, so the owner can switch the feature off.
    FBreakerDropTableParams Always;
    Always.TrashDropChance = 1.0f;
    for (int32 Index = 0; Index < 500; ++Index)
    {
        if (!UBreakerDropTableLibrary::RollsDrop(Index, EBreakerMonsterRank::Trash, 0.0f, Always))
        {
            AddError(TEXT("A 1.0 rank chance must always drop"));
            break;
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDropRarityGateTest,
    "RiorsEdge.Items.Drops.TrashCannotRollAberrant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDropRarityGateTest::RunTest(const FString& Parameters)
{
    const FBreakerDropTableParams Params;

    // THE OWNER'S SPECIFIC COMPLAINT: Aberrants "at this item level when it
    // shouldn't even be fundamentally possible". Fundamentally impossible is
    // the bar, so this is checked exhaustively over the whole item level range
    // rather than sampled — a gate that leaks one in ten thousand is not a gate.
    for (int32 ItemLevel = 1; ItemLevel <= 50; ++ItemLevel)
    {
        TestFalse(FString::Printf(TEXT("Trash can never unlock Aberrant (ilvl %d)"), ItemLevel),
            UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Aberrant, ItemLevel, EBreakerMonsterRank::Trash, Params));
        TestFalse(FString::Printf(TEXT("Trash can never unlock Anomalous (ilvl %d)"), ItemLevel),
            UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Anomalous, ItemLevel, EBreakerMonsterRank::Trash, Params));
    }

    // And through the ROLL, not only the predicate, because the predicate could
    // be right while the weight table ignored it.
    for (int32 Index = 0; Index < BreakerDropSweepKills; ++Index)
    {
        const EBreakerItemRarity Rarity = UBreakerDropTableLibrary::RollGatedRarity(
            BreakerDropSweepSeed(Index, 0x22), 50, EBreakerMonsterRank::Trash, 100.0f, Params);
        if (Rarity >= EBreakerItemRarity::Aberrant)
        {
            AddError(FString::Printf(TEXT("A level-50 trash kill rolled rarity %d with max Drop Chance"), static_cast<int32>(Rarity)));
            break;
        }
    }

    // The ITEM LEVEL half of the gate, on a rank that is otherwise allowed.
    for (int32 ItemLevel = 1; ItemLevel < Params.AberrantMinimumItemLevel; ++ItemLevel)
    {
        TestFalse(FString::Printf(TEXT("Aberrant is gated below its unlock level (ilvl %d)"), ItemLevel),
            UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Aberrant, ItemLevel, EBreakerMonsterRank::Boss, Params));
    }
    TestTrue(TEXT("Aberrant unlocks at its authored level on an elite"),
        UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Aberrant, Params.AberrantMinimumItemLevel, EBreakerMonsterRank::Elite, Params));

    // A gate that closed everything would be a different bug. Standard and
    // Uncommon are ungated on purpose so a passed drop always has a table.
    TestTrue(TEXT("Standard is always available"),
        UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Standard, 1, EBreakerMonsterRank::Trash, Params));
    TestTrue(TEXT("Uncommon is always available"),
        UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Uncommon, 1, EBreakerMonsterRank::Trash, Params));

    // Trash must still be worth picking up, or a 10% drop rate is 10% of
    // nothing and the player stops looking at the floor.
    TestTrue(TEXT("Exceptional is reachable from trash at a high item level"),
        UBreakerDropTableLibrary::IsRarityUnlocked(EBreakerItemRarity::Exceptional, 50, EBreakerMonsterRank::Trash, Params));

    // An elite CAN produce one, or the gate would have deleted the rarity
    // rather than gated it.
    bool bFoundAberrant = false;
    for (int32 Index = 0; Index < BreakerDropSweepKills && !bFoundAberrant; ++Index)
    {
        bFoundAberrant = UBreakerDropTableLibrary::RollGatedRarity(
            BreakerDropSweepSeed(Index, 0x33), 50, EBreakerMonsterRank::Elite, 0.0f, Params) == EBreakerItemRarity::Aberrant;
    }
    TestTrue(TEXT("An on-level elite can produce an Aberrant"), bFoundAberrant);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDropDeterminismTest,
    "RiorsEdge.Items.Drops.Determinism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDropDeterminismTest::RunTest(const FString& Parameters)
{
    const FBreakerDropTableParams Params;

    // LOCKED: the roll pipeline is deterministic from a seed and a seed must
    // reproduce an item exactly. The new chance and gate steps sit in front of
    // it, so they inherit the obligation.
    for (int32 Seed = 0; Seed < 400; ++Seed)
    {
        EBreakerItemRarity FirstRarity = EBreakerItemRarity::Standard;
        EBreakerItemRarity SecondRarity = EBreakerItemRarity::Standard;
        const bool bFirst = UBreakerDropTableLibrary::RollDrop(Seed, 50, EBreakerMonsterRank::Elite, 12.0f, Params, FirstRarity);
        const bool bSecond = UBreakerDropTableLibrary::RollDrop(Seed, 50, EBreakerMonsterRank::Elite, 12.0f, Params, SecondRarity);
        if (bFirst != bSecond || FirstRarity != SecondRarity)
        {
            AddError(FString::Printf(TEXT("RollDrop is not deterministic at seed %d"), Seed));
            break;
        }
        if (!bFirst) continue;

        // All the way through to the item, which is the property the loot tests
        // already pin and the one a caller actually depends on.
        const FBreakerItemInstance A = UBreakerLootLibrary::RollItem(TEXT("Det"), EBreakerEquipSlot::Helmet, FirstRarity, 50, Seed);
        const FBreakerItemInstance B = UBreakerLootLibrary::RollItem(TEXT("Det"), EBreakerEquipSlot::Helmet, FirstRarity, 50, Seed);
        if (A.Affixes.Num() != B.Affixes.Num())
        {
            AddError(FString::Printf(TEXT("Item roll is not deterministic at seed %d"), Seed));
            break;
        }
        for (int32 Index = 0; Index < A.Affixes.Num(); ++Index)
        {
            if (A.Affixes[Index].AffixId != B.Affixes[Index].AffixId
                || A.Affixes[Index].Tier != B.Affixes[Index].Tier
                || !FMath::IsNearlyEqual(A.Affixes[Index].Value, B.Affixes[Index].Value))
            {
                AddError(FString::Printf(TEXT("Affix %d diverged at seed %d"), Index, Seed));
            }
        }
    }

    // The chance step and the rarity step must not share a stream. If they did,
    // "dropped at all" and "dropped well" would be the same coin and the rare
    // tiers would cluster in the same seeds. Measured as: the rarity mix of the
    // kills that DROPPED matches the unconditional mix.
    int32 DroppedHigh = 0;
    int32 Dropped = 0;
    int32 AllHigh = 0;
    for (int32 Index = 0; Index < BreakerDropSweepKills; ++Index)
    {
        const int32 Seed = BreakerDropSweepSeed(Index, 0x44);
        EBreakerItemRarity Rarity = EBreakerItemRarity::Standard;
        if (UBreakerDropTableLibrary::RollDrop(Seed, 50, EBreakerMonsterRank::Trash, 0.0f, Params, Rarity))
        {
            ++Dropped;
            if (Rarity >= EBreakerItemRarity::Exceptional) ++DroppedHigh;
        }
        if (UBreakerDropTableLibrary::RollGatedRarity(Seed, 50, EBreakerMonsterRank::Trash, 0.0f, Params) >= EBreakerItemRarity::Exceptional) ++AllHigh;
    }
    const float DroppedShare = Dropped > 0 ? static_cast<float>(DroppedHigh) / Dropped : 0.0f;
    const float AllShare = static_cast<float>(AllHigh) / BreakerDropSweepKills;
    AddInfo(FString::Printf(TEXT("Exceptional share among drops %.4f vs unconditional %.4f"), DroppedShare, AllShare));
    TestTrue(TEXT("The chance step does not bias the rarity step"), FMath::Abs(DroppedShare - AllShare) < 0.02f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDropLootPerHourTest,
    "RiorsEdge.Items.Drops.LootPerHour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDropLootPerHourTest::RunTest(const FString& Parameters)
{
    // THE NUMBER THE OWNER WANTS TO TUNE AGAINST, and nothing in the project
    // could answer it before this pass. `ProjectLootRate` computes it
    // analytically; this test proves the analysis by SIMULATING the same hour
    // through the real RollDrop, so the table in Docs/Item-Foundation.md cannot
    // drift away from what the pipeline actually does.
    const FBreakerDropTableParams Params;
    const FBreakerKillRateSample Kills;

    struct FSweepRow { EBreakerMonsterRank Rank; float KillsPerHour; };
    const FSweepRow Rows[] = {
        { EBreakerMonsterRank::Trash,           Kills.TrashKillsPerHour },
        { EBreakerMonsterRank::Elite,           Kills.EliteKillsPerHour },
        { EBreakerMonsterRank::ModifierBearing, Kills.ModifierBearingKillsPerHour },
        { EBreakerMonsterRank::Boss,            Kills.BossKillsPerHour } };

    // Documented stops. Area level 5 is below every high gate, 10 opens
    // Exceptional, 25 opens Aberrant, 50 opens Anomalous.
    const int32 ItemLevels[] = { 5, 10, 25, 50 };
    for (const int32 ItemLevel : ItemLevels)
    {
        const FBreakerLootRateProjection Projection = UBreakerDropTableLibrary::ProjectLootRate(Kills, ItemLevel, 0.0f, Params);

        // Simulate 200 hours so even the Anomalous rate (fractions of one an
        // hour) has enough events to compare against.
        constexpr int32 SimulatedHours = 200;
        double Items = 0.0, ExceptionalOrBetter = 0.0, Aberrant = 0.0, Anomalous = 0.0;
        for (const FSweepRow& Row : Rows)
        {
            const int32 KillCount = FMath::RoundToInt(Row.KillsPerHour) * SimulatedHours;
            for (int32 Index = 0; Index < KillCount; ++Index)
            {
                const int32 Seed = BreakerDropSweepSeed(Index, ItemLevel * 977 + static_cast<int32>(Row.Rank));
                EBreakerItemRarity Rarity = EBreakerItemRarity::Standard;
                if (!UBreakerDropTableLibrary::RollDrop(Seed, ItemLevel, Row.Rank, 0.0f, Params, Rarity)) continue;
                Items += 1.0;
                if (Rarity >= EBreakerItemRarity::Exceptional) ExceptionalOrBetter += 1.0;
                if (Rarity == EBreakerItemRarity::Aberrant) Aberrant += 1.0;
                if (Rarity == EBreakerItemRarity::Anomalous) Anomalous += 1.0;
            }
        }
        const float MeasuredItems = static_cast<float>(Items / SimulatedHours);
        const float MeasuredExceptional = static_cast<float>(ExceptionalOrBetter / SimulatedHours);
        const float MeasuredAberrant = static_cast<float>(Aberrant / SimulatedHours);
        const float MeasuredAnomalous = static_cast<float>(Anomalous / SimulatedHours);

        AddInfo(FString::Printf(
            TEXT("ilvl %d | items/h %.1f (proj %.1f) | Exc+/h %.2f (proj %.2f) | Aberrant/h %.3f (proj %.3f) | Anomalous/h %.4f (proj %.4f)"),
            ItemLevel, MeasuredItems, Projection.ItemsPerHour, MeasuredExceptional, Projection.ExceptionalOrBetterPerHour,
            MeasuredAberrant, Projection.AberrantPerHour, MeasuredAnomalous, Projection.AnomalousPerHour));

        TestTrue(FString::Printf(TEXT("Items per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(MeasuredItems - Projection.ItemsPerHour) < 2.0f);
        TestTrue(FString::Printf(TEXT("Exceptional-or-better per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(MeasuredExceptional - Projection.ExceptionalOrBetterPerHour) < 1.0f);
        TestTrue(FString::Printf(TEXT("Aberrant per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(MeasuredAberrant - Projection.AberrantPerHour) < 0.15f);
        TestTrue(FString::Printf(TEXT("Anomalous per hour matches the projection (ilvl %d)"), ItemLevel),
            FMath::Abs(MeasuredAnomalous - Projection.AnomalousPerHour) < 0.08f);
    }

    // The DOCUMENTED figures. These are the lines in Docs/Item-Foundation.md;
    // if a retune moves them the doc is now wrong and this fails, which is the
    // point. Bounds are wide bands rather than equalities, because the values
    // are O2 PLACEHOLDER and the SHAPE is what is being asserted.
    const FBreakerLootRateProjection AtFifty = UBreakerDropTableLibrary::ProjectLootRate(Kills, 50, 0.0f, Params);
    TestTrue(TEXT("A documented hour is roughly 130 items, not roughly 690"),
        AtFifty.ItemsPerHour > 110.0f && AtFifty.ItemsPerHour < 160.0f);
    TestTrue(TEXT("Exceptional or better is a handful an hour, not a stream"),
        AtFifty.ExceptionalOrBetterPerHour > 10.0f && AtFifty.ExceptionalOrBetterPerHour < 20.0f);

    // O11 caps Aberrant at THREE EQUIPPED, globally. A rarity capped at 3 has
    // to be rare enough that 3 is a goal: at the old flat 2.5% on every kill
    // the cap filled in about ten minutes and was thereafter only displaced.
    // Roughly an hour each is the shape this asserts; the exact number is the
    // owner's to move.
    TestTrue(TEXT("An Aberrant is about an hour of on-level elites, so O11's cap of 3 is a goal"),
        AtFifty.HoursPerAberrant > 0.5f && AtFifty.HoursPerAberrant < 3.0f);

    // Gated content answers "never", not "eventually".
    const FBreakerLootRateProjection AtTen = UBreakerDropTableLibrary::ProjectLootRate(Kills, 10, 0.0f, Params);
    TestTrue(TEXT("No Aberrant exists at all in a level-10 area"), AtTen.AberrantPerHour <= 0.0f);
    TestTrue(TEXT("Hours per Aberrant reports never rather than eventually"), AtTen.HoursPerAberrant > 1000.0f);

    // Drop Chance must be worth rolling on both axes.
    const FBreakerLootRateProjection Juiced = UBreakerDropTableLibrary::ProjectLootRate(Kills, 50, 100.0f, Params);
    TestTrue(TEXT("Drop Chance raises items per hour"), Juiced.ItemsPerHour > AtFifty.ItemsPerHour);
    TestTrue(TEXT("Drop Chance raises Exceptional-or-better per hour"),
        Juiced.ExceptionalOrBetterPerHour > AtFifty.ExceptionalOrBetterPerHour);
    return true;
}

#endif
