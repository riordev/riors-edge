#include "Misc/AutomationTest.h"
#include "Items/BreakerAffixLibrary.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerLootLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBreakerWeaponUtilityAffixTest, "RiorsEdge.Items.PrimaryWeaponUtilityAffixes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBreakerWeaponUtilityAffixTest::RunTest(const FString& Parameters)
{
    const auto Empty = UBreakerEquipmentComponent::AggregateStats({});
    TestEqual(TEXT("No gear retains authored magazine"), Empty.PrimaryMagazineCapacityMultiplier, 1.0f);
    TestEqual(TEXT("No gear retains authored range"), Empty.PrimaryEffectiveRangeMultiplier, 1.0f);
    for (bool bMagazine : {true, false})
    {
        const FName Id(bMagazine ? TEXT("Weapon.MagazineCapacity") : TEXT("Weapon.EffectiveRange"));
        const auto* Definition = UBreakerAffixLibrary::FindAffix(UBreakerAffixLibrary::GetSliceAffixPool(), Id);
        if (!TestNotNull(TEXT("Ordinary authored weapon utility line"), Definition)) return false;
        TestEqual(TEXT("Current T12 anchor"), Definition->ValueAtT12, bMagazine ? 8.0f : 5.0f);
        TestEqual(TEXT("Current T1 anchor"), Definition->ValueAtT1, bMagazine ? 35.0f : 22.0f);
        TestEqual(TEXT("Utility uses Increased, not More"), Definition->StatBucket, EBreakerStatBucket::IncreasedPercent);
        TestEqual(TEXT("Exactly one authored slot"), Definition->AllowedSlots.Num(), 1);
        TestTrue(TEXT("Primary is authored"), Definition->AllowsSlot(EBreakerEquipSlot::Primary));
        TestEqual(TEXT("T0 follows current shared 2.2 spike"), UBreakerAffixLibrary::ValueForTier(*Definition, 0), Definition->ValueAtT1 * 2.2f, 0.001f);
        TestEqual(TEXT("Top follows current shared 3.6 spike"), UBreakerAffixLibrary::ValueForTier(*Definition, -1), Definition->ValueAtT1 * 3.6f, 0.001f);
        FBreakerItemInstance Item;
        int32 Index = INDEX_NONE;
        for (int32 Seed = 1; Seed <= 4096; ++Seed)
        {
            Item = UBreakerLootLibrary::RollItem(TEXT("Test.Utility"), EBreakerEquipSlot::Primary, EBreakerItemRarity::Standard, 1, Seed);
            Index = Item.Affixes.IndexOfByPredicate([&](const auto& Line) { return Line.AffixId == Id; });
            if (Index != INDEX_NONE) break;
        }
        if (!TestTrue(TEXT("Affix participates in actual ordinary loot"), Index != INDEX_NONE)) return false;
        const float Value = Item.Affixes[Index].Value;
        const auto Stats = UBreakerEquipmentComponent::AggregateStats({Item});
        TestEqual(TEXT("Actual Primary contributes its rolled percentage"),
            bMagazine ? Stats.PrimaryMagazineCapacityMultiplier : Stats.PrimaryEffectiveRangeMultiplier, 1.0f + Value / 100.0f, 0.0001f);
        for (EBreakerEquipSlot Slot : {EBreakerEquipSlot::Secondary, EBreakerEquipSlot::Helmet, EBreakerEquipSlot::BodyArmour,
            EBreakerEquipSlot::Gloves, EBreakerEquipSlot::Boots, EBreakerEquipSlot::Necklace, EBreakerEquipSlot::Waist})
        {
            // Malformed imported/save copy, not a claimed legal loot roll.
            FBreakerItemInstance WrongSlot = Item;
            WrongSlot.Slot = Slot;
            const auto Rejected = UBreakerEquipmentComponent::AggregateStats({WrongSlot});
            TestEqual(TEXT("Malformed other-slot copy cannot affect Primary magazine"), Rejected.PrimaryMagazineCapacityMultiplier, 1.0f);
            TestEqual(TEXT("Malformed other-slot copy cannot affect Primary range"), Rejected.PrimaryEffectiveRangeMultiplier, 1.0f);
        }
    }
    return true;
}
#endif
