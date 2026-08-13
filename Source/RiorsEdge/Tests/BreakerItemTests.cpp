#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Attributes/BreakerAttributeSet.h"
#include "Combat/BreakerDamageLibrary.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixTierCurveTest,
    "RiorsEdge.Items.Affixes.TierCurve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixTierCurveTest::RunTest(const FString& Parameters)
{
    FBreakerAffixDefinition Affix;
    Affix.ValueAtT8 = 25.0f;
    Affix.ValueAtT1 = 180.0f;

    TestEqual(TEXT("T8 returns the floor value"), UBreakerAffixLibrary::ValueForTier(Affix, 8), 25.0f);
    TestEqual(TEXT("T1 returns the ceiling value"), UBreakerAffixLibrary::ValueForTier(Affix, 1), 180.0f);
    TestEqual(TEXT("T0 spikes to 1.4x T1"), UBreakerAffixLibrary::ValueForTier(Affix, 0), 252.0f);
    TestEqual(TEXT("T-1 spikes to 1.8x T1"), UBreakerAffixLibrary::ValueForTier(Affix, -1), 324.0f);
    const float T4 = UBreakerAffixLibrary::ValueForTier(Affix, 4);
    TestTrue(TEXT("Middle tiers interpolate linearly"), T4 > 25.0f && T4 < 180.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerItemLevelGatingTest,
    "RiorsEdge.Items.Affixes.ItemLevelGating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerItemLevelGatingTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Level 1 rolls only T8"), UBreakerAffixLibrary::BestTierForItemLevel(1), 8);
    TestEqual(TEXT("Level 50 opens T1"), UBreakerAffixLibrary::BestTierForItemLevel(50), 1);
    TestTrue(TEXT("Level 25 sits between"), UBreakerAffixLibrary::BestTierForItemLevel(25) < 8 && UBreakerAffixLibrary::BestTierForItemLevel(25) > 1);
    TestEqual(TEXT("Standard rarity caps at T3"), UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity::Standard), 3);
    TestEqual(TEXT("Exceptional rarity ceiling is T-1"), UBreakerAffixLibrary::TierCapForRarity(EBreakerItemRarity::Exceptional), -1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootRollTest,
    "RiorsEdge.Items.Loot.RollPipeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootRollTest::RunTest(const FString& Parameters)
{
    for (int32 Seed = 0; Seed < 200; ++Seed)
    {
        const FBreakerItemInstance Item = UBreakerLootLibrary::RollItem(TEXT("Test"), EBreakerEquipSlot::Boots, EBreakerItemRarity::Exceptional, 30, Seed);
        TestTrue(TEXT("Item is valid"), Item.IsValid());
        TestTrue(TEXT("Affix count within Exceptional range"), Item.Affixes.Num() >= 3 && Item.Affixes.Num() <= 5);
        TestTrue(TEXT("Prefix cap holds"), UBreakerLootLibrary::CountAffixesOfCategory(Item, EBreakerAffixCategory::Prefix) <= 4);
        TestTrue(TEXT("Suffix cap holds"), UBreakerLootLibrary::CountAffixesOfCategory(Item, EBreakerAffixCategory::Suffix) <= 4);

        const int32 BestTier = UBreakerAffixLibrary::BestTierForItemLevel(30);
        TSet<FName> Seen;
        for (const FBreakerRolledAffix& Affix : Item.Affixes)
        {
            TestFalse(TEXT("No duplicate affixes"), Seen.Contains(Affix.AffixId));
            Seen.Add(Affix.AffixId);
            TestTrue(TEXT("Tier respects item level gate"), Affix.Tier >= BestTier);
            const FBreakerAffixDefinition* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Affix.AffixId);
            TestNotNull(TEXT("Rolled affix exists in pool"), Definition);
            if (Definition) TestTrue(TEXT("Affix legal for the slot"), Definition->AllowsSlot(EBreakerEquipSlot::Boots));
        }
    }

    // Weapon Damage is a weapon/hands/neck affix: it must never appear on
    // Boots, and must be reachable on an allowed slot.
    const FBreakerAffixDefinition* WeaponDamage = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), TEXT("Offense.WeaponDamage"));
    TestNotNull(TEXT("Weapon Damage affix exists in the slice pool"), WeaponDamage);
    if (WeaponDamage)
    {
        TestFalse(TEXT("Weapon Damage is illegal on Boots"), WeaponDamage->AllowsSlot(EBreakerEquipSlot::Boots));
        TestTrue(TEXT("Weapon Damage is legal on Primary"), WeaponDamage->AllowsSlot(EBreakerEquipSlot::Primary));
        TestTrue(TEXT("Weapon Damage is legal on Gloves"), WeaponDamage->AllowsSlot(EBreakerEquipSlot::Gloves));

        bool bRolledOnPrimary = false;
        for (int32 Seed = 0; Seed < 200; ++Seed)
        {
            const FBreakerItemInstance Weapon = UBreakerLootLibrary::RollItem(TEXT("Test"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Exceptional, 30, Seed);
            for (const FBreakerRolledAffix& Affix : Weapon.Affixes)
            {
                if (Affix.AffixId == WeaponDamage->AffixId) bRolledOnPrimary = true;
            }
        }
        TestTrue(TEXT("Weapon Damage can roll on an allowed slot"), bRolledOnPrimary);
    }

    // Standard items must stay fodder: 1-2 affixes, tiers no better than T3.
    const FBreakerItemInstance White = UBreakerLootLibrary::RollItem(TEXT("Test"), EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 50, 7);
    TestTrue(TEXT("Standard affix count"), White.Affixes.Num() >= 1 && White.Affixes.Num() <= 2);
    for (const FBreakerRolledAffix& Affix : White.Affixes)
    {
        TestTrue(TEXT("Standard tier cap"), Affix.Tier >= 3);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipmentAggregationTest,
    "RiorsEdge.Items.Equipment.StatAggregation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipmentAggregationTest::RunTest(const FString& Parameters)
{
    auto MakeRolled = [](FName AffixId, float Value, EBreakerAffixCategory Category)
    {
        FBreakerRolledAffix Rolled;
        Rolled.AffixId = AffixId;
        Rolled.Tier = 4;
        Rolled.Value = Value;
        Rolled.Category = Category;
        return Rolled;
    };

    FBreakerItemInstance Boots;
    Boots.ItemId = FGuid::NewGuid();
    Boots.Slot = EBreakerEquipSlot::Boots;
    Boots.Affixes.Add(MakeRolled(TEXT("Core.Health"), 100.0f, EBreakerAffixCategory::Suffix));
    Boots.Affixes.Add(MakeRolled(TEXT("Core.MoveSpeed"), 5.0f, EBreakerAffixCategory::Prefix));

    FBreakerItemInstance Helmet;
    Helmet.ItemId = FGuid::NewGuid();
    Helmet.Slot = EBreakerEquipSlot::Helmet;
    Helmet.Affixes.Add(MakeRolled(TEXT("Core.Health"), 50.0f, EBreakerAffixCategory::Suffix));
    Helmet.Affixes.Add(MakeRolled(TEXT("Core.MoveSpeed"), 3.0f, EBreakerAffixCategory::Prefix));
    Helmet.Affixes.Add(MakeRolled(TEXT("Crit.Chance"), 6.0f, EBreakerAffixCategory::Prefix));

    // Two sources of increased Weapon Damage must land in one additive bucket.
    FBreakerItemInstance Gloves;
    Gloves.ItemId = FGuid::NewGuid();
    Gloves.Slot = EBreakerEquipSlot::Gloves;
    Gloves.Affixes.Add(MakeRolled(TEXT("Offense.WeaponDamage"), 12.0f, EBreakerAffixCategory::Prefix));

    FBreakerItemInstance Necklace;
    Necklace.ItemId = FGuid::NewGuid();
    Necklace.Slot = EBreakerEquipSlot::Necklace;
    Necklace.Affixes.Add(MakeRolled(TEXT("Offense.WeaponDamage"), 8.0f, EBreakerAffixCategory::Prefix));

    const FBreakerEquipmentStats Stats = UBreakerEquipmentComponent::AggregateStats({Boots, Helmet, Gloves, Necklace});
    TestEqual(TEXT("Flat health sums"), Stats.BonusHealth, 150.0f);
    TestTrue(TEXT("Increased move speed is one additive bucket"), FMath::IsNearlyEqual(Stats.MoveSpeedMultiplier, 1.08f, 0.0001f));
    TestTrue(TEXT("Crit chance converts percent to fraction"), FMath::IsNearlyEqual(Stats.CriticalChanceBonus, 0.06f, 0.0001f));
    TestTrue(TEXT("Increased weapon damage is one additive bucket"), FMath::IsNearlyEqual(Stats.WeaponDamageMultiplier, 1.20f, 0.0001f));

    // No weapon damage equipped leaves the multiplier neutral.
    const FBreakerEquipmentStats BareStats = UBreakerEquipmentComponent::AggregateStats({Boots});
    TestTrue(TEXT("Weapon damage multiplier defaults to 1"), FMath::IsNearlyEqual(BareStats.WeaponDamageMultiplier, 1.0f, 0.0001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipmentContributionTest,
    "RiorsEdge.Items.Equipment.AttributeContribution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipmentContributionTest::RunTest(const FString& Parameters)
{
    FBreakerItemInstance Gloves;
    Gloves.ItemId = FGuid::NewGuid();
    Gloves.Slot = EBreakerEquipSlot::Gloves;

    FBreakerRolledAffix Health;
    Health.AffixId = TEXT("Core.Health");
    Health.Tier = 4;
    Health.Value = 120.0f;
    Health.Category = EBreakerAffixCategory::Suffix;
    Gloves.Affixes.Add(Health);

    FBreakerRolledAffix Speed;
    Speed.AffixId = TEXT("Core.MoveSpeed");
    Speed.Tier = 4;
    Speed.Value = 6.0f;
    Speed.Category = EBreakerAffixCategory::Prefix;
    Gloves.Affixes.Add(Speed);

    // The contribution carries RAW buckets, not composed multipliers: the
    // Increased percentage has to reach the attribute set unmerged so it can
    // join the tree's percentages in one additive bucket.
    FBreakerAttributeContribution Contribution;
    UBreakerEquipmentComponent::AggregateStats({Gloves}, &Contribution);
    TestEqual(TEXT("Flat health lands in the flat lane"), Contribution.GetFlat(EBreakerAggregatedAttribute::MaxHealth), 120.0f);
    TestEqual(TEXT("Increased move speed stays a raw percentage"), Contribution.GetIncreasedPercent(EBreakerAggregatedAttribute::MoveSpeed), 6.0f);
    TestEqual(TEXT("Gear authors no More multipliers"), Contribution.GetMore(EBreakerAggregatedAttribute::MoveSpeed), 1.0f);

    // End to end through the single application path.
    UBreakerAttributeSet* Attributes = NewObject<UBreakerAttributeSet>();
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    Equipment->BindAttributes(Attributes);
    const float BaseHealth = Attributes->GetMaxHealth();
    const float BaseSpeed = Attributes->GetMoveSpeed();

    TestTrue(TEXT("Equip succeeds"), Equipment->EquipItem(Gloves));
    TestEqual(TEXT("Equipping raises max health"), Attributes->GetMaxHealth(), BaseHealth + 120.0f);
    TestEqual(TEXT("Equipping raises move speed"), Attributes->GetMoveSpeed(), BaseSpeed * 1.06f, 0.001f);

    TestTrue(TEXT("Unequip succeeds"), Equipment->UnequipSlot(EBreakerEquipSlot::Gloves));
    TestEqual(TEXT("Unequipping restores the pre-equip health exactly"), Attributes->GetMaxHealth(), BaseHealth);
    TestEqual(TEXT("Unequipping restores the pre-equip move speed exactly"), Attributes->GetMoveSpeed(), BaseSpeed);
    TestEqual(TEXT("The base value is never overwritten"), Attributes->GetAttributeBase(EBreakerAggregatedAttribute::MaxHealth), BaseHealth);
    return true;
}

namespace BreakerEquipmentTestHelpers
{
    // A minimal item that only needs to be identifiable and rarity-tagged.
    FBreakerItemInstance MakeItem(EBreakerEquipSlot Slot, EBreakerItemRarity Rarity, int32 ItemLevel)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.Slot = Slot;
        Item.Rarity = Rarity;
        Item.ItemLevel = ItemLevel;
        return Item;
    }

    FBreakerRolledAffix MakeAffix(FName AffixId, float Value)
    {
        FBreakerRolledAffix Rolled;
        Rolled.AffixId = AffixId;
        Rolled.Tier = 4;
        Rolled.Value = Value;
        return Rolled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipLimitCountTest,
    "RiorsEdge.Items.Equipment.RarityLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipLimitCountTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    // O11 / master sheet 4.1. Everything below Aberrant is uncapped.
    TestEqual(TEXT("Aberrant caps at three"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Aberrant), 3);
    TestEqual(TEXT("Anomalous caps at one"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Anomalous), 1);
    TestEqual(TEXT("Exceptional is uncapped"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Exceptional), INDEX_NONE);
    TestEqual(TEXT("Standard is uncapped"), UBreakerEquipmentComponent::EquipLimitForRarity(EBreakerItemRarity::Standard), INDEX_NONE);

    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 30));
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 30));
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Exceptional, 30));

    TestEqual(TEXT("Aberrant count reads the equipped list"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 2);
    TestEqual(TEXT("Exceptional count reads the equipped list"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Exceptional), 1);
    TestEqual(TEXT("Anomalous count is zero"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Anomalous), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerEquipDisplacementTest,
    "RiorsEdge.Items.Equipment.LimitDisplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerEquipDisplacementTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    // Three Aberrant equipped: the cap is met.
    const FBreakerItemInstance Helmet = MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 40);
    const FBreakerItemInstance Gloves = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 20);
    const FBreakerItemInstance Boots = MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 30);
    const TArray<FBreakerItemInstance> Loadout = {Helmet, Gloves, Boots};

    // A fourth Aberrant into an EMPTY slot ejects the weakest — the ilvl 20
    // gloves — not the piece in the target slot, because there is none.
    const FBreakerItemInstance Waist = MakeItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Aberrant, 50);
    FBreakerEquipPreview Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(Loadout, Waist);
    TestFalse(TEXT("The empty target slot displaces nothing itself"), Preview.bSlotOccupied);
    TestTrue(TEXT("A fourth Aberrant exceeds the cap"), Preview.bExceedsRarityLimit);
    TestEqual(TEXT("The preview reports the current count"), Preview.RarityCount, 3);
    TestEqual(TEXT("The preview reports the cap"), Preview.RarityLimit, 3);
    TestEqual(TEXT("The weakest Aberrant is ejected"), Preview.LimitDisplaced.ItemId.ToString(), Gloves.ItemId.ToString());

    // Swapping Aberrant for Aberrant IN THE SAME SLOT ejects nothing extra:
    // the outgoing piece already frees its place under the cap.
    const FBreakerItemInstance BetterGloves = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 45);
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(Loadout, BetterGloves);
    TestTrue(TEXT("The occupied slot is reported"), Preview.bSlotOccupied);
    TestEqual(TEXT("The slot swap names the outgoing gloves"), Preview.SlotDisplaced.ItemId.ToString(), Gloves.ItemId.ToString());
    TestFalse(TEXT("A same-slot same-rarity swap does not exceed the cap"), Preview.bExceedsRarityLimit);
    TestFalse(TEXT("Nothing extra is ejected"), Preview.LimitDisplaced.IsValid());

    // Ties break on wear order, so the disclosure is deterministic.
    const TArray<FBreakerItemInstance> Tied =
    {
        MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Aberrant, 30),
        MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Aberrant, 30),
        MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Aberrant, 30),
    };
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(Tied, Waist);
    TestEqual(TEXT("Equal item levels break on wear order"), static_cast<int32>(Preview.LimitDisplaced.Slot), static_cast<int32>(EBreakerEquipSlot::Helmet));

    // Anomalous is capped at one, and a lower rarity is never capped.
    const TArray<FBreakerItemInstance> OneAnomalous = {MakeItem(EBreakerEquipSlot::Necklace, EBreakerItemRarity::Anomalous, 10)};
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(OneAnomalous, MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Anomalous, 50));
    TestTrue(TEXT("A second Anomalous exceeds its cap of one"), Preview.bExceedsRarityLimit);
    Preview = UBreakerEquipmentComponent::PreviewEquipAgainst(OneAnomalous, MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Exceptional, 50));
    TestFalse(TEXT("Exceptional is never capped"), Preview.bExceedsRarityLimit);
    TestEqual(TEXT("Uncapped rarities report no limit"), Preview.RarityLimit, INDEX_NONE);

    // End to end: the cap is disclosed, never blocked. The equip succeeds, the
    // named piece leaves, and the loadout is still legal.
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    for (const FBreakerItemInstance& Item : Loadout) Equipment->EquipItem(Item);
    TestEqual(TEXT("Three Aberrant equipped"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 3);

    const FBreakerEquipPreview Live = Equipment->PreviewEquip(Waist);
    TestTrue(TEXT("The live preview agrees the cap is met"), Live.bExceedsRarityLimit);
    TestTrue(TEXT("Equipping over the cap is never refused"), Equipment->EquipItem(Waist));
    TestEqual(TEXT("The loadout stays at the cap"), Equipment->CountEquippedOfRarity(EBreakerItemRarity::Aberrant), 3);

    FBreakerItemInstance Occupant;
    TestFalse(TEXT("The ejected piece's slot is now empty"), Equipment->GetEquippedItem(EBreakerEquipSlot::Gloves, Occupant));
    TestTrue(TEXT("The new piece is equipped"), Equipment->GetEquippedItem(EBreakerEquipSlot::Waist, Occupant));
    TestEqual(TEXT("The piece named in the preview is the piece that left"), Occupant.ItemId.ToString(), Waist.ItemId.ToString());
    TestTrue(TEXT("The ejected piece returned to the backpack"),
        Equipment->GetBackpack().ContainsByPredicate([&Gloves](const FBreakerItemInstance& Item) { return Item.ItemId == Gloves.ItemId; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerAffixDeltaTest,
    "RiorsEdge.Items.Equipment.AffixDeltas",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerAffixDeltaTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    FBreakerItemInstance Candidate = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Exceptional, 40);
    Candidate.Affixes.Add(MakeAffix(TEXT("Core.Health"), 150.0f));       // better
    Candidate.Affixes.Add(MakeAffix(TEXT("Crit.Chance"), 4.0f));         // worse
    Candidate.Affixes.Add(MakeAffix(TEXT("Crit.Damage"), 20.0f));        // parity
    Candidate.Affixes.Add(MakeAffix(TEXT("Move.DashCooldown"), 9.0f));   // not on the equipped piece

    FBreakerItemInstance Reference = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Exceptional, 40);
    Reference.Affixes.Add(MakeAffix(TEXT("Core.Health"), 100.0f));
    Reference.Affixes.Add(MakeAffix(TEXT("Crit.Chance"), 6.0f));
    Reference.Affixes.Add(MakeAffix(TEXT("Crit.Damage"), 20.0f));

    TArray<FBreakerAffixComparison> Deltas = UBreakerEquipmentComponent::CompareAffixes(Candidate, Reference);
    TestEqual(TEXT("One row per candidate affix"), Deltas.Num(), 4);
    TestEqual(TEXT("A higher roll is better"), static_cast<int32>(Deltas[0].Delta), static_cast<int32>(EBreakerAffixDelta::Better));
    TestEqual(TEXT("The compared value is the equipped roll"), Deltas[0].ComparedValue, 100.0f);
    TestEqual(TEXT("The difference is signed"), Deltas[0].GetDifference(), 50.0f);
    TestEqual(TEXT("A lower roll is worse"), static_cast<int32>(Deltas[1].Delta), static_cast<int32>(EBreakerAffixDelta::Worse));
    TestEqual(TEXT("An equal roll is parity"), static_cast<int32>(Deltas[2].Delta), static_cast<int32>(EBreakerAffixDelta::Parity));
    TestEqual(TEXT("A stat the equipped piece lacks is better"), static_cast<int32>(Deltas[3].Delta), static_cast<int32>(EBreakerAffixDelta::Better));
    TestEqual(TEXT("A stat the equipped piece lacks compares against zero"), Deltas[3].ComparedValue, 0.0f);

    // Matching is by stat target and bucket, so two equipped affixes feeding
    // the same stat are one number to the player.
    FBreakerItemInstance DoubleUp = MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Exceptional, 40);
    DoubleUp.Affixes.Add(MakeAffix(TEXT("Core.Health"), 90.0f));
    DoubleUp.Affixes.Add(MakeAffix(TEXT("Core.Health"), 80.0f));
    Deltas = UBreakerEquipmentComponent::CompareAffixes(Candidate, DoubleUp);
    TestEqual(TEXT("Same-target rolls sum before comparison"), Deltas[0].ComparedValue, 170.0f);
    TestEqual(TEXT("Losing on the total reads as worse"), static_cast<int32>(Deltas[0].Delta), static_cast<int32>(EBreakerAffixDelta::Worse));

    // An empty slot: everything is an improvement, and the preview says so.
    const FBreakerEquipPreview Preview = UBreakerEquipmentComponent::PreviewEquipAgainst({}, Candidate);
    TestFalse(TEXT("An empty slot displaces nothing"), Preview.bSlotOccupied);
    TestEqual(TEXT("The preview carries the affix deltas"), Preview.AffixDeltas.Num(), 4);
    for (const FBreakerAffixComparison& Row : Preview.AffixDeltas)
    {
        TestEqual(TEXT("Every affix improves on an empty slot"), static_cast<int32>(Row.Delta), static_cast<int32>(EBreakerAffixDelta::Better));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerBulkDiscardCountTest,
    "RiorsEdge.Items.Equipment.BulkDiscardCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerBulkDiscardCountTest::RunTest(const FString& Parameters)
{
    using namespace BreakerEquipmentTestHelpers;

    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>(NewObject<AActor>());
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Helmet, EBreakerItemRarity::Standard, 10));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Boots, EBreakerItemRarity::Standard, 12));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Gloves, EBreakerItemRarity::Uncommon, 14));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Waist, EBreakerItemRarity::Exceptional, 20));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Necklace, EBreakerItemRarity::Aberrant, 22));
    Equipment->AddToBackpack(MakeItem(EBreakerEquipSlot::Primary, EBreakerItemRarity::Anomalous, 24));
    // Equipped gear is a separate container and is never a discard candidate.
    Equipment->EquipItem(MakeItem(EBreakerEquipSlot::Secondary, EBreakerItemRarity::Standard, 5));

    TestEqual(TEXT("Below Uncommon counts only the Standards"), Equipment->CountBackpackBelowRarity(EBreakerItemRarity::Uncommon), 2);
    const int32 Predicted = Equipment->CountBackpackBelowRarity(EBreakerItemRarity::Exceptional);
    TestEqual(TEXT("Below Exceptional counts Standard and Uncommon"), Predicted, 3);

    // The number the modal states is the number destroyed — same predicate.
    const int32 Removed = Equipment->DiscardBackpackBelowRarity(EBreakerItemRarity::Exceptional);
    TestEqual(TEXT("The stated count is the destroyed count"), Removed, Predicted);
    TestEqual(TEXT("Nothing left to discard"), Equipment->CountBackpackBelowRarity(EBreakerItemRarity::Exceptional), 0);

    // Aberrant and Anomalous are above every threshold the screen offers, so
    // a bulk discard can never take them.
    TestTrue(TEXT("Aberrant survives"), Equipment->GetBackpack().ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Rarity == EBreakerItemRarity::Aberrant; }));
    TestTrue(TEXT("Anomalous survives"), Equipment->GetBackpack().ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Rarity == EBreakerItemRarity::Anomalous; }));
    TestTrue(TEXT("Equipped gear is untouched"), Equipment->GetEquipped().ContainsByPredicate(
        [](const FBreakerItemInstance& Item) { return Item.Slot == EBreakerEquipSlot::Secondary; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerDodgeBlockTest,
    "RiorsEdge.Combat.Defense.DodgeAndBlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerDodgeBlockTest::RunTest(const FString& Parameters)
{
    FBreakerDamageRequest Request;
    Request.BaseDamage = 100.0f;
    Request.bCanCritical = false;
    Request.DamageFamily = EBreakerDamageFamily::TrueDamage;

    FBreakerDefenseState Defense;
    Defense.Health = 1000.0f;

    // A guaranteed dodge fully evades the hit.
    Defense.DodgeChance = 1.0f;
    FBreakerDamageResult Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestTrue(TEXT("Guaranteed dodge evades"), Result.bDodged);
    TestEqual(TEXT("No health damage on a dodge"), Result.HealthDamage, 0.0f);

    // A guaranteed block halves the hit at default mitigation.
    Defense.DodgeChance = 0.0f;
    Defense.BlockChance = 1.0f;
    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestTrue(TEXT("Guaranteed block triggers"), Result.bBlocked);
    TestEqual(TEXT("Block mitigates half"), Result.HealthDamage, 50.0f);

    // Dodge and block never apply to damage over time.
    Defense.DodgeChance = 1.0f;
    Request.bIsDamageOverTime = true;
    Result = UBreakerDamageLibrary::ResolveDamage(Request, Defense);
    TestFalse(TEXT("DoTs cannot be dodged"), Result.bDodged);
    TestFalse(TEXT("DoTs cannot be blocked"), Result.bBlocked);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerRarityRollTest,
    "RiorsEdge.Items.Loot.RarityWeights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerRarityRollTest::RunTest(const FString& Parameters)
{
    int32 StandardCount = 0;
    int32 StandardCountWithBonus = 0;
    for (int32 Seed = 0; Seed < 2000; ++Seed)
    {
        if (UBreakerLootLibrary::RollRarity(Seed, 0.0f) == EBreakerItemRarity::Standard) ++StandardCount;
        if (UBreakerLootLibrary::RollRarity(Seed, 50.0f) == EBreakerItemRarity::Standard) ++StandardCountWithBonus;
    }
    TestTrue(TEXT("Standard dominates at zero bonus"), StandardCount > 1000);
    TestTrue(TEXT("Drop chance shifts weight out of Standard"), StandardCountWithBonus < StandardCount);
    TestEqual(TEXT("Rarity roll is deterministic"), static_cast<int32>(UBreakerLootLibrary::RollRarity(42, 10.0f)), static_cast<int32>(UBreakerLootLibrary::RollRarity(42, 10.0f)));
    return true;
}

#endif
