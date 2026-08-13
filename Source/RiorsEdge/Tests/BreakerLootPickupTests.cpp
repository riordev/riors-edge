#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Items/BreakerEquipmentComponent.h"
#include "Items/BreakerItemTypes.h"
#include "Items/BreakerLootPickup.h"

namespace
{
    FBreakerItemInstance MakeTestItem(EBreakerItemRarity Rarity, EBreakerEquipSlot Slot)
    {
        FBreakerItemInstance Item;
        Item.ItemId = FGuid::NewGuid();
        Item.DefinitionId = TEXT("TestDrop");
        Item.Rarity = Rarity;
        Item.Slot = Slot;
        Item.ItemLevel = 12;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupHoldsItemTest,
    "RiorsEdge.Items.LootPickup.HoldsItem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupHoldsItemTest::RunTest(const FString& Parameters)
{
    ABreakerLootPickup* Pickup = NewObject<ABreakerLootPickup>();
    TestNotNull(TEXT("Pickup constructs"), Pickup);
    if (!Pickup) return false;

    TestFalse(TEXT("A fresh pickup holds no valid item"), Pickup->GetItem().IsValid());

    const FBreakerItemInstance Item = MakeTestItem(EBreakerItemRarity::Aberrant, EBreakerEquipSlot::Boots);
    Pickup->SetItem(Item);
    TestTrue(TEXT("SetItem stores the item id"), Pickup->GetItem().ItemId == Item.ItemId);
    TestEqual(TEXT("SetItem stores the rarity"), static_cast<int32>(Pickup->GetItem().Rarity), static_cast<int32>(EBreakerItemRarity::Aberrant));
    TestEqual(TEXT("SetItem stores the slot"), static_cast<int32>(Pickup->GetItem().Slot), static_cast<int32>(EBreakerEquipSlot::Boots));
    TestTrue(TEXT("Pickup lives for five minutes"), FMath::IsNearlyEqual(Pickup->InitialLifeSpan, 300.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupLabelTest,
    "RiorsEdge.Items.LootPickup.DisplayLabel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupLabelTest::RunTest(const FString& Parameters)
{
    ABreakerLootPickup* Pickup = NewObject<ABreakerLootPickup>();
    if (!Pickup) return false;

    Pickup->SetItem(MakeTestItem(EBreakerItemRarity::Exceptional, EBreakerEquipSlot::BodyArmour));
    TestEqual(TEXT("Label is RARITY SLOTNAME"), Pickup->GetDisplayLabel().ToString(), FString(TEXT("EXCEPTIONAL BODYARMOUR")));

    Pickup->SetItem(MakeTestItem(EBreakerItemRarity::Anomalous, EBreakerEquipSlot::Primary));
    TestEqual(TEXT("Label tracks the current item"), Pickup->GetDisplayLabel().ToString(), FString(TEXT("ANOMALOUS PRIMARY")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupRarityColorTest,
    "RiorsEdge.Items.LootPickup.RarityColor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupRarityColorTest::RunTest(const FString& Parameters)
{
    // Beam chroma must match the UI rarity colours exactly, including the
    // reserved teal that only rift-class objects (Anomalous) may use.
    TestTrue(TEXT("Uncommon is blue"), ABreakerLootPickup::ColorForRarity(EBreakerItemRarity::Uncommon).Equals(FLinearColor(0.25f, 0.55f, 1.0f)));
    TestTrue(TEXT("Exceptional is purple"), ABreakerLootPickup::ColorForRarity(EBreakerItemRarity::Exceptional).Equals(FLinearColor(0.72f, 0.4f, 1.0f)));
    TestTrue(TEXT("Aberrant is red"), ABreakerLootPickup::ColorForRarity(EBreakerItemRarity::Aberrant).Equals(FLinearColor(1.0f, 0.25f, 0.25f)));
    TestTrue(TEXT("Anomalous keeps rift teal"), ABreakerLootPickup::ColorForRarity(EBreakerItemRarity::Anomalous).Equals(FLinearColor(0.15f, 0.95f, 0.85f)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBreakerLootPickupTransferTest,
    "RiorsEdge.Items.LootPickup.TransferToBackpack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBreakerLootPickupTransferTest::RunTest(const FString& Parameters)
{
    // TryPickup itself needs a live actor (authority + Destroy), which a
    // world-free test cannot provide, so this covers the payload it moves:
    // the exact instance the pickup holds lands in the backpack.
    UBreakerEquipmentComponent* Equipment = NewObject<UBreakerEquipmentComponent>();
    ABreakerLootPickup* Pickup = NewObject<ABreakerLootPickup>();
    if (!Equipment || !Pickup) return false;

    const FBreakerItemInstance Item = MakeTestItem(EBreakerItemRarity::Uncommon, EBreakerEquipSlot::Helmet);
    Pickup->SetItem(Item);

    TestEqual(TEXT("Backpack starts empty"), Equipment->GetBackpack().Num(), 0);
    TestTrue(TEXT("The pickup carries a backpack-worthy item"), Pickup->GetItem().IsValid());
    TestTrue(TEXT("The carried item is the one that was dropped"), Pickup->GetItem().ItemId == Item.ItemId);

    // The same authority gate TryPickup relies on: an ownerless component
    // refuses the add, so nothing sneaks into the backpack off-server.
    Equipment->AddToBackpack(Pickup->GetItem());
    TestEqual(TEXT("Ownerless equipment refuses the add"), Equipment->GetBackpack().Num(), 0);

    TestFalse(TEXT("TryPickup rejects a null character"), Pickup->TryPickup(nullptr));
    return true;
}

#endif
