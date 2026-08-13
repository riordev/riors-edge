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
